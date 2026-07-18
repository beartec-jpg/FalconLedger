#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""Set LoanBroker pool InterestRate (tenth-bps). Requires lending-v5+ node.

Default: 5000 = 5% APR for the genesis Falcon testnet pool.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_RATE = 5000  # 5%
PUBLIC = "http://127.0.0.1:6005"
ADMIN = "http://127.0.0.1:5005"
CONTAINER = "qxrp-full"


def rpc(url: str, method: str, params: dict | None = None) -> dict:
    payload = json.dumps({"method": method, "params": [params or {}]}).encode()
    cmd = [
        "docker",
        "exec",
        CONTAINER,
        "curl",
        "-sf",
        "-X",
        "POST",
        url,
        "-H",
        "Content-Type: application/json",
        "-d",
        payload.decode(),
    ]
    body = json.loads(subprocess.check_output(cmd, text=True))
    res = body.get("result", body)
    if isinstance(res, dict) and res.get("error"):
        raise RuntimeError(res)
    return res


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rate", type=int, default=DEFAULT_RATE, help="tenth-bps (5000=5%)")
    ap.add_argument(
        "--stables-state",
        default="/var/lib/qxrp-stables/stables_state.json",
    )
    ap.add_argument(
        "--manifest",
        default="/var/lib/qxrp-lending/lending.json",
    )
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    stables = json.loads(Path(args.stables_state).read_text())
    lp = stables.get("liquidity_provider") or {}
    secret = lp.get("falcon_secret") or lp.get("seed") or ""
    owner = lp.get("address") or ""
    man = json.loads(Path(args.manifest).read_text())
    vault = man["vault_id"]
    broker = man["loan_broker_id"]
    if not secret or not owner:
        print("missing broker secret/address", file=sys.stderr)
        return 2

    node = rpc(PUBLIC, "ledger_entry", {"index": broker, "ledger_index": "validated"}).get(
        "node", {}
    )
    print(f"broker {broker[:16]}… current InterestRate={node.get('InterestRate', 0)}")
    print(f"set → {args.rate} tenth-bps ({args.rate / 1000:.2f}% APR)")
    if args.dry_run:
        return 0

    seq = int(
        rpc(PUBLIC, "account_info", {"account": owner, "ledger_index": "validated"})[
            "account_data"
        ]["Sequence"]
    )
    ll = int(rpc(PUBLIC, "ledger", {"ledger_index": "validated"})["ledger_index"]) + 40
    tx = {
        "TransactionType": "LoanBrokerSet",
        "Account": owner,
        "VaultID": vault.upper(),
        "LoanBrokerID": broker.upper(),
        "InterestRate": args.rate,
        "Fee": "12",
        "Sequence": seq,
        "LastLedgerSequence": ll,
    }
    signed = rpc(
        ADMIN,
        "sign",
        {"falcon_secret": secret, "tx_json": tx}
        if not secret.startswith(("s", "S"))
        else {"secret": secret, "tx_json": tx},
    )
    if signed.get("status") != "success":
        print("sign failed", signed, file=sys.stderr)
        return 1
    sub = rpc(PUBLIC, "submit", {"tx_blob": signed["tx_blob"]})
    print("submit", sub.get("engine_result"), signed.get("tx_json", {}).get("hash"))
    time.sleep(4)
    node2 = rpc(PUBLIC, "ledger_entry", {"index": broker, "ledger_index": "validated"}).get(
        "node", {}
    )
    print(f"after InterestRate={node2.get('InterestRate', 0)}")
    return 0 if sub.get("engine_result") in ("tesSUCCESS", "terQUEUED") else 1


if __name__ == "__main__":
    sys.exit(main())
