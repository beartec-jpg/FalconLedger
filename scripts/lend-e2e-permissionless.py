#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""On-ledger E2E: fund wallets, supply, permissionless borrow, repay."""

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
    print(f"[e2e] {msg}")


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


def fusdc_balance(rpc: Rpc, account: str, issuer: str) -> float:
    r = rpc.public_rpc("account_lines", {"account": account, "ledger_index": "validated"})
    for line in r.get("lines", []):
        if line.get("currency") == CURRENCY and line.get("account") == issuer:
            return float(line.get("balance", 0))
    return 0.0


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


def main() -> int:
    rpc = Rpc(ADMIN_RPC, PUBLIC_RPC, CONTAINER)

    st = json.loads(Path("/var/lib/qxrp-stables/stables_state.json").read_text())
    manifest = json.loads(Path("/var/lib/qxrp-stables/lending_state.json").read_text())
    faucet = json.loads(Path("/root/qxrp-bootstrap/faucet.json").read_text())

    issuer = st["qUSDC_issuer"]["address"]
    issuer_sec = st["qUSDC_issuer"]["falcon_secret"]
    faucet_acct = faucet["account"]
    faucet_sec = faucet["falcon_secret"]
    vault_id = manifest["vault_id"]
    broker_id = manifest["loan_broker_id"]

    lender, lender_sec = propose_wallet(rpc)
    borrower, borrower_sec = propose_wallet(rpc)
    log(f"lender={lender}")
    log(f"borrower={borrower}")

    steps: list[tuple[str, str, str]] = []

    def run_step(name: str, secret: str, tx: dict) -> bool:
        er, h = sign_submit(rpc, secret, tx)
        final = wait_tx(rpc, h) if er in ("tesSUCCESS", "terQUEUED") else er
        steps.append((name, er, final, h))
        log(f"{name}: submit={er} validated={final} hash={h}")
        return final == "tesSUCCESS"

    # Fund FALCON from faucet (AMM dumps depress price → higher collateral requirement)
    for dest, label in ((lender, "lender"), (borrower, "borrower")):
        tx = {
            **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
            "TransactionType": "Payment",
            "Destination": dest,
            "Amount": str(10000 * DROPS),
        }
        if not run_step(f"fund_{label}", faucet_sec, tx):
            return 1
        time.sleep(1)

    # Trust lines
    for acct, sec, label in ((lender, lender_sec, "lender"), (borrower, borrower_sec, "borrower")):
        tx = {
            **base_tx(rpc, acct, account_seq(rpc, acct)),
            "TransactionType": "TrustSet",
            "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "1000000"},
        }
        if not run_step(f"trust_{label}", sec, tx):
            return 1

    # Mint F-USDC from issuer
    for dest, amt, label in ((lender, "30", "lender"), (borrower, "15", "borrower")):
        tx = {
            **base_tx(rpc, issuer, account_seq(rpc, issuer)),
            "TransactionType": "Payment",
            "Destination": dest,
            "Amount": {"currency": CURRENCY, "issuer": issuer, "value": amt},
        }
        if not run_step(f"mint_{label}", issuer_sec, tx):
            return 1

    principal = 5.0

    # Supply (skip when vault already has enough liquidity for the borrow)
    vault_entry = rpc.public_rpc("ledger_entry", {"index": vault_id, "ledger_index": "validated"})
    assets_available = float(vault_entry.get("node", {}).get("AssetsAvailable", 0))
    supply_needed = principal + 5.0
    if assets_available >= supply_needed:
        log(f"supply_skip: vault has {assets_available:.4f} F-USDC available (need {supply_needed})")
    else:
        deposit_amt = str(max(20, math.ceil(supply_needed - assets_available)))
        tx = {
            **base_tx(rpc, lender, account_seq(rpc, lender)),
            "TransactionType": "VaultDeposit",
            "VaultID": vault_id,
            "Amount": {"currency": CURRENCY, "issuer": issuer, "value": deposit_amt},
        }
        if not run_step("supply", lender_sec, tx):
            return 1

    price = amm_falcon_per_fusdc(rpc, issuer)
    falcon_collateral = math.ceil((principal * 1.5 / price) * 1.05)
    coll_drops = str(int(falcon_collateral * DROPS))
    log(f"AMM {price:.6f} F-USDC/FALCON — posting {falcon_collateral} FALCON collateral for {principal} F-USDC borrow")

    # Permissionless borrow
    tx = {
        **base_tx(rpc, borrower, account_seq(rpc, borrower), "24"),
        "TransactionType": "LoanSet",
        "LoanBrokerID": broker_id,
        "PrincipalRequested": "5",
        "Collateral": coll_drops,
        "InterestRate": 500,
        "PaymentInterval": payment_interval_for_epochs(DEFAULT_LOAN_EPOCHS),
        "PaymentTotal": 1,
        "GracePeriod": 3600,
        "Flags": 0x00010000,  # tfLoanOverpayment
    }
    if not run_step("borrow", borrower_sec, tx):
        return 1

    objs = rpc.public_rpc("account_objects", {
        "account": borrower,
        "type": "loan",
        "ledger_index": "validated",
    }).get("account_objects", [])
    if not objs:
        log("no loan object after borrow")
        return 1
    loan = objs[-1]
    loan_id = loan.get("index")
    due = loan.get("PeriodicPayment")
    if isinstance(due, dict):
        due = due.get("value")
    due_f = float(due)
    repay_amt = f"{math.ceil(due_f * 1_000_000 - 1e-12) / 1_000_000:.6f}"
    log(f"loan_id={loan_id} installment_due={due} repay_amount={repay_amt}")

    # Repay full installment (round up to 6 dp — matches portal Pay full amount)
    tx = {
        **base_tx(rpc, borrower, account_seq(rpc, borrower)),
        "TransactionType": "LoanPay",
        "LoanID": loan_id,
        "Amount": {"currency": CURRENCY, "issuer": issuer, "value": repay_amt},
    }
    if not run_step("repay", borrower_sec, tx):
        return 1

    log("── final balances ──")
    for label, acct in (("lender", lender), ("borrower", borrower)):
        log(f"{label}: {falcon_balance(rpc, acct):.4f} FALCON, {fusdc_balance(rpc, acct, issuer):.6f} F-USDC")

    log("── summary ──")
    for name, submit, validated, h in steps:
        log(f"  {name}: {submit} → {validated} ({h})")

    log("E2E PASS: supply → permissionless borrow → repay")
    return 0


if __name__ == "__main__":
    sys.exit(main())