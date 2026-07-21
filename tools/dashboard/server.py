# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# qXRP Validator Dashboard — live network metrics + 24h history charts

from __future__ import annotations

import json
import os
import threading
import time
from collections import deque
from pathlib import Path
from typing import Any, Deque, Dict, List

import httpx
import uvicorn
from fastapi import FastAPI, Response
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, JSONResponse

RPC_URL = os.environ.get("XRPLD_RPC_URL", "http://127.0.0.1:5005")
NETWORK_RPC_URL = os.environ.get("NETWORK_RPC_URL", "http://46.224.0.140:6005")
LISTEN_PORT = int(os.environ.get("DASHBOARD_PORT", "8080"))
VALIDATOR_ACCOUNT = os.environ.get("VALIDATOR_ACCOUNT", "")
TRAFFIC_STATS_FILE = os.environ.get("TRAFFIC_STATS_FILE", "/var/lib/qxrp-traffic/stats.json")
HISTORY_FILE = os.environ.get("METRICS_HISTORY_FILE", "/var/lib/qxrp-dashboard/history.json")
HISTORY_SECONDS = int(os.environ.get("METRICS_HISTORY_SECONDS", str(24 * 3600)))
POLL_INTERVAL = float(os.environ.get("METRICS_POLL_INTERVAL", "15"))

DROPS_PER_QXRP = 1_000_000

app = FastAPI(title="qXRP Validator Dashboard", docs_url=None, redoc_url=None)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET"],
    allow_headers=["*"],
)

_history_lock = threading.Lock()
_history: Deque[Dict[str, Any]] = deque(maxlen=max(2880, int(HISTORY_SECONDS / POLL_INTERVAL)))
_prev_ledger_seq: int | None = None
_prev_ledger_ts: float | None = None


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


def read_traffic_stats() -> Dict[str, Any]:
    path = Path(TRAFFIC_STATS_FILE)
    if not path.is_file():
        return {}
    try:
        return json.loads(path.read_text())
    except Exception:
        return {}


def collect_stats() -> Dict[str, Any]:
    local_info = rpc_local("server_info").get("info", {})
    net_info = rpc_network("server_info").get("info", {})

    # Prefer validated; Falcon joiners often only expose closed_ledger.
    local_vl = local_info.get("validated_ledger") or local_info.get("closed_ledger") or {}
    net_vl = net_info.get("validated_ledger") or net_info.get("closed_ledger") or {}

    # Bond/balance always from public network (joiners may lack local ledger APIs).
    bond = fetch_bond(VALIDATOR_ACCOUNT, rpc_network) if VALIDATOR_ACCOUNT else {}
    if not bond and VALIDATOR_ACCOUNT:
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

    traffic = read_traffic_stats()

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
        "traffic": traffic,
    }


def _sample_point(stats: Dict[str, Any]) -> Dict[str, Any]:
    global _prev_ledger_seq, _prev_ledger_ts
    now = time.time()
    net_seq = int(stats.get("network", {}).get("ledger_seq") or 0)
    ledger_rate = 0.0
    if _prev_ledger_seq is not None and _prev_ledger_ts and net_seq > _prev_ledger_seq:
        dt = max(0.001, now - _prev_ledger_ts)
        ledger_rate = (net_seq - _prev_ledger_seq) / dt * 60.0
    _prev_ledger_seq = net_seq
    _prev_ledger_ts = now

    traffic = stats.get("traffic") or {}
    return {
        "t": int(now),
        "ledger_seq": net_seq,
        "node_ledger_seq": int(stats.get("node", {}).get("ledger_seq") or 0),
        "ledger_lag": stats.get("node", {}).get("ledger_lag"),
        "peers": int(stats.get("node", {}).get("peers") or 0),
        "net_peers": int(stats.get("network", {}).get("peers") or 0),
        "load_factor": float(stats.get("network", {}).get("load_factor") or 1),
        "ledger_rate_per_min": round(ledger_rate, 2),
        "tx_per_min": float(traffic.get("tx_per_min") or 0),
        "traffic_submitted": int(traffic.get("submitted") or 0),
        "traffic_validated": int(traffic.get("validated") or 0),
        "bonded_validators": int(stats.get("network", {}).get("bonded_validator_count") or 0),
        "composite_score": int((stats.get("node", {}).get("bond") or {}).get("composite_score") or 0),
    }


def _load_history() -> None:
    path = Path(HISTORY_FILE)
    if not path.is_file():
        return
    try:
        rows = json.loads(path.read_text())
        cutoff = time.time() - HISTORY_SECONDS
        with _history_lock:
            _history.clear()
            for row in rows:
                if row.get("t", 0) >= cutoff:
                    _history.append(row)
    except Exception:
        pass


def _save_history() -> None:
    path = Path(HISTORY_FILE)
    path.parent.mkdir(parents=True, exist_ok=True)
    with _history_lock:
        path.write_text(json.dumps(list(_history)))


def _collector_loop() -> None:
    while True:
        try:
            stats = collect_stats()
            point = _sample_point(stats)
            with _history_lock:
                _history.append(point)
            if len(_history) % 4 == 0:
                _save_history()
        except Exception:
            pass
        time.sleep(POLL_INTERVAL)


@app.on_event("startup")
def _startup() -> None:
    _load_history()
    t = threading.Thread(target=_collector_loop, daemon=True, name="metrics-collector")
    t.start()


@app.get("/api/stats")
def api_stats() -> JSONResponse:
    return JSONResponse(collect_stats())


@app.get("/api/history")
def api_history(metric: str | None = None) -> JSONResponse:
    cutoff = time.time() - HISTORY_SECONDS
    with _history_lock:
        rows = [r for r in _history if r.get("t", 0) >= cutoff]
    if metric:
        rows = [{"t": r["t"], "v": r.get(metric)} for r in rows if metric in r]
    return JSONResponse({"metric": metric, "points": rows, "seconds": HISTORY_SECONDS})


@app.get("/", response_class=HTMLResponse)
def index() -> str:
    return _HTML_PAGE


_HTML_PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>qXRP Network Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
<style>
:root {
  --bg:#070b12; --panel:#0f1623; --card:#131d2e; --border:#243044;
  --text:#e6edf7; --muted:#8ba3bf; --accent:#4cc9f0; --good:#3dd68c;
  --warn:#f5b942; --bad:#ff6b6b; --glow:rgba(76,201,240,.35);
}
* { box-sizing:border-box; }
body { margin:0; font-family:Inter,Segoe UI,system-ui,sans-serif; background:radial-gradient(1200px 600px at 10% -10%, #132038 0%, var(--bg) 55%); color:var(--text); }
header { padding:24px 28px 8px; display:flex; justify-content:space-between; align-items:flex-end; gap:16px; flex-wrap:wrap; }
h1 { margin:0; font-size:1.55rem; letter-spacing:-.02em; }
.sub { color:var(--muted); font-size:.85rem; }
.live-pill { display:inline-flex; align-items:center; gap:8px; padding:6px 12px; border-radius:999px; background:#102018; border:1px solid #1f4d38; color:var(--good); font-size:.75rem; font-weight:600; }
.live-dot { width:8px; height:8px; border-radius:50%; background:var(--good); box-shadow:0 0 12px var(--good); animation:pulse 1.6s infinite; }
@keyframes pulse { 0%,100%{opacity:1;transform:scale(1)} 50%{opacity:.45;transform:scale(.85)} }
.wrap { padding:8px 16px 32px; }
@media (min-width:640px){ .wrap { padding:8px 28px 32px; } }
.section-title { font-size:.72rem; text-transform:uppercase; letter-spacing:.12em; color:var(--muted); margin:18px 0 12px; font-weight:700; }
.grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(min(100%,210px),1fr)); gap:14px; }
.card { background:linear-gradient(180deg,#162236 0%,var(--card) 100%); border:1px solid var(--border); border-radius:14px; padding:16px 16px 14px; cursor:pointer; -webkit-tap-highlight-color:transparent; touch-action:manipulation; transition:transform .15s,border-color .15s,box-shadow .15s; position:relative; overflow:hidden; user-select:none; }
.card:hover,.card:active { transform:translateY(-2px); border-color:#3a5578; box-shadow:0 8px 28px rgba(0,0,0,.35), 0 0 0 1px rgba(76,201,240,.08); }
.card::after { content:''; position:absolute; inset:auto -30% -60% auto; width:120px; height:120px; background:radial-gradient(circle, var(--glow), transparent 70%); pointer-events:none; opacity:.5; }
.label { font-size:.68rem; color:var(--muted); text-transform:uppercase; letter-spacing:.08em; margin-bottom:8px; }
.value { font-size:1.55rem; font-weight:800; line-height:1.1; }
.value.good{color:var(--good)} .value.warn{color:var(--warn)} .value.bad{color:var(--bad)}
.spark { height:34px; margin-top:10px; opacity:.85; }
.hint { font-size:.68rem; color:var(--muted); margin-top:6px; }
.panel { background:var(--panel); border:1px solid var(--border); border-radius:14px; padding:0; overflow:hidden; }
table { width:100%; border-collapse:collapse; font-size:.82rem; }
th,td { text-align:left; padding:10px 14px; border-bottom:1px solid var(--border); }
th { color:var(--muted); font-size:.68rem; text-transform:uppercase; letter-spacing:.06em; }
.mono { font-family:ui-monospace,Menlo,monospace; font-size:.78rem; color:var(--muted); word-break:break-all; }
.modal { display:none; position:fixed; inset:0; background:rgba(4,8,14,.78); backdrop-filter:blur(4px); z-index:50; align-items:center; justify-content:center; padding:20px; }
.modal.open { display:flex; }
.modal-box { width:min(920px,100%); background:#0c1420; border:1px solid var(--border); border-radius:16px; padding:20px 22px 18px; box-shadow:0 24px 80px rgba(0,0,0,.55); }
.modal-head { display:flex; justify-content:space-between; align-items:center; margin-bottom:14px; }
.modal-head h3 { margin:0; font-size:1.1rem; }
.close { background:#1a2738; border:1px solid var(--border); color:var(--text); border-radius:8px; padding:6px 12px; cursor:pointer; }
.chart-box { height:320px; min-height:220px; position:relative; }
.chart-fallback { font-size:.8rem; color:var(--muted); padding:12px 0; max-height:280px; overflow:auto; }
@media (max-width:640px){
  header { padding:16px 16px 8px; }
  h1 { font-size:1.25rem; }
  .modal { padding:0; align-items:stretch; }
  .modal.open { display:flex; }
  .modal-box { width:100%; max-width:100%; height:100%; border-radius:0; border-left:none; border-right:none; display:flex; flex-direction:column; }
  .chart-box { flex:1; height:auto; min-height:50vh; }
}
body.modal-open { overflow:hidden; position:fixed; width:100%; }
.footer { margin-top:22px; color:var(--muted); font-size:.75rem; }
a { color:var(--accent); text-decoration:none; }
.badge { display:inline-block; padding:2px 8px; border-radius:999px; font-size:.68rem; border:1px solid var(--border); background:#101a28; color:var(--muted); }
</style>
</head>
<body>
<header>
  <div>
    <h1>qXRP Network Dashboard</h1>
    <div class="sub" id="subtitle">Loading network…</div>
  </div>
  <div class="live-pill"><span class="live-dot"></span><span id="liveLabel">LIVE</span></div>
</header>
<div class="wrap">
  <div class="section-title">Node health · click any tile for 24h chart</div>
  <div class="grid" id="nodeGrid"></div>

  <div class="section-title">Network activity</div>
  <div class="grid" id="netGrid"></div>

  <div class="section-title">Traffic generator</div>
  <div class="grid" id="trafficGrid"></div>

  <div class="section-title">Bonded validators</div>
  <div class="panel">
    <table>
      <thead><tr><th>Account</th><th>Status</th><th>Bond</th><th>Score</th></tr></thead>
      <tbody id="valTable"></tbody>
    </table>
  </div>
  <div class="footer">API: <a href="/api/stats">/api/stats</a> · <a href="/api/history">/api/history</a> · <a href="/metrics">/metrics</a></div>
</div>

<div class="modal" id="modal">
  <div class="modal-box">
    <div class="modal-head">
      <h3 id="modalTitle">Metric</h3>
      <button class="close" onclick="closeModal()">Close</button>
    </div>
    <div class="chart-box"><canvas id="modalChart"></canvas><div id="chartFallback" class="chart-fallback" hidden></div></div>
  </div>
</div>

<script>
const METRICS = {
  ledger_seq: { title: 'Network ledger sequence', color: '#4cc9f0' },
  node_ledger_seq: { title: 'Node ledger sequence', color: '#72efdd' },
  ledger_lag: { title: 'Sync lag (ledgers)', color: '#f5b942' },
  peers: { title: 'Node peers', color: '#3dd68c' },
  net_peers: { title: 'Network peers', color: '#95e06c' },
  load_factor: { title: 'Load factor', color: '#ff9f1c' },
  ledger_rate_per_min: { title: 'Ledger close rate (/min)', color: '#4895ef' },
  tx_per_min: { title: 'Traffic tx rate (/min)', color: '#f72585' },
  traffic_validated: { title: 'Cumulative validated txs', color: '#b5179e' },
  bonded_validators: { title: 'Bonded validators', color: '#560bad' },
  composite_score: { title: 'Composite score', color: '#4cc9f0' },
};

let modalChart = null;
let lastLedger = 0;
let tileSeq = 0;
let historyCache = [];

function cls(state, good, warn) {
  if (good.includes(state)) return 'good';
  if (warn.includes(state)) return 'warn';
  return '';
}

function tile(id, label, value, sub, metric, valueClass='') {
  const tid = 't' + (++tileSeq);
  return `<div class="card" data-metric="${metric}" data-label="${label.replace(/"/g,'')}" data-tile="${tid}" role="button" tabindex="0" aria-label="${label} chart">
    <div class="label">${label}</div>
    <div class="value ${valueClass}" id="${id}">${value}</div>
    <canvas class="spark" data-spark="${metric}" data-tile="${tid}"></canvas>
    <div class="hint">${sub} · tap for 24h</div>
  </div>`;
}

function fmtUptime(sec) {
  sec = Math.max(0, Math.floor(Number(sec) || 0));
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (h <= 0) return m + 'm';
  if (m <= 0) return h + 'h';
  return h + 'h ' + m + 'm';
}

function drawSpark(canvas, points, color) {
  if (!canvas || !points.length) return;
  const cssW = canvas.clientWidth || canvas.parentElement?.clientWidth || 180;
  if (cssW < 2) return;
  const ctx = canvas.getContext('2d');
  const ratio = Math.min(window.devicePixelRatio || 1, 2);
  const w = canvas.width = Math.floor(cssW * ratio);
  const h = canvas.height = Math.floor(34 * ratio);
  const vals = points.map(p => p.v ?? 0);
  const min = Math.min(...vals), max = Math.max(...vals);
  const span = Math.max(max - min, 1);
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle = color;
  ctx.lineWidth = 2 * ratio;
  ctx.beginPath();
  vals.forEach((v,i) => {
    const x = (i / Math.max(vals.length-1,1)) * (w-8) + 4;
    const y = h - 4 - ((v - min) / span) * (h-10);
    i ? ctx.lineTo(x,y) : ctx.moveTo(x,y);
  });
  ctx.stroke();
}

function historyForMetric(metric) {
  return historyCache.map(p => ({ t: p.t, v: p[metric] }));
}

async function refresh() {
  const [stats, histAll] = await Promise.all([
    fetch('/api/stats').then(r => r.json()),
    fetch('/api/history').then(r => r.json()),
  ]);
  historyCache = histAll.points || [];
  const histMap = {};
  Object.keys(METRICS).forEach((m) => { histMap[m] = historyForMetric(m); });

  const node = stats.node || {};
  const net = stats.network || {};
  const traffic = stats.traffic || {};
  const bond = node.bond || {};

  const ledger = Number(net.ledger_seq || 0);
  if (ledger > lastLedger) {
    document.getElementById('liveLabel').textContent = 'LIVE +' + (ledger - lastLedger);
    lastLedger = ledger;
  }
  document.getElementById('subtitle').innerHTML =
    `<span class="badge">${stats.updated_at || ''}</span> &nbsp; Node: <span class="mono">${node.validator_account || 'n/a'}</span>`;

  const nodeCards = [
    tile('v_state', 'Server state', node.server_state || '—', node.validation_pubkey ? node.validation_pubkey.slice(0,24)+'…' : '', 'peers', cls(node.server_state, ['proposing'],['full','connected'])),
    tile('v_ledger', 'Node ledger', '#' + Number(node.ledger_seq||0).toLocaleString(), (node.ledger_hash||'').slice(0,20)+'…', 'node_ledger_seq', 'good'),
    tile('v_lag', 'Sync lag', (node.ledger_lag ?? '—') + ' ledgers', node.complete_ledgers || '', 'ledger_lag', (node.ledger_lag||0) <= 5 ? 'good' : 'warn'),
    tile('v_peers', 'Peers', String(node.peers ?? '—'), 'P2P connections', 'peers', (node.peers||0) >= 3 ? 'good' : 'warn'),
    tile('v_bond', 'Bond status', bond.status || '—', (bond.bonded_amount_qxrp || '—') + ' qXRP', 'bonded_validators', bond.status === 'bonded' ? 'good' : 'warn'),
    tile('v_score', 'Composite score', bond.composite_score ?? '—', 'basis points', 'composite_score', (bond.composite_score||0) >= 5000 ? 'good' : 'warn'),
    tile('v_bal', 'Balance', (node.balance_qxrp ?? '—') + ' qXRP', 'validator account', 'ledger_rate_per_min'),
    tile('v_uptime', 'Uptime', fmtUptime(node.uptime_seconds), 'load ×' + (node.load_factor||1), 'load_factor'),
  ];
  document.getElementById('nodeGrid').innerHTML = nodeCards.join('');

  const netCards = [
    tile('n_ledger', 'Network ledger', '#' + ledger.toLocaleString(), net.complete_ledgers || '', 'ledger_seq', 'good'),
    tile('n_state', 'Network state', net.server_state || '—', net.rpc || '', 'net_peers'),
    tile('n_validators', 'Bonded validators', String(net.bonded_validator_count||0), (net.total_validator_entries||0) + ' on ledger', 'bonded_validators', 'good'),
    tile('n_load', 'Load factor', String(net.load_factor || 1), 'network pressure', 'load_factor'),
    tile('n_rate', 'Ledger rate', '…', 'closes per minute', 'ledger_rate_per_min', 'good'),
  ];
  document.getElementById('netGrid').innerHTML = netCards.join('');

  const pump = traffic.in_pump ? '<span class="badge" style="color:var(--warn)">PUMP</span>' : 'baseline';
  const trafficCards = [
    tile('t_rate', 'Tx rate', (traffic.tx_per_min ?? 0) + '/min', pump, 'tx_per_min', (traffic.tx_per_min||0) > 0 ? 'good' : ''),
    tile('t_val', 'Validated txs', String(traffic.validated ?? 0), 'submitted ' + (traffic.submitted ?? 0), 'traffic_validated'),
    tile('t_wallets', 'Load wallets', String(traffic.wallets ?? '—'), '5 per server × 6 hosts', 'tx_per_min'),
    tile('t_refill', 'Refills', String(traffic.refills ?? 0), 'auto top-up from genesis', 'traffic_validated'),
  ];
  document.getElementById('trafficGrid').innerHTML = trafficCards.join('');

  const rows = (net.validators || []).map(v => `<tr>
    <td class="mono">${(v.account||'').slice(0,18)}…</td>
    <td>${v.bond_status}</td>
    <td>${v.bonded_amount_qxrp ?? '—'}</td>
    <td>${v.composite_score ?? '—'}</td>
  </tr>`).join('');
  document.getElementById('valTable').innerHTML = rows || '<tr><td colspan="4">No validators</td></tr>';

  document.querySelectorAll('canvas.spark').forEach((c) => {
    const m = c.dataset.spark;
    const cfg = METRICS[m] || { color: '#4cc9f0' };
    drawSpark(c, (histMap[m] || []).slice(-40), cfg.color);
  });
  requestAnimationFrame(() => {
    document.querySelectorAll('canvas.spark').forEach((c) => {
      const m = c.dataset.spark;
      const cfg = METRICS[m] || { color: '#4cc9f0' };
      drawSpark(c, (histMap[m] || []).slice(-40), cfg.color);
    });
  });

  const ratePts = histMap.ledger_rate_per_min || [];
  const lastRate = ratePts.length ? ratePts[ratePts.length-1].v : 0;
  const el = document.getElementById('n_rate');
  if (el) el.textContent = (lastRate || 0).toFixed(1) + '/min';
}

function renderChartFallback(points, cfg) {
  const el = document.getElementById('chartFallback');
  const canvas = document.getElementById('modalChart');
  canvas.hidden = true;
  el.hidden = false;
  if (!points.length) {
    el.textContent = 'Collecting data — check back in a few minutes.';
    return;
  }
  const rows = points.slice(-12).map(p => {
    const t = new Date(p.t * 1000).toLocaleString();
    return `${t}: ${p.v ?? 0}`;
  });
  el.innerHTML = '<strong>' + cfg.title + '</strong><br>' + rows.join('<br>');
}

function openChart(metric, label) {
  const cfg = METRICS[metric] || { title: label, color: '#4cc9f0' };
  const points = historyForMetric(metric);
  document.getElementById('modalTitle').textContent = cfg.title + ' · last 24h';
  document.getElementById('modal').classList.add('open');
  document.body.classList.add('modal-open');
  const canvas = document.getElementById('modalChart');
  const fallback = document.getElementById('chartFallback');
  canvas.hidden = false;
  fallback.hidden = true;
  if (modalChart) { modalChart.destroy(); modalChart = null; }
  if (typeof Chart === 'undefined') {
    renderChartFallback(points, cfg);
    return;
  }
  const labels = points.map(p => new Date(p.t * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }));
  const data = points.map(p => p.v ?? 0);
  requestAnimationFrame(() => {
    modalChart = new Chart(canvas, {
      type: 'line',
      data: { labels, datasets: [{ label: cfg.title, data, borderColor: cfg.color, backgroundColor: cfg.color + '33', fill: true, tension: .25, pointRadius: 0, borderWidth: 2 }] },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: { duration: 200 },
        plugins: { legend: { display: false } },
        interaction: { mode: 'index', intersect: false },
        scales: {
          x: { ticks: { maxTicksLimit: window.innerWidth < 640 ? 5 : 8, color: '#8ba3bf', maxRotation: 0 }, grid: { color: '#1b2838' } },
          y: { ticks: { color: '#8ba3bf' }, grid: { color: '#1b2838' } }
        }
      }
    });
    setTimeout(() => { if (modalChart) modalChart.resize(); }, 80);
  });
}

function closeModal() {
  document.getElementById('modal').classList.remove('open');
  document.body.classList.remove('modal-open');
}

function onCardActivate(e) {
  const card = e.target.closest('.card[data-metric]');
  if (!card) return;
  e.preventDefault();
  openChart(card.dataset.metric, card.dataset.label || card.dataset.metric);
}

document.getElementById('modal').addEventListener('click', e => { if (e.target.id === 'modal') closeModal(); });
document.addEventListener('click', onCardActivate);
document.addEventListener('keydown', e => {
  if (e.key === 'Escape') closeModal();
  if (e.key === 'Enter' && e.target.closest('.card[data-metric]')) onCardActivate(e);
});

refresh();
setInterval(refresh, 15000);
window.addEventListener('resize', () => { if (modalChart) modalChart.resize(); });
</script>
</body>
</html>"""


@app.get("/metrics")
def metrics() -> Response:
    s = collect_stats()
    node = s["node"]
    bond = node.get("bond") or {}
    traffic = s.get("traffic") or {}
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
        "# HELP qxrp_traffic_tx_per_min Traffic generator tx per minute",
        "# TYPE qxrp_traffic_tx_per_min gauge",
        f"qxrp_traffic_tx_per_min {traffic.get('tx_per_min', 0)}",
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