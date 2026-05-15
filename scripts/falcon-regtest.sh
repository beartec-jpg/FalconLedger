#!/usr/bin/env bash
# falcon-regtest.sh — End-to-end Falcon-512 load test for qXRP regtest.
#
# Orchestrates:
#   1. Start a 5-validator regtest network (via start-regtest.sh)
#   2. Bond all validators using Falcon-512 keys (via bond-validators.py)
#   3. Run a Payment transaction load test with metrics collection
#   4. Print a full sustainability report
#
# Usage:
#   ./scripts/falcon-regtest.sh [<xrpld-binary>] [--accounts N] [--duration S]
#
# Example:
#   ./scripts/falcon-regtest.sh build/build/Release/xrpld --accounts 30 --duration 120
#
# Prerequisites:
#   • xrpld built with -Dqxrp=ON -Dqxrp_epoch_override=100
#   • python3 available

set -euo pipefail

ROOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOTDIR"

# ── argument parsing ───────────────────────────────────────────────────────────
BINARY="${1:-./build/build/Release/xrpld}"
ACCOUNTS=30
DURATION=120
shift 2>/dev/null || true

while [[ $# -gt 0 ]]; do
    case "$1" in
        --accounts) ACCOUNTS="$2"; shift 2 ;;
        --duration) DURATION="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

[[ -x "$BINARY" ]] || {
    echo "ERROR: binary not found or not executable: $BINARY"
    echo "  Build with: cmake --build build/build/Release --target xrpld -j\$(nproc)"
    exit 1
}

RUNTIME_DIR="$ROOTDIR/data/regtest"
PIDS_FILE="$RUNTIME_DIR/pids.txt"
BASE_PORT=5005

log()  { echo "[falcon-regtest] $*"; }
die()  { echo "ERROR: $*" >&2; exit 1; }

rpc_call() {
    local port="$1" body="$2"
    curl -s -X POST "http://127.0.0.1:${port}" \
         -H 'Content-Type: application/json' \
         -d "$body"
}

wait_server() {
    local port="$1" label="$2"
    log "Waiting for $label on port $port..."
    for i in $(seq 1 60); do
        CODE=$(curl -s -o /dev/null -w '%{http_code}' \
                    -X POST "http://127.0.0.1:$port" \
                    -H 'Content-Type: application/json' \
                    -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
        if [[ "$CODE" == "200" ]]; then
            STATE=$(rpc_call "$port" '{"method":"server_info","params":[{}]}' | \
                    python3 -c "import sys,json; d=json.load(sys.stdin); print(d['result']['info']['server_state'])" 2>/dev/null || echo unknown)
            if [[ "$STATE" == "proposing" || "$STATE" == "full" ]]; then
                log "  $label ready (state=$STATE)"
                return 0
            fi
        fi
        sleep 2
        printf "."
    done
    echo ""
    die "$label did not become ready in time"
}

# ── cleanup handler ────────────────────────────────────────────────────────────
cleanup() {
    if [[ -f "$PIDS_FILE" ]]; then
        log "Stopping validators..."
        while IFS= read -r pid; do
            kill "$pid" 2>/dev/null || true
        done < "$PIDS_FILE"
        wait 2>/dev/null || true
        rm -f "$PIDS_FILE"
    fi
}
trap cleanup EXIT INT TERM

# ── Phase 1: start the network ─────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║         qXRP Falcon-512 Regtest — Full Load Test Suite           ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

log "Phase 1 — Starting 5-node regtest network..."
log "Binary  : $BINARY"
log "Runtime : $RUNTIME_DIR"

# start-regtest.sh blocks until all validators are up and consensus converges,
# then waits.  We run it in the background so this script can continue.
# We also forward the binary as arg 1.
"$ROOTDIR/scripts/start-regtest.sh" "$BINARY" "$RUNTIME_DIR" &
REGTEST_PID=$!

# Wait for port 5005 to accept connections and reach proposing/full state
wait_server $BASE_PORT "Validator 1"
echo ""

# ── Phase 2: Bond validators with Falcon keys ──────────────────────────────────
log "Phase 2 — Bonding validators with Falcon-512 keys..."
python3 "$ROOTDIR/scripts/bond-validators.py"
echo ""

# ── Phase 3: Wait for first epoch (100 ledgers) ───────────────────────────────
log "Phase 3 — Waiting for epoch 1 (100 ledgers)..."
python3 - "$BASE_PORT" <<'PY'
import sys, time, json, urllib.request

port = int(sys.argv[1])
def rpc(method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}", data=body,
                                  headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read())

deadline = time.monotonic() + 900  # 15 min max
while time.monotonic() < deadline:
    try:
        info = rpc("server_info")["result"]["info"]
        seq  = info.get("validated_ledger", {}).get("seq", 0)
        nxt  = 100 - (seq % 100) if seq % 100 != 0 else 100
        print(f"\r  ledger {seq:6}  next epoch in {nxt:3} ledgers  ", end="", flush=True)
        if seq >= 100:
            try:
                ep = rpc("ledger_entry", {"reward_epoch": True, "ledger_index": "validated"})
                epoch = ep["result"].get("node", {}).get("EpochNumber", 0)
                if epoch >= 1:
                    print(f"\n  Epoch {epoch} reached at ledger {seq} ✓")
                    sys.exit(0)
            except Exception:
                pass
    except Exception:
        pass
    time.sleep(3)

print("\n  WARNING: epoch 1 not reached within 15 minutes — continuing anyway")
PY
echo ""

# ── Phase 4: Run load test ─────────────────────────────────────────────────────
log "Phase 4 — Running Payment TPS load test ($ACCOUNTS accounts, ${DURATION}s)..."
echo ""
python3 "$ROOTDIR/scripts/load-test.py" \
    --accounts "$ACCOUNTS" \
    --duration "$DURATION"

echo ""
log "Load test complete.  Press Ctrl-C to stop the network."
wait "$REGTEST_PID" 2>/dev/null || true
