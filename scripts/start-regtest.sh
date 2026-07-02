#!/usr/bin/env bash
# start-regtest.sh  –  Bootstrap and run a 5-validator qXRP regtest network.
#
# Usage:
#   ./scripts/start-regtest.sh [<xrpld-binary>] [<runtime-dir>]
#
# Defaults:
#   xrpld-binary : ./build/xrpld
#   runtime-dir  : ./data/regtest
#
# The script is idempotent: if <runtime-dir>/validators.txt already exists
# (i.e. keys were already bootstrapped) it skips Phase 1 and goes straight
# to starting the network.

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
BINARY="${1:-./build/xrpld}"
ROOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
RUNTIME_DIR="${2:-$ROOTDIR/data/regtest}"

NVALIDATORS=5
QUORUM=4
NETWORK_ID=1001
BASE_RPC_PORT=5005
BASE_PEER_PORT=51235
BOOTSTRAP_RPC_PORT=5100

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
die()  { echo "ERROR: $*" >&2; exit 1; }
log()  { echo "[regtest] $*"; }
rpc()  {
    local port="$1" body="$2"
    curl -s -X POST "http://127.0.0.1:${port}" \
         -H 'Content-Type: application/json' \
         -d "$body"
}
json_field() {
    # json_field <json-string> <dotted.path>
    python3 - "$1" "$2" <<'PY'
import sys, json
data = json.loads(sys.argv[1])
for key in sys.argv[2].split('.'):
    data = data[key]
print(data)
PY
}

# ---------------------------------------------------------------------------
# Pre-flight checks
# ---------------------------------------------------------------------------
[[ -x "$BINARY" ]] || die "Binary not found or not executable: $BINARY"
command -v curl   >/dev/null 2>&1 || die "curl is required"
command -v python3 >/dev/null 2>&1 || die "python3 is required"

log "Binary  : $BINARY"
log "Runtime : $RUNTIME_DIR"

# ---------------------------------------------------------------------------
# Phase 1 – Key generation (skipped if already done)
# ---------------------------------------------------------------------------
mkdir -p "$RUNTIME_DIR"
VALFILE="$(cd "$RUNTIME_DIR" && pwd)/validators.txt"

declare -a SEEDS
declare -a PUBKEYS

if [[ -f "$VALFILE" ]]; then
    log "validators.txt already exists – loading existing keys..."
    SEEDFILE="$RUNTIME_DIR/seeds.txt"
    [[ -f "$SEEDFILE" ]] || die "Found validators.txt but missing seeds.txt – remove $RUNTIME_DIR to re-bootstrap"
    mapfile -t SEEDS   < "$SEEDFILE"
    mapfile -t PUBKEYS < <(grep -v '^\[' "$VALFILE" | grep -v '^$')
    log "Loaded ${#PUBKEYS[@]} public keys from previous bootstrap"
else
    log "Phase 1: generating validator keys via bootstrap node..."

    BOOTSTRAP_DIR="$RUNTIME_DIR/bootstrap"
    mkdir -p "$BOOTSTRAP_DIR/db"

    cat > "$BOOTSTRAP_DIR/xrpld.cfg" <<CFG
[node_size]
tiny

[ledger_history]
0

[server]
port_rpc

[port_rpc]
port = $BOOTSTRAP_RPC_PORT
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[node_db]
type = NuDB
path = $BOOTSTRAP_DIR/db

[database_path]
$BOOTSTRAP_DIR

[debug_logfile]
$BOOTSTRAP_DIR/debug.log
CFG

    "$BINARY" --conf "$BOOTSTRAP_DIR/xrpld.cfg" --standalone \
        >> "$BOOTSTRAP_DIR/stdout.log" 2>&1 &
    BOOTSTRAP_PID=$!

    # Wait up to 30 s for the RPC port to come up
    log "  Waiting for bootstrap node (pid $BOOTSTRAP_PID)..."
    for i in $(seq 1 30); do
        CODE=$(curl -s -o /dev/null -w '%{http_code}' \
                    -X POST "http://127.0.0.1:$BOOTSTRAP_RPC_PORT" \
                    -H 'Content-Type: application/json' \
                    -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
        [[ "$CODE" == "200" ]] && break
        [[ $i -eq 30 ]] && { kill "$BOOTSTRAP_PID" 2>/dev/null; die "Bootstrap node did not start in time"; }
        sleep 1
    done

    for i in $(seq 1 $NVALIDATORS); do
        RESP=$(rpc "$BOOTSTRAP_RPC_PORT" \
               '{"method":"wallet_propose","params":[{"key_type":"falcon512"}]}')
        SECRET=$(json_field "$RESP" result.falcon_secret)
        PUBKEY=$(json_field "$RESP" result.public_key_hex)
        SEEDS+=("$SECRET")
        PUBKEYS+=("$PUBKEY")
        log "  Validator $i  falcon_pk=${PUBKEY:0:16}…"
    done

    log "  Stopping bootstrap node..."
    kill "$BOOTSTRAP_PID" 2>/dev/null || true
    wait "$BOOTSTRAP_PID" 2>/dev/null || true

    # Persist generated keys (falcon_secret per line — used for consensus + tx signing)
    printf '%s\n' "${SEEDS[@]}"   > "$RUNTIME_DIR/seeds.txt"
    printf '[validators]\n'        > "$VALFILE"
    printf '%s\n' "${PUBKEYS[@]}" >> "$VALFILE"
    log "  Wrote $VALFILE"
fi

# ---------------------------------------------------------------------------
# Phase 2 – Write per-validator configs and start the network
# ---------------------------------------------------------------------------
log "Phase 2: writing validator configs..."

for i in $(seq 1 $NVALIDATORS); do
    mkdir -p "$RUNTIME_DIR/v${i}/db"

    RPC_PORT=$((BASE_RPC_PORT  + i - 1))
    PEER_PORT=$((BASE_PEER_PORT + i - 1))

    # Build [ips_fixed] for all other validators
    IPS_FIXED=""
    for j in $(seq 1 $NVALIDATORS); do
        [[ "$j" == "$i" ]] && continue
        OTHER_PEER=$((BASE_PEER_PORT + j - 1))
        IPS_FIXED+="127.0.0.1 ${OTHER_PEER}"$'\n'
    done

    cat > "$RUNTIME_DIR/v${i}/xrpld.cfg" <<CFG
[network_id]
$NETWORK_ID

[node_size]
tiny

[ledger_history]
256

[validation_quorum]
$QUORUM

[validation_falcon_secret]
${SEEDS[$((i-1))]}

[validators_file]
$VALFILE

[features]
ProofOfParticipation

[peer_private]
1

[server]
port_rpc_admin_local
port_peer

[port_rpc_admin_local]
port = $RPC_PORT
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[port_peer]
port = $PEER_PORT
ip = 0.0.0.0
protocol = peer

[node_db]
type = NuDB
path = $RUNTIME_DIR/v${i}/db
advisory_delete = 0

[database_path]
$RUNTIME_DIR/v${i}

[debug_logfile]
$RUNTIME_DIR/v${i}/debug.log

[ips_fixed]
${IPS_FIXED}

[transaction_queue]
# Minimum txs allowed into a ledger before fee escalation kicks in.
# Higher = more cheap txs per ledger, better steady-state throughput.
minimum_txn_in_ledger = 100
# Target txs-per-ledger that fee escalation works toward.
# At ~3 s/ledger this gives ~333 TPS without escalating fees.
target_txn_in_ledger = 1000
# Queue holds 30 ledgers-worth of transactions in reserve.
ledgers_in_queue = 30
# Absolute floor on queue capacity regardless of ledger size.
minimum_queue_size = 10000
# Max queued transactions per account.
# Load-test sends many in-flight per account, so raise this significantly.
maximum_txn_per_account = 100
# After a normal-speed ledger, grow the expected ledger size by 25%.
normal_consensus_increase_percent = 25
# After a slow ledger, only shrink the expected ledger size by 25%
# (default 50%) so a single slow round doesn't collapse throughput.
slow_consensus_decrease_percent = 25
CFG

    log "  Validator $i  RPC=http://127.0.0.1:${RPC_PORT}  peer=51235+$((i-1))"
done

# ---------------------------------------------------------------------------
# Start all validators
# ---------------------------------------------------------------------------
declare -a PIDS=()

cleanup() {
    log "Shutting down validators..."
    for PID in "${PIDS[@]}"; do
        kill "$PID" 2>/dev/null || true
    done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Write a PID file so stop-regtest.sh can find the processes
rm -f "$RUNTIME_DIR/pids.txt"

log "Starting $NVALIDATORS validators..."
for i in $(seq 1 $NVALIDATORS); do
    "$BINARY" --conf "$RUNTIME_DIR/v${i}/xrpld.cfg" --start \
        >> "$RUNTIME_DIR/v${i}/stdout.log" 2>&1 &
    PID=$!
    PIDS+=($PID)
    echo "$PID" >> "$RUNTIME_DIR/pids.txt"
    log "  Validator $i started (pid $PID)"
    sleep 0.3
done

# ---------------------------------------------------------------------------
# Wait for network convergence
# ---------------------------------------------------------------------------
log "Waiting for network to converge (ledger ≥ 3)..."
CONVERGED=false
for ATTEMPT in $(seq 1 90); do
    RESP=$(rpc "$BASE_RPC_PORT" \
               '{"method":"server_info","params":[{}]}' 2>/dev/null || echo '{}')
    SEQ=$(python3 - "$RESP" <<'PY'
import sys, json
try:
    r = json.loads(sys.argv[1])
    print(r['result']['info']['validated_ledger']['seq'])
except Exception:
    print(0)
PY
)
    if [[ "$SEQ" -ge 3 ]]; then
        log "Network converged at ledger $SEQ  ✓"
        CONVERGED=true
        break
    fi
    [[ $((ATTEMPT % 10)) -eq 0 ]] && log "  Still waiting... (attempt $ATTEMPT, last ledger=$SEQ)"
    sleep 2
done

$CONVERGED || log "WARNING: network did not reach ledger 3 within timeout – check $RUNTIME_DIR/v1/debug.log"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
log "========================================="
log "qXRP regtest network is RUNNING"
log "========================================="
for i in $(seq 1 $NVALIDATORS); do
    RPC_PORT=$((BASE_RPC_PORT + i - 1))
    log "  Validator $i  http://127.0.0.1:${RPC_PORT}  (pid ${PIDS[$((i-1))]})"
done
echo ""
log "Validator public keys:"
for i in $(seq 1 $NVALIDATORS); do
    log "  v$i  ${PUBKEYS[$((i-1))]}"
done
echo ""
log "Logs : $RUNTIME_DIR/vN/debug.log"
log "Stop : ./scripts/stop-regtest.sh  (or Ctrl-C)"
log "========================================="

# Block until interrupted (trap cleanup handles shutdown)
while true; do
    # Check that at least one validator is still alive
    ALIVE=0
    for PID in "${PIDS[@]}"; do
        kill -0 "$PID" 2>/dev/null && ALIVE=$((ALIVE+1))
    done
    [[ $ALIVE -eq 0 ]] && { log "All validators exited unexpectedly"; exit 1; }
    sleep 5
done
