#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
lend-hf-monitor.py — broker LoanManage enforcement daemon (health factor + payment default).

Polls on-chain loans, computes HF from AMM F-USDC/FALCON price, and submits:
  • tfLoanImpair   when HF < 1.1 (or < 1.0)
  • tfLoanUnimpair when HF recovers >= 1.1
  • tfLoanDefault  when payment is past due + grace (protocol requirement)

Usage (coordinator):
  python3 scripts/lend-hf-monitor.py --dry-run --once
  python3 scripts/lend-hf-monitor.py --loop --interval 60

Environment:
  PUBLIC_RPC_URL              default http://127.0.0.1:6005
  ADMIN_RPC_URL               default http://127.0.0.1:5005
  DOCKER_CONTAINER            default qxrp-full
  TESTNET_LENDING_BROKER_SECRET  broker owner falcon_secret (or from stables state)
  STABLES_STATE_FILE          default /var/lib/qxrp-stables/stables_state.json
  LENDING_MANIFEST            path to lending.json
  LEND_HF_MONITOR_STATE       default /var/lib/qxrp-lending/hf_monitor_state.json
  LEND_PORTAL_URL             optional — use portal /api/lend/loan-manage instead of local sign
  LEND_HF_MONITOR_TOKEN       bearer token for portal loan-manage API
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

REPO_ROOT = Path(os.environ.get("QXRP_REPO_ROOT", Path(__file__).resolve().parent.parent))

DROPS = 1_000_000
RIPPLE_EPOCH = 946684800
LSF_LOAN_DEFAULT = 0x00010000
LSF_LOAN_IMPAIRED = 0x00020000
TF_LOAN_DEFAULT = 0x00010000
TF_LOAN_IMPAIR = 0x00020000
TF_LOAN_UNIMPAIR = 0x00040000
HF_IMPAIR_THRESHOLD = 1.1
HF_LIQUIDATABLE = 1.0


# ── Minimal RPC helpers (no import of bootstrap / launch_guards) ─────────────


class RpcClient:
    def __init__(self, admin_url: str, public_url: str, container: str = ""):
        self.admin_url = admin_url
        self.public_url = public_url
        self.container = container

    def _post(self, url: str, method: str, params: dict | None = None) -> dict:
        payload = json.dumps({"method": method, "params": [params or {}]}).encode()
        if self.container:
            cmd = [
                "docker",
                "exec",
                self.container,
                "curl",
                "-sf",
                "-X",
                "POST",
                url,
                "-H",
                "Content-Type: application/json",
                "-d",
                payload.decode(),
            ]
            body = json.loads(subprocess.check_output(cmd, text=True))
        else:
            req = urllib.request.Request(
                url, data=payload, headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=60) as resp:
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
    print(f"[{time.strftime('%H:%M:%S')}] OK {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] WARN {msg}", flush=True)


def err(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] ERR {msg}", flush=True)


def iou_value(raw: object) -> float:
    if raw is None:
        return 0.0
    if isinstance(raw, (int, float)):
        return float(raw)
    if isinstance(raw, str):
        try:
            return float(raw)
        except ValueError:
            return 0.0
    if isinstance(raw, dict) and "value" in raw:
        return float(raw["value"])
    return 0.0


def sign_and_submit(rpc: RpcClient, secret: str, tx_json: dict) -> tuple[str, str]:
    params = (
        {"falcon_secret": secret, "tx_json": tx_json}
        if not (secret.startswith(("s", "S")) and len(secret) < 128)
        else {"secret": secret, "tx_json": tx_json}
    )
    signed = rpc.admin("sign", params)
    if signed.get("status") != "success":
        return f"SIGN_ERR: {signed.get('error_message', signed)}", ""
    blob = signed["tx_blob"]
    tx_hash = signed.get("tx_json", {}).get("hash", "")
    sub = rpc.public("submit", {"tx_blob": blob})
    return sub.get("engine_result", "?"), tx_hash


def submit_tx(rpc: RpcClient, secret: str, tx_json: dict, dry_run: bool, label: str) -> bool:
    if dry_run:
        log(f"[DRY RUN] {label}")
        return True
    result, tx_hash = sign_and_submit(rpc, secret, tx_json)
    if result in ("tesSUCCESS", "terQUEUED"):
        ok(f"{label}: {result} {tx_hash}")
        return True
    warn(f"{label}: {result} {tx_hash}")
    return False


def ripple_now() -> int:
    return int(time.time()) - RIPPLE_EPOCH


def load_json(path: Path) -> dict:
    if not path.is_file():
        return {}
    return json.loads(path.read_text())


def save_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2))


def broker_secret(stables_path: Path) -> str:
    env = os.environ.get("TESTNET_LENDING_BROKER_SECRET", "").strip()
    if env:
        return env
    stables = load_json(stables_path)
    lp = stables.get("liquidity_provider") or {}
    return str(lp.get("falcon_secret") or lp.get("seed") or "").strip()


def collateral_falcon(obj: dict) -> float:
    raw = obj.get("Collateral")
    if raw is None:
        return 0.0
    try:
        drops = int(raw)
        return drops / DROPS if drops > 0 else 0.0
    except (TypeError, ValueError):
        return 0.0


def loan_outstanding(obj: dict) -> float:
    total = iou_value(obj.get("TotalValueOutstanding"))
    if total and total > 0:
        return total
    principal = iou_value(obj.get("PrincipalOutstanding"))
    if principal and principal > 0:
        return principal
    remaining = int(obj.get("PaymentRemaining") or 0)
    installment = iou_value(obj.get("PeriodicPayment")) or 0
    if remaining > 0 and installment > 0:
        return installment * remaining
    return 0.0


def is_active_loan(obj: dict) -> bool:
    if obj.get("LedgerEntryType") != "Loan":
        return False
    remaining = int(obj.get("PaymentRemaining") or 0)
    if remaining > 0:
        return True
    return loan_outstanding(obj) > 0


def loan_flags(obj: dict) -> tuple[bool, bool]:
    flags = int(obj.get("Flags") or 0)
    return (flags & LSF_LOAN_IMPAIRED) != 0, (flags & LSF_LOAN_DEFAULT) != 0


def payment_default_eligible(obj: dict, now: int | None = None) -> bool:
    due = int(obj.get("NextPaymentDueDate") or 0)
    grace = int(obj.get("GracePeriod") or 0)
    if due <= 0:
        return False
    now = now if now is not None else ripple_now()
    return now >= due + grace


def health_factor(collateral: float, debt: float, falcon_per_fusdc: float | None) -> float | None:
    if debt <= 0 or collateral <= 0 or not falcon_per_fusdc or falcon_per_fusdc <= 0:
        return None
    return (collateral * falcon_per_fusdc) / debt


def recommend_action(
    hf: float | None,
    impaired: bool,
    defaulted: bool,
    pay_default: bool,
) -> str:
    """Prefer full default on liquidatable HF (permissionless) so books clear."""
    if defaulted:
        return "none"
    # Payment past grace → default (protocol allows anyone on permissionless loans).
    if pay_default:
        return "default"
    if hf is None:
        return "monitor"
    # HF below liquidation threshold (1.1): default immediately — do not leave
    # loans stuck in "impaired but still open" where repay returns tecEXPIRED.
    if hf < HF_IMPAIR_THRESHOLD:
        return "default"
    if impaired and hf >= HF_IMPAIR_THRESHOLD:
        return "unimpair"
    return "none"


def list_loans(rpc: RpcClient) -> list[dict]:
    loans: list[dict] = []
    marker = None
    for _ in range(24):
        params: dict = {"type": "loan", "ledger_index": "validated", "limit": 200}
        if marker:
            params["marker"] = marker
        r = rpc.public("ledger_data", params)
        for obj in r.get("state") or []:
            if not is_active_loan(obj):
                continue
            if loan_outstanding(obj) <= 0:
                continue
            loans.append(obj)
        marker = r.get("marker")
        if not marker:
            break
    return loans


def amm_falcon_per_fusdc(rpc: RpcClient, currency: str, issuer: str) -> float | None:
    try:
        r = rpc.public("amm_info", {
            "asset": {"currency": "XRP"},
            "asset2": {"currency": currency, "issuer": issuer},
            "ledger_index": "validated",
        })
        amm = r.get("amm")
        if not amm:
            return None
        xrp_drops = int(amm.get("amount") or 0)
        amount2 = amm.get("amount2") or {}
        tok = float(amount2.get("value") or 0)
        if tok <= 0:
            return None
        return tok / (xrp_drops / DROPS)
    except Exception:
        return None


def portal_loan_manage(portal_url: str, token: str, loan_id: str, action: str) -> tuple[bool, str]:
    url = portal_url.rstrip("/") + "/api/lend/loan-manage?network=testnet"
    body = json.dumps({"loanId": loan_id, "action": action}).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {token}",
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        data = json.loads(resp.read())
    if data.get("success"):
        return True, data.get("result", "tesSUCCESS")
    return False, data.get("result") or data.get("error") or "failed"


def run_cycle(
    rpc: RpcClient,
    broker_owner: str,
    secret: str,
    currency: str,
    issuer: str,
    state_path: Path,
    dry_run: bool,
    portal_url: str,
    portal_token: str,
) -> int:
    state = load_json(state_path)
    acted = 0
    now = ripple_now()
    price = amm_falcon_per_fusdc(rpc, currency, issuer)
    if price is None:
        warn("AMM price unavailable — skipping HF enforcement this cycle")
        return 0

    for obj in list_loans(rpc):
        loan_id = str(obj.get("index") or "")
        if not loan_id:
            continue
        impaired, defaulted = loan_flags(obj)
        if defaulted:
            continue
        debt = loan_outstanding(obj)
        collateral = collateral_falcon(obj)
        hf = health_factor(collateral, debt, price)
        pay_def = payment_default_eligible(obj, now)
        action = recommend_action(hf, impaired, defaulted, pay_def)
        if action in ("none", "monitor"):
            continue

        key = f"{loan_id}:{action}"
        last = state.get("last_actions", {}).get(key, 0)
        if time.time() - last < 300:
            continue

        borrower = obj.get("Borrower", "?")
        log(
            f"Loan {loan_id[:12]}… borrower={borrower} HF={hf:.3f if hf else '—'} "
            f"→ LoanManage {action}"
        )

        if portal_url and portal_token:
            if dry_run:
                log(f"[dry-run] portal LoanManage {action} {loan_id[:16]}…")
                acted += 1
                continue
            ok_act, result = portal_loan_manage(portal_url, portal_token, loan_id, action)
            if ok_act:
                ok(f"Portal LoanManage {action}: {result}")
                state.setdefault("last_actions", {})[key] = time.time()
                acted += 1
            else:
                warn(f"Portal LoanManage {action} failed: {result}")
            continue

        seq_r = rpc.public("account_info", {"account": broker_owner, "ledger_index": "validated"})
        seq = int(seq_r["account_data"]["Sequence"])
        ledger = rpc.public("ledger", {"ledger_index": "validated"})
        ll = int(ledger["ledger_index"]) + 20
        flags = {
            "impair": TF_LOAN_IMPAIR,
            "unimpair": TF_LOAN_UNIMPAIR,
            "default": TF_LOAN_DEFAULT,
        }[action]
        tx = {
            "TransactionType": "LoanManage",
            "Account": broker_owner,
            "LoanID": loan_id.upper(),
            "Flags": flags,
            "Sequence": seq,
            "Fee": "12",
            "LastLedgerSequence": ll,
        }
        if submit_tx(rpc, secret, tx, dry_run, f"LoanManage {action}"):
            state.setdefault("last_actions", {})[key] = time.time()
            acted += 1

    state["last_cycle_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    state["last_price"] = price
    if not dry_run:
        save_json(state_path, state)
    return acted


def main() -> int:
    parser = argparse.ArgumentParser(description="Lending HF + payment default enforcement daemon")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--loop", action="store_true")
    parser.add_argument("--interval", type=int, default=60)
    parser.add_argument("--public-rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"))
    parser.add_argument("--admin-rpc", default=os.environ.get("ADMIN_RPC_URL", "http://127.0.0.1:5005"))
    parser.add_argument("--container", default=os.environ.get("DOCKER_CONTAINER", "qxrp-full"))
    parser.add_argument(
        "--manifest",
        default=os.environ.get(
            "LENDING_MANIFEST",
            str(REPO_ROOT.parent / "qXRP-faucet-wallet" / "public" / "config" / "lending.json"),
        ),
    )
    parser.add_argument(
        "--stables-state",
        default=os.environ.get("STABLES_STATE_FILE", "/var/lib/qxrp-stables/stables_state.json"),
    )
    parser.add_argument(
        "--state",
        default=os.environ.get("LEND_HF_MONITOR_STATE", "/var/lib/qxrp-lending/hf_monitor_state.json"),
    )
    args = parser.parse_args()

    manifest = load_json(Path(args.manifest))
    broker_owner = manifest.get("broker_owner", "")
    currency = (manifest.get("asset") or {}).get("currency", "QUC")
    issuer = (manifest.get("asset") or {}).get("issuer", "")
    if not broker_owner:
        err("broker_owner missing from lending manifest")
        return 1

    secret = broker_secret(Path(args.stables_state))
    if not secret and not args.dry_run:
        err("Set TESTNET_LENDING_BROKER_SECRET or stables liquidity_provider.falcon_secret")
        return 1

    portal_url = os.environ.get("LEND_PORTAL_URL", "").strip()
    portal_token = os.environ.get("LEND_HF_MONITOR_TOKEN", "").strip()

    rpc = RpcClient(args.admin_rpc, args.public_rpc, args.container.strip())
    state_path = Path(args.state)

    def cycle() -> int:
        return run_cycle(
            rpc,
            broker_owner,
            secret,
            currency,
            issuer,
            state_path,
            args.dry_run,
            portal_url,
            portal_token,
        )

    if args.loop:
        log(f"HF monitor loop every {args.interval}s (dry_run={args.dry_run})")
        while True:
            try:
                n = cycle()
                if n:
                    log(f"Enforced {n} LoanManage action(s)")
            except Exception as e:
                warn(f"Cycle error: {e}")
            time.sleep(args.interval)
    else:
        n = cycle()
        log(f"Cycle complete — {n} action(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())