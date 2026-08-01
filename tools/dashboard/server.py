# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# qXRP Validator Dashboard — live network metrics + 24h history charts

from __future__ import annotations

import json
import os
import shutil
import threading
import time
from collections import deque
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any, Deque, Dict, List, Optional, Tuple

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
# Traffic generator is internal load-test only. Off by default so public
# validator dashboards do not show pump/refill metrics.
SHOW_TRAFFIC = os.environ.get("SHOW_TRAFFIC", "0").strip().lower() in (
    "1",
    "true",
    "yes",
    "on",
)
HISTORY_FILE = os.environ.get("METRICS_HISTORY_FILE", "/var/lib/qxrp-dashboard/history.json")
# Persistent on-ledger tx index (sum of txs in every closed ledger).
# Default: sit next to metrics history (container mount is usually /data).
_DATA_DIR = Path(HISTORY_FILE).expanduser().resolve().parent
TX_INDEX_FILE = os.environ.get(
    "TX_INDEX_FILE",
    str(_DATA_DIR / "tx_index.json"),
)
# Host-written capacity stats (disk/ledger size). Safe public ops signal.
HOST_STATS_FILE = os.environ.get(
    "HOST_STATS_FILE",
    str(_DATA_DIR / "host_stats.json"),
)
# Optional mount of node data dir (nudb/db) for live size without host agent.
LEDGER_DATA_DIR = os.environ.get("LEDGER_DATA_DIR", "").strip()
# Full-history dashboard that owns the chain-wide tx index (validators pull from here).
FULL_DASH_URL = os.environ.get("FULL_DASH_URL", "http://46.224.0.140:8080").rstrip("/")
HISTORY_SECONDS = int(os.environ.get("METRICS_HISTORY_SECONDS", str(24 * 3600)))
POLL_INTERVAL = float(os.environ.get("METRICS_POLL_INTERVAL", "15"))
TX_SCAN_WORKERS = int(os.environ.get("TX_SCAN_WORKERS", "24"))
TX_SCAN_BATCH = int(os.environ.get("TX_SCAN_BATCH", "500"))
# Only the full-history (non-validator) dashboard walks every ledger by default.
# Validators mirror totals from FULL_DASH_URL so we do not 5×-hammer network RPC.
_TX_SCAN_DEFAULT = "0" if os.environ.get("VALIDATOR_ACCOUNT", "").strip() else "1"
TX_SCAN_ENABLE = os.environ.get("TX_SCAN_ENABLE", _TX_SCAN_DEFAULT).strip().lower() in (
    "1", "true", "yes", "on",
)
# Host disk / memory / ledger size on public operator dashboards (not secrets).
SHOW_HOST_METRICS = os.environ.get("SHOW_HOST_METRICS", "1").strip().lower() in (
    "1", "true", "yes", "on",
)
# Expected Falcon testnet pin (mainnet-v2). Shown in header; joiners must match.
EXPECTED_IMAGE_TAG = os.environ.get("EXPECTED_XRPLD_TAG", "qxrp/xrpld:mainnet-v2")
EXPECTED_IMAGE_DIGEST = os.environ.get(
    "EXPECTED_XRPLD_DIGEST",
    "sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5",
)
EXPECTED_REVISION = os.environ.get("EXPECTED_XRPLD_REVISION", "b007db22d")
# XRPL empty transaction tree hash — ledger with no txs.
_EMPTY_TX_HASH = "0" * 64

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

# On-ledger transaction index: sum(len(txs)) for every closed ledger from
# genesis..counted_through, advanced by a background scanner against the
# full-history network RPC. Not "what this process happened to see".
_tx_lock = threading.Lock()
_tx_start_seq: int = 1          # first ledger we count from (raised if early missing)
_tx_counted_through: int = 0    # contiguous prefix fully counted
_tx_total: int = 0              # sum of txs in ledgers [_tx_start_seq .. _tx_counted_through]
_tx_tip_seq: int = 0            # latest validated ledger known
_tx_last_ledger_txs: int = 0
_tx_per_sec: float = 0.0
_tx_rate_prev_seq: int | None = None
_tx_rate_prev_ts: float | None = None
_tx_scan_active: bool = False
_tx_scan_error: str | None = None
_tx_index_dirty: bool = False


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


def fetch_bonded_validators(page_limit: int = 100) -> List[Dict[str, Any]]:
    """All ValidatorBond objects — paginate until the marker is exhausted."""
    out: List[Dict[str, Any]] = []
    marker: Any = None
    for _ in range(64):  # safety cap
        params: Dict[str, Any] = {
            "ledger_index": "validated",
            "type": "validator_bond",
            "limit": page_limit,
        }
        if marker is not None:
            params["marker"] = marker
        data = rpc_network("ledger_data", params)
        if data.get("error"):
            break
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
        marker = data.get("marker")
        if marker is None:
            break
    # Highest score first
    out.sort(
        key=lambda v: (
            -(v.get("composite_score") or 0),
            str(v.get("account") or ""),
        )
    )
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


def _ledger_tx_count(seq: int) -> Tuple[int, int]:
    """Return (seq, n_txs) for one ledger via network full-history RPC.

    Empty ledgers have transaction_hash all zeros — skip expanding them.
    """
    header = rpc_network("ledger", {"ledger_index": int(seq)})
    led = header.get("ledger") or {}
    if not led:
        # Missing ledger (pre-history on some nodes) — treat as 0.
        return seq, 0
    th = (led.get("transaction_hash") or "").lower()
    if not th or th == _EMPTY_TX_HASH:
        return seq, 0
    full = rpc_network("ledger", {
        "ledger_index": int(seq),
        "transactions": True,
        "expand": False,
    })
    txs = (full.get("ledger") or {}).get("transactions") or []
    return seq, len(txs) if isinstance(txs, list) else 0


def _count_seq_range(start: int, end: int) -> Tuple[int, Dict[int, int]]:
    """Count txs for ledgers [start, end] inclusive. Returns (sum, {seq: n})."""
    if end < start:
        return 0, {}
    counts: Dict[int, int] = {}
    workers = max(1, min(TX_SCAN_WORKERS, end - start + 1))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futs = [pool.submit(_ledger_tx_count, seq) for seq in range(start, end + 1)]
        for fut in as_completed(futs):
            try:
                seq, n = fut.result()
                counts[seq] = n
            except Exception:
                pass
    # Fill gaps as 0 so contiguous advance stays honest.
    total = 0
    for seq in range(start, end + 1):
        n = counts.get(seq, 0)
        counts[seq] = n
        total += n
    return total, counts


def _save_tx_index() -> None:
    global _tx_index_dirty
    path = Path(TX_INDEX_FILE)
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with _tx_lock:
            payload = {
                "start_seq": _tx_start_seq,
                "counted_through": _tx_counted_through,
                "total_txs": _tx_total,
                "tip_seq": _tx_tip_seq,
                "saved_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            }
            _tx_index_dirty = False
        path.write_text(json.dumps(payload))
    except Exception:
        pass


def _load_tx_index() -> None:
    global _tx_start_seq, _tx_counted_through, _tx_total, _tx_tip_seq
    path = Path(TX_INDEX_FILE)
    if not path.is_file():
        return
    try:
        data = json.loads(path.read_text())
        with _tx_lock:
            _tx_start_seq = int(data.get("start_seq") or 1)
            _tx_counted_through = int(data.get("counted_through") or 0)
            _tx_total = int(data.get("total_txs") or 0)
            _tx_tip_seq = int(data.get("tip_seq") or 0)
    except Exception:
        pass


def get_tx_metrics() -> Dict[str, Any]:
    """Snapshot of on-ledger tx totals + live rate."""
    with _tx_lock:
        tip = _tx_tip_seq
        through = _tx_counted_through
        start = _tx_start_seq
        total = _tx_total
        complete = tip > 0 and through >= tip
        remaining = max(0, tip - through) if tip else 0
        span = max(1, tip - start + 1) if tip else 1
        done = max(0, through - start + 1) if through >= start else 0
        progress = min(100.0, round(100.0 * done / span, 2)) if tip else 0.0
        return {
            "tx_per_sec": round(_tx_per_sec, 4),
            "tx_per_min": round(_tx_per_sec * 60.0, 2),
            "total_txs": int(total),
            "last_ledger_txs": int(_tx_last_ledger_txs),
            "tx_index_complete": complete,
            "tx_index_scanned_through": int(through),
            "tx_index_tip": int(tip),
            "tx_index_start": int(start),
            "tx_index_remaining": int(remaining),
            "tx_index_progress_pct": progress,
            "tx_index_scanning": _tx_scan_active and not complete,
            "tx_index_error": _tx_scan_error,
        }


def note_network_tip(net_seq: int) -> None:
    """Update tip + live tx/s from newly closed ledgers (network-wide)."""
    global _tx_tip_seq, _tx_last_ledger_txs, _tx_per_sec
    global _tx_rate_prev_seq, _tx_rate_prev_ts
    global _tx_counted_through, _tx_total, _tx_index_dirty

    net_seq = int(net_seq or 0)
    if not net_seq:
        return
    now = time.time()

    with _tx_lock:
        _tx_tip_seq = max(_tx_tip_seq, net_seq)
        prev_rate_seq = _tx_rate_prev_seq
        prev_rate_ts = _tx_rate_prev_ts

    # Live rate: count txs in newly closed tip ledgers (small window).
    if prev_rate_seq is None:
        _, n = _ledger_tx_count(net_seq)
        with _tx_lock:
            _tx_last_ledger_txs = n
            _tx_rate_prev_seq = net_seq
            _tx_rate_prev_ts = now
            _tx_per_sec = 0.0
            # If already fully indexed to previous tip, extend total for this ledger.
            if _tx_counted_through == net_seq - 1 or (
                _tx_counted_through == 0 and net_seq == _tx_start_seq
            ):
                # First ledger only if starting cold at tip — avoid claiming full history.
                pass
        return

    if net_seq <= prev_rate_seq:
        with _tx_lock:
            if prev_rate_ts and (now - prev_rate_ts) > POLL_INTERVAL * 2:
                _tx_per_sec = 0.0
        return

    start = prev_rate_seq + 1
    # Cap rate walk so a long sleep does not stall the API.
    if net_seq - prev_rate_seq > 64:
        start = net_seq - 63
    interval_sum, counts = _count_seq_range(start, net_seq)
    last_n = counts.get(net_seq, 0)
    dt = max(0.001, now - (prev_rate_ts or now))

    with _tx_lock:
        _tx_last_ledger_txs = last_n
        _tx_per_sec = interval_sum / dt
        _tx_rate_prev_seq = net_seq
        _tx_rate_prev_ts = now
        _tx_tip_seq = max(_tx_tip_seq, net_seq)
        # If the historical index is caught up, extend it with tip ledgers.
        if _tx_counted_through >= start - 1 and _tx_counted_through < net_seq:
            for seq in range(_tx_counted_through + 1, net_seq + 1):
                _tx_total += counts.get(seq, 0)
                _tx_counted_through = seq
            _tx_index_dirty = True


def _mirror_tx_index_from_full_dash() -> None:
    """Validators: copy on-ledger total from the full-history dashboard."""
    global _tx_counted_through, _tx_total, _tx_start_seq, _tx_tip_seq
    global _tx_scan_active, _tx_scan_error, _tx_index_dirty
    if not FULL_DASH_URL:
        return
    try:
        r = httpx.get(f"{FULL_DASH_URL}/api/stats", timeout=6.0)
        r.raise_for_status()
        net = (r.json() or {}).get("network") or {}
        total = net.get("total_txs")
        through = net.get("tx_index_scanned_through")
        tip = net.get("tx_index_tip") or net.get("ledger_seq")
        if total is None:
            return
        with _tx_lock:
            _tx_total = int(total or 0)
            if through is not None:
                _tx_counted_through = int(through)
            if tip is not None:
                _tx_tip_seq = max(_tx_tip_seq, int(tip))
            _tx_scan_active = bool(net.get("tx_index_scanning"))
            _tx_scan_error = None
            _tx_index_dirty = True
    except Exception as exc:
        with _tx_lock:
            _tx_scan_error = f"mirror: {str(exc)[:160]}"


def _tx_mirror_loop() -> None:
    """Background: pull full on-ledger totals from the archive dashboard."""
    while True:
        try:
            _mirror_tx_index_from_full_dash()
        except Exception:
            pass
        time.sleep(max(10.0, POLL_INTERVAL))


def _tx_scanner_loop() -> None:
    """Background: walk every closed ledger and sum on-chain transactions."""
    global _tx_counted_through, _tx_total, _tx_start_seq, _tx_tip_seq
    global _tx_scan_active, _tx_scan_error, _tx_index_dirty

    # Prefer network full-history RPC (complete ledgers from early genesis).
    while True:
        try:
            info = rpc_network("server_info").get("info", {})
            vl = info.get("validated_ledger") or info.get("closed_ledger") or {}
            tip = int(vl.get("seq") or 0)
            complete = str(info.get("complete_ledgers") or "")
            # Parse lowest available ledger, e.g. "4-256545" or "4-10,12-256545"
            start_avail = _tx_start_seq
            if complete:
                first = complete.split(",")[0].split("-")[0].strip()
                try:
                    start_avail = max(1, int(first))
                except ValueError:
                    pass

            with _tx_lock:
                if tip:
                    _tx_tip_seq = max(_tx_tip_seq, tip)
                if _tx_counted_through < start_avail - 1:
                    # Skip gap before first available ledger.
                    _tx_start_seq = start_avail
                    _tx_counted_through = start_avail - 1
                through = _tx_counted_through
                tip_now = _tx_tip_seq

            if not tip_now or through >= tip_now:
                with _tx_lock:
                    _tx_scan_active = False
                    _tx_scan_error = None
                if _tx_index_dirty:
                    _save_tx_index()
                time.sleep(2.0)
                continue

            batch_end = min(tip_now, through + TX_SCAN_BATCH)
            batch_start = through + 1
            with _tx_lock:
                _tx_scan_active = True
                _tx_scan_error = None

            batch_sum, counts = _count_seq_range(batch_start, batch_end)

            with _tx_lock:
                # Only apply if we still own this contiguous window (no races).
                if _tx_counted_through == through:
                    for seq in range(batch_start, batch_end + 1):
                        _tx_total += counts.get(seq, 0)
                        _tx_counted_through = seq
                    _tx_index_dirty = True
                    _tx_scan_error = None
                _tx_scan_active = _tx_counted_through < _tx_tip_seq

            _save_tx_index()
            # Yield briefly so tip-rate polls stay snappy.
            time.sleep(0.05)
        except Exception as exc:
            with _tx_lock:
                _tx_scan_active = False
                _tx_scan_error = str(exc)[:200]
            time.sleep(3.0)


def _dir_size_bytes(path: Path, max_entries: int = 50_000) -> Optional[int]:
    """Best-effort recursive size; caps walk so a huge tree cannot hang the API."""
    if not path.is_dir():
        return None
    total = 0
    n = 0
    try:
        for root, _dirs, files in os.walk(path):
            for name in files:
                fp = Path(root) / name
                try:
                    total += fp.stat().st_size
                except OSError:
                    pass
                n += 1
                if n >= max_entries:
                    return total
    except OSError:
        return None
    return total


def _read_meminfo() -> Dict[str, Any]:
    """Host/container memory from /proc/meminfo (public capacity signal)."""
    out: Dict[str, Any] = {}
    try:
        raw = Path("/proc/meminfo").read_text()
    except OSError:
        return out
    kv: Dict[str, int] = {}
    for line in raw.splitlines():
        parts = line.replace(":", " ").split()
        if len(parts) >= 2 and parts[1].isdigit():
            # values in kB
            kv[parts[0]] = int(parts[1]) * 1024
    total = kv.get("MemTotal")
    avail = kv.get("MemAvailable")
    if total and avail is not None:
        used = max(0, total - avail)
        out = {
            "total_bytes": total,
            "available_bytes": avail,
            "used_bytes": used,
            "used_percent": round(100.0 * used / total, 1) if total else None,
        }
    return out


def _disk_usage_dict(path: str) -> Optional[Dict[str, Any]]:
    try:
        u = shutil.disk_usage(path)
    except OSError:
        return None
    total, used, free = int(u.total), int(u.used), int(u.free)
    if total <= 0:
        return None
    return {
        "path": path,
        "total_bytes": total,
        "used_bytes": used,
        "free_bytes": free,
        "used_percent": round(100.0 * used / total, 1),
    }


def _growth_per_day(history_key: str) -> Optional[float]:
    """Bytes/day from oldest→newest non-null samples of a history metric."""
    with _history_lock:
        pts = [
            (int(r["t"]), r.get(history_key))
            for r in _history
            if r.get(history_key) is not None
        ]
    if len(pts) < 2:
        return None
    t0, v0 = pts[0]
    t1, v1 = pts[-1]
    try:
        v0f, v1f = float(v0), float(v1)
    except (TypeError, ValueError):
        return None
    dt = t1 - t0
    if dt < 300:  # need ≥5 minutes of samples
        return None
    return (v1f - v0f) / dt * 86400.0


def collect_host_metrics() -> Dict[str, Any]:
    """Disk / memory / ledger-db size for this node (operator-facing, public-safe)."""
    if not SHOW_HOST_METRICS:
        return {}

    host_file: Dict[str, Any] = {}
    path = Path(HOST_STATS_FILE)
    if path.is_file():
        try:
            host_file = json.loads(path.read_text())
            if not isinstance(host_file, dict):
                host_file = {}
        except Exception:
            host_file = {}

    disk = host_file.get("disk") if isinstance(host_file.get("disk"), dict) else None
    if not disk:
        disk = _disk_usage_dict("/data") or _disk_usage_dict("/")
    # Prefer root disk if /data is a tiny volume and host file missing.
    disk_root = _disk_usage_dict("/")
    if disk and disk_root and disk.get("total_bytes", 0) < disk_root.get("total_bytes", 0) * 0.2:
        # /data is a small bind mount; show root capacity for ops.
        disk = {**disk_root, "path": disk_root.get("path", "/"), "note": "root filesystem"}

    mem = host_file.get("memory") if isinstance(host_file.get("memory"), dict) else None
    if not mem:
        mem = _read_meminfo() or None

    ledger_bytes = host_file.get("ledger_bytes")
    ledger_detail = host_file.get("ledger") if isinstance(host_file.get("ledger"), dict) else {}
    if ledger_bytes is None and LEDGER_DATA_DIR:
        base = Path(LEDGER_DATA_DIR)
        parts = {}
        total = 0
        for name in ("nudb", "db", "data"):
            p = base / name
            if p.is_dir():
                sz = _dir_size_bytes(p)
                if sz is not None:
                    parts[name] = sz
                    total += sz
        if parts:
            ledger_bytes = total
            ledger_detail = parts

    # Growth from measured history (not inferred from tx/s).
    ledger_growth = _growth_per_day("ledger_bytes")
    disk_growth = _growth_per_day("disk_used_bytes")

    # Soft ETA if we have free space + positive growth.
    days_to_full = None
    if disk and disk_growth and disk_growth > 0 and disk.get("free_bytes"):
        days_to_full = round(float(disk["free_bytes"]) / disk_growth, 1)

    xrpld = host_file.get("xrpld") if isinstance(host_file.get("xrpld"), dict) else None

    return {
        "disk": disk,
        "memory": mem,
        "ledger_bytes": int(ledger_bytes) if ledger_bytes is not None else None,
        "ledger": ledger_detail or None,
        "ledger_growth_bytes_per_day": round(ledger_growth, 0) if ledger_growth is not None else None,
        "disk_growth_bytes_per_day": round(disk_growth, 0) if disk_growth is not None else None,
        "days_to_disk_full_est": days_to_full,
        "xrpld": xrpld,
        "source": "host_stats" if host_file else "local",
        "updated_at": host_file.get("updated_at"),
    }


def version_status(build_version: Any, host: Dict[str, Any]) -> Dict[str, Any]:
    """Compare this node to the expected Falcon testnet pin."""
    xrpld = (host or {}).get("xrpld") or {}
    image = xrpld.get("image") or os.environ.get("LOCAL_XRPLD_IMAGE")
    digest = xrpld.get("digest") or os.environ.get("LOCAL_XRPLD_DIGEST")
    revision = xrpld.get("revision") or os.environ.get("LOCAL_XRPLD_REVISION")

    match = None
    if digest:
        match = digest.replace("sha256:", "") == EXPECTED_IMAGE_DIGEST.replace("sha256:", "")
    elif image:
        # Tag match is weaker than digest but still useful.
        match = (
            "mainnet-v2" in str(image)
            or EXPECTED_IMAGE_DIGEST in str(image)
            or str(image).endswith("@" + EXPECTED_IMAGE_DIGEST)
        )

    return {
        "build_version": build_version,
        "image": image,
        "digest": digest,
        "revision": revision,
        "expected_tag": EXPECTED_IMAGE_TAG,
        "expected_digest": EXPECTED_IMAGE_DIGEST,
        "expected_revision": EXPECTED_REVISION,
        "matches_expected": match,
        "network_id_expected": 1001,
    }


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

    # Internal traffic generator stats — only when explicitly enabled.
    traffic = read_traffic_stats() if SHOW_TRAFFIC else {}
    # Live tip/rate from network; total comes from full ledger index scan.
    note_network_tip(int(net_seq or 0))
    tx_metrics = get_tx_metrics()
    host = collect_host_metrics()
    version = version_status(local_info.get("build_version"), host)

    # Archive (full-history) storage view for network activity.
    # On the archive node itself, use local host metrics; validators mirror.
    archive_ledger_bytes = host.get("ledger_bytes") if not VALIDATOR_ACCOUNT else None
    archive_growth = host.get("ledger_growth_bytes_per_day") if not VALIDATOR_ACCOUNT else None
    if VALIDATOR_ACCOUNT and FULL_DASH_URL:
        try:
            r = httpx.get(f"{FULL_DASH_URL}/api/stats", timeout=4.0)
            if r.status_code == 200:
                remote = (r.json() or {}).get("node") or {}
                rh = remote.get("host") or {}
                if rh.get("ledger_bytes") is not None:
                    archive_ledger_bytes = rh.get("ledger_bytes")
                if rh.get("ledger_growth_bytes_per_day") is not None:
                    archive_growth = rh.get("ledger_growth_bytes_per_day")
                # Prefer remote network.archive_* if present.
                rnet = (r.json() or {}).get("network") or {}
                if rnet.get("archive_ledger_bytes") is not None:
                    archive_ledger_bytes = rnet.get("archive_ledger_bytes")
                if rnet.get("archive_growth_bytes_per_day") is not None:
                    archive_growth = rnet.get("archive_growth_bytes_per_day")
        except Exception:
            pass

    return {
        "updated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "show_traffic": SHOW_TRAFFIC,
        "show_host_metrics": SHOW_HOST_METRICS,
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
            "host": host or None,
            "version": version,
        },
        "network": {
            "rpc": NETWORK_RPC_URL,
            "server_state": net_info.get("server_state"),
            "build_version": net_info.get("build_version"),
            "network_id": net_info.get("network_id") or local_info.get("network_id"),
            "ledger_seq": net_seq,
            "complete_ledgers": net_info.get("complete_ledgers"),
            "peers": net_info.get("peers", 0),
            "load_factor": net_info.get("load_factor"),
            "bonded_validator_count": bonded_count,
            "total_validator_entries": len(validators),
            # Live consensus participation (last closed ledger), not bond count.
            "proposing_count": int(
                ((net_info.get("last_close") or {}).get("proposers")) or 0
            ),
            "validators": validators,
            "epoch": fetch_epoch(),
            "tx_per_sec": tx_metrics["tx_per_sec"],
            "tx_per_min": tx_metrics["tx_per_min"],
            "total_txs": tx_metrics["total_txs"],
            "last_ledger_txs": tx_metrics["last_ledger_txs"],
            "tx_index_complete": tx_metrics["tx_index_complete"],
            "tx_index_scanned_through": tx_metrics["tx_index_scanned_through"],
            "tx_index_tip": tx_metrics["tx_index_tip"],
            "tx_index_progress_pct": tx_metrics["tx_index_progress_pct"],
            "tx_index_scanning": tx_metrics["tx_index_scanning"],
            "tx_index_remaining": tx_metrics["tx_index_remaining"],
            # Full-history archive footprint (not "sum of all nodes").
            "archive_ledger_bytes": archive_ledger_bytes,
            "archive_growth_bytes_per_day": archive_growth,
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
    net = stats.get("network") or {}
    node = stats.get("node") or {}
    bond = node.get("bond") or {}
    host = node.get("host") or {}
    disk = host.get("disk") or {}
    composite = bond.get("composite_score")
    return {
        "t": int(now),
        "ledger_seq": net_seq,
        "node_ledger_seq": int(node.get("ledger_seq") or 0),
        "ledger_lag": node.get("ledger_lag"),
        "peers": int(node.get("peers") or 0),
        "net_peers": int(net.get("peers") or 0),
        "load_factor": float(net.get("load_factor") or 1),
        "ledger_rate_per_min": round(ledger_rate, 2),
        # Network-wide tx metrics (not the internal traffic generator).
        "net_tx_per_sec": float(net.get("tx_per_sec") or 0),
        "net_tx_per_min": float(net.get("tx_per_min") or 0),
        "total_txs": int(net.get("total_txs") or 0),
        "tx_per_min": float(traffic.get("tx_per_min") or 0),
        "traffic_submitted": int(traffic.get("submitted") or 0),
        "traffic_validated": int(traffic.get("validated") or 0),
        "bonded_validators": int(net.get("bonded_validator_count") or 0),
        "proposing_validators": int(net.get("proposing_count") or 0),
        # Only chart a real score; missing bond → omit (avoids flat orange "0" line).
        "composite_score": int(composite) if composite is not None else None,
        # Capacity history for growth charts.
        "disk_used_percent": disk.get("used_percent"),
        "disk_used_bytes": disk.get("used_bytes"),
        "disk_free_bytes": disk.get("free_bytes"),
        "ledger_bytes": host.get("ledger_bytes"),
        "mem_used_percent": (host.get("memory") or {}).get("used_percent"),
        "archive_ledger_bytes": net.get("archive_ledger_bytes"),
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
    _load_tx_index()
    t = threading.Thread(target=_collector_loop, daemon=True, name="metrics-collector")
    t.start()
    if TX_SCAN_ENABLE:
        # Full on-ledger tx index — walks every closed ledger via network RPC.
        s = threading.Thread(target=_tx_scanner_loop, daemon=True, name="tx-ledger-scanner")
        s.start()
    else:
        # Validator dashboards mirror the archive node's completed index.
        m = threading.Thread(target=_tx_mirror_loop, daemon=True, name="tx-index-mirror")
        m.start()


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
    <div class="sub" id="versionBar">Loading version…</div>
    <div class="sub" id="subtitle">Loading network…</div>
  </div>
  <div class="live-pill"><span class="live-dot"></span><span id="liveLabel">LIVE</span></div>
</header>
<div class="wrap">
  <div class="section-title">Node health · click any tile for 24h chart</div>
  <div class="grid" id="nodeGrid"></div>

  <div class="section-title">Network activity</div>
  <div class="grid" id="netGrid"></div>

  <div id="trafficSection" hidden>
    <div class="section-title">Traffic generator <span class="badge">internal only</span></div>
    <div class="grid" id="trafficGrid"></div>
  </div>

  <div class="section-title" id="valSectionTitle">Validators</div>
  <p style="color:var(--muted);font-size:12px;margin:-6px 0 12px;line-height:1.4">
    <strong>Bonded</strong> = stake on ledger (still listed if the machine is off).
    <strong>Active</strong> = proposed in the last consensus close.
  </p>
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
  net_tx_per_sec: { title: 'Network tx rate (tx/s)', color: '#f72585' },
  total_txs: { title: 'Total txs on ledger', color: '#b5179e' },
  tx_per_min: { title: 'Traffic generator tx rate (/min)', color: '#f72585' },
  traffic_validated: { title: 'Traffic generator validated txs', color: '#b5179e' },
  bonded_validators: { title: 'Total bonded validators', color: '#560bad' },
  proposing_validators: { title: 'Active proposing (last close)', color: '#3dd68c' },
  composite_score: { title: 'Composite score', color: '#4cc9f0' },
  disk_used_percent: { title: 'Disk used %', color: '#f5b942' },
  disk_free_bytes: { title: 'Disk free (bytes)', color: '#3dd68c' },
  ledger_bytes: { title: 'Ledger DB size (bytes)', color: '#4895ef' },
  mem_used_percent: { title: 'Memory used %', color: '#ff9f1c' },
  archive_ledger_bytes: { title: 'Archive ledger size (bytes)', color: '#b5179e' },
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
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  // Never show a bare "0" — under 1 minute still say "<1m"
  if (d > 0) return h > 0 ? (d + 'd ' + h + 'h') : (d + 'd');
  if (h > 0) return m > 0 ? (h + 'h ' + m + 'm') : (h + 'h');
  if (m > 0) return m + 'm';
  return sec > 0 ? '<1m' : 'just started';
}

function drawSpark(canvas, points, color) {
  // Drop null/undefined samples so "n/a" metrics do not paint a flat zero line.
  const vals = (points || []).map(p => p.v).filter(v => v != null && !Number.isNaN(Number(v)));
  if (!canvas || !vals.length) {
    if (canvas) {
      const ctx = canvas.getContext('2d');
      if (ctx) {
        canvas.width = canvas.clientWidth || 0;
        canvas.height = 34;
        ctx.clearRect(0, 0, canvas.width, canvas.height);
      }
    }
    return;
  }
  const cssW = canvas.clientWidth || canvas.parentElement?.clientWidth || 180;
  if (cssW < 2) return;
  const ctx = canvas.getContext('2d');
  const ratio = Math.min(window.devicePixelRatio || 1, 2);
  const w = canvas.width = Math.floor(cssW * ratio);
  const h = canvas.height = Math.floor(34 * ratio);
  const nums = vals.map(Number);
  const min = Math.min(...nums), max = Math.max(...nums);
  const span = Math.max(max - min, 1);
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle = color;
  ctx.lineWidth = 2 * ratio;
  ctx.beginPath();
  nums.forEach((v,i) => {
    const x = (i / Math.max(nums.length-1,1)) * (w-8) + 4;
    const y = h - 4 - ((v - min) / span) * (h-10);
    i ? ctx.lineTo(x,y) : ctx.moveTo(x,y);
  });
  ctx.stroke();
}

function historyForMetric(metric) {
  return historyCache.map(p => ({ t: p.t, v: p[metric] })).filter(p => p.v != null);
}

function fmtTxPerSec(n) {
  const v = Number(n) || 0;
  if (v >= 10) return v.toFixed(1) + '/s';
  if (v >= 1) return v.toFixed(2) + '/s';
  if (v > 0) return v.toFixed(3) + '/s';
  return '0/s';
}

function fmtCount(n) {
  const v = Number(n) || 0;
  return v.toLocaleString();
}

function fmtBytes(n) {
  const v = Number(n);
  if (n == null || Number.isNaN(v) || v < 0) return '—';
  const u = ['B','KB','MB','GB','TB'];
  let x = v, i = 0;
  while (x >= 1024 && i < u.length-1) { x /= 1024; i++; }
  return (i === 0 ? String(Math.round(x)) : x.toFixed(x >= 10 ? 1 : 2)) + ' ' + u[i];
}

function fmtBytesPerDay(n) {
  if (n == null || Number.isNaN(Number(n))) return null;
  const v = Number(n);
  const sign = v < 0 ? '-' : '';
  return sign + fmtBytes(Math.abs(v)) + '/day';
}

function diskTone(pct) {
  if (pct == null) return '';
  if (pct >= 90) return 'bad';
  if (pct >= 75) return 'warn';
  return 'good';
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
  const ver = node.version || {};
  const match = ver.matches_expected;
  const matchBadge = match === true
    ? '<span class="badge" style="color:var(--good);border-color:#1f4d38">✓ expected pin</span>'
    : (match === false
      ? '<span class="badge" style="color:var(--bad);border-color:#5c2a2a">⚠ image skew — upgrade</span>'
      : '<span class="badge">pin check pending</span>');
  const imgLabel = ver.image
    ? ver.image.replace('qxrp/xrpld@sha256:', 'digest:').replace('qxrp/xrpld:', '')
    : (ver.expected_tag || 'mainnet-v2').replace('qxrp/xrpld:', '');
  const digShort = (ver.digest || ver.expected_digest || '').replace('sha256:', '').slice(0, 12);
  const rev = ver.revision || ver.expected_revision || '—';
  const netId = net.network_id || node.network_id || 1001;
  document.getElementById('versionBar').innerHTML =
    `<span class="badge">network ${netId}</span> &nbsp; ` +
    `<span class="badge">image <span class="mono">${imgLabel}</span></span> &nbsp; ` +
    `<span class="badge">build ${ver.build_version || node.build_version || '—'}</span> &nbsp; ` +
    `<span class="badge">rev <span class="mono">${rev}</span></span> &nbsp; ` +
    (digShort ? `<span class="badge">sha256:${digShort}…</span> &nbsp; ` : '') +
    matchBadge +
    ` &nbsp; <span class="badge" style="opacity:.85">expected ${ (ver.expected_tag||'mainnet-v2').replace('qxrp/xrpld:','') }</span>`;

  document.getElementById('subtitle').innerHTML =
    `<span class="badge">${stats.updated_at || ''}</span> &nbsp; ` +
    (node.validator_account
      ? `Validator: <span class="mono">${node.validator_account}</span>`
      : `<span class="badge">full-history node</span> (no local bond — list below is network-wide)`);

  // Full-history / non-validator dashboards have no VALIDATOR_ACCOUNT — bond tiles are n/a.
  const isValidatorNode = !!(node.validator_account);
  const bondOk = bond.status === 'bonded';
  const bondStatus = isValidatorNode
    ? (bond.status || 'unknown')
    : 'n/a';
  const bondSub = isValidatorNode
    ? ((bond.bonded_amount_qxrp != null ? bond.bonded_amount_qxrp : '—') + ' FALCON locked')
    : 'full-history node · not a validator';
  const bondClass = bondOk ? 'good' : (isValidatorNode ? 'warn' : '');
  // Bond tile chart: only validators with a score; full nodes get no flat-zero sparkline.
  const bondMetric = isValidatorNode ? 'composite_score' : '';
  const scoreVal = isValidatorNode
    ? (bond.composite_score != null ? bond.composite_score : '—')
    : 'n/a';
  const scoreClass = isValidatorNode
    ? ((bond.composite_score || 0) >= 5000 ? 'good' : 'warn')
    : '';
  const scoreMetric = isValidatorNode ? 'composite_score' : '';
  const balVal = isValidatorNode
    ? ((node.balance_qxrp != null ? Number(node.balance_qxrp).toLocaleString(undefined,{maximumFractionDigits:2}) : '—') + ' FALCON')
    : 'n/a';

  const host = node.host || {};
  const disk = host.disk || {};
  const mem = host.memory || {};
  const diskPct = disk.used_percent;
  const diskVal = (disk.used_bytes != null && disk.total_bytes != null)
    ? (fmtBytes(disk.used_bytes) + ' / ' + fmtBytes(disk.total_bytes))
    : '—';
  const diskSub = disk.free_bytes != null
    ? (fmtBytes(disk.free_bytes) + ' free' + (diskPct != null ? ' · ' + diskPct + '% used' : '')
      + (host.days_to_disk_full_est != null ? ' · ~' + host.days_to_disk_full_est + 'd to full' : ''))
    : (disk.path || 'host disk');
  const ledgerSz = host.ledger_bytes;
  const ledgerGrowth = fmtBytesPerDay(host.ledger_growth_bytes_per_day);
  const ledgerSub = ledgerGrowth
    ? ('measured ' + ledgerGrowth)
    : (host.ledger ? Object.keys(host.ledger).join('+') : 'nudb+db (host)');
  const memVal = mem.used_percent != null ? (mem.used_percent + '%') : '—';
  const memSub = (mem.used_bytes != null && mem.total_bytes != null)
    ? (fmtBytes(mem.used_bytes) + ' / ' + fmtBytes(mem.total_bytes))
    : 'host memory';

  const nodeCards = [
    tile('v_state', 'Server state', node.server_state || '—', node.validation_pubkey ? node.validation_pubkey.slice(0,24)+'…' : (isValidatorNode ? '' : 'full-history node'), 'peers', cls(node.server_state, ['proposing'],['full','connected'])),
    tile('v_ledger', 'Node ledger', '#' + Number(node.ledger_seq||0).toLocaleString(), (node.ledger_hash||'').slice(0,20)+'…', 'node_ledger_seq', 'good'),
    tile('v_lag', 'Sync lag', (node.ledger_lag ?? '—') + ' ledgers', node.complete_ledgers || '', 'ledger_lag', (node.ledger_lag||0) <= 5 ? 'good' : 'warn'),
    tile('v_peers', 'Peers', String(node.peers ?? '—'), 'P2P connections', 'peers', (node.peers||0) >= 3 ? 'good' : 'warn'),
    tile('v_bond', 'Bond status', bondStatus, bondSub, bondMetric, bondClass),
    tile('v_score', 'Composite score', scoreVal, isValidatorNode ? 'basis points / 10000' : 'see bonded table below', scoreMetric, scoreClass),
    tile('v_bal', 'Balance', balVal, isValidatorNode ? 'validator account' : 'see bonded table', isValidatorNode ? 'ledger_rate_per_min' : ''),
    tile('v_uptime', 'Uptime', fmtUptime(node.uptime_seconds), 'load ×' + (node.load_factor||1), 'load_factor'),
    tile('v_disk', 'Disk', diskVal, diskSub, 'disk_used_percent', diskTone(diskPct)),
    tile('v_ledgerdb', 'Ledger DB', fmtBytes(ledgerSz), ledgerSub, 'ledger_bytes', ledgerSz != null ? 'good' : ''),
    tile('v_mem', 'Memory', memVal, memSub, 'mem_used_percent', (mem.used_percent||0) >= 90 ? 'bad' : ((mem.used_percent||0) >= 75 ? 'warn' : '')),
  ];
  document.getElementById('nodeGrid').innerHTML = nodeCards.join('');

  const txPerSec = net.tx_per_sec;
  const totalTxs = net.total_txs;
  const txComplete = !!net.tx_index_complete;
  const txScanSub = txComplete
    ? ('all closed ledgers · tip #' + Number(net.tx_index_tip||ledger).toLocaleString())
    : ('indexing on-ledger… ' + (net.tx_index_progress_pct != null ? Number(net.tx_index_progress_pct).toFixed(1) + '%' : '')
      + (net.tx_index_scanned_through != null ? ' · through #' + Number(net.tx_index_scanned_through).toLocaleString() : ''));
  const archBytes = net.archive_ledger_bytes;
  const archGrowth = fmtBytesPerDay(net.archive_growth_bytes_per_day);
  const archSub = archGrowth
    ? ('full-history archive · ' + archGrowth)
    : 'full-history archive size (measured)';
  const bondedN = Number(net.bonded_validator_count || 0);
  const activeN = Number(net.proposing_count || 0);
  const valTitle = document.getElementById('valSectionTitle');
  if (valTitle) {
    valTitle.textContent = 'Validators · ' + bondedN + ' bonded total · ' + activeN + ' active proposing';
  }
  const netCards = [
    tile('n_ledger', 'Network ledger', '#' + ledger.toLocaleString(), net.complete_ledgers || '', 'ledger_seq', 'good'),
    tile('n_state', 'Network state', net.server_state || '—', net.rpc || '', 'net_peers'),
    tile('n_bonded', 'Total bonded', String(bondedN), 'on-ledger stake (incl. offline)', 'bonded_validators', 'good'),
    tile('n_active', 'Active proposing', String(activeN), 'last consensus close', 'proposing_validators', activeN > 0 ? 'good' : 'warn'),
    tile('n_load', 'Load factor', String(net.load_factor || 1), 'network pressure', 'load_factor'),
    tile('n_rate', 'Ledger rate', '…', 'closes per minute', 'ledger_rate_per_min', 'good'),
    tile('n_tps', 'Tx rate', fmtTxPerSec(txPerSec), 'network-wide closes' + (net.last_ledger_txs != null ? ' · last ledger ' + net.last_ledger_txs : ''), 'net_tx_per_sec', (Number(txPerSec)||0) > 0 ? 'good' : ''),
    tile('n_txtotal', 'Total txs', fmtCount(totalTxs), txScanSub, 'total_txs', txComplete ? 'good' : 'warn'),
    tile('n_arch', 'Archive size', fmtBytes(archBytes), archSub, 'archive_ledger_bytes', archBytes != null ? 'good' : ''),
  ];
  document.getElementById('netGrid').innerHTML = netCards.join('');

  const showTraffic = !!(stats.show_traffic && traffic && Object.keys(traffic).length);
  const trafficSection = document.getElementById('trafficSection');
  if (trafficSection) trafficSection.hidden = !showTraffic;
  if (showTraffic) {
    const pump = traffic.in_pump ? '<span class="badge" style="color:var(--warn)">PUMP</span>' : 'baseline';
    const trafficCards = [
      tile('t_rate', 'Tx rate', (traffic.tx_per_min ?? 0) + '/min', pump, 'tx_per_min', (traffic.tx_per_min||0) > 0 ? 'good' : ''),
      tile('t_val', 'Validated txs', String(traffic.validated ?? 0), 'submitted ' + (traffic.submitted ?? 0), 'traffic_validated'),
      tile('t_wallets', 'Load wallets', String(traffic.wallets ?? '—'), 'internal load-test', 'tx_per_min'),
      tile('t_refill', 'Refills', String(traffic.refills ?? 0), 'auto top-up from genesis', 'traffic_validated'),
    ];
    document.getElementById('trafficGrid').innerHTML = trafficCards.join('');
  }

  const rows = (net.validators || []).map(v => {
    const acc = v.account || '';
    const short = acc.length > 12 ? acc.slice(0, 8) + '…' + acc.slice(-6) : acc;
    return `<tr>
    <td class="mono" title="${acc}">${short}</td>
    <td>${v.bond_status}</td>
    <td>${v.bonded_amount_qxrp ?? '—'}</td>
    <td>${v.composite_score ?? '—'}</td>
  </tr>`;
  }).join('');
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