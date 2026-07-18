#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""E2E: default forfeits FALCON → vault LP claim pool → VaultClaimCollateral to LP wallet."""

from __future__ import annotations

import json
import math
import subprocess
import sys
import time
from pathlib import Path

from lend_epoch_constants import DEFAULT_LOAN_EPOCHS, payment_interval_for_epochs

DROPS = 1_000_000
CURRENCY = "QUC"
PUBLIC_RPC = "http://46.224.0.140:6005"
ADMIN_RPC = "http://127.0.0.1:5005"
CONTAINER = "qxrp-full"
TF_LOAN_DEFAULT = 0x00010000
TF_PARTIAL_PAYMENT = 0x00020000


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
    print(f"[lp-claim-e2e] {msg}")


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


def amm_price(rpc: Rpc, issuer: str) -> float:
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


def vault_liq_pool(rpc: Rpc, vault_id: str) -> float:
    node = rpc.public_rpc(
        "ledger_entry", {"index": vault_id, "ledger_index": "validated"}
    ).get("node", {})
    raw = node.get("LiquidationCollateral")
    if raw is None:
        return 0.0
    if isinstance(raw, str):
        return int(raw) / DROPS
    if isinstance(raw, dict):
        return float(raw.get("value", 0))
    return float(raw) / DROPS if float(raw) > 1000 else float(raw)


def main() -> int:
    rpc = Rpc(ADMIN_RPC, PUBLIC_RPC, CONTAINER)
    manifest = json.loads(Path("/var/lib/qxrp-stables/lending_state.json").read_text())
    st = json.loads(Path("/var/lib/qxrp-stables/stables_state.json").read_text())
    faucet = json.loads(Path("/root/qxrp-bootstrap/faucet.json").read_text())

    issuer = st["qUSDC_issuer"]["address"]
    issuer_sec = st["qUSDC_issuer"]["falcon_secret"]
    broker_id = manifest["loan_broker_id"]
    vault_id = manifest["vault_id"]
    faucet_acct = faucet["account"]
    faucet_sec = faucet["falcon_secret"]
    principal = 5.0
    payment_interval = payment_interval_for_epochs(DEFAULT_LOAN_EPOCHS)

    # Supplier LP (must hold vault shares before default to earn claim)
    lender, lender_sec = propose_wallet(rpc)
    borrower, borrower_sec = propose_wallet(rpc)
    liquidator, liquidator_sec = propose_wallet(rpc)
    log(f"lender={lender} borrower={borrower}")

    for dest, amt in ((lender, 15000), (borrower, 15000), (liquidator, 3000)):
        tx = {
            **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
            "TransactionType": "Payment",
            "Destination": dest,
            "Amount": str(amt * DROPS),
        }
        er, h = sign_submit(rpc, faucet_sec, tx)
        if wait_tx(rpc, h) != "tesSUCCESS":
            log(f"fund failed {er}")
            return 1
        time.sleep(1)

    for acct, sec in ((lender, lender_sec), (borrower, borrower_sec)):
        tx = {
            **base_tx(rpc, acct, account_seq(rpc, acct)),
            "TransactionType": "TrustSet",
            "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "1000000"},
        }
        er, h = sign_submit(rpc, sec, tx)
        if wait_tx(rpc, h) != "tesSUCCESS":
            return 1

    # Mint F-USDC and supply so lender holds LP shares
    tx = {
        **base_tx(rpc, issuer, account_seq(rpc, issuer)),
        "TransactionType": "Payment",
        "Destination": lender,
        "Amount": {"currency": CURRENCY, "issuer": issuer, "value": "50"},
    }
    er, h = sign_submit(rpc, issuer_sec, tx)
    if wait_tx(rpc, h) != "tesSUCCESS":
        return 1

    tx = {
        **base_tx(rpc, lender, account_seq(rpc, lender)),
        "TransactionType": "VaultDeposit",
        "VaultID": vault_id,
        "Amount": {"currency": CURRENCY, "issuer": issuer, "value": "25"},
    }
    er, h = sign_submit(rpc, lender_sec, tx)
    if wait_tx(rpc, h) != "tesSUCCESS":
        log(f"supply failed {er}")
        return 1
    log("lender supplied 25 F-USDC to vault")

    pool_before = vault_liq_pool(rpc, vault_id)
    lender_falcon_before = falcon_balance(rpc, lender)

    price = amm_price(rpc, issuer)
    falcon_collateral = math.ceil((principal * 1.5 / price) * 1.05)
    coll_drops = str(int(falcon_collateral * DROPS))
    log(f"borrow {principal} · {falcon_collateral} FALCON coll @ {price:.6f}")

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
    loan_id = objs[-1]["index"]

    # Crash price via AMM dumps
    tx = {
        **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct)),
        "TransactionType": "TrustSet",
        "LimitAmount": {"currency": CURRENCY, "issuer": issuer, "value": "1000000"},
    }
    er, h = sign_submit(rpc, faucet_sec, tx)
    wait_tx(rpc, h)
    time.sleep(1)
    for swap in (4000, 8000, 12000):
        price = amm_price(rpc, issuer)
        max_usdc = f"{swap * price * 0.9:.6f}"
        min_usdc = f"{swap * price * 0.4:.6f}"
        tx = {
            **base_tx(rpc, faucet_acct, account_seq(rpc, faucet_acct), "24"),
            "TransactionType": "Payment",
            "Destination": faucet_acct,
            "Amount": {"currency": CURRENCY, "issuer": issuer, "value": max_usdc},
            "SendMax": str(int(swap * DROPS)),
            "DeliverMin": {"currency": CURRENCY, "issuer": issuer, "value": min_usdc},
            "Flags": TF_PARTIAL_PAYMENT,
        }
        er, h = sign_submit(rpc, faucet_sec, tx)
        wait_tx(rpc, h)
        price = amm_price(rpc, issuer)
        hf = falcon_collateral * price / principal
        log(f"after dump {swap}: price={price:.6f} HF≈{hf:.3f}")
        if hf < 1.1:
            break

    tx = {
        **base_tx(rpc, liquidator, account_seq(rpc, liquidator)),
        "TransactionType": "LoanManage",
        "LoanID": loan_id,
        "Flags": TF_LOAN_DEFAULT,
    }
    er, h = sign_submit(rpc, liquidator_sec, tx)
    if wait_tx(rpc, h) != "tesSUCCESS":
        log(f"default failed {er}")
        return 1

    pool_after = vault_liq_pool(rpc, vault_id)
    log(f"vault LiquidationCollateral: {pool_before:.4f} → {pool_after:.4f} FALCON")
    if pool_after < pool_before + falcon_collateral * 0.5:
        log("expected liquidation pool to rise by ~forfeited FALCON")
        return 1

    # LP claims FALCON
    tx = {
        **base_tx(rpc, lender, account_seq(rpc, lender)),
        "TransactionType": "VaultClaimCollateral",
        "LoanBrokerID": broker_id,
    }
    er, h = sign_submit(rpc, lender_sec, tx)
    final = wait_tx(rpc, h)
    log(f"VaultClaimCollateral: {er} → {final}")
    if final != "tesSUCCESS":
        return 1

    lender_falcon_after = falcon_balance(rpc, lender)
    gained = lender_falcon_after - lender_falcon_before
    log(f"lender FALCON: {lender_falcon_before:.4f} → {lender_falcon_after:.4f} (Δ {gained:.4f})")
    if gained < 1.0:
        log("lender should receive substantial forfeited FALCON as LP")
        return 1

    log("E2E PASS: default → LP FALCON claim pool → VaultClaimCollateral")
    return 0


if __name__ == "__main__":
    sys.exit(main())
