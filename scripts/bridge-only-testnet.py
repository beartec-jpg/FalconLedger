#!/usr/bin/env python3
"""Cancel bootstrap LP DEX offers — testnet uses bridge-minted QUC only."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
GENESIS_SECRET = "masterpassphrase"


class RpcClient:
    def __init__(self, admin_url: str, public_url: str, container: str = "qxrp-full"):
        self.admin_url = admin_url
        self.public_url = public_url
        self.container = container

    def _post(self, url: str, method: str, params: dict | None = None) -> dict:
        payload = json.dumps({"method": method, "params": [params or {}]}).encode()
        if self.container:
            cmd = [
                "docker", "exec", self.container,
                "curl", "-sf", "-X", "POST", url,
                "-H", "Content-Type: application/json",
                "-d", payload.decode(),
            ]
            out = subprocess.check_output(cmd, text=True)
            body = json.loads(out)
        else:
            req = urllib.request.Request(
                url, data=payload, headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=30) as resp:
                body = json.loads(resp.read())
        if body.get("error"):
            raise RuntimeError(str(body["error"]))
        return body.get("result", body)

    def admin(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.admin_url, method, params)

    def public(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.public_url, method, params)


def sign_params(secret: str, tx: dict) -> dict:
    if secret.startswith(("s", "S")) and len(secret) < 128:
        return {"secret": secret, "tx_json": tx}
    return {"falcon_secret": secret, "tx_json": tx}


def main() -> int:
    state_path = Path(os.environ.get(
        "STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json",
    ))
    state = json.loads(state_path.read_text())
    lp = state.get("liquidity_provider")
    if not lp:
        print("liquidity_provider missing from stables state", file=sys.stderr)
        return 1

    rpc = RpcClient(
        os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"),
        os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"),
        os.environ.get("DOCKER_CONTAINER", "qxrp-full"),
    )

    secret = lp.get("falcon_secret") or lp.get("seed")
    address = lp["address"]
    offers = rpc.public("account_offers", {"account": address, "ledger_index": "validated"})
    seq_offers = offers.get("offers", [])
    if not seq_offers:
        print(f"No open offers on {address}")
        return 0

    acct = rpc.public("account_info", {"account": address, "ledger_index": "validated"})
    seq = acct["account_data"]["Sequence"]
    ledger = rpc.public("server_info", {})["info"]["validated_ledger"]["seq"]

    print(f"Cancelling {len(seq_offers)} bootstrap offer(s) from {address}")
    for offer in seq_offers:
        tx = {
            "TransactionType": "OfferCancel",
            "Account": address,
            "OfferSequence": offer["seq"],
            "Fee": "12",
            "Sequence": seq,
            "LastLedgerSequence": ledger + 30,
        }
        signed = rpc.admin("sign", sign_params(secret, tx))
        if signed.get("status") != "success":
            print("sign failed:", signed, file=sys.stderr)
            return 1
        sub = rpc.public("submit", {"tx_blob": signed["tx_blob"]})
        print("cancel", offer["seq"], sub.get("engine_result"), sub.get("engine_result_message"))
        seq += 1
        time.sleep(4)

    print("Bootstrap offers cancelled. Use bridge-minted QUC + user-posted DEX orders only.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())