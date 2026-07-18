#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Split the 4B genesis circulating allocation into launch wallets:

  2.0B  (1.0% of total supply)  → AIRDROP
  1.0B  (0.5%)                  → FAUCET
  1.0B  (0.5%)                  → DEV

Does NOT touch the 196B protocol treasury.

Usage (mainnet ceremony — dry-run first):

  export GENESIS_SECRET='...'          # classical or falcon secret for genesis account
  export AIRDROP_ADDRESS='r...'
  export FAUCET_ADDRESS='r...'
  export DEV_ADDRESS='r...'
  export PUBLIC_RPC='https://...'
  export ADMIN_RPC='http://127.0.0.1:5005'   # if signing via admin curl
  export CONTAINER='qxrp-full'               # optional docker exec target

  python3 scripts/mainnet-genesis-split.py --dry-run
  python3 scripts/mainnet-genesis-split.py --execute
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time

DROPS = 1_000_000
# Exact mainnet-planned split (must sum to 4e9)
AIRDROP_FALCON = 2_000_000_000
FAUCET_FALCON = 1_000_000_000
DEV_FALCON = 1_000_000_000
TOTAL = AIRDROP_FALCON + FAUCET_FALCON + DEV_FALCON
assert TOTAL == 4_000_000_000


def log(msg: str) -> None:
    print(f"[genesis-split] {msg}", flush=True)


def rpc_url(kind: str) -> str:
    if kind == "public":
        return os.environ.get("PUBLIC_RPC", "http://127.0.0.1:6005")
    return os.environ.get("ADMIN_RPC", "http://127.0.0.1:5005")


def post(url: str, method: str, params: dict | None = None) -> dict:
    payload = json.dumps({"method": method, "params": [params or {}]}).encode()
    container = os.environ.get("CONTAINER", "").strip()
    if container:
        out = subprocess.check_output(
            [
                "docker",
                "exec",
                container,
                "curl",
                "-sf",
                "-X",
                "POST",
                url,
                "-H",
                "Content-Type: application/json",
                "-d",
                payload.decode(),
            ],
            text=True,
        )
    else:
        import urllib.request

        req = urllib.request.Request(
            url, data=payload, headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=60) as resp:
            out = resp.read().decode()
    body = json.loads(out)
    if body.get("result", {}).get("error"):
        raise RuntimeError(body["result"])
    return body["result"]


def account_info(account: str) -> dict:
    return post(rpc_url("public"), "account_info", {
        "account": account,
        "ledger_index": "validated",
    })


def balance_falcon(account: str) -> float:
    try:
        bal = int(account_info(account)["account_data"]["Balance"])
        return bal / DROPS
    except Exception:
        return 0.0


def sequence(account: str) -> int:
    return int(account_info(account)["account_data"]["Sequence"])


def last_ledger() -> int:
    r = post(rpc_url("public"), "ledger", {"ledger_index": "validated"})
    return int(r["ledger_index"]) + 40


def sign_submit(secret: str, tx: dict) -> tuple[str, str]:
    params = (
        {"falcon_secret": secret, "tx_json": tx}
        if not secret.startswith(("s", "S"))
        else {"secret": secret, "tx_json": tx}
    )
    signed = post(rpc_url("admin"), "sign", params)
    if signed.get("status") != "success":
        raise RuntimeError(f"sign failed: {signed}")
    blob = signed["tx_blob"]
    h = signed.get("tx_json", {}).get("hash", "")
    sub = post(rpc_url("public"), "submit", {"tx_blob": blob})
    return sub.get("engine_result", "?"), h


def wait_tx(tx_hash: str) -> str:
    for _ in range(40):
        time.sleep(2)
        try:
            r = post(rpc_url("public"), "tx", {"transaction": tx_hash, "binary": False})
            if r.get("validated"):
                return r.get("meta", {}).get("TransactionResult", "?")
        except Exception:
            pass
    return "TIMEOUT"


def pay(from_acct: str, secret: str, to: str, falcon: int, dry_run: bool) -> None:
    drops = str(int(falcon) * DROPS)
    log(f"{'DRY ' if dry_run else ''}Payment {falcon:,} FALCON → {to}")
    if dry_run:
        return
    tx = {
        "TransactionType": "Payment",
        "Account": from_acct,
        "Destination": to,
        "Amount": drops,
        "Fee": "12",
        "Sequence": sequence(from_acct),
        "LastLedgerSequence": last_ledger(),
    }
    er, h = sign_submit(secret, tx)
    final = wait_tx(h) if er in ("tesSUCCESS", "terQUEUED") else er
    log(f"  {er} → {final}  hash={h}")
    if final != "tesSUCCESS":
        raise SystemExit(f"payment failed: {final}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Split 4B genesis into airdrop/faucet/dev")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--execute", action="store_true")
    args = ap.parse_args()
    if not args.dry_run and not args.execute:
        log("Pass --dry-run or --execute")
        return 2

    genesis_secret = os.environ.get("GENESIS_SECRET", "").strip()
    airdrop = os.environ.get("AIRDROP_ADDRESS", "").strip()
    faucet = os.environ.get("FAUCET_ADDRESS", "").strip()
    dev = os.environ.get("DEV_ADDRESS", "").strip()
    genesis_acct = os.environ.get("GENESIS_ADDRESS", "").strip()

    if not all([airdrop, faucet, dev]):
        log("Set AIRDROP_ADDRESS, FAUCET_ADDRESS, DEV_ADDRESS")
        return 2
    if not args.dry_run and not genesis_secret:
        log("Set GENESIS_SECRET for --execute")
        return 2

    if not genesis_acct and genesis_secret:
        # Derive via wallet_propose if possible
        try:
            if genesis_secret.startswith(("s", "S")):
                r = post(rpc_url("admin"), "wallet_propose", {"seed": genesis_secret})
            else:
                r = post(rpc_url("admin"), "wallet_propose", {"falcon_secret": genesis_secret})
            genesis_acct = r.get("account_id") or r.get("account") or ""
        except Exception as e:
            log(f"could not derive genesis account: {e}")

    if not genesis_acct:
        log("Set GENESIS_ADDRESS (or GENESIS_SECRET for admin derive)")
        return 2

    log(f"Genesis account: {genesis_acct}")
    bal = balance_falcon(genesis_acct)
    log(f"Genesis balance: {bal:,.0f} FALCON (need ≥ {TOTAL:,})")
    log(f"Plan: airdrop={AIRDROP_FALCON:,} faucet={FAUCET_FALCON:,} dev={DEV_FALCON:,}")

    if not args.dry_run and bal + 1 < TOTAL:
        log("Insufficient genesis balance for full split")
        return 1

    dry = bool(args.dry_run)
    pay(genesis_acct, genesis_secret, airdrop, AIRDROP_FALCON, dry)
    pay(genesis_acct, genesis_secret, faucet, FAUCET_FALCON, dry)
    pay(genesis_acct, genesis_secret, dev, DEV_FALCON, dry)

    if not dry:
        log("Done. Verify:")
        for label, addr in (("airdrop", airdrop), ("faucet", faucet), ("dev", dev)):
            log(f"  {label} {addr}: {balance_falcon(addr):,.0f} FALCON")
    else:
        log("Dry-run complete — no txs submitted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
