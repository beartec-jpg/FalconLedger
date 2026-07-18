#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Testnet lending book cleanup:

1. LoanManage default on every non-defaulted loan that is late (past grace) or HF < 1.1
2. LoanDelete every loan with PaymentRemaining == 0 (shell paid / already defaulted)

Uses broker owner secret from stables_state.json (permissionless default may be
any account; broker can also LoanDelete).

Usage (on coordinator):
  python3 scripts/lend-cleanup-books.py --dry-run
  python3 scripts/lend-cleanup-books.py --execute
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

DROPS = 1_000_000
TF_LOAN_DEFAULT = 0x00010000
LOAN_FLAG_DEFAULTED = 0x00010000  # lsfLoanDefault
HF_LIQ = 1.1
PUBLIC_RPC = "http://127.0.0.1:6005"
ADMIN_RPC = "http://127.0.0.1:5005"
CONTAINER = "qxrp-full"
CURRENCY = "QUC"
ISSUER = "rsJoDhjVV78jr6huHxKjtT8uG8RGeGmd1N"
BROKER_ID = "0DF028DFE8928921B9474B5EB09531E1E7A3655441C53ECFECF41C82F374D334"


def log(msg: str) -> None:
    print(f"[lend-cleanup] {msg}", flush=True)


def rpc_post(url: str, method: str, params: dict | None = None) -> dict:
    payload = json.dumps({"method": method, "params": [params or {}]}).encode()
    cmd = [
        "docker",
        "exec",
        CONTAINER,
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
    out = json.loads(subprocess.check_output(cmd, text=True))
    if out.get("error"):
        raise RuntimeError(out["error"])
    res = out.get("result", out)
    if isinstance(res, dict) and res.get("error"):
        raise RuntimeError(res)
    return res


def public(method: str, params: dict | None = None) -> dict:
    return rpc_post(PUBLIC_RPC, method, params)


def admin(method: str, params: dict | None = None) -> dict:
    return rpc_post(ADMIN_RPC, method, params)


def list_loans() -> list[dict]:
    loans: list[dict] = []
    marker = None
    while True:
        p: dict = {"ledger_index": "validated", "binary": False, "limit": 400}
        if marker:
            p["marker"] = marker
        r = public("ledger_data", p)
        for o in r.get("state") or []:
            if o.get("LedgerEntryType") == "Loan":
                loans.append(o)
        marker = r.get("marker")
        if not marker:
            break
    return loans


def close_time() -> int:
    r = public("ledger", {"ledger_index": "validated"})
    return int(r["ledger"]["close_time"])


def amm_price() -> float:
    """F-USDC per 1 FALCON from native AMM."""
    r = public(
        "amm_info",
        {
            "asset": {"currency": "XRP"},
            "asset2": {"currency": CURRENCY, "issuer": ISSUER},
            "ledger_index": "validated",
        },
    )
    amm = r.get("amm") or {}
    xrp = int(amm.get("amount") or 0) / DROPS
    usdc = float((amm.get("amount2") or {}).get("value") or 0)
    if xrp <= 0:
        raise RuntimeError("AMM has no FALCON leg")
    return usdc / xrp


def debt_of(loan: dict) -> float:
    if loan.get("TotalValueOutstanding") is not None:
        return float(loan["TotalValueOutstanding"])
    if loan.get("PrincipalOutstanding") is not None:
        return float(loan["PrincipalOutstanding"])
    return 0.0


def coll_falcon(loan: dict) -> float:
    c = loan.get("Collateral")
    if c is None:
        return 0.0
    if isinstance(c, str) and c.isdigit():
        return int(c) / DROPS
    if isinstance(c, (int, float)):
        return float(c) / DROPS if float(c) > 1e6 else float(c)
    return 0.0


def payment_remaining(loan: dict) -> int:
    v = loan.get("PaymentRemaining")
    if v is None:
        return 0
    return int(v)


def is_defaulted(loan: dict) -> bool:
    return bool(int(loan.get("Flags") or 0) & LOAN_FLAG_DEFAULTED)


def is_late(loan: dict, now: int) -> bool:
    nxt = int(loan.get("NextPaymentDueDate") or 0)
    grace = int(loan.get("GracePeriod") or 0)
    if nxt <= 0:
        return False
    return now > nxt + grace


def account_seq(account: str) -> int:
    r = public("account_info", {"account": account, "ledger_index": "validated"})
    return int(r["account_data"]["Sequence"])


def last_ledger() -> int:
    r = public("ledger", {"ledger_index": "validated"})
    return int(r["ledger_index"]) + 40


def sign_submit(secret: str, tx: dict) -> tuple[str, str]:
    params = (
        {"falcon_secret": secret, "tx_json": tx}
        if not secret.startswith(("s", "S"))
        else {"secret": secret, "tx_json": tx}
    )
    signed = admin("sign", params)
    if signed.get("status") != "success":
        raise RuntimeError(f"sign failed: {signed}")
    blob = signed["tx_blob"]
    h = signed.get("tx_json", {}).get("hash", "")
    sub = public("submit", {"tx_blob": blob})
    return sub.get("engine_result", "?"), h


def wait_tx(tx_hash: str) -> str:
    if not tx_hash:
        return "NO_HASH"
    for _ in range(40):
        time.sleep(1.5)
        try:
            r = public("tx", {"transaction": tx_hash, "binary": False})
            if r.get("validated"):
                return r.get("meta", {}).get("TransactionResult", "?")
        except Exception:
            pass
    return "TIMEOUT"


def load_broker_secret(stables: Path) -> tuple[str, str]:
    d = json.loads(stables.read_text())
    lp = d.get("liquidity_provider") or d.get("broker") or {}
    secret = (
        lp.get("falcon_secret")
        or lp.get("seed")
        or d.get("broker_secret")
        or d.get("TESTNET_LENDING_BROKER_SECRET")
        or ""
    )
    addr = lp.get("address") or d.get("broker_owner") or ""
    if not secret or not addr:
        raise SystemExit(f"broker secret/address missing in {stables}")
    return addr, secret


def broker_snapshot() -> dict:
    try:
        r = public("ledger_entry", {"index": BROKER_ID, "ledger_index": "validated"})
        return r.get("node") or {}
    except Exception as e:
        return {"error": str(e)}


def vault_snapshot(vault_id: str) -> dict:
    try:
        r = public("vault_info", {"vault_id": vault_id, "ledger_index": "validated"})
        return r.get("vault") or {}
    except Exception as e:
        return {"error": str(e)}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--execute", action="store_true")
    ap.add_argument(
        "--stables-state",
        default="/var/lib/qxrp-stables/stables_state.json",
    )
    ap.add_argument(
        "--also-force-healthy",
        action="store_true",
        help="Attempt default on HF-healthy loans too (will usually tecTOO_SOON)",
    )
    args = ap.parse_args()
    if not args.dry_run and not args.execute:
        log("Pass --dry-run or --execute")
        return 2

    dry = bool(args.dry_run)
    owner, secret = load_broker_secret(Path(args.stables_state))
    price = amm_price()
    now = close_time()
    loans = list_loans()
    log(f"close_time={now} price={price:.8f} F-USDC/FALCON loans={len(loans)} actor={owner}")

    to_default: list[dict] = []
    to_delete: list[dict] = []
    leave: list[dict] = []

    for loan in loans:
        debt = debt_of(loan)
        hf = None
        if debt > 0 and coll_falcon(loan) > 0:
            hf = (coll_falcon(loan) * price) / debt
        defaulted = is_defaulted(loan)
        rem = payment_remaining(loan)
        late = is_late(loan, now)
        liq = (not defaulted) and rem > 0 and debt > 0 and (late or (hf is not None and hf < HF_LIQ))
        if args.also_force_healthy and not defaulted and rem > 0 and debt > 0 and not liq:
            liq = True  # will try; may fail
        if liq:
            to_default.append(loan)
        elif rem == 0 or defaulted or debt <= 0:
            to_delete.append(loan)
        else:
            leave.append(loan)

    log(f"plan: default={len(to_default)} delete={len(to_delete)} leave_active={len(leave)}")
    for loan in to_default:
        debt = debt_of(loan)
        hf = (coll_falcon(loan) * price) / debt if debt else None
        hf_s = f"{hf:.3f}" if hf is not None else "—"
        log(
            f"  DEFAULT seq={loan.get('LoanSequence')} id={loan['index'][:12]}… "
            f"debt={debt:.4f} coll={coll_falcon(loan):.1f} HF={hf_s} "
            f"late={is_late(loan, now)}"
        )
    for loan in leave:
        debt = debt_of(loan)
        hf = (coll_falcon(loan) * price) / debt if debt else None
        hf_s = f"{hf:.3f}" if hf is not None else "—"
        log(
            f"  LEAVE   seq={loan.get('LoanSequence')} id={loan['index'][:12]}… "
            f"debt={debt:.4f} HF={hf_s} (healthy — repay or wait)"
        )

    if dry:
        log("Dry-run only — no txs")
        return 0

    # Phase 1: defaults
    for loan in to_default:
        loan_id = loan["index"]
        try:
            seq = account_seq(owner)
            tx = {
                "TransactionType": "LoanManage",
                "Account": owner,
                "LoanID": loan_id.upper(),
                "Flags": TF_LOAN_DEFAULT,
                "Fee": "12",
                "Sequence": seq,
                "LastLedgerSequence": last_ledger(),
            }
            er, h = sign_submit(secret, tx)
            final = wait_tx(h) if er in ("tesSUCCESS", "terQUEUED") else er
            log(f"  LoanManage default {loan_id[:12]}… {er} → {final} hash={h}")
        except Exception as e:
            log(f"  LoanManage default FAILED {loan_id[:12]}… {e}")

    time.sleep(3)
    # refresh list for deletes
    loans = list_loans()
    to_delete = [L for L in loans if payment_remaining(L) == 0 or is_defaulted(L) or debt_of(L) <= 0]

    # Phase 2: delete closed shells
    for loan in to_delete:
        loan_id = loan["index"]
        if payment_remaining(loan) > 0 and debt_of(loan) > 0 and not is_defaulted(loan):
            continue
        try:
            seq = account_seq(owner)
            tx = {
                "TransactionType": "LoanDelete",
                "Account": owner,
                "LoanID": loan_id.upper(),
                "Fee": "12",
                "Sequence": seq,
                "LastLedgerSequence": last_ledger(),
            }
            er, h = sign_submit(secret, tx)
            final = wait_tx(h) if er in ("tesSUCCESS", "terQUEUED") else er
            log(f"  LoanDelete {loan_id[:12]}… {er} → {final} hash={h}")
        except Exception as e:
            log(f"  LoanDelete FAILED {loan_id[:12]}… {e}")

    time.sleep(2)
    remaining = list_loans()
    b = broker_snapshot()
    log(f"DONE remaining_loans={len(remaining)} broker_DebtTotal={b.get('DebtTotal')} OwnerCount={b.get('OwnerCount')}")
    for loan in remaining:
        debt = debt_of(loan)
        hf = (coll_falcon(loan) * price) / debt if debt else None
        hf_s = f"{hf:.3f}" if hf is not None else "—"
        log(
            f"  still: seq={loan.get('LoanSequence')} id={loan['index'][:12]}… "
            f"debt={debt:.4f} HF={hf_s} flags={loan.get('Flags')} "
            f"defaulted={is_defaulted(loan)} rem={payment_remaining(loan)}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
