#!/usr/bin/env python3
"""
bridge-withdraw-relay.py
────────────────────────
Watches Falcon QUC payments to the bridge issuer (with sepolia-withdraw memo)
and releases matching Sepolia USDC from FalconCollateralLock.

Usage:
  python3 scripts/bridge-withdraw-relay.py --dry-run
  python3 scripts/bridge-withdraw-relay.py --once
  python3 scripts/bridge-withdraw-relay.py --loop --interval 30

Environment:
  SEPOLIA_OWNER_PRIVATE_KEY  Sepolia key that owns the lock contract
  SEPOLIA_LOCK_CONTRACT      default from config/usdc-bridge.json
  PUBLIC_RPC_URL             Falcon public RPC
  BRIDGE_WITHDRAW_STATE_FILE default /var/lib/qxrp-bridge/withdraw_state.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
QUSDC_CURRENCY = "QUC"
MEMO_TYPE = "sepolia-withdraw"
EVM_RE = re.compile(r"^0x[a-fA-F0-9]{40}$")


class RpcClient:
    def __init__(self, public_url: str, container: str = ""):
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

    def public(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.public_url, method, params)


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def ok(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ✓ {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ⚠ {msg}", flush=True)


def load_json(path: Path) -> dict:
    return json.loads(path.read_text())


def save_state(path: Path, state: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state, indent=2) + "\n")


def hex_to_utf8(hex_str: str) -> str:
    h = hex_str.strip()
    if h.startswith("0x"):
        h = h[2:]
    if len(h) % 2:
        h = "0" + h
    try:
        return bytes.fromhex(h).decode("utf-8").rstrip("\x00")
    except Exception:
        return ""


def parse_sepolia_memo(tx_json: dict) -> str | None:
    memos = tx_json.get("Memos") or []
    for entry in memos:
        memo = entry.get("Memo") or entry
        memo_type = hex_to_utf8(str(memo.get("MemoType", "")))
        if memo_type != MEMO_TYPE:
            continue
        data = hex_to_utf8(str(memo.get("MemoData", ""))).strip()
        if EVM_RE.match(data):
            return data
    return None


def amount_to_raw(amount_str: str, decimals: int = 6) -> int:
    parts = amount_str.split(".")
    whole = int(parts[0]) if parts[0] else 0
    frac = (parts[1] if len(parts) > 1 else "").ljust(decimals, "0")[:decimals]
    return whole * (10 ** decimals) + int(frac or "0")


def withdrawal_id(falcon_tx: str, falcon_account: str, amount_raw: int, recipient: str) -> str:
    payload = f"{falcon_tx}:{falcon_account}:{amount_raw}:{recipient.lower()}".encode()
    return "0x" + hashlib.sha256(payload).hexdigest()


def fetch_withdrawal_candidates(
    rpc: RpcClient,
    issuer: str,
    currency: str,
    min_ledger: int,
) -> list[dict]:
    params: dict[str, Any] = {
        "account": issuer,
        "ledger_index_min": min_ledger if min_ledger > 0 else -1,
        "ledger_index_max": -1,
        "limit": 100,
        "forward": False,
    }
    r = rpc.public("account_tx", params)
    out: list[dict] = []
    for entry in r.get("transactions", []):
        if entry.get("validated") is False:
            continue
        meta = entry.get("meta", {})
        if meta.get("TransactionResult") != "tesSUCCESS":
            continue
        tx = entry.get("tx") or entry.get("transaction") or {}
        if tx.get("TransactionType") != "Payment":
            continue
        if tx.get("Destination") != issuer:
            continue
        amount = tx.get("Amount")
        if not isinstance(amount, dict):
            continue
        if amount.get("currency") != currency:
            continue
        if amount.get("issuer") != issuer:
            continue
        evm = parse_sepolia_memo(tx)
        if not evm:
            continue
        tx_hash = entry.get("hash") or tx.get("hash") or ""
        if not tx_hash:
            continue
        sender = tx.get("Account", "")
        value = str(amount.get("value", "0"))
        out.append({
            "falcon_tx": tx_hash,
            "falcon_account": sender,
            "amount_usdc": float(value),
            "amount_raw": amount_to_raw(value),
            "sepolia_recipient": evm,
            "ledger_index": entry.get("ledger_index"),
        })
    return out


def release_on_sepolia(
    lock_contract: str,
    amount_raw: int,
    recipient: str,
    wid: str,
    falcon_account: str,
    falcon_tx: str,
    dry_run: bool,
) -> tuple[bool, str]:
    if dry_run:
        log(
            f"[DRY RUN] withdraw {amount_raw} raw USDC → {recipient} "
            f"(falcon {falcon_tx[:16]}…)",
        )
        return True, "dry-run"

    script = Path(__file__).resolve().parent / "bridge-sepolia-withdraw.js"
    if not script.exists():
        script = REPO_ROOT / "scripts" / "bridge-sepolia-withdraw.js"
    env = os.environ.copy()
    cmd = [
        "node", str(script),
        "--lock", lock_contract,
        "--amount-raw", str(amount_raw),
        "--recipient", recipient,
        "--withdrawal-id", wid,
        "--falcon-account", falcon_account,
        "--falcon-tx", falcon_tx,
    ]
    try:
        out = subprocess.check_output(cmd, env=env, text=True, stderr=subprocess.STDOUT).strip()
        return True, out.splitlines()[-1]
    except subprocess.CalledProcessError as e:
        return False, (e.output or str(e)).strip()


def process_withdrawals(
    falcon: RpcClient,
    issuer: dict,
    lock_contract: str,
    state: dict,
    state_path: Path,
    dry_run: bool,
) -> int:
    issuer_addr = issuer["address"]
    min_ledger = int(state.get("last_ledger", 0))
    candidates = fetch_withdrawal_candidates(
        falcon, issuer_addr, QUSDC_CURRENCY, min_ledger,
    )
    processed_ids: set[str] = set(state.get("released_withdrawals", []))
    done = 0
    max_ledger = min_ledger

    for c in candidates:
        wid = withdrawal_id(
            c["falcon_tx"],
            c["falcon_account"],
            c["amount_raw"],
            c["sepolia_recipient"],
        ).lower()
        if wid in processed_ids:
            continue

        log(
            f"withdraw {c['amount_usdc']} QUC from {c['falcon_account']} "
            f"→ Sepolia {c['sepolia_recipient'][:10]}…",
        )

        success, detail = release_on_sepolia(
            lock_contract,
            c["amount_raw"],
            c["sepolia_recipient"],
            wid,
            c["falcon_account"],
            c["falcon_tx"],
            dry_run,
        )
        if not success:
            warn(f"Sepolia release failed: {detail}")
            continue

        ok(f"released {c['amount_usdc']} USDC on Sepolia ({detail[:18]}…)")
        processed_ids.add(wid)
        state.setdefault("released_withdrawals", [])
        if wid not in state["released_withdrawals"]:
            state["released_withdrawals"].append(wid)
        state.setdefault("releases", []).append({
            "withdrawal_id": wid,
            "falcon_tx": c["falcon_tx"],
            "falcon_account": c["falcon_account"],
            "amount_usdc": c["amount_usdc"],
            "sepolia_recipient": c["sepolia_recipient"],
            "sepolia_tx": detail if not dry_run else None,
            "released_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        })
        if c.get("ledger_index"):
            max_ledger = max(max_ledger, int(c["ledger_index"]))
        if not dry_run:
            save_state(state_path, state)
        done += 1

    if max_ledger > min_ledger:
        state["last_ledger"] = max_ledger
        if not dry_run:
            save_state(state_path, state)

    return done


def main() -> int:
    p = argparse.ArgumentParser(description="Falcon QUC return → Sepolia USDC release relay")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--once", action="store_true")
    p.add_argument("--loop", action="store_true")
    p.add_argument("--interval", type=int, default=30)
    p.add_argument(
        "--bridge-manifest",
        default=os.environ.get("BRIDGE_MANIFEST", str(REPO_ROOT / "config" / "usdc-bridge.json")),
    )
    p.add_argument(
        "--stables-state",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    p.add_argument(
        "--withdraw-state",
        default=os.environ.get("BRIDGE_WITHDRAW_STATE_FILE", "/var/lib/qxrp-bridge/withdraw_state.json"),
    )
    p.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"))
    p.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    args = p.parse_args()

    if not args.once and not args.loop and not args.dry_run:
        args.once = True

    bridge_cfg = load_json(Path(args.bridge_manifest))
    stables_state = load_json(Path(args.stables_state))

    lock = (
        os.environ.get("SEPOLIA_LOCK_CONTRACT", "").strip()
        or bridge_cfg.get("sepolia", {}).get("lock_contract", "")
    )
    if not lock.startswith("0x"):
        log("SEPOLIA_LOCK_CONTRACT not set")
        return 1

    if not args.dry_run and not (
        os.environ.get("SEPOLIA_OWNER_PRIVATE_KEY") or os.environ.get("PRIVATE_KEY")
    ):
        log("SEPOLIA_OWNER_PRIVATE_KEY required for live releases")
        return 1

    issuer = stables_state.get("qUSDC_issuer")
    if not issuer or not issuer.get("address"):
        log("qUSDC_issuer missing — run issue-testnet-stables.py first")
        return 1

    state_path = Path(args.withdraw_state)
    state = load_json(state_path) if state_path.exists() else {}

    falcon = RpcClient(args.public_rpc, args.container)
    log(f"withdraw relay: lock={lock} issuer={issuer['address']}")

    def poll_once() -> int:
        return process_withdrawals(
            falcon, issuer, lock, state, state_path, args.dry_run,
        )

    if args.loop:
        log(f"looping every {args.interval}s")
        while True:
            try:
                n = poll_once()
                if n:
                    ok(f"processed {n} withdrawal(s)")
            except Exception as e:
                warn(f"poll error: {e}")
            time.sleep(args.interval)
    else:
        n = poll_once()
        ok(f"done — released {n} withdrawal(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())