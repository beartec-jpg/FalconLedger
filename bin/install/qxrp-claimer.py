#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Minimal standalone qXRP reward claimer.
#
# - Claims accumulated rewards into the validator account when composite score is high enough.
# - Does NOT auto-sweep to a payout wallet (user controls withdrawals via portal or manually).
# - Intended to be run from cron, systemd timer, or as a docker sidecar.
#
# Usage:
#   python3 bin/install/qxrp-claimer.py --keys ~/.qxrp/keys/validator-keys.json
#   python3 bin/install/qxrp-claimer.py --rpc http://127.0.0.1:5005 --once

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from typing import Any, Dict


def rpc(url: str, method: str, params: Dict[str, Any] | None = None) -> Dict[str, Any]:
    payload = {"method": method, "params": [params or {}]}
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=20) as resp:
        body = json.loads(resp.read())
        if "error" in body.get("result", {}):
            raise RuntimeError(body["result"]["error_message"])
        return body["result"]


def main() -> None:
    ap = argparse.ArgumentParser(description="qXRP validator reward claimer")
    ap.add_argument("--keys", default=os.path.expanduser("~/.qxrp/keys/validator-keys.json"),
                    help="Path to validator-keys.json produced by the installer")
    ap.add_argument("--rpc", default="http://127.0.0.1:5005", help="xrpld RPC URL")
    ap.add_argument("--once", action="store_true", help="Run one claim attempt and exit")
    ap.add_argument("--min-score", type=int, default=500, help="Minimum composite score (bps) to attempt claim")
    args = ap.parse_args()

    if not os.path.isfile(args.keys):
        print(f"[claimer] No keys file at {args.keys} — nothing to do.", file=sys.stderr)
        sys.exit(0)

    with open(args.keys) as f:
        k = json.load(f)

    account = k["account_address"]
    seed = k["validation_seed"]
    ck = k["consensus_key_hex"]

    print(f"[claimer] Validator account: {account}")

    try:
        info = rpc(args.rpc, "server_info")["info"]
        print(f"[claimer] Node state: {info.get('server_state')}")
    except Exception as exc:
        print(f"[claimer] Cannot reach node at {args.rpc}: {exc}")
        sys.exit(1)

    # Read bond status
    try:
        bond_resp = rpc(args.rpc, "ledger_entry", {
            "validator_bond": {"account": account},
            "ledger_index": "validated",
        })
        bond = bond_resp.get("node", {})
        score = int(bond.get("CompositeScore") or 0)
        status = bond.get("BondStatus")
        print(f"[claimer] BondStatus={status}  CompositeScore={score} bps")
        if score < args.min_score:
            print(f"[claimer] Score < {args.min_score} — skipping claim this round.")
            return
    except Exception as exc:
        print(f"[claimer] No bond object found or error reading it: {exc}")
        return

    # Submit ClaimReward
    try:
        tx_json = {
            "TransactionType": "ClaimReward",
            "Account": account,
            "ConsensusKey": ck,
            "Fee": "12",
        }
        result = rpc(args.rpc, "submit", {"tx_json": tx_json, "secret": seed})
        eng = result.get("engine_result", "unknown")
        print(f"[claimer] ClaimReward → {eng}")
        if eng == "tesSUCCESS":
            print("[claimer] Rewards successfully moved into validator account.")
    except Exception as exc:
        print(f"[claimer] Claim attempt failed: {exc}")


if __name__ == "__main__":
    main()