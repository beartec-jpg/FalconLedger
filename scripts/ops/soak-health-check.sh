#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Lightweight soak health probe (item 2).
# Usage:
#   PUBLIC_RPC=http://127.0.0.1:6005 bash scripts/ops/soak-health-check.sh
#   SOAK_OUT=scripts/mainnet-ceremony/dry-runs/soak-$(date -u +%Y-%m-%dT%H%MZ).json \
#     PUBLIC_RPC=... bash scripts/ops/soak-health-check.sh
#
set -euo pipefail

RPC="${PUBLIC_RPC:-http://127.0.0.1:6005}"
TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
OUT="${SOAK_OUT:-}"

echo "[soak-health] $TS rpc=$RPC"

body="$(curl -sf --max-time 15 -X POST "$RPC" \
  -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}')" || {
  echo "[soak-health] FAIL: server_info unreachable"
  exit 1
}

python3 - "$body" "$TS" "$RPC" "$OUT" <<'PY'
import json, sys, os
body = json.loads(sys.argv[1])
ts, rpc, out = sys.argv[2], sys.argv[3], sys.argv[4]
r = body.get("result") or {}
info = r.get("info") or r
vl = info.get("validated_ledger") or {}
state = info.get("server_state") or ""
seq = vl.get("seq")
peers = info.get("peers")
uptime = info.get("uptime") or 0
uptime_h = round(float(uptime) / 3600.0, 2)
print(f"[soak-health] server_state={state!s} validated_seq={seq!s} peers={peers!s} uptime_h={uptime_h}")
ok_states = {"full", "proposing", "validating", "syncing"}
ok = state in ok_states or seq is not None
check = "OK" if ok else "WARN"
if out:
    artifact = {
        "captured_at": ts,
        "check": "PASS_EARLY" if ok else "FAIL",
        "rpc": rpc,
        "network_id": info.get("network_id"),
        "server_info": {
            "server_state": state,
            "validated_seq": seq,
            "complete_ledgers": info.get("complete_ledgers"),
            "peers": peers,
            "proposers": (info.get("last_close") or {}).get("proposers"),
            "uptime_s": uptime,
            "uptime_h": uptime_h,
            "build_version": info.get("build_version"),
            "peer_disconnects": info.get("peer_disconnects"),
            "load_factor": info.get("load_factor"),
            "io_latency_ms": info.get("io_latency_ms"),
        },
        "criteria": {
            "peers_ge_2": (peers or 0) >= 2,
            "seq_advancing": seq is not None,
            "multi_day_48h": uptime_h >= 48,
            "multi_day_7d": uptime_h >= 168,
        },
    }
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(artifact, f, indent=2)
        f.write("\n")
    print(f"[soak-health] wrote {out}")
if ok:
    print("[soak-health] OK")
    sys.exit(0)
print("[soak-health] WARN: unexpected server_state")
sys.exit(2)
PY
