# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# qXRP Validator Dashboard — node + network metrics
#
# Usage:
#   XRPLD_RPC_URL=http://127.0.0.1:5005 \
#   NETWORK_RPC_URL=http://46.224.0.140:6005 \
#   VALIDATOR_ACCOUNT=r... \
#   python3 tools/dashboard/server.py
#
# Open: http://localhost:8080  ·  JSON: /api/stats

from __future__ import annotations

import json
import os
import time
from typing import Any, Dict, List

import httpx
import uvicorn
from fastapi import FastAPI, Response
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, JSONResponse

RPC_URL = os.environ.get("XRPLD_RPC_URL", "http://127.0.0.1:5005")
NETWORK_RPC_URL = os.environ.get("NETWORK_RPC_URL", "http://46.224.0.140:6005")
LISTEN_PORT = int(os.environ.get("DASHBOARD_PORT", "8080"))
VALIDATOR_ACCOUNT = os.environ.get("VALIDATOR_ACCOUNT", "")
DROPS_PER_QXRP = 1_000_000

app = FastAPI(title="qXRP Validator Dashboard", docs_url=None, redoc_url=None)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET"],
    allow_headers=["*"],
)


def rpc_call(url: str, method: str, params: Dict[str, Any] | None = None) -> Dict[str, Any]:
    payload = {"method": method, "params": [params or {}]}
    try:
        r = httpx.post(url, json=payload, timeout=8.0)
        r.raise_for_status()
        body = r.json()
        result = body.get("result", body)
        if isinstance(result, dict) and result.get("error"):
            return {"error": result.get("error_message", result.get("error"))}
        return result if isinstance(result, dict) else {}
    except Exception as exc:
        return {"error": str(exc)}


def rpc_local(method: str, params: Dict[str, Any] | None = None) -> Dict[str, Any]:
    return rpc_call(RPC_URL, method, params)


def rpc_network(method: str, params: Dict[str, Any] | None = None) -> Dict[str, Any]:
    return rpc_call(NETWORK_RPC_URL, method, params)


def drops_to_qxrp(drops: Any) -> float | None:
    try:
        return int(drops) / DROPS_PER_QXRP
    except (TypeError, ValueError):
        return None


def bond_status_label(raw: Any) -> str:
    if isinstance(raw, int):
        return {0: "registered", 1: "bonded", 2: "unbonding"}.get(raw, str(raw))
    return str(raw) if raw is not None else "unknown"


def fetch_bond(account: str, rpc_fn) -> Dict[str, Any]:
    if not account:
        return {}
    data = rpc_fn("ledger_entry", {
        "validator_bond": {"account": account},
        "ledger_index": "validated",
    })
    return data.get("node", {})


def fetch_bonded_validators(limit: int = 32) -> List[Dict[str, Any]]:
    data = rpc_network("ledger_data", {
        "ledger_index": "validated",
        "type": "validator_bond",
        "limit": limit,
    })
    out: List[Dict[str, Any]] = []
    for entry in data.get("state", []) or []:
        if entry.get("LedgerEntryType") != "ValidatorBond":
            continue
        out.append({
            "account": entry.get("Account"),
            "bond_status": bond_status_label(entry.get("BondStatus")),
            "bonded_amount_qxrp": drops_to_qxrp(entry.get("BondedAmount")),
            "composite_score": entry.get("CompositeScore"),
            "reward_accum_qxrp": drops_to_qxrp(entry.get("RewardAccumulator")),
            "consensus_key": (entry.get("ConsensusKey") or "")[:16],
        })
    return out


def fetch_epoch() -> Dict[str, Any]:
    data = rpc_network("ledger_entry", {
        "reward_epoch": True,
        "ledger_index": "validated",
    })
    node = data.get("node", {})
    if not node:
        return {}
    return {
        "epoch_number": node.get("EpochNumber"),
        "emission_rate_qxrp": drops_to_qxrp(node.get("EmissionRate")),
        "epoch_pool_balance_qxrp": drops_to_qxrp(node.get("EpochPoolBalance")),
    }


def collect_stats() -> Dict[str, Any]:
    local_info = rpc_local("server_info").get("info", {})
    net_info = rpc_network("server_info").get("info", {})

    local_vl = local_info.get("validated_ledger", {}) or {}
    net_vl = net_info.get("validated_ledger", {}) or {}

    bond = fetch_bond(VALIDATOR_ACCOUNT, rpc_local)
    balance_qxrp = None
    if VALIDATOR_ACCOUNT:
        acct = rpc_network("account_info", {
            "account": VALIDATOR_ACCOUNT,
            "ledger_index": "validated",
        })
        if acct.get("account_data"):
            balance_qxrp = drops_to_qxrp(acct["account_data"].get("Balance"))

    validators = fetch_bonded_validators()
    bonded_count = sum(1 for v in validators if v.get("bond_status") == "bonded")

    local_seq = local_vl.get("seq", 0) or 0
    net_seq = net_vl.get("seq", 0) or 0
    ledger_lag = max(0, int(net_seq) - int(local_seq)) if net_seq and local_seq else None

    return {
        "updated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "node": {
            "validator_account": VALIDATOR_ACCOUNT or None,
            "validation_pubkey": local_info.get("pubkey_validator"),
            "server_state": local_info.get("server_state"),
            "peers": local_info.get("peers", 0),
            "complete_ledgers": local_info.get("complete_ledgers"),
            "ledger_seq": local_seq,
            "ledger_hash": local_vl.get("hash"),
            "ledger_lag": ledger_lag,
            "load_factor": local_info.get("load_factor"),
            "uptime_seconds": local_info.get("uptime"),
            "network_id": local_info.get("network_id"),
            "build_version": local_info.get("build_version"),
            "balance_qxrp": balance_qxrp,
            "bond": {
                "status": bond_status_label(bond.get("BondStatus")),
                "bonded_amount_qxrp": drops_to_qxrp(bond.get("BondedAmount")),
                "composite_score": bond.get("CompositeScore"),
                "reward_accum_qxrp": drops_to_qxrp(bond.get("RewardAccumulator")),
                "uptime_score": bond.get("UptimeScore"),
                "vote_accuracy_score": bond.get("VoteAccuracyScore"),
                "slash_multiplier": bond.get("SlashMultiplier"),
            } if bond else None,
        },
        "network": {
            "rpc": NETWORK_RPC_URL,
            "server_state": net_info.get("server_state"),
            "ledger_seq": net_seq,
            "complete_ledgers": net_info.get("complete_ledgers"),
            "peers": net_info.get("peers", 0),
            "load_factor": net_info.get("load_factor"),
            "bonded_validator_count": bonded_count,
            "total_validator_entries": len(validators),
            "validators": validators,
            "epoch": fetch_epoch(),
        },
    }


def _css() -> str:
    return """
  :root { --bg:#0d1117; --card:#161b22; --border:#30363d; --text:#c9d1d9; --muted:#8b949e;
          --good:#3fb950; --warn:#d29922; --bad:#f85149; --accent:#58a6ff; }
  * { box-sizing: border-box; }
  body { font-family: 'Segoe UI', system-ui, sans-serif; background: var(--bg); color: var(--text);
         margin: 0; padding: 20px; line-height: 1.45; }
  h1 { color: var(--accent); margin: 0 0 4px; font-size: 1.6rem; }
  h2 { font-size: 0.72rem; color: var(--muted); text-transform: uppercase; letter-spacing: .08em;
       margin: 0 0 14px; font-weight: 600; }
  .subtitle { color: var(--muted); font-size: 0.85rem; margin-bottom: 22px; }
  .section { margin-bottom: 28px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; }
  .card { background: var(--card); border: 1px solid var(--border); border-radius: 10px; padding: 16px; }
  .value { font-size: 1.35rem; font-weight: 700; color: #f0f6fc; }
  .value.good { color: var(--good); }
  .value.warn { color: var(--warn); }
  .value.bad { color: var(--bad); }
  .label { font-size: 0.7rem; color: var(--muted); text-transform: uppercase; letter-spacing: .06em; margin-bottom: 6px; }
  .mono { font-family: ui-monospace, monospace; font-size: 0.75rem; color: var(--muted);
          word-break: break-all; margin-top: 6px; }
  table { width: 100%; border-collapse: collapse; font-size: 0.8rem; }
  th, td { text-align: left; padding: 8px 10px; border-bottom: 1px solid var(--border); }
  th { color: var(--muted); font-weight: 600; font-size: 0.7rem; text-transform: uppercase; }
  .footer { margin-top: 24px; color: var(--muted); font-size: 0.75rem; }
  .pill { display: inline-block; padding: 2px 8px; border-radius: 999px; font-size: 0.7rem;
          background: #21262d; border: 1px solid var(--border); }
"""


def _card(label: str, value: str, cls: str = "", sub: str = "") -> str:
    sub_html = f'<div class="mono">{sub}</div>' if sub else ""
    return f"""<div class="card"><div class="label">{label}</div>
    <div class="value {cls}">{value}</div>{sub_html}</div>"""


@app.get("/api/stats")
def api_stats() -> JSONResponse:
    return JSONResponse(collect_stats())


@app.get("/", response_class=HTMLResponse)
def index() -> str:
    s = collect_stats()
    node = s["node"]
    net = s["network"]
    bond = node.get("bond") or {}

    state = node.get("server_state", "unknown")
    state_cls = "good" if state == "proposing" else "warn"
    peers = int(node.get("peers") or 0)
    peer_cls = "good" if peers >= 3 else "warn"
    lag = node.get("ledger_lag")
    lag_cls = "good" if lag is not None and lag <= 5 else "warn"
    score = bond.get("composite_score", "—")
    score_cls = "good" if isinstance(score, int) and score >= 5000 else "warn"
    bstat = bond.get("status", "—")
    bstat_cls = "good" if bstat == "bonded" else "warn"

    val_rows = ""
    for v in net.get("validators", []):
        val_rows += f"""<tr>
          <td class="mono">{v.get('account','')[:12]}…</td>
          <td>{v.get('bond_status')}</td>
          <td>{v.get('bonded_amount_qxrp') or '—'}</td>
          <td>{v.get('composite_score') or '—'}</td>
        </tr>"""

    epoch = net.get("epoch") or {}
    epoch_txt = (
        f"Epoch {epoch.get('epoch_number', '—')} · "
        f"pool {epoch.get('epoch_pool_balance_qxrp', '—')} qXRP"
        if epoch else "No epoch data yet"
    )

    cards_node = "".join([
        _card("Server state", str(state), state_cls, str(node.get("validation_pubkey", ""))[:28] + "…"),
        _card("Ledger", f"#{int(node.get('ledger_seq') or 0):,}", "good", (node.get("ledger_hash") or "")[:24] + "…"),
        _card("Sync lag", f"{lag if lag is not None else '—'} ledgers", lag_cls, node.get("complete_ledgers") or ""),
        _card("Peers", str(peers), peer_cls),
        _card("Bond", str(bstat), bstat_cls, f"{bond.get('bonded_amount_qxrp') or '—'} qXRP bonded"),
        _card("Composite score", str(score), score_cls, "basis points / 10000"),
        _card("Reward accumulator", f"{bond.get('reward_accum_qxrp') or '—'}", "", "qXRP pending claim"),
        _card("Validator balance", f"{node.get('balance_qxrp') if node.get('balance_qxrp') is not None else '—'}", "", "qXRP"),
        _card("Uptime", f"{int((node.get('uptime_seconds') or 0) // 3600)}h", "", f"load ×{node.get('load_factor') or 1}"),
    ])

    cards_net = "".join([
        _card("Network ledger", f"#{int(net.get('ledger_seq') or 0):,}", "good", net.get("complete_ledgers") or ""),
        _card("Network state", str(net.get("server_state", "—")), ""),
        _card("Bonded validators", str(net.get("bonded_validator_count", 0)), "good",
              f"{net.get('total_validator_entries', 0)} on ledger"),
        _card("Epoch", str(epoch.get("epoch_number", "—")), "", epoch_txt),
    ])

    acct = node.get("validator_account") or "not configured"

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="15">
<title>qXRP Validator Dashboard</title>
<style>{_css()}</style>
</head>
<body>
<h1>qXRP Validator Dashboard</h1>
<p class="subtitle">
  <span class="pill">Node RPC: {RPC_URL}</span>
  &nbsp;·&nbsp; <span class="pill">Network: {NETWORK_RPC_URL}</span>
  &nbsp;·&nbsp; Auto-refresh 15s · Updated {s['updated_at']}
</p>

<div class="section">
  <h2>Your node</h2>
  <p class="mono" style="margin:-8px 0 14px">{acct}</p>
  <div class="grid">{cards_node}</div>
</div>

<div class="section">
  <h2>Network</h2>
  <div class="grid">{cards_net}</div>
</div>

<div class="section">
  <h2>All bonded validators</h2>
  <div class="card" style="padding:0; overflow:hidden">
    <table>
      <thead><tr><th>Account</th><th>Status</th><th>Bond qXRP</th><th>Score</th></tr></thead>
      <tbody>{val_rows or '<tr><td colspan="4">No validator bonds found</td></tr>'}</tbody>
    </table>
  </div>
</div>

<div class="footer">JSON API: <a href="/api/stats" style="color:var(--accent)">/api/stats</a> · Prometheus: <a href="/metrics" style="color:var(--accent)">/metrics</a></div>
</body>
</html>"""


@app.get("/metrics")
def metrics() -> Response:
    s = collect_stats()
    node = s["node"]
    bond = node.get("bond") or {}
    lines = [
        "# HELP qxrp_ledger_seq Current validated ledger sequence",
        "# TYPE qxrp_ledger_seq gauge",
        f"qxrp_ledger_seq {node.get('ledger_seq', 0)}",
        "# HELP qxrp_peers Connected peer count",
        "# TYPE qxrp_peers gauge",
        f"qxrp_peers {node.get('peers', 0)}",
        "# HELP qxrp_proposing 1 if validator is actively proposing",
        "# TYPE qxrp_proposing gauge",
        f"qxrp_proposing {1 if node.get('server_state') == 'proposing' else 0}",
        "# HELP qxrp_composite_score Validator composite score basis points",
        "# TYPE qxrp_composite_score gauge",
        f"qxrp_composite_score {bond.get('composite_score') or 0}",
    ]
    return Response(content="\n".join(lines) + "\n", media_type="text/plain")


@app.get("/health")
def health():
    info = rpc_local("server_info")
    if "error" in info:
        return JSONResponse(
            {"status": "unhealthy", "error": info["error"]},
            status_code=503,
        )
    return {"status": "ok"}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=LISTEN_PORT)