#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Lightweight soak health probe (item 2).
# Usage:
#   PUBLIC_RPC=http://127.0.0.1:6005 bash scripts/ops/soak-health-check.sh
#
set -euo pipefail

RPC="${PUBLIC_RPC:-http://127.0.0.1:6005}"
TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

echo "[soak-health] $TS rpc=$RPC"

body="$(curl -sf --max-time 15 -X POST "$RPC" \
  -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}')" || {
  echo "[soak-health] FAIL: server_info unreachable"
  exit 1
}

python3 - "$body" <<'PY'
import json, sys
body = json.loads(sys.argv[1])
r = body.get("result") or {}
info = r.get("info") or r
vl = info.get("validated_ledger") or {}
state = info.get("server_state") or ""
seq = vl.get("seq")
peers = info.get("peers")
print(f"[soak-health] server_state={state!s} validated_seq={seq!s} peers={peers!s}")
ok_states = {"full", "proposing", "validating", "syncing"}
if state in ok_states or seq is not None:
    print("[soak-health] OK")
    sys.exit(0)
print("[soak-health] WARN: unexpected server_state")
sys.exit(2)
PY
