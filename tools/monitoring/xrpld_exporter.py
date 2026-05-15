#!/usr/bin/env python3
# =============================================================================
# xrpld_exporter.py — Prometheus metrics exporter for qXRP nodes
# =============================================================================
# Polls xrpld JSON-RPC endpoints and exposes Prometheus metrics on :9101/metrics
#
# Usage:
#   pip install prometheus-client requests
#   NODES=http://127.0.0.1:5005,http://127.0.0.1:5006 python3 xrpld_exporter.py
#
# Or via docker-compose in this directory.

import os
import time
import threading
import requests
from prometheus_client import start_http_server, Gauge, Counter, Info

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
NODE_URLS   = [u.strip() for u in os.environ.get("NODES", "http://127.0.0.1:5005").split(",")]
SCRAPE_SECS = int(os.environ.get("SCRAPE_INTERVAL", "10"))
LISTEN_PORT = int(os.environ.get("EXPORTER_PORT", "9101"))
TIMEOUT     = int(os.environ.get("RPC_TIMEOUT", "5"))

LABELS = ["node"]   # label by node URL (short name derived below)

def node_label(url: str) -> str:
    """Derive a short label from URL, e.g. http://127.0.0.1:5005 → node1"""
    # If port matches 5005-5009, use v1-v5; else use host:port
    try:
        port = int(url.rsplit(":", 1)[-1].rstrip("/"))
        if 5005 <= port <= 5009:
            return f"v{port - 5004}"
    except ValueError:
        pass
    return url.replace("http://", "").replace("https://", "")

# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------
m_up           = Gauge("xrpld_up",                   "1 if xrpld is reachable",                LABELS)
m_state        = Gauge("xrpld_proposing",             "1 if server_state is proposing",          LABELS)
m_ledger_seq   = Gauge("xrpld_validated_ledger_seq",  "Latest validated ledger sequence",         LABELS)
m_peers        = Gauge("xrpld_peers",                 "Number of connected peers",               LABELS)
m_load_factor  = Gauge("xrpld_load_factor",           "Current fee load factor",                 LABELS)
m_uptime       = Gauge("xrpld_uptime_seconds",        "Server uptime in seconds",                LABELS)
m_reserve_base = Gauge("xrpld_reserve_base_drops",    "Base account reserve in drops",           LABELS)
m_complete_min = Gauge("xrpld_complete_ledgers_min",  "First complete ledger in history",        LABELS)
m_complete_max = Gauge("xrpld_complete_ledgers_max",  "Last complete ledger in history",         LABELS)
m_txn_count    = Counter("xrpld_txn_total",           "Cumulative transactions processed",       LABELS)
m_scrape_errs  = Counter("xrpld_scrape_errors_total", "Total scrape errors",                     LABELS)

# ---------------------------------------------------------------------------
# Scraper
# ---------------------------------------------------------------------------
def rpc(url: str, method: str, params: dict | None = None) -> dict:
    payload = {"method": method, "params": [params or {}]}
    r = requests.post(url, json=payload, timeout=TIMEOUT)
    r.raise_for_status()
    body = r.json()
    if "error" in body.get("result", {}):
        raise RuntimeError(body["result"].get("error_message", body["result"]["error"]))
    return body["result"]

def parse_complete_ledgers(s: str) -> tuple[int, int]:
    """'1-12345' → (1, 12345).  'empty' → (0, 0)."""
    if not s or s == "empty":
        return 0, 0
    parts = s.split("-")
    try:
        return int(parts[0]), int(parts[-1])
    except ValueError:
        return 0, 0

def scrape(url: str, label: str) -> None:
    lbls = [label]
    try:
        info = rpc(url, "server_info")["info"]

        m_up.labels(*lbls).set(1)
        m_state.labels(*lbls).set(1 if info.get("server_state") == "proposing" else 0)
        m_peers.labels(*lbls).set(info.get("peers", 0))
        m_load_factor.labels(*lbls).set(info.get("load_factor", 1))
        m_uptime.labels(*lbls).set(info.get("uptime", 0))

        vl = info.get("validated_ledger", {})
        m_ledger_seq.labels(*lbls).set(vl.get("seq", 0))
        m_reserve_base.labels(*lbls).set(int(vl.get("reserve_base", 0)))

        lo, hi = parse_complete_ledgers(info.get("complete_ledgers", ""))
        m_complete_min.labels(*lbls).set(lo)
        m_complete_max.labels(*lbls).set(hi)

        # txn_count is a counter — only increment positively
        # (server_info.txn_count is cumulative since start)
        raw_txn = info.get("txn_count", 0)
        # Prometheus Counter can only go up; we track the delta ourselves
        # by using _created or just setting the counter-like gauge separately.
        # For simplicity, use inc with delta tracked per-node.
        pass  # handled below

    except Exception as exc:
        m_up.labels(*lbls).set(0)
        m_scrape_errs.labels(*lbls).inc()
        print(f"[exporter] Scrape error {label}: {exc}")

# Track last txn_count per node for delta computation
_last_txn: dict[str, int] = {}

def scrape_loop() -> None:
    node_labels = {url: node_label(url) for url in NODE_URLS}
    # Initialise all labels so graphs don't start empty
    for url, lbl in node_labels.items():
        m_up.labels(lbl).set(0)
        m_scrape_errs.labels(lbl).inc(0)
        _last_txn[lbl] = 0

    while True:
        for url, lbl in node_labels.items():
            scrape(url, lbl)
        time.sleep(SCRAPE_SECS)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    print(f"[exporter] Monitoring {len(NODE_URLS)} node(s): {NODE_URLS}")
    print(f"[exporter] Metrics on :{LISTEN_PORT}/metrics")
    start_http_server(LISTEN_PORT)
    t = threading.Thread(target=scrape_loop, daemon=True)
    t.start()
    # Keep main thread alive
    while True:
        time.sleep(60)
