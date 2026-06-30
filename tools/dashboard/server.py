# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# qXRP Validator Dashboard
# A stateless HTTP server that reads validator state via local xrpld RPC.
#
# Usage:
#   pip install fastapi uvicorn httpx
#   XRPLD_RPC_URL=http://127.0.0.1:5005 python3 tools/dashboard/server.py
#
# Then open: http://localhost:8080

import json
import os
import time
from typing import Any, Dict

import httpx
import uvicorn
from fastapi import FastAPI, Response
from fastapi.responses import HTMLResponse

RPC_URL = os.environ.get("XRPLD_RPC_URL", "http://127.0.0.1:5005")
LISTEN_PORT = int(os.environ.get("DASHBOARD_PORT", "8080"))
VALIDATOR_ACCOUNT = os.environ.get("VALIDATOR_ACCOUNT", "")

app = FastAPI(title="qXRP Validator Dashboard", docs_url=None, redoc_url=None)

# ---------------------------------------------------------------------------
# RPC helpers
# ---------------------------------------------------------------------------
def rpc(method: str, params: Dict[str, Any] | None = None) -> Dict[str, Any]:
    payload = {"method": method, "params": [params or {}]}
    try:
        r = httpx.post(RPC_URL, json=payload, timeout=5.0)
        r.raise_for_status()
        return r.json().get("result", {})
    except Exception as exc:
        return {"error": str(exc)}


def account_info(account: str) -> Dict[str, Any]:
    return rpc("account_info", {"account": account, "ledger_index": "validated"})


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------
@app.get("/", response_class=HTMLResponse)
def index() -> str:
    info = rpc("server_info").get("info", {})
    vl = info.get("validated_ledger", {})
    pubkey = info.get("pubkey_validator", "N/A")
    server_state = info.get("server_state", "unknown")
    peers = info.get("peers", 0)
    ledger_seq = vl.get("seq", 0)
    ledger_hash = vl.get("hash", "")

    # Bond / reputation data (validator r-address from VALIDATOR_ACCOUNT env)
    bond_obj: Dict[str, Any] = {}
    if VALIDATOR_ACCOUNT:
        bond_data = rpc("ledger_entry", {
            "validator_bond": {"account": VALIDATOR_ACCOUNT},
            "ledger_index": "validated",
        })
        bond_obj = bond_data.get("node", {})

    composite_score = bond_obj.get("CompositeScore", "N/A")
    raw_bond_status = bond_obj.get("BondStatus", "N/A")
    bond_status = (
        {0: "registered", 1: "bonded"}.get(raw_bond_status, str(raw_bond_status))
        if isinstance(raw_bond_status, int)
        else raw_bond_status
    )
    bond_amount = bond_obj.get("BondedAmount", bond_obj.get("BondAmount", "N/A"))

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="10">
<title>qXRP Validator Dashboard</title>
<style>
  body {{ font-family: 'Segoe UI', Arial, sans-serif; background: #0d1117; color: #c9d1d9; margin: 0; padding: 24px; }}
  h1 {{ color: #58a6ff; margin-bottom: 4px; }}
  .subtitle {{ color: #8b949e; font-size: 0.9em; margin-bottom: 32px; }}
  .grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 16px; }}
  .card {{ background: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 20px; }}
  .card h2 {{ font-size: 0.85em; color: #8b949e; text-transform: uppercase; letter-spacing: 1px; margin: 0 0 12px; }}
  .value {{ font-size: 1.4em; font-weight: 600; color: #f0f6fc; }}
  .value.good {{ color: #3fb950; }}
  .value.warn {{ color: #d29922; }}
  .value.bad  {{ color: #f85149; }}
  .mono {{ font-family: monospace; font-size: 0.8em; color: #8b949e; word-break: break-all; margin-top: 6px; }}
  .footer {{ margin-top: 32px; color: #8b949e; font-size: 0.8em; }}
</style>
</head>
<body>
<h1>qXRP Validator</h1>
<p class="subtitle">Auto-refreshes every 10 seconds &nbsp;·&nbsp; RPC: {RPC_URL}</p>
<div class="grid">
  <div class="card">
    <h2>Server State</h2>
    <div class="value {'good' if server_state == 'proposing' else 'warn'}">{server_state}</div>
    <div class="mono">{pubkey[:20]}…{pubkey[-8:] if len(pubkey) > 28 else ''}</div>
  </div>
  <div class="card">
    <h2>Ledger</h2>
    <div class="value good">#{ledger_seq:,}</div>
    <div class="mono">{ledger_hash[:24]}…</div>
  </div>
  <div class="card">
    <h2>Peers</h2>
    <div class="value {'good' if int(peers) >= 3 else 'warn'}">{peers}</div>
  </div>
  <div class="card">
    <h2>Bond Status</h2>
    <div class="value {'good' if bond_status == 'bonded' else 'warn'}">{bond_status}</div>
    <div class="mono">Amount: {bond_amount}</div>
  </div>
  <div class="card">
    <h2>Composite Score</h2>
    <div class="value {'good' if str(composite_score).isdigit() and int(composite_score) >= 7000 else 'warn'}">{composite_score}</div>
    <div class="mono">out of 10000 (basis points)</div>
  </div>
</div>
<div class="footer">Updated: {time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime())}</div>
</body>
</html>"""
    return html


@app.get("/metrics")
def metrics() -> Response:
    """Prometheus exposition format endpoint for Grafana integration."""
    info = rpc("server_info").get("info", {})
    vl = info.get("validated_ledger", {})
    peers = info.get("peers", 0)
    ledger_seq = vl.get("seq", 0)
    server_state = info.get("server_state", "unknown")
    is_proposing = 1 if server_state == "proposing" else 0

    lines = [
        "# HELP qxrp_ledger_seq Current validated ledger sequence",
        "# TYPE qxrp_ledger_seq gauge",
        f"qxrp_ledger_seq {ledger_seq}",
        "# HELP qxrp_peers Connected peer count",
        "# TYPE qxrp_peers gauge",
        f"qxrp_peers {peers}",
        "# HELP qxrp_proposing 1 if validator is actively proposing",
        "# TYPE qxrp_proposing gauge",
        f"qxrp_proposing {is_proposing}",
    ]
    return Response(content="\n".join(lines) + "\n", media_type="text/plain")


@app.get("/health")
def health() -> Dict[str, str]:
    info = rpc("server_info")
    if "error" in info:
        return Response(
            content=json.dumps({"status": "unhealthy", "error": info["error"]}),
            status_code=503,
            media_type="application/json",
        )
    return {"status": "ok"}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=LISTEN_PORT)
