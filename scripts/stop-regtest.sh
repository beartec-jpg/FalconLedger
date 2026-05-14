#!/usr/bin/env bash
# stop-regtest.sh  –  Gracefully stop a running qXRP regtest network.
#
# Usage:
#   ./scripts/stop-regtest.sh [<runtime-dir>]
#
# Default runtime-dir: ./data/regtest

set -euo pipefail

ROOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
RUNTIME_DIR="${1:-$ROOTDIR/data/regtest}"
PIDFILE="$RUNTIME_DIR/pids.txt"

die() { echo "ERROR: $*" >&2; exit 1; }
log() { echo "[stop-regtest] $*"; }

[[ -f "$PIDFILE" ]] || die "No pids.txt found in $RUNTIME_DIR – network may not be running"

STOPPED=0
while IFS= read -r PID; do
    [[ -z "$PID" ]] && continue
    if kill -0 "$PID" 2>/dev/null; then
        log "Stopping pid $PID..."
        kill "$PID" 2>/dev/null || true
        STOPPED=$((STOPPED + 1))
    else
        log "pid $PID is already dead"
    fi
done < "$PIDFILE"

# Wait briefly for clean shutdown
sleep 2

# Force-kill anything still alive
while IFS= read -r PID; do
    [[ -z "$PID" ]] && continue
    if kill -0 "$PID" 2>/dev/null; then
        log "Force-killing pid $PID"
        kill -9 "$PID" 2>/dev/null || true
    fi
done < "$PIDFILE"

rm -f "$PIDFILE"
log "Stopped $STOPPED validator(s)"
