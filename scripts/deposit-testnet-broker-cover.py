#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
deposit-testnet-broker-cover.py — post F-USDC first-loss cover for the testnet loan broker.

Prerequisite: broker owner (rJePmBh…) holds F-USDC on Falcon (QUC trust line + balance).
Send from any Falcon wallet via Wallet → Send F-USDC — NOT Sepolia.

Usage (coordinator or laptop with signer proxy access):
  export SIGNER_PROXY_URL=http://46.224.0.140:3001
  export SIGNER_PROXY_TOKEN=...
  export TESTNET_LENDING_BROKER_SECRET=...   # broker owner falcon_secret
  python3 scripts/deposit-testnet-broker-cover.py --amount 50
  python3 scripts/deposit-testnet-broker-cover.py --all   # deposit entire QUC balance
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PUBLIC_RPC = os.environ.get("FALCON_PUBLIC_RPC", "http://46.224.0.140:6005")
NETWORK_ID = 1001
CURRENCY = "QUC"


def log(msg: str) -> None:
    print(f"[deposit-cover] {msg}")


def rpc(url: str, method: str, params: dict) -> dict:
    payload = json.dumps({"method": method, "params": [params]}).encode()
    req = urllib.request.Request(
        url, data=payload, headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        body = json.loads(resp.read())
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body.get("result", body)


def sign_tx(proxy_url: str, token: str, secret: str, tx_json: dict) -> tuple[str, str]:
    body = {"falcon_secret": secret, "tx_json": tx_json}
    data = json.dumps(body).encode()
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(f"{proxy_url.rstrip('/')}/sign", data=data, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as resp:
        out = json.loads(resp.read())
    if out.get("error"):
        raise RuntimeError(out["error"])
    tx_hash = out.get("tx_json", {}).get("hash", "")
    return out["tx_blob"], tx_hash


def submit(rpc_url: str, tx_blob: str) -> str:
    r = rpc(rpc_url, "submit", {"tx_blob": tx_blob})
    return r.get("engine_result", "?")


def wait_tx(rpc_url: str, tx_hash: str, retries: int = 30) -> str:
    for _ in range(retries):
        time.sleep(2)
        try:
            r = rpc(rpc_url, "tx", {"transaction": tx_hash, "binary": False})
            if r.get("validated"):
                return r.get("meta", {}).get("TransactionResult", "?")
        except Exception:
            pass
    return "TIMEOUT"


def load_manifest() -> dict:
    portal = REPO_ROOT.parent / "qXRP-faucet-wallet" / "public" / "config" / "lending.json"
    local = REPO_ROOT / "config" / "lending.json"
    path = portal if portal.is_file() else local
    if not path.is_file():
        raise FileNotFoundError(f"lending manifest not found: {path}")
    return json.loads(path.read_text())


def quc_balance(rpc_url: str, account: str, issuer: str) -> float:
    r = rpc(rpc_url, "account_lines", {"account": account, "ledger_index": "validated"})
    for line in r.get("lines", []):
        if line.get("currency") == CURRENCY and line.get("account") == issuer:
            return float(line.get("balance", 0))
    return 0.0


def account_seq(rpc_url: str, account: str) -> int:
    r = rpc(rpc_url, "account_info", {"account": account, "ledger_index": "validated"})
    return int(r["account_data"]["Sequence"])


def ledger_index(rpc_url: str) -> int:
    r = rpc(rpc_url, "ledger", {"ledger_index": "validated"})
    return int(r["ledger_index"])


def main() -> int:
    parser = argparse.ArgumentParser(description="Deposit testnet loan broker cover (F-USDC)")
    parser.add_argument("--amount", type=float, default=0, help="F-USDC to deposit as cover")
    parser.add_argument("--all", action="store_true", help="Deposit full QUC balance on broker owner")
    parser.add_argument("--rpc", default=PUBLIC_RPC)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    manifest = load_manifest()
    broker_owner = manifest["broker_owner"]
    broker_id = manifest["loan_broker_id"]
    issuer = manifest["asset"]["issuer"]

    secret = os.environ.get("TESTNET_LENDING_BROKER_SECRET", "").strip()
    if not secret and not args.dry_run:
        log("Set TESTNET_LENDING_BROKER_SECRET (broker owner falcon_secret)")
        return 1

    proxy_url = os.environ.get("SIGNER_PROXY_URL", "http://46.224.0.140:3001").strip()
    proxy_token = os.environ.get("SIGNER_PROXY_TOKEN", "").strip()

    bal = quc_balance(args.rpc, broker_owner, issuer)
    log(f"Broker owner {broker_owner} F-USDC balance: {bal}")

    if bal <= 0:
        log(
            "No F-USDC on broker owner. Send F-USDC on Falcon (not Sepolia) to:\n"
            f"  {broker_owner}\n"
            "From Wallet → Send F-USDC, or bridge in then send. Re-run this script after.",
        )
        return 1

    amount = bal if args.all else args.amount
    if amount <= 0:
        log("Specify --amount N or --all")
        return 1
    if amount > bal:
        log(f"Requested {amount} but only {bal} F-USDC available on broker owner")
        return 1

    amount_str = f"{amount:.6f}".rstrip("0").rstrip(".")
    log(f"Depositing {amount_str} F-USDC as broker cover…")

    if args.dry_run:
        log(f"[dry-run] LoanBrokerCoverDeposit {amount_str} → broker {broker_id[:16]}…")
        return 0

    seq = account_seq(args.rpc, broker_owner)
    lls = ledger_index(args.rpc) + 20
    tx_json = {
        "TransactionType": "LoanBrokerCoverDeposit",
        "Account": broker_owner,
        "LoanBrokerID": broker_id.upper(),
        "Amount": {"currency": CURRENCY, "issuer": issuer, "value": amount_str},
        "Sequence": seq,
        "Fee": "24",
        "LastLedgerSequence": lls,
    }

    blob, tx_hash = sign_tx(proxy_url, proxy_token, secret, tx_json)
    result = submit(args.rpc, blob)
    log(f"submit: {result} ({tx_hash[:16]}…)")
    if result not in ("tesSUCCESS", "terQUEUED"):
        return 1
    final = wait_tx(args.rpc, tx_hash)
    log(f"validated: {final}")
    if final != "tesSUCCESS":
        return 1

    br = rpc(args.rpc, "ledger_entry", {
        "loan_broker": broker_id,
        "ledger_index": "validated",
    })
    cover = br.get("node", {}).get("CoverAvailable", "?")
    log(f"CoverAvailable now: {cover}")
    log("Borrow should work once cover ≥ ~1% of loan size.")
    return 0


if __name__ == "__main__":
    sys.exit(main())