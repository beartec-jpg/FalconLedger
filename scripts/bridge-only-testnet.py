#!/usr/bin/env python3
"""Drain bootstrap LP liquidity — testnet F-USDC comes from Sepolia bridge only."""

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
DROPS_PER_QXRP = 1_000_000
QUSDC_CURRENCY = "QUC"

TF_LP_TOKEN = 0x00010000


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


def account_seq_ledger(rpc: RpcClient, address: str) -> tuple[int, int]:
    acct = rpc.public("account_info", {"account": address, "ledger_index": "validated"})
    seq = acct["account_data"]["Sequence"]
    ledger = rpc.public("server_info", {})["info"]["validated_ledger"]["seq"]
    return seq, ledger


def submit_signed(rpc: RpcClient, secret: str, tx: dict, label: str, dry_run: bool) -> tuple[bool, int]:
    if dry_run:
        print(f"[dry-run] {label}: {tx['TransactionType']}")
        return True, tx["Sequence"] + 1

    signed = rpc.admin("sign", sign_params(secret, tx))
    if signed.get("status") != "success":
        print(f"sign failed ({label}):", signed, file=sys.stderr)
        return False, tx["Sequence"]

    sub = rpc.public("submit", {"tx_blob": signed["tx_blob"]})
    result = sub.get("engine_result", "?")
    print(f"{label}: {result} {sub.get('engine_result_message', '')}")
    if result not in ("tesSUCCESS", "terQUEUED"):
        return False, tx["Sequence"]
    time.sleep(4)
    return True, tx["Sequence"] + 1


def cancel_offers(rpc: RpcClient, secret: str, address: str, dry_run: bool) -> int:
    offers = rpc.public("account_offers", {"account": address, "ledger_index": "validated"})
    seq_offers = offers.get("offers", [])
    if not seq_offers:
        print(f"No open DEX offers on {address}")
        return 0

    seq, ledger = account_seq_ledger(rpc, address)
    print(f"Cancelling {len(seq_offers)} DEX offer(s) from {address}")
    for offer in seq_offers:
        tx = {
            "TransactionType": "OfferCancel",
            "Account": address,
            "OfferSequence": offer["seq"],
            "Fee": "12",
            "Sequence": seq,
            "LastLedgerSequence": ledger + 30,
        }
        ok, seq = submit_signed(rpc, secret, tx, f"cancel offer #{offer['seq']}", dry_run)
        if not ok:
            return 1
    return 0


def withdraw_amm(
    rpc: RpcClient,
    secret: str,
    lp_address: str,
    currency: str,
    issuer: str,
    dry_run: bool,
) -> int:
    try:
        amm_r = rpc.public("amm_info", {
            "asset": {"currency": "XRP"},
            "asset2": {"currency": currency, "issuer": issuer},
            "ledger_index": "validated",
        })
    except Exception:
        print("No AMM pool found — skip withdraw")
        return 0

    amm = amm_r.get("amm")
    if not amm:
        print("No AMM pool found — skip withdraw")
        return 0

    amm_account = str(amm.get("account", ""))
    lp_meta = amm.get("lp_token") or {}
    lp_currency = str(lp_meta.get("currency", ""))
    if not amm_account or not lp_currency:
        print("AMM pool missing LP token metadata — skip withdraw")
        return 0

    lines = rpc.public("account_lines", {"account": lp_address, "ledger_index": "validated"})
    lp_line = next(
        (l for l in lines.get("lines", [])
         if l.get("account") == amm_account and l.get("currency") == lp_currency),
        None,
    )
    if not lp_line or float(lp_line.get("balance", "0")) <= 0:
        print(f"No LP tokens on {lp_address} — skip AMM withdraw")
        return 0

    lp_balance = lp_line["balance"]
    pool_xrp = int(amm.get("amount", "0")) / DROPS_PER_QXRP
    pool_usdc = float((amm.get("amount2") or {}).get("value", "0"))
    print(
        f"AMM pool before withdraw: {pool_xrp:,.0f} FALCON + {pool_usdc:,.2f} {currency} "
        f"(LP tokens: {lp_balance})",
    )

    seq, ledger = account_seq_ledger(rpc, lp_address)
    tx = {
        "TransactionType": "AMMWithdraw",
        "Account": lp_address,
        "Asset": {"currency": "XRP"},
        "Asset2": {"currency": currency, "issuer": issuer},
        "LPTokenIn": {
            "currency": lp_currency,
            "issuer": amm_account,
            "value": lp_balance,
        },
        "Flags": TF_LP_TOKEN,
        "Fee": "12",
        "Sequence": seq,
        "LastLedgerSequence": ledger + 30,
    }
    ok, _ = submit_signed(rpc, secret, tx, "AMM withdraw all", dry_run)
    return 0 if ok else 1


def return_quc_to_issuer(
    rpc: RpcClient,
    secret: str,
    lp_address: str,
    issuer_address: str,
    currency: str,
    dry_run: bool,
) -> int:
    lines = rpc.public("account_lines", {"account": lp_address, "ledger_index": "validated"})
    quc_line = next(
        (l for l in lines.get("lines", [])
         if l.get("currency") == currency and l.get("account") == issuer_address),
        None,
    )
    if not quc_line:
        print(f"No {currency} balance on {lp_address}")
        return 0

    amount_str = str(quc_line.get("balance", "0")).strip()
    if not amount_str or float(amount_str) <= 0:
        print(f"No {currency} to return from {lp_address}")
        return 0
    seq, ledger = account_seq_ledger(rpc, lp_address)
    tx = {
        "TransactionType": "Payment",
        "Account": lp_address,
        "Destination": issuer_address,
        "Amount": {"currency": currency, "issuer": issuer_address, "value": amount_str},
        "Fee": "12",
        "Sequence": seq,
        "LastLedgerSequence": ledger + 30,
    }
    ok, _ = submit_signed(
        rpc, secret, tx, f"return {amount_str} {currency} to issuer", dry_run,
    )
    return 0 if ok else 1


def update_manifest(manifest_path: Path, dry_run: bool) -> None:
    if not manifest_path.is_file():
        return
    manifest = json.loads(manifest_path.read_text())
    for tok in manifest.get("tokens", []):
        if tok.get("currency") == QUSDC_CURRENCY:
            tok["liquidity"] = "bridge"
    manifest["bridge_only"] = True
    manifest["bootstrap_liquidity_drained_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    if dry_run:
        print(f"[dry-run] would update manifest {manifest_path}")
        return
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Updated manifest → liquidity=bridge ({manifest_path})")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Drain bootstrap LP AMM/DEX — F-USDC supply from Sepolia bridge only",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    state_path = Path(os.environ.get(
        "STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json",
    ))
    manifest_path = Path(os.environ.get(
        "STABLES_MANIFEST", str(REPO_ROOT / "config" / "testnet-stables.json"),
    ))

    state = json.loads(state_path.read_text())
    lp = state.get("liquidity_provider")
    issuer = state.get("qUSDC_issuer") or state.get("USDC_issuer")
    if not lp:
        print("liquidity_provider missing from stables state", file=sys.stderr)
        return 1
    if not issuer:
        print("qUSDC_issuer missing from stables state", file=sys.stderr)
        return 1

    rpc = RpcClient(
        os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"),
        os.environ.get("PUBLIC_RPC_URL", "http://46.224.0.140:6005"),
        os.environ.get("DOCKER_CONTAINER", "qxrp-full"),
    )

    secret = lp.get("falcon_secret") or lp.get("seed")
    lp_address = lp["address"]
    issuer_address = issuer["address"]

    print(f"Bridge-only cleanup for LP {lp_address}")
    print(f"QUC issuer: {issuer_address}")
    if args.dry_run:
        print("*** DRY RUN — no transactions submitted ***")

    steps = [
        ("cancel DEX offers", lambda: cancel_offers(rpc, secret, lp_address, args.dry_run)),
        ("AMM withdraw", lambda: withdraw_amm(
            rpc, secret, lp_address, QUSDC_CURRENCY, issuer_address, args.dry_run,
        )),
        ("return QUC to issuer", lambda: return_quc_to_issuer(
            rpc, secret, lp_address, issuer_address, QUSDC_CURRENCY, args.dry_run,
        )),
    ]
    for name, fn in steps:
        print(f"\n── {name} ──")
        if fn() != 0:
            print(f"Failed at: {name}", file=sys.stderr)
            return 1

    update_manifest(manifest_path, args.dry_run)
    print("\nDone. F-USDC on Falcon should now be bridge-minted only; users post DEX/AMM liquidity.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())