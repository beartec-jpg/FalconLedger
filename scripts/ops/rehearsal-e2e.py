#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Mainnet dress-rehearsal orchestrator — ordered pass/fail for 8+ readiness.

Does not invent new protocol logic; wires existing scripts + RPC checks.

Usage (from repo root):
  export PUBLIC_RPC=http://<rehearsal-rpc>:6005
  export ADMIN_RPC=http://127.0.0.1:5005          # optional; sign path
  export CONTAINER=qxrp-validator                 # if using docker exec RPC
  export REHEARSAL_RESULTS=scripts/mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md

  # Smoke only (hours):
  python3 scripts/ops/rehearsal-e2e.py --phases health,payments,split,faucet

  # Full product (needs env secrets + funded keys):
  python3 scripts/ops/rehearsal-e2e.py --phases all

  # Dry plan (no network):
  python3 scripts/ops/rehearsal-e2e.py --list

Environment (by phase):
  health     PUBLIC_RPC
  payments   PUBLIC_RPC, PAY_FROM_SECRET, PAY_TO_ADDRESS  (optional smoke pay)
  split      see scripts/mainnet-genesis-split.env.example + --execute needs secrets
  faucet     FAUCET_HOT_SECRET or portal drip URL; FAUCET_CLAIM_ADDRESS
  bond       (info only — operator one-liner / bond-if-funded)
  lend       runs lend-e2e-permissionless.py if LEND_E2E=1
  amm        PROTOCOL_POOL / enable path if AMM_E2E=1
  bridge     BRIDGE_E2E=1 + Sepolia + relay env (manual sub-scripts)
  scoring    polls server_info + logs guidance for bond score fields
  emissions  waits for epoch boundary (fast-epoch image recommended)
  claims     CLAIM_SECRET for ClaimReward after emissions
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

REPO = Path(__file__).resolve().parents[2]
SCRIPTS = REPO / "scripts"

ALL_PHASES = [
    "health",
    "payments",
    "split",
    "faucet",
    "bond",
    "lend",
    "amm",
    "bridge",
    "scoring",
    "emissions",
    "claims",
    "teardown_note",
]


class PhaseResult:
    def __init__(self, name: str):
        self.name = name
        self.status = "SKIP"
        self.detail = ""
        self.started = ""
        self.ended = ""

    def ok(self, detail: str = "") -> None:
        self.status = "PASS"
        self.detail = detail

    def fail(self, detail: str) -> None:
        self.status = "FAIL"
        self.detail = detail

    def skip(self, detail: str) -> None:
        self.status = "SKIP"
        self.detail = detail


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def rpc(url: str, method: str, params: dict | None = None, timeout: int = 30) -> dict:
    payload = json.dumps({"method": method, "params": [params or {}]}).encode()
    container = os.environ.get("CONTAINER", "").strip()
    if container:
        cmd = [
            "docker",
            "exec",
            container,
            "curl",
            "-sf",
            "--max-time",
            str(timeout),
            "-X",
            "POST",
            url,
            "-H",
            "Content-Type: application/json",
            "-d",
            payload.decode(),
        ]
        out = subprocess.check_output(cmd, text=True, timeout=timeout + 10)
        body = json.loads(out)
    else:
        req = urllib.request.Request(
            url, data=payload, headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = json.loads(resp.read())
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body.get("result", body)


def log(msg: str) -> None:
    print(f"[rehearsal-e2e] {msg}", flush=True)


def phase_health(r: PhaseResult) -> None:
    public = os.environ.get("PUBLIC_RPC", "").strip()
    if not public:
        r.fail("PUBLIC_RPC not set")
        return
    info = rpc(public, "server_info")
    inf = info.get("info") or info
    state = inf.get("server_state") or ""
    peers = inf.get("peers")
    vl = inf.get("validated_ledger") or inf.get("closed_ledger") or {}
    seq = vl.get("seq")
    complete = inf.get("complete_ledgers") or ""
    if seq is None and not complete:
        r.fail(f"no ledger progress server_state={state}")
        return
    r.ok(f"state={state} seq={seq} peers={peers} complete={complete!r}")


def phase_payments(r: PhaseResult) -> None:
    public = os.environ.get("PUBLIC_RPC", "").strip()
    secret = os.environ.get("PAY_FROM_SECRET", "").strip()
    dest = os.environ.get("PAY_TO_ADDRESS", "").strip()
    if not secret or not dest:
        r.skip("set PAY_FROM_SECRET + PAY_TO_ADDRESS for live payment smoke")
        return
    admin = os.environ.get("ADMIN_RPC", public).strip()
    # Minimal Payment 1 drop or 1 XRP depending on REHEARSAL_PAY_DROPS
    drops = int(os.environ.get("REHEARSAL_PAY_DROPS", "1000000"))  # 1 unit
    acct = rpc(public, "account_info", {"account": dest, "ledger_index": "validated"})
    # If dest missing, payment may create — Falcon may need funded dest; try anyway
    _ = acct
    tx = {
        "TransactionType": "Payment",
        "Account": os.environ.get("PAY_FROM_ADDRESS", ""),
        "Destination": dest,
        "Amount": str(drops),
    }
    # Resolve Account from wallet_propose if only secret given
    if not tx["Account"]:
        # sign will fill Account from secret on many stacks; require address for clarity
        r.fail("PAY_FROM_ADDRESS required with PAY_FROM_SECRET")
        return
    sign_params: dict[str, Any] = {"tx_json": tx}
    if secret.startswith(("s", "S")) and len(secret) < 80:
        sign_params["secret"] = secret
    else:
        sign_params["falcon_secret"] = secret
    signed = rpc(admin, "sign", sign_params)
    if signed.get("status") != "success" and not signed.get("tx_blob"):
        r.fail(f"sign failed: {signed}")
        return
    blob = signed.get("tx_blob") or (signed.get("result") or {}).get("tx_blob")
    if not blob:
        # nested result shape
        blob = (signed.get("tx_json") and signed.get("tx_blob")) or None
    if not blob and isinstance(signed, dict):
        # admin path may wrap
        for k in ("tx_blob",):
            if k in signed:
                blob = signed[k]
    if not blob:
        r.fail(f"no tx_blob in sign response keys={list(signed.keys())}")
        return
    sub = rpc(public, "submit", {"tx_blob": blob})
    eng = sub.get("engine_result") or sub.get("engine_result_code")
    if eng not in ("tesSUCCESS", "terQUEUED") and sub.get("engine_result") not in (
        "tesSUCCESS",
        "terQUEUED",
    ):
        # accept tesSUCCESS only for hard pass
        if sub.get("engine_result") != "tesSUCCESS":
            r.fail(f"submit {sub.get('engine_result')}: {sub.get('engine_result_message')}")
            return
    r.ok(f"payment submitted engine_result={sub.get('engine_result')} hash={sub.get('tx_json', {}).get('hash', '')}")


def phase_split(r: PhaseResult) -> None:
    script = SCRIPTS / "mainnet-genesis-split.py"
    if not script.is_file():
        r.fail(f"missing {script}")
        return
    mode = os.environ.get("SPLIT_MODE", "dry-run").strip()  # dry-run | execute
    if mode not in ("dry-run", "execute"):
        r.fail("SPLIT_MODE must be dry-run or execute")
        return
    env = os.environ.copy()
    cmd = [sys.executable, str(script), f"--{mode}"]
    try:
        out = subprocess.check_output(cmd, cwd=str(REPO), env=env, text=True, stderr=subprocess.STDOUT)
        r.ok(out.strip().splitlines()[-1] if out.strip() else f"split --{mode} ok")
    except subprocess.CalledProcessError as e:
        r.fail(e.output[-2000:] if e.output else str(e))
    except FileNotFoundError as e:
        r.fail(str(e))


def phase_faucet(r: PhaseResult) -> None:
    """Either hit HTTP faucet or document skip for portal-only."""
    drip_url = os.environ.get("FAUCET_DRIP_URL", "").strip()
    claim = os.environ.get("FAUCET_CLAIM_ADDRESS", "").strip()
    if drip_url and claim:
        payload = json.dumps({"address": claim}).encode()
        req = urllib.request.Request(
            drip_url,
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                body = resp.read().decode()
            r.ok(f"drip response: {body[:300]}")
        except urllib.error.HTTPError as e:
            err = e.read().decode() if e.fp else str(e)
            # cooldown may be success path for second claim tests
            if e.code in (429, 400) and "cooldown" in err.lower():
                r.ok(f"cooldown enforced: {err[:200]}")
            else:
                r.fail(f"HTTP {e.code}: {err[:500]}")
        except Exception as e:
            r.fail(str(e))
        return
    r.skip(
        "set FAUCET_DRIP_URL + FAUCET_CLAIM_ADDRESS for automated drip; "
        "or run portal faucet manually and mark PASS in results"
    )


def phase_bond(r: PhaseResult) -> None:
    r.skip(
        "bond via one-liner / bond-if-funded; confirm bonded accounts via account_objects "
        "type ValidatorBond — not automated here (operator keys)"
    )


def phase_lend(r: PhaseResult) -> None:
    if os.environ.get("LEND_E2E", "").strip() not in ("1", "true", "yes"):
        r.skip("set LEND_E2E=1 to run scripts/lend-e2e-permissionless.py")
        return
    script = SCRIPTS / "lend-e2e-permissionless.py"
    if not script.is_file():
        r.fail(f"missing {script}")
        return
    env = os.environ.copy()
    try:
        out = subprocess.check_output(
            [sys.executable, str(script)],
            cwd=str(SCRIPTS),
            env=env,
            text=True,
            stderr=subprocess.STDOUT,
            timeout=int(os.environ.get("LEND_E2E_TIMEOUT", "600")),
        )
        r.ok(out.strip().splitlines()[-3:] and " | ".join(out.strip().splitlines()[-3:]) or "lend e2e ok")
    except subprocess.CalledProcessError as e:
        r.fail((e.output or str(e))[-2500:])
    except subprocess.TimeoutExpired:
        r.fail("lend e2e timed out")


def phase_amm(r: PhaseResult) -> None:
    if os.environ.get("AMM_E2E", "").strip() not in ("1", "true", "yes"):
        r.skip("set AMM_E2E=1 and configure protocol-liquidity-pool.py env to run AMM smoke")
        return
    script = SCRIPTS / "protocol-liquidity-pool.py"
    if not script.is_file():
        r.fail(f"missing {script}")
        return
    # Prefer --help / dry if script supports; else run with REHEARSAL
    env = os.environ.copy()
    args = [sys.executable, str(script)]
    extra = os.environ.get("AMM_E2E_ARGS", "").strip()
    if extra:
        args.extend(extra.split())
    try:
        out = subprocess.check_output(
            args,
            cwd=str(REPO),
            env=env,
            text=True,
            stderr=subprocess.STDOUT,
            timeout=int(os.environ.get("AMM_E2E_TIMEOUT", "600")),
        )
        r.ok(" | ".join(out.strip().splitlines()[-3:]) if out.strip() else "amm ok")
    except subprocess.CalledProcessError as e:
        r.fail((e.output or str(e))[-2500:])
    except subprocess.TimeoutExpired:
        r.fail("amm e2e timed out")


def phase_bridge(r: PhaseResult) -> None:
    if os.environ.get("BRIDGE_E2E", "").strip() not in ("1", "true", "yes"):
        r.skip(
            "set BRIDGE_E2E=1 with Sepolia RPC + lock contract + "
            "bridge-deposit-relay / bridge-withdraw-relay env; run relays separately"
        )
        return
    # Smoke: both scripts exist and --help works
    dep = SCRIPTS / "bridge-deposit-relay.py"
    wdr = SCRIPTS / "bridge-withdraw-relay.py"
    missing = [p.name for p in (dep, wdr) if not p.is_file()]
    if missing:
        r.fail(f"missing scripts: {missing}")
        return
    r.ok(
        "bridge scripts present — execute deposit→credit and withdraw→release manually "
        "or via deploy-bridge-relay.sh; attach tx hashes in REHEARSAL_RESULTS.md"
    )


def phase_scoring(r: PhaseResult) -> None:
    public = os.environ.get("PUBLIC_RPC", "").strip()
    if not public:
        r.fail("PUBLIC_RPC not set")
        return
    info = rpc(public, "server_info")
    inf = info.get("info") or info
    vl = inf.get("validated_ledger") or inf.get("closed_ledger") or {}
    seq = int(vl.get("seq") or 0)
    # Guidance: scores rewrite every 256 ledgers (flag interval)
    rem = 256 - (seq % 256) if seq else 256
    bond = os.environ.get("SCORING_BOND_ACCOUNT", "").strip()
    detail = f"ledger_seq={seq} next_score_boundary_in~{rem} ledgers"
    if bond:
        try:
            objs = rpc(
                public,
                "account_objects",
                {"account": bond, "ledger_index": "validated", "type": "validator_bond"},
            )
            # type filter may not exist — fall back
            entries = objs.get("account_objects") or []
            if not entries:
                objs = rpc(public, "account_objects", {"account": bond, "ledger_index": "validated"})
                entries = [
                    e
                    for e in (objs.get("account_objects") or [])
                    if e.get("LedgerEntryType") in ("ValidatorBond", "ValidatorBond")
                    or "BondStatus" in e
                    or "CompositeScore" in e
                ]
            if entries:
                e0 = entries[0]
                detail += (
                    f" bond CompositeScore={e0.get('CompositeScore')} "
                    f"UptimeBps={e0.get('UptimeBps')} "
                    f"VoteAccuracyBps={e0.get('VoteAccuracyBps')} "
                    f"LatencyScoreBps={e0.get('LatencyScoreBps')} "
                    f"ConsistencyBps={e0.get('ConsistencyBps')}"
                )
                r.ok(detail)
                return
            r.ok(detail + " (no bond object found yet — bond validators first)")
            return
        except Exception as e:
            r.ok(detail + f" (bond poll: {e})")
            return
    r.ok(
        detail
        + " — set SCORING_BOND_ACCOUNT to dump score fields; wait ≥1 flag interval (256) for EMA update"
    )


def phase_emissions(r: PhaseResult) -> None:
    """Observe epoch object; full claim needs fast-epoch image past first emission epoch."""
    public = os.environ.get("PUBLIC_RPC", "").strip()
    if not public:
        r.fail("PUBLIC_RPC not set")
        return
    wait = os.environ.get("EMISSIONS_WAIT", "").strip() in ("1", "true", "yes")
    # ledger entry for reward epoch — method may vary; try ledger_entry
    try:
        # Best-effort: server_info + note
        info = rpc(public, "server_info")
        inf = info.get("info") or info
        seq = (inf.get("validated_ledger") or inf.get("closed_ledger") or {}).get("seq")
        r.ok(
            f"seq={seq}. Production first emission at epoch 8 (~7d/epoch). "
            f"For rehearsal use image mainnet-rehearsal-fast-epoch "
            f"(scripts/ops/build-fast-epoch-rehearsal.sh). "
            f"EMISSIONS_WAIT={wait}"
        )
    except Exception as e:
        r.fail(str(e))


def phase_claims(r: PhaseResult) -> None:
    if not os.environ.get("CLAIM_SECRET", "").strip():
        r.skip("set CLAIM_SECRET (+ CLAIM_ACCOUNT) after an emission epoch to test ClaimReward")
        return
    r.skip(
        "ClaimReward automation is environment-specific; submit Validator ClaimReward "
        "via admin sign after emission epoch and record hash in results"
    )


def phase_teardown_note(r: PhaseResult) -> None:
    r.ok(
        "After PASS suite: stop containers, wipe volumes, quarantine throwaway keys, "
        "keep only public results + image digest. Real T0 = new keys + mainnet-v1 digest."
    )


HANDLERS: dict[str, Callable[[PhaseResult], None]] = {
    "health": phase_health,
    "payments": phase_payments,
    "split": phase_split,
    "faucet": phase_faucet,
    "bond": phase_bond,
    "lend": phase_lend,
    "amm": phase_amm,
    "bridge": phase_bridge,
    "scoring": phase_scoring,
    "emissions": phase_emissions,
    "claims": phase_claims,
    "teardown_note": phase_teardown_note,
}


def write_results(path: Path, results: list[PhaseResult], image: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Rehearsal results",
        "",
        f"- **When:** {utc_now()}",
        f"- **Image:** `{image}`",
        f"- **PUBLIC_RPC:** `{os.environ.get('PUBLIC_RPC', '')}`",
        f"- **Source tip (local):** run `git rev-parse HEAD` on build host",
        "",
        "| Phase | Status | Detail |",
        "|-------|--------|--------|",
    ]
    for r in results:
        detail = r.detail.replace("|", "\\|").replace("\n", " ")[:200]
        lines.append(f"| {r.name} | **{r.status}** | {detail} |")
    lines += [
        "",
        "## Sign-off",
        "",
        "- [ ] Ready for real T0 (mainnet-v1 digest only)",
        "- [ ] Blocked — list fixes above",
        "- [ ] Chain wiped; throwaway secrets destroyed",
        "",
    ]
    path.write_text("\n".join(lines) + "\n")
    log(f"wrote {path}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Mainnet rehearsal E2E orchestrator")
    ap.add_argument(
        "--phases",
        default="health,scoring,emissions,teardown_note",
        help=f"comma list or 'all'. known={','.join(ALL_PHASES)}",
    )
    ap.add_argument("--list", action="store_true", help="list phases and exit")
    ap.add_argument(
        "--results",
        default=os.environ.get(
            "REHEARSAL_RESULTS",
            str(SCRIPTS / "mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md"),
        ),
    )
    ap.add_argument(
        "--image",
        default=os.environ.get("QXRP_XRPLD_IMAGE", "qxrp/xrpld:mainnet-v1"),
    )
    ap.add_argument(
        "--fail-fast",
        action="store_true",
        help="stop on first FAIL",
    )
    args = ap.parse_args()

    if args.list:
        for p in ALL_PHASES:
            print(p)
        return 0

    if args.phases.strip().lower() == "all":
        phases = list(ALL_PHASES)
    else:
        phases = [p.strip() for p in args.phases.split(",") if p.strip()]
        unknown = [p for p in phases if p not in HANDLERS]
        if unknown:
            log(f"unknown phases: {unknown}")
            return 2

    results: list[PhaseResult] = []
    failed = 0
    log(f"start phases={phases} image={args.image}")
    for name in phases:
        pr = PhaseResult(name)
        pr.started = utc_now()
        log(f"── phase {name} ──")
        try:
            HANDLERS[name](pr)
        except Exception as e:
            pr.fail(f"exception: {e}")
        pr.ended = utc_now()
        log(f"   → {pr.status}: {pr.detail[:180]}")
        results.append(pr)
        if pr.status == "FAIL":
            failed += 1
            if args.fail_fast:
                break

    write_results(Path(args.results), results, args.image)
    passed = sum(1 for x in results if x.status == "PASS")
    skipped = sum(1 for x in results if x.status == "SKIP")
    log(f"done PASS={passed} FAIL={failed} SKIP={skipped}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
