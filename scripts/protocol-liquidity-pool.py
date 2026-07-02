#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
protocol-liquidity-pool.py
──────────────────────────
Protocol-operated AMM liquidity manager for Falcon Ledger testnet.

Funds pools from the genesis circulating allocation (4B FALCON budget) via a
dedicated Falcon LP operator account. This is the operational layer for
"protocol-level" liquidity until an on-chain treasury→AMM amendment exists.

Budget model (configurable):
  - GENESIS only signs Payment — genesis → LP operator top-ups
  - LP operator holds FALCON + IOUs and runs AMMCreate / AMMDeposit

Usage:
  python3 scripts/protocol-liquidity-pool.py --status
  python3 scripts/protocol-liquidity-pool.py --seed-amm
  python3 scripts/protocol-liquidity-pool.py --top-up-operator 50000000

Environment:
  ADMIN_RPC_URL, PUBLIC_RPC_URL, DOCKER_CONTAINER
  STABLES_MANIFEST  — config/testnet-stables.json
  PROTOCOL_LP_STATE — /var/lib/qxrp-stables/protocol_lp_state.json
  GENESIS_LIQUIDITY_BUDGET_QXRP — max FALCON spend from genesis (default 500M)
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

REPO_ROOT = Path(__file__).resolve().parent.parent
GENESIS_ACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
GENESIS_SECRET = "masterpassphrase"
DROPS_PER_QXRP = 1_000_000

DEFAULT_BUDGET_QXRP = 500_000_000  # 500M of 4B genesis carve-out for LP
OPERATOR_RESERVE_QXRP = 50_000_000   # initial operator FALCON float
AMM_TRADING_FEE = 500


class Rpc:
    def __init__(self, admin: str, public: str, container: str = ""):
        self.admin = admin
        self.public = public
        self.container = container

    def _post(self, url: str, method: str, params: dict | list | None = None) -> dict:
        body = {"method": method, "params": [params] if params is not None else [{}]}
        payload = json.dumps(body).encode()
        if self.container:
            cmd = [
                "docker", "exec", self.container, "curl", "-sf", "-X", "POST", url,
                "-H", "Content-Type: application/json", "-d", payload.decode(),
            ]
            out = subprocess.check_output(cmd, text=True)
            parsed = json.loads(out)
        else:
            req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
            with urllib.request.urlopen(req, timeout=30) as resp:
                parsed = json.loads(resp.read())
        return parsed.get("result", parsed)

    def sign_submit(self, secret: str, tx: dict) -> tuple[str, str]:
        if secret == GENESIS_SECRET or (secret.startswith(("s", "S")) and len(secret) < 128):
            sign_params = {"secret": secret, "tx_json": tx}
        else:
            sign_params = {"falcon_secret": secret, "tx_json": tx}
        signed = self._post(self.admin, "sign", sign_params)
        if signed.get("status") != "success":
            return f"SIGN_ERR: {signed}", ""
        blob = signed["tx_blob"]
        h = signed.get("tx_json", {}).get("hash", "")
        sub = self._post(self.public, "submit", {"tx_blob": blob})
        return sub.get("engine_result", "?"), h


def load_json(path: Path, default: dict) -> dict:
    try:
        return json.loads(path.read_text())
    except FileNotFoundError:
        return default


def save_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n")


def amm_enabled(rpc: Rpc) -> bool:
    try:
        r = rpc._post(rpc.public, "feature", {"feature": "AMM"})
        for v in r.values():
            if isinstance(v, dict) and v.get("name") == "AMM":
                return bool(v.get("enabled"))
    except Exception:
        pass
    return False


def ensure_operator(rpc: Rpc, state: dict, state_path: Path, top_up: int, dry_run: bool) -> dict:
    if "operator" in state:
        return state["operator"]
    wp = rpc._post(rpc.admin, "wallet_propose", {"key_type": "falcon512"})
    op = {
        "address": wp["account_id"],
        "falcon_secret": wp["falcon_secret"],
        "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    if dry_run:
        print(f"[dry-run] would create operator {op['address']}")
        return op
    drops = str(top_up * DROPS_PER_QXRP)
    eng, _ = rpc.sign_submit(GENESIS_SECRET, {
        "TransactionType": "Payment",
        "Account": GENESIS_ACCOUNT,
        "Destination": op["address"],
        "Amount": drops,
    })
    print(f"Fund operator {op['address']}: {eng}")
    state["operator"] = op
    state["genesis_spent_qxrp"] = state.get("genesis_spent_qxrp", 0) + top_up
    save_json(state_path, state)
    time.sleep(5)
    return op


def seed_amm_for_token(
    rpc: Rpc,
    operator: dict,
    currency: str,
    issuer: str,
    symbol: str,
    xrp_amt: int,
    token_amt: str,
    dry_run: bool,
) -> None:
    try:
        info = rpc._post(rpc.public, "amm_info", {
            "asset": {"currency": "XRP"},
            "asset2": {"currency": currency, "issuer": issuer},
            "ledger_index": "validated",
        })
        if info.get("amm"):
            print(f"  {symbol}: AMM pool already exists")
            return
    except Exception:
        pass

    if dry_run:
        print(f"  [dry-run] AMMCreate {symbol}: {xrp_amt} FALCON + {token_amt} {symbol}")
        return

    eng, h = rpc.sign_submit(operator["falcon_secret"], {
        "TransactionType": "AMMCreate",
        "Account": operator["address"],
        "Amount": str(xrp_amt * DROPS_PER_QXRP),
        "Amount2": {"currency": currency, "issuer": issuer, "value": token_amt},
        "TradingFee": AMM_TRADING_FEE,
    })
    print(f"  {symbol}: AMMCreate {eng} ({h[:12]}…)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Protocol LP manager (genesis budget → AMM)")
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--seed-amm", action="store_true", help="Create AMM pools from manifest issuers")
    parser.add_argument("--top-up-operator", type=int, default=0, metavar="QXRP")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--manifest", default=os.environ.get(
        "STABLES_MANIFEST", str(REPO_ROOT / "config" / "testnet-stables.json")))
    parser.add_argument("--state", default=os.environ.get(
        "PROTOCOL_LP_STATE", "/var/lib/qxrp-stables/protocol_lp_state.json"))
    args = parser.parse_args()

    rpc = Rpc(
        os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"),
        os.environ.get("PUBLIC_RPC_URL", "http://46.224.0.140:6005"),
        os.environ.get("DOCKER_CONTAINER", "qxrp-full"),
    )
    state_path = Path(args.state)
    state = load_json(state_path, {"genesis_spent_qxrp": 0, "budget_cap_qxrp": DEFAULT_BUDGET_QXRP})
    manifest = load_json(Path(args.manifest), {})

    if args.status:
        print("AMM enabled:", amm_enabled(rpc))
        print("Genesis budget cap:", state.get("budget_cap_qxrp", DEFAULT_BUDGET_QXRP), "FALCON")
        print("Genesis spent (LP):", state.get("genesis_spent_qxrp", 0), "FALCON")
        if op := state.get("operator"):
            print("LP operator:", op.get("address"))
        for tok in manifest.get("tokens", []):
            print(f"  {tok.get('symbol')}: issuer={tok.get('issuer')} liquidity={tok.get('liquidity')}")
        return 0

    if not amm_enabled(rpc):
        print("ERROR: AMM amendment not enabled — run scripts/enable-amm-fleet.sh first")
        return 1

    budget = int(state.get("budget_cap_qxrp", DEFAULT_BUDGET_QXRP))
    spent = int(state.get("genesis_spent_qxrp", 0))

    if args.top_up_operator > 0:
        if spent + args.top_up_operator > budget:
            print(f"ERROR: would exceed genesis LP budget ({spent}+{args.top_up_operator} > {budget})")
            return 1
        ensure_operator(rpc, state, state_path, args.top_up_operator, args.dry_run)

    if args.seed_amm:
        operator = ensure_operator(rpc, state, state_path, OPERATOR_RESERVE_QXRP, args.dry_run)
        if spent + OPERATOR_RESERVE_QXRP > budget and not args.dry_run:
            print("ERROR: operator fund exceeds budget")
            return 1
        print("Seeding protocol AMM pools (100k FALCON + 100k tokens each @ 0.5% fee)...")
        for tok in manifest.get("tokens", []):
            if not tok.get("issuer"):
                continue
            seed_amm_for_token(
                rpc, operator,
                tok["currency"], tok["issuer"], tok["symbol"],
                xrp_amt=100_000, token_amt="100000",
                dry_run=args.dry_run,
            )

    if not any([args.seed_amm, args.top_up_operator]):
        parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())