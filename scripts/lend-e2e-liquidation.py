#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""On-ledger E2E: permissionless borrow → AMM price shock → LoanManage default."""

from __future__ import annotations

import json
import math
import subprocess
import sys
import time
from pathlib import Path

DROPS = 1_000_000
CURRENCY = "QUC"
PUBLIC_RPC = "http://46.224.0.140:6005"
ADMIN_RPC = "http://127.0.0.1:5005"
CONTAINER = "qxrp-full"
TF_LOAN_DEFAULT = 0x00010000
TF_PARTIAL_PAYMENT = 0x00020000
HF_LIQUIDATION_BPS = 11000


class Rpc:
    def __init__(self, admin_url: str, public_url: str, container: str):
        self.admin_url = admin_url
        self.public_url = public_url
        self.container = container

    def _post(self, url: str, method: str, params: dict | None = None) -> dict:
        payload = json.dumps({"method": method, "params": [params or {}]}).encode()
        cmd = [
            "docker", "exec", self.container,
            "curl", "-sf", "-X", "POST", url,
            "-H", "Content-Type: application/json",
            "-d", payload.decode(),
        ]
        out = json.loads(subprocess.check_output(cmd, text=True))
        if out.get("error"):
            raise RuntimeError(out["error"])
        return out["result"]

    def admin_rpc(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.admin_url, method, params)

    def public_rpc(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.public_url, method, params)


def log(msg: str) -> None:
    print(f"[liq-e2e] {msg}")


def sign_submit(rpc: Rpc, secret: str, tx: dict) -> tuple[str, str]:
    params = (
        {"falcon_secret": secret, "tx_json": tx}
        if not secret.startswith(("s", "S"))
        else {"secret": secret, "tx_json": tx}
    )
    signed = rpc.admin_rpc("sign", params)
    if signed.get("status") != "success":
        raise RuntimeError(f"sign failed: {signed}")
    blob = signed["tx_blob"]
    tx_hash = signed.get("tx_json", {}).get("hash", "")
    sub = rpc.public_rpc("submit", {"tx_blob": blob})
    return sub.get("engine_result", "?"), tx_hash


def wait_tx(rpc: Rpc, tx_hash: str) -> str:
    for _ in range(30):
        time.sleep(2)
        try:
            r = rpc.public_rpc("tx", {"transaction": tx_hash, "binary": False})
            if r.get("validated"):
                return r.get("meta", {}).get("TransactionResult", "?")
        except Exception:
            pass
    return "TIMEOUT"


def account_seq(rpc: Rpc, account: str) -> int:
    r = rpc.public_rpc("account_info", {"account": account, "ledger_index": "validated"})
    return int(r["account_data"]["Sequence"])


def last_ledger(rpc: Rpc) -> int:
    r = rpc.public_rpc("ledger", {"ledger_index": "validated"})
    return int(r["ledger_index"]) + 30


def base_tx(rpc: Rpc, account: str, sequence: int, fee: str = "12") -> dict:
    return {
        "Account": account,
        "Fee": fee,
        "Sequence": sequence,
        "LastLedgerSequence": last_ledger(rpc),
    }


def propose_wallet(rpc: Rpc) -> tuple[str, str]:
    r = rpc.admin_rpc("wallet_propose", {})
    return r["account_id"], r["falcon_secret"]


def falcon_balance(rpc: Rpc, account: str) -> float:
    r = rpc.public_rpc("account_info", {"account": account, "ledger_index": "validated"})
    return int(r["account_data"]["Balance"]) / DROPS


def amount_value(raw: object) -> float:
    if raw is None:
        return 0.0
    if isinstance(raw, str):
        return float(raw) / DROPS
    if isinstance(raw, dict):
        return float(raw.get("value", 0))
    return float(raw)


def amm_falcon_per_fusdc(rpc: Rpc, issuer: str) -> float:
    r = rpc.public_rpc("amm_info", {
        "asset": {"currency": "XRP"},
        "asset2": {"currency": CURRENCY, "issuer": issuer},
    })
    amm = r.get("amm") or r
    xrp = amount_value(amm.get("amount"))
    usdc = amount_value(amm.get("amount2"))
    if xrp <= 0 or usdc <= 0:
        raise RuntimeError("AMM pool empty or unavailable")
    return usdc / xrp


def hf_bps(collateral_falcon: float, debt_fusdc: float, price: float) -> float:
    if debt_fusdc <= 0 or collateral_falcon <= 0 or price <= 0:
        return 0.0
    return (collateral_falcon * price / debt_fusdc) * 10000


def loan_entry(rpc: Rpc, loan_id: str) -> dict:
    r = rpc.public_rpc("ledger_entry", {"index": loan_id, "ledger_index": "validated"})
    return r.get("node", {})


def main() -> int:
    rpc = Rpc(ADMIN_RPC, PUBLIC_RPC, CONTAINER)

    st = json.loads(Path("/var/lib/qxrp-stables/stables_state.json").read_text())
    manifest = json.loads(Path("/var/lib/qxrp-stables/lending_state.json").read_text())
    faucet = json.loads(Path("/root/qxrp-bootstrap/faucet.json").read_text())

    issuer = st["qUSDC_issuer"]["address"]
    issuer_sec = st["qUSDC_issuer"]["falcon_secret"]
    faucet_acct = faucet["account"]
    faucet_sec = faucet["falcon_secret"]
    broker_id = manifest["loan_broker_id"]
    vault_id = manifest["vault_id"]

    vault_node = rpc.public_rpc(
        "ledger_entry", {"index": vault_id, "ledger_index": "validated"}
    ).get("node", {})
    vault_pseudo = vault_node.get("Account")
    if not vault_pseudo:
        log("vault pseudo-account missing from ledger_entry")
        return 1

    borrower, borrower_sec = propose_wallet(rpc)
    liquidator, liquidator_sec = propose_wallet(rpc)
    log(f"borrower={borrower}")
    log(f"liquidator={liquidator}")

    steps: list[tuple[str, str, str, str]] = []

    def run_step(name: str, secret: str, tx: dict) -> bool:
        er, h = sign_submit(rpc, secret, tx)
        final = wait_tx(rpc, h) if er in ("tesSUCCESS", "terQUEUED") else er
        steps.append((name, er, final, h))
        log(f"{name}: submit={er} validated={final} hash={h}")
        return final == "tesSUCCESS"

    # Fund borrower + liquidator
    for dest, label in ((borrower, "borrower"), (liquidator, "liquidator")):
        tx = {
            **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
            "TransactionType": "Payment",
            "Destination": dest,
            "Amount": str(5000 * DROPS),
        }
        if not run_step(f"fund_{label}", faucet_sec, tx):
            return 1

    tx = {
        **base_tx(rpc, borrower, account_seq(rpc, borrower)),
        "TransactionType": "TrustSet",
        "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "1000000"},
    }
    if not run_step("trust_borrower", borrower_sec, tx):
        return 1

    principal = 5.0
    price = amm_falcon_per_fusdc(rpc, issuer)
    # Minimum collateral at 1.5 HF (no borrow buffer) — easier to liquidate after price drop
    falcon_collateral = math.ceil(principal * 1.5 / price)
    coll_drops = str(int(falcon_collateral * DROPS))
    log(f"AMM {price:.6f} F-USDC/FALCON — {falcon_collateral} FALCON collateral for {principal} F-USDC")

    tx = {
        **base_tx(rpc, borrower, account_seq(rpc, borrower), "24"),
        "TransactionType": "LoanSet",
        "LoanBrokerID": broker_id,
        "PrincipalRequested": "5",
        "Collateral": coll_drops,
        "InterestRate": 500,
        "PaymentInterval": 86400,
        "PaymentTotal": 1,
        "GracePeriod": 3600,
        "Flags": 0x00010000,
    }
    if not run_step("borrow", borrower_sec, tx):
        return 1

    objs = rpc.public_rpc("account_objects", {
        "account": borrower,
        "type": "loan",
        "ledger_index": "validated",
    }).get("account_objects", [])
    if not objs:
        log("no loan after borrow")
        return 1
    loan = objs[-1]
    loan_id = loan.get("index")
    debt = float(loan.get("TotalValueOutstanding", principal))
    log(f"loan_id={loan_id} debt={debt}")

    pre_hf = hf_bps(falcon_collateral, debt, price)
    log(f"pre-crash HF={pre_hf/10000:.3f} ({pre_hf:.0f} bps)")

    # Trust line for faucet to receive F-USDC from AMM swap
    tx = {
        **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
        "TransactionType": "TrustSet",
        "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "10000000"},
    }
    if not run_step("trust_faucet", faucet_sec, tx):
        return 1

    # Dump FALCON into AMM until HF < 1.1
    swap_sizes = [4000, 8000, 12000, 20000]
    crashed = False
    for swap_falcon in swap_sizes:
        price = amm_falcon_per_fusdc(rpc, issuer)
        cur_hf = hf_bps(falcon_collateral, debt, price)
        if cur_hf < HF_LIQUIDATION_BPS:
            log(f"HF already {cur_hf/10000:.3f} — skip swap")
            crashed = True
            break

        # Sell up to swap_falcon FALCON for F-USDC (Amount = max deliver, not 1 USDC)
        max_usdc_out = f"{swap_falcon * price * 0.85:.6f}"
        min_usdc_out = f"{swap_falcon * price * 0.50:.6f}"
        tx = {
            **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct), "24"),
            "TransactionType": "Payment",
            "Destination": faucet_acct,
            "Amount": {"currency": CURRENCY, "issuer": issuer, "value": max_usdc_out},
            "SendMax": str(int(swap_falcon * DROPS)),
            "DeliverMin": {"currency": CURRENCY, "issuer": issuer, "value": min_usdc_out},
            "Flags": TF_PARTIAL_PAYMENT,
        }
        if not run_step(f"amm_dump_{swap_falcon}", faucet_sec, tx):
            continue
        price = amm_falcon_per_fusdc(rpc, issuer)
        cur_hf = hf_bps(falcon_collateral, debt, price)
        log(f"after {swap_falcon} FALCON dump: price={price:.6f} HF={cur_hf/10000:.3f}")
        if cur_hf < HF_LIQUIDATION_BPS:
            crashed = True
            break

    if not crashed:
        log("could not breach HF < 1.1 via AMM dump")
        return 1

    liq_falcon_before = falcon_balance(rpc, liquidator)
    vault_falcon_before = falcon_balance(rpc, vault_pseudo)
    tx = {
        **base_tx(rpc, liquidator, account_seq(rpc, liquidator)),
        "TransactionType": "LoanManage",
        "LoanID": loan_id,
        "Flags": TF_LOAN_DEFAULT,
    }
    if not run_step("liquidate", liquidator_sec, tx):
        return 1

    entry = loan_entry(rpc, loan_id)
    flags = int(entry.get("Flags", 0))
    if (flags & 0x00010000) == 0:  # lsfLoanDefault
        log(f"loan not in default state flags={flags}")
        return 1

    liq_falcon_after = falcon_balance(rpc, liquidator)
    liq_gained = liq_falcon_after - liq_falcon_before
    if liq_gained > 0.01:
        log(f"liquidator must not receive collateral (gained {liq_gained:.4f} FALCON)")
        return 1

    vault_falcon_after = falcon_balance(rpc, vault_pseudo)
    vault_gained = vault_falcon_after - vault_falcon_before
    log(
        f"vault pseudo FALCON: {vault_falcon_before:.4f} → {vault_falcon_after:.4f} "
        f"(+{vault_gained:.4f})"
    )
    if vault_gained < falcon_collateral * 0.99:
        log("vault did not receive seized collateral")
        return 1

    log("── summary ──")
    for name, submit, validated, h in steps:
        log(f"  {name}: {submit} → {validated} ({h})")
    log("E2E PASS: borrow → HF breach → permissionless liquidation")
    return 0


if __name__ == "__main__":
    sys.exit(main())