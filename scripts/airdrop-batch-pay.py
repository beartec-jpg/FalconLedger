#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Batch-pay airdrop allocations from the AIRDROP wallet.

Input CSV (from freeze export or manual):
  address,falcon_amount

  export AIRDROP_SECRET='...'
  export AIRDROP_ADDRESS='r...'
  export PUBLIC_RPC=http://46.224.0.140:6005
  export ADMIN_RPC=http://127.0.0.1:5005
  export CONTAINER=qxrp-full

  python3 scripts/airdrop-batch-pay.py --csv allocations.csv --dry-run
  python3 scripts/airdrop-batch-pay.py --csv allocations.csv --execute --min-falcon 1
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import time

DROPS = 1_000_000


def log(msg: str) -> None:
    print(f"[airdrop-pay] {msg}", flush=True)


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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--execute", action="store_true")
    ap.add_argument("--min-falcon", type=float, default=1.0)
    ap.add_argument("--limit", type=int, default=0, help="Max rows to pay (0=all)")
    args = ap.parse_args()
    if not args.dry_run and not args.execute:
        log("Pass --dry-run or --execute")
        return 2

    secret = os.environ.get("AIRDROP_SECRET", "").strip()
    from_acct = os.environ.get("AIRDROP_ADDRESS", "").strip()
    public = os.environ.get("PUBLIC_RPC", "http://127.0.0.1:6005")
    admin = os.environ.get("ADMIN_RPC", "http://127.0.0.1:5005")
    if not from_acct:
        log("Set AIRDROP_ADDRESS")
        return 2
    if args.execute and not secret:
        log("Set AIRDROP_SECRET for --execute")
        return 2

    rows: list[tuple[str, float]] = []
    with open(args.csv, newline="") as f:
        r = csv.DictReader(f)
        for row in r:
            addr = (row.get("address") or row.get("Account") or "").strip()
            amt = float(row.get("falcon_amount") or row.get("amount") or 0)
            if addr.startswith("r") and amt >= args.min_falcon:
                rows.append((addr, amt))

    if args.limit > 0:
        rows = rows[: args.limit]
    log(f"{len(rows)} payments · total {sum(a for _, a in rows):,.2f} FALCON")

    def seq() -> int:
        return int(
            post(public, "account_info", {"account": from_acct, "ledger_index": "validated"})[
                "account_data"
            ]["Sequence"]
        )

    def ll() -> int:
        return int(post(public, "ledger", {"ledger_index": "validated"})["ledger_index"]) + 40

    paid = 0
    for addr, amt in rows:
        drops = str(int(amt * DROPS))
        log(f"{'DRY ' if args.dry_run else ''}→ {addr} {amt:.6f} FALCON")
        if args.dry_run:
            continue
        tx = {
            "TransactionType": "Payment",
            "Account": from_acct,
            "Destination": addr,
            "Amount": drops,
            "Fee": "12",
            "Sequence": seq(),
            "LastLedgerSequence": ll(),
        }
        params = (
            {"falcon_secret": secret, "tx_json": tx}
            if not secret.startswith(("s", "S"))
            else {"secret": secret, "tx_json": tx}
        )
        signed = post(admin, "sign", params)
        sub = post(public, "submit", {"tx_blob": signed["tx_blob"]})
        er = sub.get("engine_result")
        h = signed.get("tx_json", {}).get("hash", "")
        log(f"  {er} {h[:16]}…")
        if er not in ("tesSUCCESS", "terQUEUED"):
            log("  abort on failure")
            return 1
        paid += 1
        time.sleep(0.5)

    log(f"Done · submitted {paid}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
