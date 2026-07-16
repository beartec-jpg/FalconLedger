#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""On-ledger E2E: HF-breach default with collateral surplus credited to vault LPs."""

from __future__ import annotations

import json
import math
import subprocess
import sys
import time
from pathlib import Path

from lend_epoch_constants import DEFAULT_LOAN_EPOCHS, payment_interval_for_epochs

REPO_ROOT = Path(__file__).resolve().parent.parent
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
    print(f"[surplus-e2e] {msg}")


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


def iou_amount(raw) -> float:
    if raw is None:
        return 0.0
    if isinstance(raw, str):
        return float(raw)
    if isinstance(raw, (int, float)):
        return float(raw)
    if isinstance(raw, dict):
        return float(raw.get("value", 0))
    return 0.0


def vault_assets(rpc: Rpc, vault_id: str) -> tuple[float, float]:
    node = rpc.public_rpc(
        "ledger_entry", {"index": vault_id, "ledger_index": "validated"}
    ).get("node", {})
    return iou_amount(node.get("AssetsTotal")), iou_amount(node.get("AssetsAvailable"))


def amm_falcon_per_fusdc(rpc: Rpc, issuer: str) -> float:
    r = rpc.public_rpc("amm_info", {
        "asset": {"currency": "XRP"},
        "asset2": {"currency": CURRENCY, "issuer": issuer},
    })
    amm = r.get("amm") or r
    xrp = float(amm.get("amount", 0)) / DROPS if isinstance(amm.get("amount"), str) else float(
        (amm.get("amount") or {}).get("value", 0)
    )
    usdc = float((amm.get("amount2") or {}).get("value", 0))
    if xrp <= 0 or usdc <= 0:
        raise RuntimeError("AMM unavailable")
    return usdc / xrp


def hf_bps(collateral_falcon: float, debt_fusdc: float, price: float) -> float:
    if debt_fusdc <= 0 or collateral_falcon <= 0 or price <= 0:
        return 0.0
    return (collateral_falcon * price / debt_fusdc) * 10000


def main() -> int:
    rpc = Rpc(ADMIN_RPC, PUBLIC_RPC, CONTAINER)
    manifest = json.loads(Path("/var/lib/qxrp-stables/lending_state.json").read_text())
    st = json.loads(Path("/var/lib/qxrp-stables/stables_state.json").read_text())
    faucet = json.loads(Path("/root/qxrp-bootstrap/faucet.json").read_text())

    issuer = st["qUSDC_issuer"]["address"]
    broker_id = manifest["loan_broker_id"]
    vault_id = manifest["vault_id"]
    faucet_acct = faucet["account"]
    faucet_sec = faucet["falcon_secret"]
    principal = 5.0
    payment_interval = payment_interval_for_epochs(DEFAULT_LOAN_EPOCHS)

    borrower, borrower_sec = propose_wallet(rpc)
    liquidator, liquidator_sec = propose_wallet(rpc)
    log(f"borrower={borrower}")

    for dest in (borrower, liquidator):
        for amount in (3000, 5000):
            tx = {
                **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
                "TransactionType": "Payment",
                "Destination": dest,
                "Amount": str(amount * DROPS),
            }
            er, h = sign_submit(rpc, faucet_sec, tx)
            if wait_tx(rpc, h) != "tesSUCCESS":
                log(f"fund failed {er}")
                return 1
            time.sleep(1)

    tx = {
        **base_tx(rpc, borrower, account_seq(rpc, borrower)),
        "TransactionType": "TrustSet",
        "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "1000000"},
    }
    er, h = sign_submit(rpc, borrower_sec, tx)
    if wait_tx(rpc, h) != "tesSUCCESS":
        return 1

    price = amm_falcon_per_fusdc(rpc, issuer)
    # Post at 1.5 HF then dump ~8% of collateral to land just under 1.1 with surplus.
    falcon_collateral = math.ceil((principal * 1.5 / price) * 1.02)
    coll_drops = str(int(falcon_collateral * DROPS))
    log(f"borrow {principal} F-USDC · {falcon_collateral} FALCON @ price {price:.6f}")

    vault_total_before, vault_avail_before = vault_assets(rpc, vault_id)

    tx = {
        **base_tx(rpc, borrower, account_seq(rpc, borrower), "24"),
        "TransactionType": "LoanSet",
        "LoanBrokerID": broker_id,
        "PrincipalRequested": str(principal),
        "Collateral": coll_drops,
        "InterestRate": 500,
        "PaymentInterval": payment_interval,
        "PaymentTotal": 1,
        "GracePeriod": 3600,
        "Flags": 0x00010000,
    }
    er, h = sign_submit(rpc, borrower_sec, tx)
    if wait_tx(rpc, h) != "tesSUCCESS":
        log(f"borrow failed {er}")
        return 1

    objs = rpc.public_rpc("account_objects", {
        "account": borrower,
        "type": "loan",
        "ledger_index": "validated",
    }).get("account_objects", [])
    loan = objs[-1]
    loan_id = loan["index"]
    debt = float(loan.get("TotalValueOutstanding", principal))
    pre_hf = hf_bps(falcon_collateral, debt, price) / 10000
    log(f"loan {loan_id[:16]}… debt={debt:.6f} HF={pre_hf:.3f}")

    # Small dump to breach 1.1 HF while keeping collateral value > debt.
    dump = max(50, int(falcon_collateral * 0.12))
    tx = {
        **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
        "TransactionType": "TrustSet",
        "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "1000000"},
    }
    sign_submit(rpc, faucet_sec, tx)
    time.sleep(1)
    max_usdc = f"{dump * price * 0.9:.6f}"
    min_usdc = f"{dump * price * 0.4:.6f}"
    tx = {
        **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct), "24"),
        "TransactionType": "Payment",
        "Account": faucet_acct,
        "Destination": faucet_acct,
        "Amount": {"currency": CURRENCY, "issuer": issuer, "value": max_usdc},
        "SendMax": str(int(dump * DROPS)),
        "DeliverMin": {"currency": CURRENCY, "issuer": issuer, "value": min_usdc},
        "Flags": TF_PARTIAL_PAYMENT,
    }
    er, h = sign_submit(rpc, faucet_sec, tx)
    if wait_tx(rpc, h) != "tesSUCCESS":
        log(f"amm dump failed {er}")
        return 1

    price = amm_falcon_per_fusdc(rpc, issuer)
    cur_hf = hf_bps(falcon_collateral, debt, price) / 10000
    coll_value = falcon_collateral * price
    expected_surplus = max(0.0, coll_value - debt)
    log(f"after dump HF={cur_hf:.3f} collateral_value={coll_value:.6f} expected_surplus≈{expected_surplus:.6f}")

    if cur_hf >= 1.1:
        log("HF still >= 1.1 after dump; increase dump size in script")
        return 1
    if expected_surplus <= 0.001:
        log("no collateral surplus expected; adjust dump")
        return 1

    tx = {
        **base_tx(rpc, liquidator, account_seq(rpc, liquidator)),
        "TransactionType": "LoanManage",
        "LoanID": loan_id,
        "Flags": TF_LOAN_DEFAULT,
    }
    er, h = sign_submit(rpc, liquidator_sec, tx)
    log(f"default submit {er}")
    if wait_tx(rpc, h) != "tesSUCCESS":
        return 1

    vault_total_after, vault_avail_after = vault_assets(rpc, vault_id)
    total_delta = vault_total_after - vault_total_before
    avail_delta = vault_avail_after - vault_avail_before
    log(f"vault AssetsTotal: {vault_total_before:.4f} → {vault_total_after:.4f} (Δ {total_delta:.6f})")
    log(f"vault AssetsAvailable: {vault_avail_before:.4f} → {vault_avail_after:.4f} (Δ {avail_delta:.6f})")

    if total_delta < expected_surplus * 0.5:
        log("vault AssetsTotal did not increase by expected collateral surplus")
        return 1
    if avail_delta < expected_surplus * 0.5:
        log("vault AssetsAvailable did not increase by expected collateral surplus")
        return 1

    log("E2E PASS: default forfeits FALCON to pool + CollateralSurplus credits vault")
    return 0


if __name__ == "__main__":
    sys.exit(main())