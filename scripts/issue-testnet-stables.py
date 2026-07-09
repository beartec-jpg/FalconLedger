#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
issue-testnet-stables.py
────────────────────────
Issues qUSDC (QUC) and qUSDT (QUT) on the Falcon Ledger testnet and seeds
liquidity via AMM pools (when enabled) or DEX OfferCreate fallbacks.

Steps per token:
  1. Create / fund issuer account from genesis
  2. AccountSet DefaultRipple on issuer
  3. Genesis TrustSet to issuer
  4. Issuer Payment (mint) to genesis
  5. AMMCreate or DEX sell offers at ~1:1 FALCON

Idempotent — skips steps already reflected on-chain or in state file.

Usage (coordinator):
  python3 scripts/issue-testnet-stables.py --dry-run
  python3 scripts/issue-testnet-stables.py
  python3 scripts/issue-testnet-stables.py --reset

Environment:
  ADMIN_RPC_URL   default http://127.0.0.1:5005
  PUBLIC_RPC_URL  default http://46.224.0.140:6005
  DOCKER_CONTAINER default qxrp-full (empty = direct HTTP)
  STABLES_STATE_FILE default /var/lib/qxrp-stables/stables_state.json
  STABLES_MANIFEST   default config/testnet-stables.json (repo-relative)
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

DROPS_PER_QXRP = 1_000_000
GENESIS_ACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
GENESIS_SECRET = "masterpassphrase"

QUSDC_CURRENCY = "QUC"
QUSDT_CURRENCY = "QUT"
QUSDC_SUPPLY = "10000000"
QUSDT_SUPPLY = "10000000"

AMM_XRP_DROPS = str(100_000 * DROPS_PER_QXRP)
AMM_TOKEN_AMT = "100000"
AMM_TRADING_FEE = 500  # 0.5%

DEX_OFFER_XRP_AMOUNT = str(1_000_000 * DROPS_PER_QXRP)
DEX_OFFER_TOKEN_AMOUNT = "1000000"

ISSUER_RESERVE_QXRP = 15
# Falcon liquidity account — genesis can only sign Payment on Falcon-only networks.
LIQUIDITY_FUND_QXRP = 2_100_000

REPO_ROOT = Path(__file__).resolve().parent.parent


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
    print(f"[{time.strftime('%H:%M:%S')}] \033[32m✓ {msg}\033[0m")


def warn(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] \033[33m⚠ {msg}\033[0m")


def err(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] \033[31m✗ {msg}\033[0m")


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
    """Classical genesis seed vs Falcon falcon_secret hex."""
    if secret == GENESIS_SECRET or (
        secret.startswith(("s", "S", "n", "N")) and len(secret) < 128
    ):
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


def submit_tx(
    rpc: RpcClient,
    seed: str,
    tx_json: dict,
    dry_run: bool,
    label: str,
) -> bool:
    if dry_run:
        log(f"[DRY RUN] {label}: {json.dumps(tx_json, indent=2)}")
        return True
    result, tx_hash = sign_and_submit(rpc, seed, tx_json)
    if result in ("tesSUCCESS", "terQUEUED"):
        ok(f"{label}: {result} ({tx_hash[:12]}…)")
        final = wait_validated(rpc, tx_hash)
        if final != "tesSUCCESS":
            warn(f"{label}: validated as {final}")
            return final == "tesSUCCESS"
        return True
    err(f"{label}: {result}")
    return False


def create_issuer(
    rpc: RpcClient,
    state: dict,
    token: str,
    dry_run: bool,
    state_path: Path,
) -> dict:
    key = f"{token}_issuer"
    if key in state:
        address = state[key]["address"]
        if dry_run or account_exists(rpc, address):
            log(f"{token} issuer already funded: {address}")
            return state[key]
        warn(f"{token} issuer in state but not on-chain; re-funding")

    # Falcon accounts cannot be derived from a passphrase — generate fresh keys
    # and persist the falcon_secret in the state file for idempotent re-runs.
    proposed = rpc.admin("wallet_propose", {"key_type": "falcon512"})
    if proposed.get("status") != "success":
        raise RuntimeError(f"wallet_propose failed: {proposed}")

    seed = (
        proposed.get("falcon_secret")
        or proposed.get("master_seed")
        or proposed.get("master_key")
    )
    if not seed:
        raise RuntimeError(f"wallet_propose returned no signing secret: {proposed}")
    address = proposed["account_id"]
    log(f"{token} issuer: {address}")

    fund_drops = str(ISSUER_RESERVE_QXRP * DROPS_PER_QXRP)
    submit_tx(rpc, GENESIS_SECRET, {
        "TransactionType": "Payment",
        "Account": GENESIS_ACCOUNT,
        "Destination": address,
        "Amount": fund_drops,
    }, dry_run, f"Fund {token} issuer")

    if not dry_run:
        for _ in range(20):
            time.sleep(3)
            if account_exists(rpc, address):
                break
        else:
            raise RuntimeError(f"{token} issuer account never activated")

    issuer_info = {
        "seed": seed,
        "falcon_secret": seed,
        "address": address,
        "currency": token,
    }
    state[key] = issuer_info
    if not dry_run:
        save_state(state_path, state)
    return issuer_info


def create_liquidity_provider(
    rpc: RpcClient,
    state: dict,
    dry_run: bool,
    state_path: Path,
) -> dict:
    """Falcon account that holds IOUs and posts DEX/AMM liquidity."""
    key = "liquidity_provider"
    if key in state:
        address = state[key]["address"]
        if dry_run or account_exists(rpc, address):
            log(f"Liquidity provider ready: {address}")
            return state[key]
        warn("Liquidity provider in state but missing on-chain — re-funding")

    proposed = rpc.admin("wallet_propose", {"key_type": "falcon512"})
    if proposed.get("status") != "success":
        raise RuntimeError(f"wallet_propose failed: {proposed}")
    secret = proposed.get("falcon_secret") or proposed.get("master_seed")
    address = proposed["account_id"]
    log(f"Liquidity provider: {address}")

    fund_drops = str(LIQUIDITY_FUND_QXRP * DROPS_PER_QXRP)
    submit_tx(rpc, GENESIS_SECRET, {
        "TransactionType": "Payment",
        "Account": GENESIS_ACCOUNT,
        "Destination": address,
        "Amount": fund_drops,
    }, dry_run, "Fund liquidity provider")

    if not dry_run:
        for _ in range(20):
            time.sleep(3)
            if account_exists(rpc, address):
                break
        else:
            raise RuntimeError("Liquidity provider never activated")

    info = {"seed": secret, "falcon_secret": secret, "address": address}
    state[key] = info
    if not dry_run:
        save_state(state_path, state)
    return info


def set_default_ripple(rpc: RpcClient, issuer: dict, token: str, dry_run: bool) -> None:
    submit_tx(rpc, issuer["seed"], {
        "TransactionType": "AccountSet",
        "Account": issuer["address"],
        "SetFlag": 8,
    }, dry_run, f"Set DefaultRipple on {token} issuer")


def set_trust_line(
    rpc: RpcClient,
    lp: dict,
    currency: str,
    issuer_address: str,
    limit: str,
    token: str,
    dry_run: bool,
) -> None:
    existing = get_trust_line(rpc, lp["address"], currency, issuer_address)
    if existing:
        log(f"Trust line {token} already set (limit: {existing.get('limit')})")
        return
    submit_tx(rpc, lp["seed"], {
        "TransactionType": "TrustSet",
        "Account": lp["address"],
        "LimitAmount": {"currency": currency, "issuer": issuer_address, "value": limit},
    }, dry_run, f"TrustSet liquidity → {token} issuer")


def issue_tokens(
    rpc: RpcClient,
    issuer: dict,
    lp: dict,
    currency: str,
    supply: str,
    token: str,
    dry_run: bool,
) -> None:
    existing = get_trust_line(rpc, lp["address"], currency, issuer["address"])
    if existing and float(existing.get("balance", "0")) >= float(supply) * 0.99:
        log(f"{token} already fully issued to liquidity provider")
        return
    submit_tx(rpc, issuer["seed"], {
        "TransactionType": "Payment",
        "Account": issuer["address"],
        "Destination": lp["address"],
        "Amount": {"currency": currency, "issuer": issuer["address"], "value": supply},
    }, dry_run, f"Issue {supply} {token} to liquidity provider")


def dex_offer_exists(rpc: RpcClient, lp_address: str, currency: str, issuer_address: str) -> bool:
    try:
        r = rpc.public("account_offers", {"account": lp_address, "ledger_index": "validated"})
        for offer in r.get("offers", []):
            taker_gets = offer.get("taker_gets", {})
            if isinstance(taker_gets, dict):
                if taker_gets.get("currency") == currency and taker_gets.get("issuer") == issuer_address:
                    return True
    except Exception:
        pass
    return False


def create_dex_offers(
    rpc: RpcClient,
    lp: dict,
    currency: str,
    issuer_address: str,
    token: str,
    dry_run: bool,
) -> None:
    if dex_offer_exists(rpc, lp["address"], currency, issuer_address):
        log(f"DEX sell offer for {token} already exists")
        return
    submit_tx(rpc, lp["seed"], {
        "TransactionType": "OfferCreate",
        "Account": lp["address"],
        "TakerGets": {"currency": currency, "issuer": issuer_address, "value": DEX_OFFER_TOKEN_AMOUNT},
        "TakerPays": DEX_OFFER_XRP_AMOUNT,
    }, dry_run, f"OfferCreate: sell {DEX_OFFER_TOKEN_AMOUNT} {token} @ 1 FALCON each")


def amm_exists(rpc: RpcClient, currency: str, issuer_address: str) -> bool:
    try:
        r = rpc.public("amm_info", {
            "asset": {"currency": "XRP"},
            "asset2": {"currency": currency, "issuer": issuer_address},
            "ledger_index": "validated",
        })
        return "amm" in r
    except Exception:
        return False


def create_amm(
    rpc: RpcClient,
    lp: dict,
    currency: str,
    issuer_address: str,
    token: str,
    dry_run: bool,
) -> str:
    if amm_exists(rpc, currency, issuer_address):
        log(f"AMM pool FALCON/{token} already exists")
        return "amm"

    if dry_run:
        warn(f"AMM not verified in dry-run — would try AMMCreate then DEX for {token}")
        create_dex_offers(rpc, lp, currency, issuer_address, token, dry_run)
        return "dex"

    result, tx_hash = sign_and_submit(rpc, lp["seed"], {
        "TransactionType": "AMMCreate",
        "Account": lp["address"],
        "Amount": AMM_XRP_DROPS,
        "Amount2": {"currency": currency, "issuer": issuer_address, "value": AMM_TOKEN_AMT},
        "TradingFee": AMM_TRADING_FEE,
    })

    if result == "temDISABLED":
        warn(f"AMM not enabled ({result}) — creating DEX liquidity for {token}")
        create_dex_offers(rpc, lp, currency, issuer_address, token, dry_run)
        return "dex"
    if result in ("tesSUCCESS", "terQUEUED"):
        ok(f"AMMCreate FALCON/{token}: {result} ({tx_hash[:12]}…)")
        return "amm"
    err(f"AMMCreate FALCON/{token}: {result}")
    create_dex_offers(rpc, lp, currency, issuer_address, token, dry_run)
    return "dex"


def write_manifest(
    manifest_path: Path,
    state: dict,
    liquidity: dict[str, str],
    network_id: int,
    rpc_url: str,
    *,
    usdc_only: bool = False,
) -> None:
    token_defs = [("qUSDC", QUSDC_CURRENCY)]
    if not usdc_only:
        token_defs.append(("qUSDT", QUSDT_CURRENCY))

    tokens = []
    for symbol, currency in token_defs:
        issuer = state.get(f"{symbol}_issuer", {}).get("address", "")
        tokens.append({
            "symbol": symbol,
            "currency": currency,
            "issuer": issuer,
            "liquidity": liquidity.get(symbol, "none"),
        })

    lp = state.get("liquidity_provider", {})

    env = {
        "NEXT_PUBLIC_TESTNET_QUSDC_CURRENCY": QUSDC_CURRENCY,
        "NEXT_PUBLIC_TESTNET_QUSDC_ISSUER": tokens[0]["issuer"],
        "NEXT_PUBLIC_TESTNET_USDC_CURRENCY": QUSDC_CURRENCY,
        "NEXT_PUBLIC_TESTNET_USDC_ISSUER": tokens[0]["issuer"],
    }
    if not usdc_only and len(tokens) > 1:
        env["NEXT_PUBLIC_TESTNET_QUSDT_CURRENCY"] = QUSDT_CURRENCY
        env["NEXT_PUBLIC_TESTNET_QUSDT_ISSUER"] = tokens[1]["issuer"]

    manifest = {
        "network_id": network_id,
        "rpc_url": rpc_url,
        "issued_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "genesis": GENESIS_ACCOUNT,
        "bridge_only": all(v == "bridge" for v in liquidity.values()),
        "liquidity_provider": lp.get("address", ""),
        "tokens": tokens,
        "env": env,
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    ok(f"Manifest written to {manifest_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Issue qUSDC and qUSDT on Falcon Ledger testnet")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument(
        "--bridge-only",
        action="store_true",
        help="Create issuer accounts only — no bootstrap mint/AMM/DEX (F-USDC from Sepolia bridge)",
    )
    parser.add_argument(
        "--usdc-only",
        action="store_true",
        help="Issue qUSDC (QUC) only — skip qUSDT",
    )
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--admin-rpc", default=os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"))
    parser.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://46.224.0.140:6005"))
    parser.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    parser.add_argument(
        "--state-file",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    parser.add_argument(
        "--manifest",
        default=os.environ.get("STABLES_MANIFEST", str(REPO_ROOT / "config" / "testnet-stables.json")),
    )
    args = parser.parse_args()

    state_path = Path(args.state_file)
    manifest_path = Path(args.manifest)
    container = args.container.strip()

    if args.reset and not args.dry_run and state_path.is_file():
        state_path.unlink()
        warn("State file removed — starting fresh.")

    rpc = RpcClient(args.admin_rpc, args.public_rpc, container if container else "")
    state = load_state(state_path)
    liquidity: dict[str, str] = {}

    print()
    print("═" * 60)
    print("  Falcon Ledger — Stablecoin Issuance")
    print("═" * 60)
    print(f"  Genesis:    {GENESIS_ACCOUNT}")
    print(f"  Admin RPC:  {args.admin_rpc}" + (f" (via {container})" if container else ""))
    print(f"  Public RPC: {args.public_rpc}")
    print(f"  qUSDC:      {QUSDC_CURRENCY}  supply={QUSDC_SUPPLY}")
    if not args.usdc_only:
        print(f"  qUSDT:      {QUSDT_CURRENCY}  supply={QUSDT_SUPPLY}")
    else:
        print("  qUSDT:      (skipped — --usdc-only)")
    if args.dry_run:
        print("  *** DRY RUN — no transactions submitted ***")
    print()

    try:
        net = rpc.public("server_info")
        network_id = int(net.get("info", {}).get("network_id", 1001))
    except Exception:
        network_id = 1001

    tokens = [("qUSDC", QUSDC_CURRENCY, QUSDC_SUPPLY)]
    if not args.usdc_only:
        tokens.append(("qUSDT", QUSDT_CURRENCY, QUSDT_SUPPLY))

    lp: dict | None = None
    if not args.bridge_only:
        print("── Liquidity provider ───────────────────────────────────────")
        lp = create_liquidity_provider(rpc, state, args.dry_run, state_path)
        print()
    else:
        warn("Bridge-only mode — skipping LP funding, bootstrap mint, and AMM/DEX seed")

    for token_name, currency, supply in tokens:
        print(f"── {token_name} ──────────────────────────────────────────────")
        issuer = create_issuer(rpc, state, token_name, args.dry_run, state_path)
        set_default_ripple(rpc, issuer, token_name, args.dry_run)
        if args.bridge_only:
            liquidity[token_name] = "bridge"
        else:
            assert lp is not None
            set_trust_line(rpc, lp, currency, issuer["address"], supply, token_name, args.dry_run)
            issue_tokens(rpc, issuer, lp, currency, supply, token_name, args.dry_run)
            liquidity[token_name] = create_amm(rpc, lp, currency, issuer["address"], token_name, args.dry_run)
        print()

    if not args.dry_run:
        save_state(state_path, state)

    write_manifest(
        manifest_path, state, liquidity, network_id, args.public_rpc, usdc_only=args.usdc_only
    )

    print("═" * 60)
    print("  Summary")
    print("═" * 60)
    for token_name, currency, _ in tokens:
        addr = state.get(f"{token_name}_issuer", {}).get("address", "(dry-run)")
        liq = liquidity.get(token_name, "?")
        print(f"  {token_name} ({currency}) issuer: {addr}  liquidity={liq}")
    print()
    print("  Wallet .env / Vercel:")
    manifest = json.loads(manifest_path.read_text())
    for k, v in manifest.get("env", {}).items():
        print(f"  {k}={v}")
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())