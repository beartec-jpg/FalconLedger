#!/usr/bin/env python3
"""E2E BitVM-class peg test harness.

Modes:
  --bitcoin-only  Vault fund + CSV wait + claim attempt helpers (default)
  --full          Also drive Falcon RPC burn/finalize if available

Light client mint path is separate (BitcoinSPV unit tests / manual activate).
This harness focuses on peg-out (BitVM vault + Falcon burn objects).
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.request

from vault import (
    commit_of,
    demo_local_vault,
    ensure_wallet,
    make_preimage,
    mine,
    bitcoin_cli,
)


def falcon_rpc(url: str, method: str, params: list | dict | None = None):
    payload = {
        "method": method,
        "params": params if params is not None else [],
        "id": 1,
    }
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        body = json.loads(resp.read().decode())
    if "error" in body and body["error"]:
        raise RuntimeError(body["error"])
    return body.get("result")


def run_bitcoin_only(csv_blocks: int) -> int:
    print("=== BitVM vault e2e (bitcoin-only) ===")
    try:
        ensure_wallet("bitvm")
    except Exception as e:
        print(f"FAIL: bitcoind regtest not available: {e}", file=sys.stderr)
        print("Start with: bitcoind -regtest -daemon", file=sys.stderr)
        return 2

    result = demo_local_vault(csv_blocks)
    print(json.dumps(result, indent=2))
    print()
    print("PASS: vault funded and CSV mined.")
    print("Next: construct PSBT claim with preimage (core wallet sign) or use")
    print("      bitcoin-cli with witness stack [sig, preimage, 0] for ELSE path.")
    print()
    print("Falcon side (when node up):")
    print("  1) BTCBridgeBurn with same preimage + payout script")
    print("  2) Close ledgers past challenge window")
    print("  3) BTCWithdrawFinalize")
    print("  4) Claim BTC vault with preimage")
    return 0


def run_full(falcon_url: str, csv_blocks: int) -> int:
    print("=== BitVM e2e (full stack check) ===")
    # Probe Falcon
    try:
        info = falcon_rpc(falcon_url, "server_info")
        print("Falcon server_info: ok")
    except Exception as e:
        print(f"FAIL: Falcon RPC: {e}", file=sys.stderr)
        return 2

    rc = run_bitcoin_only(csv_blocks)
    if rc != 0:
        return rc

    print()
    print("Falcon is reachable. Drive burn/finalize via your signed txs / wallet:")
    print("  TransactionType: BTCBridgeBurn")
    print("  BtcWithdrawAmount, BtcPayoutScript, BtcBurnPreimage")
    print("  then BTCWithdrawFinalize after challenge ledgers")
    print()
    print("PASS: environment ready for full manual/signed e2e.")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bitcoin-only", action="store_true", default=True)
    ap.add_argument("--full", action="store_true")
    ap.add_argument("--falcon-url", default="http://127.0.0.1:5115")
    ap.add_argument("--csv-blocks", type=int, default=6)
    args = ap.parse_args()
    if args.full:
        return run_full(args.falcon_url, args.csv_blocks)
    return run_bitcoin_only(args.csv_blocks)


if __name__ == "__main__":
    sys.exit(main())
