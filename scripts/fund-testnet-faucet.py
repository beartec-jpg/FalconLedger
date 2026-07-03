#!/usr/bin/env python3
"""Fund the public testnet faucet account from genesis (Payment only)."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
import urllib.request

DROPS_PER_QXRP = 1_000_000
GENESIS_SECRET = "masterpassphrase"
GENESIS_ACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
DEFAULT_FAUCET = "rwzhiWW4GYK2sQVR5Lw4iDpYLANB5krJXY"
DEFAULT_FUND_QXRP = 500_000


def rpc(url: str, method: str, params: dict | None = None, *, container: str = "") -> dict:
    payload = json.dumps({"method": method, "params": [params or {}]}).encode()
    if container:
        cmd = [
            "docker", "exec", container,
            "curl", "-sf", "-X", "POST", url,
            "-H", "Content-Type: application/json",
            "-d", payload.decode(),
        ]
        out = subprocess.check_output(cmd, text=True)
        data = json.loads(out)
    else:
        req = urllib.request.Request(
            url, data=payload, headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = json.loads(resp.read())
    if "result" in data:
        return data["result"]
    return data


def main() -> int:
    p = argparse.ArgumentParser(description="Fund testnet faucet from genesis")
    p.add_argument("--faucet", default=DEFAULT_FAUCET)
    p.add_argument("--amount-qxrp", type=int, default=DEFAULT_FUND_QXRP)
    p.add_argument("--admin-rpc", default="http://127.0.0.1:5005")
    p.add_argument("--public-rpc", default="http://127.0.0.1:6005")
    p.add_argument(
        "--container",
        default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"),
        help="docker container for admin RPC (empty = direct HTTP)",
    )
    args = p.parse_args()
    container = args.container or ""

    net = rpc(args.public_rpc, "server_info", container=container)
    network_id = int(net.get("info", {}).get("network_id", 1001))
    ledger = int(net.get("info", {}).get("validated_ledger", {}).get("seq", 0))

    try:
        acct = rpc(args.public_rpc, "account_info", {
            "account": args.faucet,
            "ledger_index": "validated",
        }, container=container)
        if "account_data" in acct:
            bal = int(acct["account_data"]["Balance"]) / DROPS_PER_QXRP
            print(f"Faucet already exists with {bal:,.2f} FALCON — skipping fund")
            return 0
    except Exception:
        pass

    genesis = rpc(args.public_rpc, "account_info", {
        "account": GENESIS_ACCOUNT,
        "ledger_index": "validated",
    }, container=container)
    seq = genesis["account_data"]["Sequence"]
    amount_drops = str(args.amount_qxrp * DROPS_PER_QXRP)

    tx = {
        "TransactionType": "Payment",
        "Account": GENESIS_ACCOUNT,
        "Destination": args.faucet,
        "Amount": amount_drops,
        "Fee": "12",
        "Sequence": seq,
        "LastLedgerSequence": ledger + 30,
    }
    if network_id > 1024:
        tx["NetworkID"] = network_id

    signed = rpc(
        args.admin_rpc,
        "sign",
        {"secret": GENESIS_SECRET, "tx_json": tx},
        container=container,
    )
    if signed.get("status") != "success":
        print("Sign failed:", signed, file=sys.stderr)
        return 1

    blob = signed["tx_blob"]
    tx_hash = signed.get("tx_json", {}).get("hash", "")
    sub = rpc(args.public_rpc, "submit", {"tx_blob": blob}, container=container)
    print("submit:", sub.get("engine_result"), sub.get("engine_result_message"))
    if sub.get("engine_result") not in ("tesSUCCESS", "terQUEUED"):
        return 1

    for _ in range(30):
        time.sleep(3)
        try:
            txr = rpc(args.public_rpc, "tx", {"transaction": tx_hash}, container=container)
            if txr.get("validated"):
                print("validated:", txr.get("meta", {}).get("TransactionResult"))
                acct = rpc(args.public_rpc, "account_info", {
                    "account": args.faucet,
                    "ledger_index": "validated",
                }, container=container)
                bal = int(acct["account_data"]["Balance"]) / DROPS_PER_QXRP
                print(f"Faucet {args.faucet} balance: {bal:,.2f} FALCON")
                return 0
        except Exception:
            pass

    print("Timed out waiting for validation", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())