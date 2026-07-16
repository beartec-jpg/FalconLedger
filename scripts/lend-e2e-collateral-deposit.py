#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""On-ledger E2E: permissionless borrow → LoanCollateralDeposit → verify collateral."""

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
    print(f"[collateral-e2e] {msg}")


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


def collateral_falcon(obj: dict) -> float:
    raw = obj.get("Collateral")
    if raw is None:
        return 0.0
    if isinstance(raw, str):
        return int(raw) / DROPS
    if isinstance(raw, dict):
        return int(raw.get("value", 0)) / DROPS
    return float(raw) / DROPS


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


def main() -> int:
    rpc = Rpc(ADMIN_RPC, PUBLIC_RPC, CONTAINER)
    manifest = json.loads(Path("/var/lib/qxrp-stables/lending_state.json").read_text())
    st = json.loads(Path("/var/lib/qxrp-stables/stables_state.json").read_text())
    faucet = json.loads(Path("/root/qxrp-bootstrap/faucet.json").read_text())

    issuer = st["qUSDC_issuer"]["address"]
    broker_id = manifest["loan_broker_id"]
    faucet_acct = faucet["account"]
    faucet_sec = faucet["falcon_secret"]
    principal = 5
    loan_epochs = DEFAULT_LOAN_EPOCHS
    payment_interval = payment_interval_for_epochs(loan_epochs)
    add_falcon = 50.0

    borrower, borrower_sec = propose_wallet(rpc)
    log(f"borrower {borrower}")

    for amount, label in ((3000, "xrp"), (8000, "falcon")):
        fund_tx = {
            **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
            "TransactionType": "Payment",
            "Destination": borrower,
            "Amount": str(amount * DROPS),
        }
        er, h = sign_submit(rpc, faucet_sec, fund_tx)
        if wait_tx(rpc, h) != "tesSUCCESS":
            log(f"fund borrower {label} failed ({er})")
            return 1
        time.sleep(1)

    price = amm_falcon_per_fusdc(rpc, issuer)
    falcon_collateral = math.ceil((principal * 1.5 / price) * 1.05)
    coll_drops = str(int(falcon_collateral * DROPS))
    log(f"borrow {principal} F-USDC · {loan_epochs} epoch(s) · interval {payment_interval}s · {falcon_collateral} FALCON collateral")

    seq = account_seq(rpc, borrower)
    borrow_tx = {
        **base_tx(rpc, borrower, seq, "24"),
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
    er, h = sign_submit(rpc, borrower_sec, borrow_tx)
    log(f"borrow submit {er} {h}")
    if er != "tesSUCCESS":
        return 1
    vr = wait_tx(rpc, h)
    log(f"borrow validated {vr}")
    if vr != "tesSUCCESS":
        return 1

    objs = rpc.public_rpc("account_objects", {
        "account": borrower,
        "type": "loan",
        "ledger_index": "validated",
    }).get("account_objects", [])
    loan = objs[-1]
    loan_id = loan["index"]
    before = collateral_falcon(loan)
    log(f"loan {loan_id[:16]}… collateral before={before:.4f} FALCON")

    add_drops = str(int(add_falcon * DROPS))
    seq = account_seq(rpc, borrower)
    deposit_tx = {
        **base_tx(rpc, borrower, seq),
        "TransactionType": "LoanCollateralDeposit",
        "LoanID": loan_id,
        "Collateral": add_drops,
    }
    er, h = sign_submit(rpc, borrower_sec, deposit_tx)
    log(f"LoanCollateralDeposit submit {er} {h}")
    if er != "tesSUCCESS":
        log("LoanCollateralDeposit not supported — rebuild fleet with latest qXRP develop")
        return 1
    vr = wait_tx(rpc, h)
    log(f"LoanCollateralDeposit validated {vr}")
    if vr != "tesSUCCESS":
        return 1

    objs = rpc.public_rpc("account_objects", {
        "account": borrower,
        "type": "loan",
        "ledger_index": "validated",
    }).get("account_objects", [])
    loan = next(o for o in objs if o.get("index") == loan_id)
    after = collateral_falcon(loan)
    log(f"collateral after={after:.4f} FALCON (added ~{add_falcon})")
    if after < before + add_falcon * 0.99:
        log("collateral did not increase as expected")
        return 1

    log("E2E PASS: borrow → LoanCollateralDeposit")
    return 0


if __name__ == "__main__":
    sys.exit(main())