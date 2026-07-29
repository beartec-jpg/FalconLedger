#!/usr/bin/env bash
# Start standalone Falcon node network_id 1101 for BitcoinSPVBridge testing.
# Requires built xrpld. Never points at 1001 validators.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CFG="${FALCON_SPV_CFG:-$ROOT/cfg/btc-spv-isolated.cfg}"
DATADIR="${FALCON_SPV_DATADIR:-$ROOT/data/btc-spv-1101}"
BIN="${XRPLD_BIN:-}"
LOG="$DATADIR/xrpld-stdout.log"

if [[ -z "$BIN" ]]; then
  for c in \
    "$ROOT/.build/xrpld" \
    "$ROOT/.build/Release/xrpld" \
    "$ROOT/build/xrpld" \
    "$ROOT/build/Release/xrpld"
  do
    [[ -x "$c" ]] && BIN="$c" && break
  done
fi

[[ -x "${BIN:-}" ]] || {
  echo "ERROR: xrpld not found. Build first: cmake --build .build -j1 --target xrpld" >&2
  exit 1
}
[[ -f "$CFG" ]] || { echo "ERROR: missing $CFG" >&2; exit 1; }

mkdir -p "$DATADIR/nudb"
export PATH

if pgrep -f "xrpld.*btc-spv-isolated" >/dev/null 2>&1; then
  echo "Falcon SPV node already running"
  curl -s -X POST http://127.0.0.1:5115 -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' | head -c 500
  echo
  exit 0
fi

echo "Starting $BIN --conf $CFG --standalone"
nohup "$BIN" --conf "$CFG" --standalone >"$LOG" 2>&1 &
echo $! >"$DATADIR/xrpld.pid"
echo "PID $(cat "$DATADIR/xrpld.pid") log=$LOG"

for i in $(seq 1 60); do
  if curl -s -o /dev/null -w '%{http_code}' -X POST http://127.0.0.1:5115 \
      -H 'Content-Type: application/json' \
      -d '{"method":"server_info","params":[{}]}' | grep -q 200; then
    echo "Falcon 1101 RPC ready on :5115"
    curl -s -X POST http://127.0.0.1:5115 -H 'Content-Type: application/json' \
      -d '{"method":"server_info","params":[{}]}' | python3 -c \
      "import sys,json; i=json.load(sys.stdin)['result']['info']; print('state=',i.get('server_state'),'net=',i.get('network_id'),'complete_ledgers=',i.get('complete_ledgers'))"
    exit 0
  fi
  sleep 1
done
echo "ERROR: Falcon RPC not ready — see $LOG" >&2
tail -40 "$LOG" >&2 || true
exit 1
