#!/usr/bin/env python3
"""
bridge-btc-deposit-relay.py
───────────────────────────
Native Bitcoin (testnet) → Falcon FBTC mint relay.

UX: wallet sends user's multi-chain BTC to the custody P2PKH address, then
registers a claim (txid + falcon account + amount). This relay verifies the
BTC tx on a public explorer and mints 1:1 FBTC (currency BTC) — no WBTC in
the product wallet.

Usage:
  python3 scripts/bridge-btc-deposit-relay.py --once
  python3 scripts/bridge-btc-deposit-relay.py --loop --interval 30

Environment:
  STABLES_STATE_FILE   default /var/lib/qxrp-stables/stables_state.json
  BTC_CUSTODY_FILE     default /var/lib/qxrp-bridge/btc-custody.json
  BTC_CLAIMS_FILE      default /var/lib/qxrp-bridge/fbtc_claims.json
  BTC_RELAY_STATE_FILE default /var/lib/qxrp-bridge/fbtc_relay_state.json
  PUBLIC_RPC_URL / ADMIN_RPC_URL / DOCKER_CONTAINER
  BTC_NETWORK          testnet | mainnet (default testnet)
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
BTC_CURRENCY = "BTC"
ISSUER_KEY = "FBTC_issuer"

EXPLORERS = {
    "testnet": [
        "https://blockstream.info/testnet/api",
        "https://mempool.space/testnet/api",
    ],
    "mainnet": [
        "https://blockstream.info/api",
        "https://mempool.space/api",
    ],
}


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


def explorer_get(network: str, path: str) -> Any:
    last: Exception | None = None
    for base in EXPLORERS[network]:
        try:
            req = urllib.request.Request(
                f"{base}{path}",
                headers={"Accept": "application/json", "User-Agent": "qxrp-fbtc-relay/1.0"},
            )
            with urllib.request.urlopen(req, timeout=45) as resp:
                return json.loads(resp.read())
        except Exception as e:
            last = e
    raise RuntimeError(f"explorer failed: {last}")


def sign_params_for_secret(secret: str, tx_json: dict) -> dict:
    if secret == "masterpassphrase" or (
        secret.startswith(("s", "S", "n", "N")) and len(secret) < 128
    ):
        return {"secret": secret, "tx_json": tx_json}
    return {"falcon_secret": secret, "tx_json": tx_json}


def account_exists(rpc: RpcClient, address: str) -> bool:
    try:
        r = rpc.public("account_info", {"account": address, "ledger_index": "validated"})
        return "account_data" in r
    except Exception:
        return False


def has_trust_line(rpc: RpcClient, address: str, currency: str, issuer: str) -> bool:
    try:
        r = rpc.public("account_lines", {"account": address, "ledger_index": "validated"})
        for line in r.get("lines", []):
            if line.get("currency") == currency and line.get("account") == issuer:
                return True
    except Exception:
        pass
    return False


def wait_validated(rpc: RpcClient, tx_hash: str) -> str:
    for _ in range(30):
        time.sleep(3)
        try:
            r = rpc.public("tx", {"transaction": tx_hash})
            if r.get("validated"):
                return r.get("meta", {}).get("TransactionResult", "?")
        except Exception:
            pass
    return "TIMEOUT"


def mint_fbtc(
    rpc: RpcClient,
    issuer: dict,
    destination: str,
    amount_btc: float,
    dry_run: bool,
) -> tuple[bool, str]:
    secret = issuer.get("falcon_secret") or issuer.get("seed")
    issuer_addr = issuer["address"]
    amount_str = f"{amount_btc:.8f}".rstrip("0").rstrip(".")
    if not amount_str:
        amount_str = "0"

    acct = rpc.public("account_info", {"account": issuer_addr, "ledger_index": "validated"})
    seq = acct["account_data"]["Sequence"]
    ledger = rpc.public("server_info", {})["info"]["validated_ledger"]["seq"]

    tx = {
        "TransactionType": "Payment",
        "Account": issuer_addr,
        "Destination": destination,
        "Amount": {
            "currency": BTC_CURRENCY,
            "issuer": issuer_addr,
            "value": amount_str,
        },
        "Fee": "12",
        "Sequence": seq,
        "LastLedgerSequence": ledger + 30,
    }

    if dry_run:
        log(f"[DRY RUN] mint {amount_str} BTC → {destination}")
        return True, "dry-run"

    signed = rpc.admin("sign", sign_params_for_secret(secret, tx))
    if signed.get("status") != "success":
        return False, str(signed.get("error_message", signed))

    blob = signed["tx_blob"]
    tx_hash = signed.get("tx_json", {}).get("hash", "")
    sub = rpc.public("submit", {"tx_blob": blob})
    result = sub.get("engine_result", "?")
    if result not in ("tesSUCCESS", "terQUEUED"):
        return False, f"{result}: {sub.get('engine_result_message', '')}"

    final = wait_validated(rpc, tx_hash)
    if final != "tesSUCCESS":
        return False, f"validated {final}"
    return True, tx_hash


def verify_btc_deposit(
    network: str,
    custody: str,
    txid: str,
    min_sats: int,
) -> tuple[bool, int, str]:
    """
    Confirm txid pays custody at least min_sats (sum of matching vouts).
    Returns (ok, credited_sats, detail).
    """
    txid = txid.strip().lower()
    if len(txid) != 64 or any(c not in "0123456789abcdef" for c in txid):
        return False, 0, "invalid txid"

    try:
        tx = explorer_get(network, f"/tx/{txid}")
    except Exception as e:
        return False, 0, f"tx lookup failed: {e}"

    credited = 0
    for vout in tx.get("vout") or []:
        addr = vout.get("scriptpubkey_address") or ""
        if addr == custody:
            credited += int(vout.get("value") or 0)

    if credited <= 0:
        return False, 0, "no output to custody address"
    if credited < min_sats:
        return False, credited, f"underpaid: {credited} < {min_sats} sats"

    # Prefer confirmed; allow mempool for testnet speed (status.confirmed)
    status = tx.get("status") or {}
    confirmed = bool(status.get("confirmed"))
    detail = "confirmed" if confirmed else "mempool"
    return True, credited, detail


def process_claims(
    falcon: RpcClient,
    issuer: dict,
    custody: str,
    network: str,
    claims_path: Path,
    state_path: Path,
    dry_run: bool,
    require_confirmed: bool,
) -> int:
    claims = load_json(claims_path)
    pending: dict = claims.get("pending") or {}
    if not pending:
        return 0

    state = load_json(state_path)
    minted_ids: set[str] = set(state.get("minted_txids", []))
    processed = 0
    resolved: list[str] = []

    for claim_id, claim in list(pending.items()):
        txid = (claim.get("btc_txid") or "").strip().lower()
        dest = (claim.get("falcon_account") or "").strip()
        amount_sats = int(claim.get("amount_sats") or 0)
        if not txid or not dest or amount_sats <= 0:
            warn(f"skip bad claim {claim_id}")
            resolved.append(claim_id)
            continue
        if txid in minted_ids:
            resolved.append(claim_id)
            continue

        log(f"claim {claim_id[:12]}… {amount_sats} sats → {dest} (btc {txid[:14]}…)")

        ok_dep, credited, detail = verify_btc_deposit(network, custody, txid, amount_sats)
        if not ok_dep:
            warn(f"  not ready: {detail}")
            continue
        if require_confirmed and detail != "confirmed":
            warn(f"  waiting for confirmation ({detail})")
            continue

        if not account_exists(falcon, dest):
            warn(f"  Falcon {dest} unfunded — keep pending")
            continue
        if not has_trust_line(falcon, dest, BTC_CURRENCY, issuer["address"]):
            warn(f"  no BTC trust line — keep pending")
            continue

        amount_btc = credited / 1e8
        success, mint_detail = mint_fbtc(falcon, issuer, dest, amount_btc, dry_run)
        if not success:
            warn(f"  mint failed: {mint_detail}")
            continue

        ok(f"minted {amount_btc} FBTC → {dest} ({str(mint_detail)[:18]}…)")
        minted_ids.add(txid)
        state.setdefault("minted_txids", [])
        if txid not in state["minted_txids"]:
            state["minted_txids"].append(txid)
        state.setdefault("mints", []).append({
            "btc_txid": txid,
            "falcon_account": dest,
            "amount_sats": credited,
            "amount_btc": amount_btc,
            "currency": BTC_CURRENCY,
            "falcon_tx": mint_detail if not dry_run else None,
            "minted_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "claim_id": claim_id,
        })
        if not dry_run:
            save_json(state_path, state)
        resolved.append(claim_id)
        processed += 1

    for cid in resolved:
        pending.pop(cid, None)
    if resolved:
        claims["pending"] = pending
        if not dry_run:
            save_json(claims_path, claims)
    return processed


def main() -> int:
    p = argparse.ArgumentParser(description="Native BTC → Falcon FBTC mint relay")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--once", action="store_true")
    p.add_argument("--loop", action="store_true")
    p.add_argument("--interval", type=int, default=30)
    p.add_argument(
        "--network",
        default=os.environ.get("BTC_NETWORK", "testnet"),
        choices=["testnet", "mainnet"],
    )
    p.add_argument(
        "--require-confirmed",
        action="store_true",
        help="Wait for BTC confirmation (default: allow mempool on testnet)",
    )
    p.add_argument(
        "--custody-file",
        default=os.environ.get("BTC_CUSTODY_FILE", "/var/lib/qxrp-bridge/btc-custody.json"),
    )
    p.add_argument(
        "--claims-file",
        default=os.environ.get("BTC_CLAIMS_FILE", "/var/lib/qxrp-bridge/fbtc_claims.json"),
    )
    p.add_argument(
        "--relay-state",
        default=os.environ.get("BTC_RELAY_STATE_FILE", "/var/lib/qxrp-bridge/fbtc_relay_state.json"),
    )
    p.add_argument(
        "--stables-state",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    p.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"))
    p.add_argument("--admin-rpc", default=os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"))
    p.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    args = p.parse_args()

    if not args.once and not args.loop and not args.dry_run:
        args.once = True

    # testnet default: allow mempool; mainnet should confirm
    require_confirmed = args.require_confirmed or args.network == "mainnet"

    custody_path = Path(args.custody_file)
    if not custody_path.exists():
        log(f"missing custody file {custody_path}")
        return 1
    custody = load_json(custody_path)
    addr_key = "address_testnet" if args.network == "testnet" else "address_mainnet"
    custody_addr = custody.get(addr_key) or custody.get("address")
    if not custody_addr:
        log("custody address missing")
        return 1

    stables = load_json(Path(args.stables_state))
    issuer = stables.get(ISSUER_KEY)
    if not issuer or not issuer.get("address"):
        log(f"{ISSUER_KEY} missing — run issue-bridge-iou.py --symbol FBTC --currency BTC")
        return 1

    falcon = RpcClient(args.admin_rpc, args.public_rpc, args.container)
    claims_path = Path(args.claims_file)
    state_path = Path(args.relay_state)

    log(
        f"FBTC relay: network={args.network} custody={custody_addr} "
        f"issuer={issuer['address']} require_confirmed={require_confirmed}",
    )

    def poll() -> int:
        return process_claims(
            falcon, issuer, custody_addr, args.network,
            claims_path, state_path, args.dry_run, require_confirmed,
        )

    if args.loop:
        log(f"looping every {args.interval}s")
        while True:
            try:
                n = poll()
                if n:
                    ok(f"processed {n} claim(s)")
            except Exception as e:
                warn(f"poll error: {e}")
            time.sleep(args.interval)
    else:
        n = poll()
        ok(f"done — processed {n} claim(s)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
