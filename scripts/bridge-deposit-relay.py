#!/usr/bin/env python3
"""
bridge-deposit-relay.py
───────────────────────
Watches Sepolia FalconCollateralLock DepositCreated events and mints matching
qUSDC (QUC) on Falcon Ledger testnet (1:1 with locked Sepolia USDC).

Testnet v1: single issuer account signs mints (from stables_state.json).
Production: replace with validator-attested CollateralBridge tx type.

Usage (coordinator):
  python3 scripts/bridge-deposit-relay.py --dry-run
  python3 scripts/bridge-deposit-relay.py --once
  python3 scripts/bridge-deposit-relay.py --loop --interval 30

Environment:
  SEPOLIA_RPC_URL          default https://ethereum-sepolia-rpc.publicnode.com
  SEPOLIA_LOCK_CONTRACT    default from config/usdc-bridge.json
  PUBLIC_RPC_URL           default http://127.0.0.1:6005
  ADMIN_RPC_URL            default http://127.0.0.1:5005
  DOCKER_CONTAINER         default qxrp-full
  STABLES_STATE_FILE       default /var/lib/qxrp-stables/stables_state.json
  BRIDGE_RELAY_STATE_FILE  default /var/lib/qxrp-bridge/relay_state.json
  BRIDGE_MANIFEST          default config/usdc-bridge.json (repo-relative)
  STABLES_MANIFEST         default config/testnet-stables.json
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

# DepositCreated(bytes32,address,uint256,string)
DEPOSIT_CREATED_TOPIC = (
    "0x77b58ff3106992e69c25650940327d9c1f8845c6dad4c0ae1a0f601640d91c87"
)

DROPS_PER_QXRP = 1_000_000
QUSDC_CURRENCY = "QUC"


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


SEPOLIA_RPC_FALLBACKS = [
    "https://ethereum-sepolia-rpc.publicnode.com",
    "https://1rpc.io/sepolia",
    "https://sepolia.drpc.org",
    "https://rpc2.sepolia.org",
]


class SepoliaClient:
    def __init__(self, rpc_urls: list[str]):
        self.rpc_urls = rpc_urls
        self._id = 0

    def call(self, method: str, params: list) -> Any:
        last_err: Exception | None = None
        for url in self.rpc_urls:
            try:
                self._id += 1
                body = json.dumps({
                    "jsonrpc": "2.0", "id": self._id, "method": method, "params": params,
                }).encode()
                req = urllib.request.Request(
                    url,
                    data=body,
                    headers={
                        "Content-Type": "application/json",
                        "User-Agent": "qxrp-bridge-relay/1.0",
                    },
                )
                with urllib.request.urlopen(req, timeout=45) as resp:
                    data = json.loads(resp.read())
                if data.get("error"):
                    raise RuntimeError(str(data["error"]))
                return data["result"]
            except Exception as e:
                last_err = e
        raise RuntimeError(f"all Sepolia RPCs failed: {last_err}")

    def block_number(self) -> int:
        return int(self.call("eth_blockNumber", []), 16)

    def get_logs(self, address: str, from_block: int, to_block: int) -> list[dict]:
        return self.call("eth_getLogs", [{
            "address": address,
            "fromBlock": hex(from_block),
            "toBlock": hex(to_block),
            "topics": [DEPOSIT_CREATED_TOPIC],
        }])


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


def pad32(hex_str: str) -> str:
    h = hex_str.lower().removeprefix("0x")
    return h.zfill(64)


def decode_deposit_log(log: dict) -> dict:
    topics = log.get("topics", [])
    if len(topics) < 3:
        raise ValueError("invalid DepositCreated log")
    deposit_id = topics[1]
    sender = "0x" + topics[2][-40:]
    data = log.get("data", "0x")[2:]
    if len(data) < 128:
        raise ValueError("deposit log data too short")
    amount = int(data[0:64], 16)
    str_off = int(data[64:128], 16) * 2
    str_len = int(data[str_off:str_off + 64], 16)
    str_hex = data[str_off + 64:str_off + 64 + str_len * 2]
    falcon_account = bytes.fromhex(str_hex).decode("utf-8")
    return {
        "deposit_id": deposit_id,
        "sender": sender,
        "amount": amount,
        "amount_usdc": amount / 1_000_000,
        "falcon_account": falcon_account,
        "block_number": int(log.get("blockNumber", "0x0"), 16),
        "tx_hash": log.get("transactionHash"),
    }


def sign_params_for_secret(secret: str, tx_json: dict) -> dict:
    if secret.startswith(("s", "S", "n", "N")) and len(secret) < 128:
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


def mint_quc(
    rpc: RpcClient,
    issuer: dict,
    destination: str,
    amount_usdc: float,
    dry_run: bool,
) -> tuple[bool, str]:
    secret = issuer.get("falcon_secret") or issuer.get("seed")
    issuer_addr = issuer["address"]
    amount_str = f"{amount_usdc:.6f}".rstrip("0").rstrip(".")

    acct = rpc.public("account_info", {"account": issuer_addr, "ledger_index": "validated"})
    seq = acct["account_data"]["Sequence"]
    ledger = rpc.public("server_info", {})["info"]["validated_ledger"]["seq"]

    tx = {
        "TransactionType": "Payment",
        "Account": issuer_addr,
        "Destination": destination,
        "Amount": {
            "currency": QUSDC_CURRENCY,
            "issuer": issuer_addr,
            "value": amount_str,
        },
        "Fee": "12",
        "Sequence": seq,
        "LastLedgerSequence": ledger + 30,
    }

    if dry_run:
        log(f"[DRY RUN] mint {amount_str} QUC → {destination}")
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


def _queue_pending(state: dict, dep: dict) -> None:
    """Remember deposits that could not mint yet so we retry on later polls."""
    dep_id = dep["deposit_id"].lower()
    pending = state.setdefault("pending_deposits", {})
    if dep_id not in pending:
        pending[dep_id] = {
            "deposit_id": dep_id,
            "falcon_account": dep["falcon_account"],
            "amount_usdc": dep["amount_usdc"],
            "sepolia_tx": dep.get("tx_hash"),
            "block_number": dep.get("block_number"),
            "queued_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }


def _retry_pending(
    falcon: RpcClient,
    issuer: dict,
    state: dict,
    state_path: Path,
    dry_run: bool,
) -> int:
    """Retry mints that were queued waiting for account or trust line."""
    pending: dict = state.get("pending_deposits") or {}
    if not pending:
        return 0
    minted_ids: set[str] = set(state.get("minted_deposits", []))
    processed = 0
    resolved: list[str] = []

    for dep_id, dep in list(pending.items()):
        if dep_id in minted_ids:
            resolved.append(dep_id)
            continue
        dest = dep["falcon_account"]
        amount = float(dep["amount_usdc"])
        log(f"retry pending {dep_id[:18]}… {amount} USDC → {dest}")

        if not account_exists(falcon, dest):
            warn(f"{dest} still unfunded — keep queued")
            continue
        if not has_trust_line(falcon, dest, QUSDC_CURRENCY, issuer["address"]):
            warn(f"{dest} still has no QUC trust line — keep queued")
            continue

        success, detail = mint_quc(falcon, issuer, dest, amount, dry_run)
        if not success:
            warn(f"pending mint failed: {detail}")
            continue

        ok(f"minted {amount} QUC → {dest} ({detail[:16]}…)")
        minted_ids.add(dep_id)
        state.setdefault("minted_deposits", [])
        if dep_id not in state["minted_deposits"]:
            state["minted_deposits"].append(dep_id)
        state.setdefault("mints", []).append({
            "deposit_id": dep_id,
            "falcon_account": dest,
            "amount_usdc": amount,
            "sepolia_tx": dep.get("sepolia_tx"),
            "falcon_tx": detail if not dry_run else None,
            "minted_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "from_pending": True,
        })
        resolved.append(dep_id)
        processed += 1
        if not dry_run:
            save_state(state_path, state)

    for dep_id in resolved:
        pending.pop(dep_id, None)
    if resolved and not dry_run:
        save_state(state_path, state)
    return processed


def process_deposits(
    sepolia: SepoliaClient,
    falcon: RpcClient,
    lock_contract: str,
    issuer: dict,
    state: dict,
    state_path: Path,
    from_block: int,
    to_block: int,
    dry_run: bool,
) -> int:
    logs = sepolia.get_logs(lock_contract, from_block, to_block)
    processed = 0
    minted_ids: set[str] = set(state.get("minted_deposits", []))

    for raw in logs:
        try:
            dep = decode_deposit_log(raw)
        except Exception as e:
            warn(f"skip log {raw.get('transactionHash')}: {e}")
            continue

        dep_id = dep["deposit_id"].lower()
        if dep_id in minted_ids:
            continue

        dest = dep["falcon_account"]
        amount = dep["amount_usdc"]
        log(
            f"deposit {dep_id[:18]}… {amount} USDC → {dest} "
            f"(sep tx {dep.get('tx_hash', '')[:14]}…)",
        )

        if not account_exists(falcon, dest):
            warn(f"Falcon account {dest} not found — queued (fund via faucet first)")
            _queue_pending(state, dep)
            continue

        if not has_trust_line(falcon, dest, QUSDC_CURRENCY, issuer["address"]):
            warn(f"{dest} has no QUC trust line — queued (open Swap tab to add trust line)")
            _queue_pending(state, dep)
            continue

        success, detail = mint_quc(falcon, issuer, dest, amount, dry_run)
        if not success:
            warn(f"mint failed: {detail}")
            continue

        ok(f"minted {amount} QUC → {dest} ({detail[:16]}…)")
        minted_ids.add(dep_id)
        (state.get("pending_deposits") or {}).pop(dep_id, None)
        state.setdefault("minted_deposits", [])
        if dep_id not in state["minted_deposits"]:
            state["minted_deposits"].append(dep_id)
        state.setdefault("mints", []).append({
            "deposit_id": dep_id,
            "falcon_account": dest,
            "amount_usdc": amount,
            "sepolia_tx": dep.get("tx_hash"),
            "falcon_tx": detail if not dry_run else None,
            "minted_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        })
        if not dry_run:
            save_state(state_path, state)
        processed += 1

    state["last_block"] = to_block
    if not dry_run:
        save_state(state_path, state)
    return processed


def main() -> int:
    p = argparse.ArgumentParser(description="Sepolia USDC lock → Falcon QUC mint relay")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--once", action="store_true", help="single poll then exit")
    p.add_argument("--loop", action="store_true", help="poll continuously")
    p.add_argument("--interval", type=int, default=30)
    p.add_argument("--from-block", type=int, default=0, help="override start block")
    p.add_argument(
        "--bridge-manifest",
        default=os.environ.get("BRIDGE_MANIFEST", str(REPO_ROOT / "config" / "usdc-bridge.json")),
    )
    p.add_argument(
        "--stables-manifest",
        default=os.environ.get("STABLES_MANIFEST", str(REPO_ROOT / "config" / "testnet-stables.json")),
    )
    p.add_argument(
        "--stables-state",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    p.add_argument(
        "--relay-state",
        default=os.environ.get("BRIDGE_RELAY_STATE_FILE", "/var/lib/qxrp-bridge/relay_state.json"),
    )
    p.add_argument("--sepolia-rpc", default=os.environ.get("SEPOLIA_RPC_URL", "https://ethereum-sepolia-rpc.publicnode.com"))
    p.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"))
    p.add_argument("--admin-rpc", default=os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"))
    p.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    args = p.parse_args()

    if not args.once and not args.loop and not args.dry_run:
        args.once = True

    bridge_cfg = load_json(Path(args.bridge_manifest))
    stables_manifest = load_json(Path(args.stables_manifest))
    stables_state = load_json(Path(args.stables_state))

    lock = (
        os.environ.get("SEPOLIA_LOCK_CONTRACT", "").strip()
        or bridge_cfg.get("sepolia", {}).get("lock_contract", "")
    )
    if not lock.startswith("0x"):
        log("SEPOLIA_LOCK_CONTRACT not set")
        return 1

    issuer = stables_state.get("qUSDC_issuer")
    if not issuer or not issuer.get("address"):
        log("qUSDC_issuer missing from stables state — run issue-testnet-stables.py first")
        return 1

    manifest_issuer = next(
        (t["issuer"] for t in stables_manifest.get("tokens", []) if t.get("currency") == QUSDC_CURRENCY),
        None,
    )
    if manifest_issuer and issuer["address"] != manifest_issuer:
        warn(f"issuer mismatch: state={issuer['address']} manifest={manifest_issuer}")

    state_path = Path(args.relay_state)
    state = load_json(state_path) if state_path.exists() else {}

    rpc_urls = [args.sepolia_rpc] + [u for u in SEPOLIA_RPC_FALLBACKS if u != args.sepolia_rpc]
    sepolia = SepoliaClient(rpc_urls)
    falcon = RpcClient(args.admin_rpc, args.public_rpc, args.container)

    log(f"relay: lock={lock} issuer={issuer['address']}")

    def poll_once() -> int:
        if args.from_block and not state.get("last_block"):
            fb = args.from_block
        elif state.get("last_block") is not None:
            fb = int(state["last_block"]) + 1
        elif state.get("deploy_block"):
            fb = int(state["deploy_block"])
        else:
            head = sepolia.block_number()
            fb = max(0, head - 50_000)
            state["deploy_block"] = fb
            log(f"first run: scanning from block {fb}")

        head = sepolia.block_number()
        total = _retry_pending(falcon, issuer, state, state_path, args.dry_run)
        if fb > head:
            return total

        chunk = 2000
        cursor = fb
        while cursor <= head:
            tb = min(cursor + chunk - 1, head)
            total += process_deposits(
                sepolia, falcon, lock, issuer, state, state_path,
                cursor, tb, args.dry_run,
            )
            cursor = tb + 1
        return total

    if args.loop:
        log(f"looping every {args.interval}s")
        while True:
            try:
                n = poll_once()
                if n:
                    ok(f"processed {n} deposit(s)")
            except Exception as e:
                warn(f"poll error: {e}")
            time.sleep(args.interval)
    else:
        n = poll_once()
        ok(f"done — minted {n} deposit(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())