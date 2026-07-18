#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
bootstrap-testnet-lending.py — F-USDC vault + loan broker for portal lending.

Usage (coordinator):
  python3 scripts/bootstrap-testnet-lending.py --dry-run
  python3 scripts/bootstrap-testnet-lending.py
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
import urllib.request
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

DROPS_PER_QXRP = 1_000_000
NETWORK_ID = 1001
PUBLIC_RPC = "http://46.224.0.140:6005"
REPO_ROOT = Path(__file__).resolve().parent.parent

try:
    from launch_guards import bridge_only_required, is_testnet_network  # noqa: E402
except ImportError:  # optional on lean coordinator installs
    def bridge_only_required() -> bool:  # type: ignore[misc]
        return False

    def is_testnet_network(network_id: int | None = None) -> bool:  # type: ignore[misc]
        return True

# Vault starts empty — LPs supply F-USDC via the portal. No operator seed deposit.
VAULT_SEED_DEPOSIT = "0"
DEBT_MAXIMUM = "100000"
COVER_RATE_MINIMUM = 1000
COVER_RATE_LIQUIDATION = 2500
MANAGEMENT_FEE_RATE = 100
# Broker posts cover when F-USDC exists and borrow is enabled — not at genesis.
COVER_ASSET_VALUE = "0"
INTEREST_RATE = 500


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
    print(f"[{time.strftime('%H:%M:%S')}] {msg}")


def ok(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ✓ {msg}")


def warn(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ⚠ {msg}")


def err(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ✗ {msg}")


def load_state(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except FileNotFoundError:
        return {}


def save_state(path: Path, state: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state, indent=2) + "\n")
    ok(f"State saved to {path}")


def account_exists(rpc: RpcClient, address: str) -> bool:
    try:
        r = rpc.public("account_info", {"account": address, "ledger_index": "validated"})
        return "account_data" in r
    except Exception:
        return False


def get_trust_line(rpc: RpcClient, address: str, currency: str, issuer: str) -> dict | None:
    try:
        r = rpc.public("account_lines", {"account": address, "ledger_index": "validated"})
        for line in r.get("lines", []):
            if line.get("currency") == currency and line.get("account") == issuer:
                return line
    except Exception:
        pass
    return None


def sign_params_for_secret(secret: str, tx_json: dict) -> dict:
    if secret.startswith(("s", "S", "n", "N")) and len(secret) < 128:
        return {"secret": secret, "tx_json": tx_json}
    return {"falcon_secret": secret, "tx_json": tx_json}


def sign_and_submit(rpc: RpcClient, secret: str, tx_json: dict) -> tuple[str, str]:
    signed = rpc.admin("sign", sign_params_for_secret(secret, tx_json))
    if signed.get("status") != "success":
        return f"SIGN_ERR: {signed.get('error_message', signed)}", ""
    blob = signed["tx_blob"]
    tx_hash = signed.get("tx_json", {}).get("hash", "")
    sub = rpc.public("submit", {"tx_blob": blob})
    return sub.get("engine_result", "?"), tx_hash


def wait_validated(rpc: RpcClient, tx_hash: str, retries: int = 30) -> str:
    for _ in range(retries):
        time.sleep(3)
        try:
            r = rpc.public("tx", {"transaction": tx_hash, "binary": False})
            if r.get("validated"):
                return r.get("meta", {}).get("TransactionResult", "?")
        except Exception:
            pass
    return "TIMEOUT"


def submit_tx(rpc: RpcClient, secret: str, tx_json: dict, dry_run: bool, label: str) -> bool:
    if dry_run:
        log(f"[DRY RUN] {label}")
        return True
    result, tx_hash = sign_and_submit(rpc, secret, tx_json)
    if result in ("tesSUCCESS", "terQUEUED"):
        ok(f"{label}: {result} ({tx_hash[:12]}…)")
        final = wait_validated(rpc, tx_hash)
        if final != "tesSUCCESS":
            warn(f"{label}: validated as {final}")
            return final == "tesSUCCESS"
        return True
    err(f"{label}: {result}")
    return False


def iou_value(v: object) -> float | None:
    if v is None:
        return None
    if isinstance(v, (str, int, float)):
        try:
            return float(v)
        except (TypeError, ValueError):
            return None
    if isinstance(v, dict) and "value" in v:
        return iou_value(v["value"])
    return None


def find_vault_for_owner(rpc: RpcClient, owner: str) -> dict | None:
    try:
        r = rpc.public("account_objects", {
            "account": owner,
            "type": "vault",
            "ledger_index": "validated",
        })
        objs = r.get("account_objects") or []
        return objs[0] if objs else None
    except Exception:
        return None


def object_id(obj: dict | None, *keys: str) -> str | None:
    if not obj:
        return None
    for key in keys:
        val = obj.get(key)
        if val:
            return str(val)
    return None


def find_broker_for_owner(rpc: RpcClient, owner: str) -> dict | None:
    try:
        r = rpc.public("account_objects", {
            "account": owner,
            "type": "loan_broker",
            "ledger_index": "validated",
        })
        objs = r.get("account_objects") or []
        return objs[0] if objs else None
    except Exception:
        return None


def ledger_entry_vault(rpc: RpcClient, vault_id: str) -> dict | None:
    try:
        r = rpc.public("ledger_entry", {"vault": vault_id, "ledger_index": "validated"})
        return r.get("node")
    except Exception:
        return None


def write_manifest(manifest_path: Path, lending: dict, usdc_issuer: str, currency: str) -> None:
    manifest = {
        "network_id": NETWORK_ID,
        "rpc_url": PUBLIC_RPC,
        "issued_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "asset": {"symbol": "F-USDC", "currency": currency, "issuer": usdc_issuer},
        "broker_owner": lending["broker_owner"],
        "vault_id": lending["vault_id"],
        "loan_broker_id": lending["loan_broker_id"],
        "interest_rate_tenth_bps": INTEREST_RATE,
        "payment_interval": 604800,
        "epoch_duration_seconds": 604800,
        "epochs_per_year": 52,
        "default_loan_epochs": 1,
        "payment_total": 1,
        "grace_period": 3600,
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    ok(f"Lending manifest → {manifest_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Bootstrap F-USDC vault + loan broker")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--admin-rpc", default=os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"))
    parser.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", PUBLIC_RPC))
    parser.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    parser.add_argument(
        "--stables-state",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    parser.add_argument(
        "--lending-state",
        default=os.environ.get("LENDING_STATE_FILE", "/var/lib/qxrp-stables/lending_state.json"),
    )
    parser.add_argument(
        "--manifest",
        default=os.environ.get(
            "LENDING_MANIFEST",
            str(REPO_ROOT.parent / "qXRP-faucet-wallet" / "public" / "config" / "lending.json"),
        ),
    )
    args = parser.parse_args()

    stables = load_state(Path(args.stables_state))
    lending_state_path = Path(args.lending_state)
    lending_state = load_state(lending_state_path)
    manifest_path = Path(args.manifest)

    lp = stables.get("liquidity_provider")
    usdc = stables.get("qUSDC_issuer")
    if not lp or not usdc:
        warn("Run issue-testnet-stables.py first")
        return 1

    # On-ledger IOU code is always QUC (state file may label it qUSDC).
    currency = "QUC"
    issuer_addr = usdc["address"]
    lp_addr = lp["address"]
    lp_secret = lp.get("falcon_secret") or lp.get("seed")

    rpc = RpcClient(args.admin_rpc, args.public_rpc, args.container.strip())

    try:
        net = rpc.public("server_info")
        network_id = int(net.get("info", {}).get("network_id", NETWORK_ID))
    except Exception:
        network_id = NETWORK_ID

    if not is_testnet_network(network_id):
        err(
            f"network_id={network_id}: bootstrap-testnet-lending.py is testnet-only. "
            "On mainnet create vault/broker with zero seed after --bridge-only stables.",
        )
        return 1

    if bridge_only_required(network_id):
        if float(VAULT_SEED_DEPOSIT or 0) > 0 or float(COVER_ASSET_VALUE or 0) > 0:
            err("FALCON_BRIDGE_ONLY_REQUIRED: vault/cover seed constants must stay 0")
            return 1
        ok("Bridge-only launch guard: no operator F-USDC seed deposits")

    if lending_state.get("vault_id") and lending_state.get("loan_broker_id"):
        if args.dry_run or ledger_entry_vault(rpc, lending_state["vault_id"]):
            log(f"Vault already exists: {lending_state['vault_id']}")
            write_manifest(manifest_path, lending_state, issuer_addr, currency)
            return 0

    print("═" * 60)
    print("  Falcon Ledger — Lending Bootstrap")
    print(f"  Broker owner: {lp_addr}")
    print(f"  Asset: {currency} / {issuer_addr}")

    if not account_exists(rpc, lp_addr):
        warn("Liquidity provider missing on-chain")
        return 1
    if not get_trust_line(rpc, lp_addr, currency, issuer_addr):
        warn("Liquidity provider has no F-USDC trust line")
        return 1

    vault_obj = find_vault_for_owner(rpc, lp_addr)
    vault_id = object_id(vault_obj, "VaultID", "index")

    if not vault_id:
        if not submit_tx(rpc, lp_secret, {
            "TransactionType": "VaultCreate",
            "Account": lp_addr,
            "Asset": {"currency": currency, "issuer": issuer_addr},
        }, args.dry_run, "VaultCreate"):
            return 1
        if not args.dry_run:
            time.sleep(4)
            vault_obj = find_vault_for_owner(rpc, lp_addr)
            vault_id = object_id(vault_obj, "VaultID", "index")
            if not vault_id:
                return 1
            ok(f"VaultID: {vault_id}")

    broker_obj = find_broker_for_owner(rpc, lp_addr)
    broker_id = object_id(broker_obj, "LoanBrokerID", "index")

    if not broker_id:
        if not submit_tx(rpc, lp_secret, {
            "TransactionType": "LoanBrokerSet",
            "Account": lp_addr,
            "VaultID": vault_id,
            "DebtMaximum": DEBT_MAXIMUM,
            "CoverRateMinimum": COVER_RATE_MINIMUM,
            "CoverRateLiquidation": COVER_RATE_LIQUIDATION,
            "ManagementFeeRate": MANAGEMENT_FEE_RATE,
        }, args.dry_run, "LoanBrokerSet"):
            return 1
        if not args.dry_run:
            time.sleep(4)
            broker_obj = find_broker_for_owner(rpc, lp_addr)
            broker_id = object_id(broker_obj, "LoanBrokerID", "index")
            if not broker_id:
                return 1
            ok(f"LoanBrokerID: {broker_id}")

    if not args.dry_run:
        vault_node = ledger_entry_vault(rpc, vault_id) or {}
        avail_raw = vault_node.get("AssetsAvailable", 0)
        avail = float(avail_raw.get("value", 0) if isinstance(avail_raw, dict) else avail_raw or 0)
        seed = float(VAULT_SEED_DEPOSIT or 0)
        if seed > 0 and avail < 1000:
            submit_tx(rpc, lp_secret, {
                "TransactionType": "VaultDeposit",
                "Account": lp_addr,
                "VaultID": vault_id,
                "Amount": {"currency": currency, "issuer": issuer_addr, "value": VAULT_SEED_DEPOSIT},
            }, args.dry_run, "VaultDeposit (seed)")
        elif seed <= 0 and avail < 1:
            log("vault empty — LPs supply F-USDC via portal (no operator seed)")

        broker_node = {}
        try:
            br = rpc.public("ledger_entry", {
                "loan_broker": broker_id,
                "ledger_index": "validated",
            })
            broker_node = br.get("node") or {}
        except Exception:
            pass
        cover_raw = broker_node.get("CoverAvailable", "0")
        cover_f = iou_value(cover_raw) or 0
        cover_seed = float(COVER_ASSET_VALUE or 0)
        if cover_seed > 0 and cover_f < 1000:
            submit_tx(rpc, lp_secret, {
                "TransactionType": "LoanBrokerCoverDeposit",
                "Account": lp_addr,
                "LoanBrokerID": broker_id,
                "Amount": {
                    "currency": currency,
                    "issuer": issuer_addr,
                    "value": COVER_ASSET_VALUE,
                },
            }, args.dry_run, "LoanBrokerCoverDeposit")
        elif cover_seed <= 0 and cover_f < 1:
            log("broker cover empty — deposit F-USDC when borrow goes live")

    lending_state = {
        "broker_owner": lp_addr,
        "vault_id": vault_id,
        "loan_broker_id": broker_id,
        "currency": currency,
        "issuer": issuer_addr,
    }
    if not args.dry_run:
        save_state(lending_state_path, lending_state)
    write_manifest(manifest_path, lending_state, issuer_addr, currency)
    ok("Lending bootstrap complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())