#!/usr/bin/env python3
"""
issue-bridge-iou.py
───────────────────
Create a Falcon Ledger issuer account for a bridged IOU (FETH, FBTC, FBNB, …)
with DefaultRipple enabled. Does NOT bootstrap free supply — minting is only
via the deposit relay after EVM lock (bridge-only model).

Usage (coordinator):
  python3 scripts/issue-bridge-iou.py --symbol FETH --currency ETH --dry-run
  python3 scripts/issue-bridge-iou.py --symbol FETH --currency ETH

State is merged into STABLES_STATE_FILE under key {SYMBOL}_issuer
(e.g. FETH_issuer). Optionally appends token to testnet-stables.json.

Environment:
  ADMIN_RPC_URL, PUBLIC_RPC_URL, DOCKER_CONTAINER
  STABLES_STATE_FILE  default /var/lib/qxrp-stables/stables_state.json
  STABLES_MANIFEST    default config/testnet-stables.json
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
import urllib.request
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
DROPS_PER_QXRP = 1_000_000
GENESIS_ACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
GENESIS_SECRET = "masterpassphrase"
ISSUER_RESERVE_QXRP = 15


class RpcClient:
    def __init__(self, admin_url: str, public_url: str, container: str = ""):
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


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def ok(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ✓ {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ⚠ {msg}", flush=True)


def load_json(path: Path) -> dict:
    if not path.exists():
        return {}
    return json.loads(path.read_text())


def save_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n")


def sign_params(secret: str, tx_json: dict) -> dict:
    if secret.startswith(("s", "S", "n", "N")) and len(secret) < 128:
        return {"secret": secret, "tx_json": tx_json}
    return {"falcon_secret": secret, "tx_json": tx_json}


def submit_tx(rpc: RpcClient, secret: str, tx: dict, dry_run: bool) -> str:
    if dry_run:
        log(f"[DRY RUN] {tx.get('TransactionType')} Account={tx.get('Account')}")
        return "dry-run"
    signed = rpc.admin("sign", sign_params(secret, tx))
    if signed.get("status") != "success":
        raise RuntimeError(str(signed.get("error_message", signed)))
    blob = signed["tx_blob"]
    tx_hash = signed.get("tx_json", {}).get("hash", "")
    sub = rpc.public("submit", {"tx_blob": blob})
    result = sub.get("engine_result", "?")
    if result not in ("tesSUCCESS", "terQUEUED"):
        raise RuntimeError(f"{result}: {sub.get('engine_result_message', '')}")
    for _ in range(30):
        time.sleep(2)
        try:
            r = rpc.public("tx", {"transaction": tx_hash})
            if r.get("validated"):
                tr = r.get("meta", {}).get("TransactionResult", "?")
                if tr != "tesSUCCESS":
                    raise RuntimeError(f"validated {tr}")
                return tx_hash
        except Exception as e:
            if "validated" in str(e):
                raise
    raise RuntimeError("timeout waiting for validation")


def account_exists(rpc: RpcClient, address: str) -> bool:
    try:
        r = rpc.public("account_info", {"account": address, "ledger_index": "validated"})
        return "account_data" in r
    except Exception:
        return False


def wallet_propose(rpc: RpcClient) -> dict[str, str]:
    r = rpc.admin("wallet_propose", {})
    # Falcon may return falcon_secret or classic seed
    secret = r.get("falcon_secret") or r.get("master_seed") or r.get("seed")
    address = r.get("account_id") or r.get("account")
    if not secret or not address:
        raise RuntimeError(f"wallet_propose unexpected: {r}")
    return {"address": address, "falcon_secret": secret}


def main() -> int:
    p = argparse.ArgumentParser(description="Create bridge-only Falcon IOU issuer (no free mint)")
    p.add_argument("--symbol", required=True, help="Display symbol e.g. FETH")
    p.add_argument("--currency", required=True, help="XRPL currency code e.g. ETH")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument(
        "--stables-state",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    p.add_argument(
        "--stables-manifest",
        default=os.environ.get("STABLES_MANIFEST", str(REPO_ROOT / "config" / "testnet-stables.json")),
    )
    p.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"))
    p.add_argument("--admin-rpc", default=os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"))
    p.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    p.add_argument("--no-manifest-update", action="store_true")
    args = p.parse_args()

    symbol = args.symbol.strip()
    currency = args.currency.strip().upper()
    if len(currency) > 3 and len(currency) != 40:
        # classic 3-char or 40-hex currency
        warn(f"currency '{currency}' is unusual (expect 3-char or 40-hex)")

    state_key = f"{symbol.replace('-', '_')}_issuer"
    state_path = Path(args.stables_state)
    state = load_json(state_path)

    rpc = RpcClient(args.admin_rpc, args.public_rpc, args.container)

    existing = state.get(state_key)
    if existing and existing.get("address") and account_exists(rpc, existing["address"]):
        ok(f"{state_key} already live at {existing['address']}")
        issuer_addr = existing["address"]
        issuer_secret = existing.get("falcon_secret") or existing.get("seed")
    else:
        log(f"proposing new issuer for {symbol} ({currency})…")
        if args.dry_run:
            issuer_addr = "rDRYRUNIssuerXXXXXXXXXXXXXXXXXXX"
            issuer_secret = "sDRYRUN"
            ok(f"[DRY RUN] would create {state_key}")
        else:
            w = wallet_propose(rpc)
            issuer_addr = w["address"]
            issuer_secret = w["falcon_secret"]
            ok(f"issuer account {issuer_addr}")

            # Fund from genesis
            fund_drops = str(ISSUER_RESERVE_QXRP * DROPS_PER_QXRP)
            ginfo = rpc.public("account_info", {"account": GENESIS_ACCOUNT, "ledger_index": "validated"})
            gseq = ginfo["account_data"]["Sequence"]
            ledger = rpc.public("server_info", {})["info"]["validated_ledger"]["seq"]
            pay = {
                "TransactionType": "Payment",
                "Account": GENESIS_ACCOUNT,
                "Destination": issuer_addr,
                "Amount": fund_drops,
                "Fee": "12",
                "Sequence": gseq,
                "LastLedgerSequence": ledger + 30,
            }
            submit_tx(rpc, GENESIS_SECRET, pay, False)
            ok(f"funded {ISSUER_RESERVE_QXRP} FALCON → issuer")

            # DefaultRipple
            iinfo = rpc.public("account_info", {"account": issuer_addr, "ledger_index": "validated"})
            iseq = iinfo["account_data"]["Sequence"]
            ledger = rpc.public("server_info", {})["info"]["validated_ledger"]["seq"]
            aset = {
                "TransactionType": "AccountSet",
                "Account": issuer_addr,
                "SetFlag": 8,  # asfDefaultRipple
                "Fee": "12",
                "Sequence": iseq,
                "LastLedgerSequence": ledger + 30,
            }
            submit_tx(rpc, issuer_secret, aset, False)
            ok("DefaultRipple enabled")

            state[state_key] = {
                "address": issuer_addr,
                "falcon_secret": issuer_secret,
                "symbol": symbol,
                "currency": currency,
                "bridge_only": True,
                "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            }
            save_json(state_path, state)
            ok(f"wrote {state_key} → {state_path}")

    if not args.no_manifest_update and not args.dry_run:
        man_path = Path(args.stables_manifest)
        man = load_json(man_path)
        tokens: list[dict[str, Any]] = man.setdefault("tokens", [])
        if not any(t.get("currency") == currency and t.get("issuer") == issuer_addr for t in tokens):
            tokens.append({
                "symbol": symbol,
                "currency": currency,
                "issuer": issuer_addr,
                "liquidity": "bridge_user_lp",
            })
            man["note"] = man.get("note") or "Bridge-only IOUs; no free bootstrap mint."
            save_json(man_path, man)
            ok(f"appended {symbol} to {man_path}")
        else:
            log("manifest already has this token")

    print()
    print("=== Bridge IOU issuer ready ===")
    print(f"  symbol:   {symbol}")
    print(f"  currency: {currency}")
    print(f"  issuer:   {issuer_addr}")
    print(f"  state:    {state_key}")
    print()
    print("Next:")
    print(f"  1. Deploy FalconCollateralLock with the source ERC-20 (WETH for FETH)")
    print(f"  2. Run deposit relay:")
    print(
        f"     python3 scripts/bridge-deposit-relay.py --loop "
        f"--currency {currency} --decimals 18 --issuer-key {state_key} "
        f"--lock-contract 0x… "
        f"--relay-state /var/lib/qxrp-bridge/{symbol.lower()}_relay_state.json",
    )
    print(f"  3. Copy issuer into faucet-wallet public/config/testnet-stables.json")
    print(f"  4. Enable {symbol} in multi-chain-assets + bridges.json")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
