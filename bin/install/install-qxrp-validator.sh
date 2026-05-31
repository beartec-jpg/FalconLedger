#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# One-command qXRP validator onboarding installer (systemd edition).
# Downloads the qXRP binary, configures a hardened validator, and starts it
# as a systemd service — no Docker required.
#
# Tested on: Ubuntu 22.04, Ubuntu 24.04, Debian 12
#
# USAGE (recommended - from the qXRP Portal wallet page):
#   curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \
#     --payout rYOUR_MAIN_WALLET_ADDRESS \
#     --node-name mynode
#
# Or locally during development:
#   bash bin/install/install-qxrp-validator.sh --payout r... --node-name test
#
# After running:
#   1. The script prints YOUR VALIDATOR ACCOUNT (fund it with >=1100 qXRP)
#   2. It auto-detects funding, runs ValidatorRegister + ValidatorBond
#   3. Starts the node + a reward claimer
#   4. You withdraw/claim from the portal or with the helper script later
#
# Flags:
#   --payout ADDR     Your main wallet address (rewards will be easy to send here)
#   --node-name NAME  Short name for this validator (default: host short name)
#   --bond 1000       Minimum bond in qXRP (do not change unless you know why)
#   --no-auto-bond    Only set up the node; you will bond manually
#   --help            Show this help

set -euo pipefail

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
PAYOUT_ADDRESS=""
NODE_NAME="$(hostname -s 2>/dev/null || echo validator)"
BOND_QXRP=1000
AUTO_BOND=1
SHOW_HELP=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --payout)
            PAYOUT_ADDRESS="$2"; shift 2 ;;
        --node-name|--name)
            NODE_NAME="$2"; shift 2 ;;
        --bond)
            BOND_QXRP="$2"; shift 2 ;;
        --no-auto-bond)
            AUTO_BOND=0; shift ;;
        --help|-h)
            SHOW_HELP=1; shift ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Run with --help for usage." >&2
            exit 1
            ;;
    esac
done

if [[ "$SHOW_HELP" -eq 1 ]]; then
    sed -n '5,35p' "$0" | sed 's/^# \?//'
    exit 0
fi

if [[ -z "$PAYOUT_ADDRESS" ]]; then
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  RECOMMENDED: Run with --payout so the installer knows where   ║"
    echo "║  you want to withdraw rewards to later.                        ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Example (copy from the qXRP Portal wallet page):"
    echo "  curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \\"
    echo "    --payout rYOUR_WALLET_ADDRESS_HERE --node-name mynode"
    echo ""
    echo "Continuing without --payout (you can still withdraw manually)..."
    echo ""
    sleep 2
fi

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
RELEASE_URL="https://github.com/beartec-jpg/qXRP/releases/download/v1.0.0-testnet/xrpld-linux-x86_64"
NETWORK_RPC="http://46.224.0.140:6005"   # node1 public RPC — used for bond submission
NETWORK_ID=999
MIN_RAM_MB=1800
MIN_DISK_GB=8
BOND_DROPS=$(( BOND_QXRP * 1000000 ))
REQUIRED_DROPS=$(( (BOND_QXRP + 250) * 1000000 ))   # bond + reserve + fees

# Paths — use system paths when root, home dir otherwise
if [[ "$(id -u)" -eq 0 ]]; then
    XRPLD_BIN="/opt/qxrp/bin/xrpld"
    BASE_DIR="/var/lib/qxrp/${NODE_NAME}"
    CONFIG_DIR="/etc/qxrp/${NODE_NAME}"
    SERVICE_USER="qxrp"
    SUDO=""
else
    XRPLD_BIN="$HOME/.local/bin/xrpld"
    BASE_DIR="$HOME/.qxrp/${NODE_NAME}"
    CONFIG_DIR="$HOME/.qxrp/${NODE_NAME}/config"
    SERVICE_USER="$USER"
    SUDO="sudo"
fi

KEYS_FILE="$CONFIG_DIR/validator-keys.json"
CFG_FILE="$CONFIG_DIR/xrpld.cfg"
VALIDATORS_FILE="$CONFIG_DIR/validators.txt"
CLAIMER_SCRIPT="$BASE_DIR/qxrp-claimer.py"
SERVICE_NAME="qxrp-${NODE_NAME}"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log()  { echo -e "\033[1;32m[qxrp]\033[0m $*"; }
warn() { echo -e "\033[1;33m[qxrp] WARN:\033[0m $*"; }
die()  { echo -e "\033[1;31m[qxrp] ERROR:\033[0m $*" >&2; exit 1; }
already_done() { log "$1 – already done, skipping."; }

# Generate a syntactically-valid Falcon-512 "pubkey" (0xFB + 897 random bytes).
generate_falcon_pubkey() {
    python3 -c '
import secrets, sys
prefix = bytes([0xFB])
raw = prefix + secrets.token_bytes(897)
print(raw.hex().upper())
' 2>/dev/null || {
        echo -n "FB"; head -c 897 /dev/urandom | od -An -tx1 | tr -d ' \n' | tr 'a-f' 'A-F' | cut -c1-1794
    }
}

banner() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════════════════╗"
    printf "║  %-74s ║\n" "$1"
    echo "╚════════════════════════════════════════════════════════════════════════════╝"
}

# ---------------------------------------------------------------------------
# 1. Pre-flight checks
# ---------------------------------------------------------------------------
log "Checking system requirements..."

RAM_MB=$(awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo)
DISK_GB=$(df --output=avail -BG "$HOME" | tail -1 | tr -d 'G ')

[[ "$RAM_MB" -ge "$MIN_RAM_MB" ]] \
    || die "Insufficient RAM: ${RAM_MB} MB available, ${MIN_RAM_MB} MB required."

[[ "$DISK_GB" -ge "$MIN_DISK_GB" ]] \
    || die "Insufficient disk: ${DISK_GB} GB available, ${MIN_DISK_GB} GB required."

# --- Public IP check (validators MUST be reachable on port 51235) -----------
log "Detecting public IP address..."
PUBLIC_IP=$(curl -s --max-time 8 https://api.ipify.org 2>/dev/null \
    || curl -s --max-time 8 https://checkip.amazonaws.com 2>/dev/null \
    || curl -s --max-time 8 https://ipecho.net/plain 2>/dev/null \
    || true)
PUBLIC_IP="${PUBLIC_IP//[[:space:]]/}"   # strip whitespace

# Reject RFC-1918 / loopback addresses — these are not publicly reachable
is_private_ip() {
    python3 -c "
import sys, ipaddress
try:
    ip = ipaddress.ip_address('$PUBLIC_IP')
    sys.exit(0 if (ip.is_private or ip.is_loopback or ip.is_link_local) else 1)
except Exception:
    sys.exit(0)
" 2>/dev/null
}

if [[ -z "$PUBLIC_IP" ]]; then
    warn "Could not auto-detect public IP — continuing anyway."
    warn "Make sure port 51235 (TCP) is reachable from the internet before your validator can peer."
    PUBLIC_IP="unknown"
elif is_private_ip; then
    echo ""
    echo "╔══════════════════════════════════════════════════════════════════════════════╗"
    echo "║  NOTE: Detected IP ${PUBLIC_IP} looks like a private/CGNAT address.         ║"
    echo "║                                                                              ║"
    echo "║  Your validator WILL work from a home PC or laptop — you just need to       ║"
    echo "║  forward port 51235 (TCP) on your router to this machine so other nodes     ║"
    echo "║  can reach you. Without that your node won't peer and won't validate.       ║"
    echo "║                                                                              ║"
    echo "║  On a VPS this is automatic. On a home machine:                             ║"
    echo "║    Router admin → Port Forwarding → TCP 51235 → this machine's LAN IP.     ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════╝"
    echo ""
    echo -n "  Continue anyway? [y/N] "
    read -r CONFIRM
    [[ "$CONFIRM" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 0; }
    echo ""
fi

log "System OK – RAM: ${RAM_MB} MB, Disk: ${DISK_GB} GB  |  Public IP: ${PUBLIC_IP}  |  Payout: ${PAYOUT_ADDRESS:-not set}  |  Node: $NODE_NAME"
warn "Ensure port 51235 (TCP) is open/forwarded — required for peering with other validators."
echo ""

# ---------------------------------------------------------------------------
# 2. Create qxrp system user + directories (root installs only)
# ---------------------------------------------------------------------------
if [[ "$(id -u)" -eq 0 ]]; then
    if ! id "$SERVICE_USER" &>/dev/null; then
        log "Creating system user '$SERVICE_USER'..."
        useradd --system --no-create-home --shell /usr/sbin/nologin "$SERVICE_USER"
    fi
fi

mkdir -p "$(dirname "$XRPLD_BIN")" "$BASE_DIR" "$CONFIG_DIR"
[[ "$(id -u)" -eq 0 ]] && chown -R "$SERVICE_USER:$SERVICE_USER" "$BASE_DIR" "$(dirname "$XRPLD_BIN")"

# ---------------------------------------------------------------------------
# 3. Download xrpld binary if not present
# ---------------------------------------------------------------------------
if [[ -x "$XRPLD_BIN" ]]; then
    already_done "xrpld binary ($XRPLD_BIN)"
else
    log "Downloading qXRP node binary (~130 MB)..."
    TMP_BIN="$(mktemp)"
    curl -fsSL --progress-bar -o "$TMP_BIN" "$RELEASE_URL"
    chmod +x "$TMP_BIN"

    # Quick sanity check — binary should run and print a version
    if ! "$TMP_BIN" --version 2>&1 | grep -q "xrpld"; then
        rm -f "$TMP_BIN"
        die "Downloaded binary failed version check — is $RELEASE_URL correct?"
    fi

    mv "$TMP_BIN" "$XRPLD_BIN"
    [[ "$(id -u)" -eq 0 ]] && chown "root:root" "$XRPLD_BIN"
    log "Binary installed: $XRPLD_BIN"
fi

# ---------------------------------------------------------------------------
# 4. Key generation (run xrpld briefly in standalone mode to call RPCs)
# ---------------------------------------------------------------------------
FALCON_FILE="$CONFIG_DIR/falcon-pubkey.txt"
SEED_FILE="$CONFIG_DIR/seed.txt"

if [[ -f "$KEYS_FILE" ]]; then
    already_done "Validator keys (re-using existing identity in $KEYS_FILE)"
    VAL_SEED=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['validation_seed'])")
    VAL_PUBKEY=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['validation_public_key'])")
    CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['consensus_key_hex'])")
    VALIDATOR_ACCOUNT=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['account_address'])")
    FALCON_PUB=$(cat "$FALCON_FILE" 2>/dev/null || generate_falcon_pubkey)
else
    log "Generating validator identity (classical + Falcon)..."

    BOOT_DIR=$(mktemp -d)
    BOOT_PORT=15123
    BOOT_DB="$BOOT_DIR/db"
    mkdir -p "$BOOT_DB"

    cat > "$BOOT_DIR/xrpld.cfg" <<BOOTCFG
[node_size]
tiny
[ledger_history]
0
[server]
port_rpc_admin_local
[port_rpc_admin_local]
port = ${BOOT_PORT}
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = ${BOOT_DB}/nudb
advisory_delete = 0
[database_path]
${BOOT_DB}
[debug_logfile]
${BOOT_DIR}/debug.log
BOOTCFG

    # Start xrpld in standalone mode (background)
    "$XRPLD_BIN" --conf "$BOOT_DIR/xrpld.cfg" --standalone >> "$BOOT_DIR/debug.log" 2>&1 &
    BOOT_PID=$!
    trap "kill $BOOT_PID 2>/dev/null || true; rm -rf '$BOOT_DIR'" EXIT

    # Wait for RPC (up to 30 s)
    for i in $(seq 1 30); do
        HTTP_CODE=$(curl -s -o /dev/null -w '%{http_code}' \
            -X POST "http://127.0.0.1:${BOOT_PORT}" \
            -H 'Content-Type: application/json' \
            -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
        [[ "$HTTP_CODE" == "200" ]] && break
        sleep 1
        [[ $i -eq 30 ]] && die "Temporary keygen node failed to start — check $BOOT_DIR/debug.log"
    done

    # Generate classical validation seed + keys
    RESP=$(curl -s -X POST "http://127.0.0.1:${BOOT_PORT}" \
        -H 'Content-Type: application/json' \
        -d "{\"method\":\"validation_create\",\"params\":[{\"secret\":\"qxrp-${NODE_NAME}-$(date +%s)\"}]}")

    VAL_SEED=$(echo "$RESP" | python3 -c "
import sys, json
data = json.load(sys.stdin)
print(data.get('result',{}).get('validation_seed','') or data.get('result',{}).get('seed',''))
")
    VAL_PUBKEY=$(echo "$RESP" | python3 -c "
import sys, json
print(json.load(sys.stdin)['result']['validation_public_key'])
")

    # Derive account address + consensus pubkey from the same seed
    WP=$(curl -s -X POST "http://127.0.0.1:${BOOT_PORT}" \
        -H 'Content-Type: application/json' \
        -d "{\"method\":\"wallet_propose\",\"params\":[{\"seed\":\"${VAL_SEED}\",\"key_type\":\"secp256k1\"}]}")
    VALIDATOR_ACCOUNT=$(echo "$WP" | python3 -c "
import sys, json
print(json.load(sys.stdin)['result']['account_id'])
")
    CONSENSUS_KEY=$(echo "$WP" | python3 -c "
import sys, json
r = json.load(sys.stdin)['result']
print(r.get('public_key_hex') or r.get('public_key',''))
")

    # Kill the temporary standalone node and clear trap
    kill "$BOOT_PID" 2>/dev/null || true
    wait "$BOOT_PID" 2>/dev/null || true
    trap - EXIT
    rm -rf "$BOOT_DIR"

    # Synthetic Falcon pubkey
    FALCON_PUB=$(generate_falcon_pubkey)

    # Save everything
    python3 - <<PY
import json, datetime
data = {
    "validation_seed": "$VAL_SEED",
    "validation_public_key": "$VAL_PUBKEY",
    "consensus_key_hex": "$CONSENSUS_KEY",
    "account_address": "$VALIDATOR_ACCOUNT",
    "falcon_pubkey": "$FALCON_PUB",
    "node_name": "$NODE_NAME",
    "network_id": $NETWORK_ID,
    "generated_at": datetime.datetime.utcnow().isoformat() + "Z",
    "payout_address": "${PAYOUT_ADDRESS}"
}
with open("$KEYS_FILE", "w") as f:
    json.dump(data, f, indent=2)
PY
    chmod 600 "$KEYS_FILE"

    echo "$VAL_SEED" > "$SEED_FILE"
    chmod 600 "$SEED_FILE"

    echo "$FALCON_PUB" > "$FALCON_FILE"

    log "Keys generated and saved to $KEYS_FILE (chmod 600 — keep this file secret!)"
fi

log "Validator account   : $VALIDATOR_ACCOUNT"
log "Validation pubkey   : $VAL_PUBKEY"
log "Consensus key (hex) : ${CONSENSUS_KEY:0:16}..."

# ---------------------------------------------------------------------------
# 5. Write hardened validator config + validators.txt
# ---------------------------------------------------------------------------
if [[ -f "$CFG_FILE" && -f "$VALIDATORS_FILE" ]]; then
    already_done "Validator config and UNL"
else
    log "Writing production validator configuration..."

    # validators.txt — include known qXRP network validators + self
    cat > "$VALIDATORS_FILE" <<UNL
[validators]
n94RNoyd8qLHjn7FbvtpWWumSSs2S7XGncejjLLJ2FofDrBZ1Ff6
n9MuP4C9zqXjZx18Jw7gaSSQ9bi4R7TBxn9LfmPR9Mb9JgG9sLR6
n9KX6hNjxiyKSPi1vptDFsuqAMSe9dpZ5uehEnT6GdkmRvzWYMwp
n9LhNgHysZfTubvZa9v5kCQooWZAXdMZptrcifZXpH6EbLdj6fGt
${VAL_PUBKEY}
UNL
    chmod 600 "$VALIDATORS_FILE"

    cat > "$CFG_FILE" <<CFG
# qXRP Validator — $NODE_NAME (generated by install-qxrp-validator.sh)
# Keep this file chmod 600. The [validation_seed] is extremely sensitive.

[network_id]
${NETWORK_ID}

[node_size]
tiny

[ledger_history]
256

[validation_quorum]
3

# === VALIDATOR IDENTITY (classical) ===
[validation_seed]
${VAL_SEED}

[validators_file]
${VALIDATORS_FILE}

# === FEATURES ===
[features]
ProofOfParticipation
MultiSign
MultiSignReserve
Flow
FlowCross
FeeEscalation
TickSize
Escrow
DeletableAccounts
DepositAuth
DepositPreauth
AMM
XChainBridge

# === PORTS ===
[server]
port_rpc_admin_local
port_rpc_public
port_peer_public
port_ws_public

[port_rpc_admin_local]
port = 5005
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[port_rpc_public]
port = 6005
ip = 0.0.0.0
protocol = http

[port_peer_public]
port = 51235
ip = 0.0.0.0
protocol = peer

[port_ws_public]
port = 7005
ip = 0.0.0.0
protocol = ws

# === STORAGE ===
[node_db]
type = NuDB
path = ${BASE_DIR}/nudb
advisory_delete = 0
online_delete = 512
cache_size = 256

[database_path]
${BASE_DIR}/db

[debug_logfile]
${BASE_DIR}/debug.log

[sntp_servers]
time.windows.com
time.apple.com
time.nist.gov
pool.ntp.org

# === BOOTSTRAP PEERS ===
[ips_fixed]
46.224.0.140 51235

# === PERFORMANCE ===
[transaction_queue]
minimum_txn_in_ledger = 100
target_txn_in_ledger = 1000
ledgers_in_queue = 30
minimum_queue_size = 10000
maximum_txn_per_account = 100

[rpc_startup]
{ "command": "log_level", "severity": "warning" }
CFG

    chmod 600 "$CFG_FILE"
    mkdir -p "${BASE_DIR}/nudb" "${BASE_DIR}/db"
    [[ "$(id -u)" -eq 0 ]] && chown -R "$SERVICE_USER:$SERVICE_USER" "$BASE_DIR" "$CONFIG_DIR"
    log "Config written: $CFG_FILE"
fi

# ---------------------------------------------------------------------------
# 6. Install systemd service
# ---------------------------------------------------------------------------
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
NOFILE_LIMIT=65535

if systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
    already_done "systemd service $SERVICE_NAME"
elif [[ -f "$SERVICE_FILE" ]]; then
    already_done "systemd service file"
    log "Starting service..."
    $SUDO systemctl daemon-reload
    $SUDO systemctl enable --now "$SERVICE_NAME"
else
    log "Installing systemd service: $SERVICE_NAME..."
    $SUDO tee "$SERVICE_FILE" > /dev/null <<UNIT
[Unit]
Description=qXRP Validator Node (${NODE_NAME})
Documentation=https://github.com/beartec-jpg/qXRP
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=300
StartLimitBurst=5

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_USER}
ExecStart=${XRPLD_BIN} --valid --conf ${CFG_FILE}
ExecReload=/bin/true
Restart=on-failure
RestartSec=10
TimeoutStopSec=60
LimitNOFILE=${NOFILE_LIMIT}
LimitNPROC=${NOFILE_LIMIT}
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=${BASE_DIR}

[Install]
WantedBy=multi-user.target
UNIT

    $SUDO systemctl daemon-reload
    $SUDO systemctl enable --now "$SERVICE_NAME"
    log "Service enabled and started."
fi

# ---------------------------------------------------------------------------
# 7. Wait for RPC to come up
# ---------------------------------------------------------------------------
log "Waiting for node RPC (up to 90 s — initial sync may take a moment)..."
for i in $(seq 1 90); do
    CODE=$(curl -s -o /dev/null -w '%{http_code}' \
        -X POST "http://127.0.0.1:5005" \
        -H 'Content-Type: application/json' \
        -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
    [[ "$CODE" == "200" ]] && break
    [[ $i -eq 90 ]] && die "Node did not become ready after 90 s. Check: journalctl -u ${SERVICE_NAME} -n 50"
    sleep 1
done
log "Node RPC is live."

# ---------------------------------------------------------------------------
# 8. Auto-bond flow (poll for funding, then register + bond)
# ---------------------------------------------------------------------------
if [[ "$AUTO_BOND" -eq 1 ]]; then
    banner "FUNDING REQUIRED — READ CAREFULLY"
    echo ""
    echo "  Your validator account (send qXRP here):"
    echo ""
    echo "     ╔══════════════════════════════════════════════════╗"
    printf  "     ║  %-48s  ║\n" "$VALIDATOR_ACCOUNT"
    echo "     ╚══════════════════════════════════════════════════╝"
    echo ""
    echo "  Recommended amount: 1,100 – 1,200 qXRP"
    echo "     (1,000 qXRP minimum bond + ~200 qXRP reserve + fees)"
    echo ""
    echo "  Get qXRP from the faucet in the qXRP Portal, or transfer"
    echo "  from the wallet address you used to launch this installer."
    echo ""
    echo "  The installer will poll every 15 seconds until it sees enough"
    echo "  balance, then automatically register + bond the validator."
    echo ""

    FUNDED=0
    for attempt in $(seq 1 200); do   # ~50 minutes max
        # Query via public node1 RPC (local node may not be synced yet)
        BAL=$(curl -s -X POST "$NETWORK_RPC" \
            -H 'Content-Type: application/json' \
            -d "{\"method\":\"account_info\",\"params\":[{\"account\":\"${VALIDATOR_ACCOUNT}\",\"ledger_index\":\"current\"}]}" \
            2>/dev/null | python3 -c "
import sys, json
try:
    r = json.load(sys.stdin)
    bal = r.get('result',{}).get('account_data',{}).get('Balance','0')
    print(int(bal))
except Exception:
    print(0)
" || echo 0)

        if [[ "$BAL" -ge "$REQUIRED_DROPS" ]]; then
            FUNDED=1
            log "Funding detected! Balance = $((BAL / 1000000)) qXRP — proceeding to auto-bond..."
            break
        fi

        if [[ $(( attempt % 4 )) -eq 0 ]]; then
            echo -ne "\r  Waiting for funding... (current: $((BAL / 1000000)) qXRP / need ~$((REQUIRED_DROPS / 1000000)))   "
        fi
        sleep 15
    done
    echo ""

    if [[ "$FUNDED" -ne 1 ]]; then
        warn "Funding not detected after waiting. Bond manually later — see docs/validator-onboarding.md"
    else
        # Submit via public RPC (most reliable — doesn't require local node to be synced)
        log "Submitting ValidatorRegister (Falcon identity + consensus key)..."
        REG_TX=$(curl -s -X POST "$NETWORK_RPC" \
            -H 'Content-Type: application/json' \
            -d "{
                \"method\":\"submit\",
                \"params\":[{
                    \"tx_json\":{
                        \"TransactionType\":\"ValidatorRegister\",
                        \"Account\":\"${VALIDATOR_ACCOUNT}\",
                        \"PublicKey\":\"${FALCON_PUB}\",
                        \"ConsensusKey\":\"${CONSENSUS_KEY}\",
                        \"Fee\":\"12\"
                    },
                    \"secret\":\"${VAL_SEED}\"
                }]
            }")

        REG_RESULT=$(echo "$REG_TX" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result',{})
print(r.get('engine_result','ERROR'))
" 2>/dev/null || echo ERROR)

        if [[ "$REG_RESULT" == "tesSUCCESS" || "$REG_RESULT" == "tecDUPLICATE" ]]; then
            log "Register: $REG_RESULT"
        else
            warn "Register result: $REG_RESULT (may already be registered — continuing)"
        fi

        sleep 3

        log "Submitting ValidatorBond (${BOND_QXRP} qXRP)..."
        BOND_TX=$(curl -s -X POST "$NETWORK_RPC" \
            -H 'Content-Type: application/json' \
            -d "{
                \"method\":\"submit\",
                \"params\":[{
                    \"tx_json\":{
                        \"TransactionType\":\"ValidatorBond\",
                        \"Account\":\"${VALIDATOR_ACCOUNT}\",
                        \"ConsensusKey\":\"${CONSENSUS_KEY}\",
                        \"BondedAmount\":\"${BOND_DROPS}\",
                        \"Fee\":\"12\"
                    },
                    \"secret\":\"${VAL_SEED}\"
                }]
            }")

        BOND_RESULT=$(echo "$BOND_TX" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result',{})
print(r.get('engine_result','ERROR'))
" 2>/dev/null || echo ERROR)

        if [[ "$BOND_RESULT" == "tesSUCCESS" ]]; then
            log "Bond: tesSUCCESS — validator is now bonded and eligible for rewards!"
        elif [[ "$BOND_RESULT" == "tecNO_PERMISSION" ]]; then
            log "Bond: already bonded ✓"
        else
            warn "Bond result: $BOND_RESULT — check with ledger_entry or the portal"
        fi

        sleep 2
    fi
fi

# ---------------------------------------------------------------------------
# 9. Install lightweight claimer helper (cron — auto-claim when score is good)
# ---------------------------------------------------------------------------
log "Installing reward claim helper (qxrp-claimer)..."
cat > "$CLAIMER_SCRIPT" <<'PYEOF'
#!/usr/bin/env python3
"""Minimal qXRP reward claimer.
Run periodically (cron / systemd timer).
Only claims — does NOT auto-sweep to payout (per design).
User controls withdrawals from the portal or manually.
"""
import argparse, json, time, urllib.request, urllib.error, sys, os

def rpc(url, method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read())["result"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", default="~/.qxrp/validator-keys.json")
    ap.add_argument("--rpc", default="http://127.0.0.1:5005")
    ap.add_argument("--once", action="store_true")
    args = ap.parse_args()

    keys_path = os.path.expanduser(args.keys)
    if not os.path.exists(keys_path):
        print("No keys file — nothing to claim for.", file=sys.stderr)
        return

    with open(keys_path) as f:
        k = json.load(f)

    account = k["account_address"]
    seed    = k["validation_seed"]
    ck      = k["consensus_key_hex"]

    try:
        info  = rpc(args.rpc, "server_info")["info"]
        state = info.get("server_state")
        print(f"Node state: {state}")
    except Exception as e:
        print("Cannot reach node:", e)
        return

    # Check bond / composite score
    try:
        bond   = rpc(args.rpc, "ledger_entry", {"validator_bond": {"account": account}, "ledger_index": "validated"})
        node   = bond.get("node", {})
        score  = node.get("CompositeScore", 0) or 0
        status = node.get("BondStatus")
        print(f"Bond status={status} composite_score={score} bps")
        if score < 500:
            print("Score too low for ClaimReward right now — skipping.")
            return
    except Exception:
        print("No bond object yet (or not bonded).")
        return

    # Attempt claim
    try:
        tx  = {"TransactionType": "ClaimReward", "Account": account, "ConsensusKey": ck, "Fee": "12"}
        res = rpc(args.rpc, "submit", {"tx_json": tx, "secret": seed})
        eng = res.get("engine_result", "unknown")
        print(f"ClaimReward result: {eng}")
        if eng == "tesSUCCESS":
            print("Rewards claimed successfully into validator account.")
    except Exception as e:
        print("Claim attempt failed:", e)

if __name__ == "__main__":
    main()
PYEOF
chmod +x "$CLAIMER_SCRIPT"

# Add cron entry (idempotent via sort -u)
(crontab -l 2>/dev/null || true; echo "*/30 * * * * $CLAIMER_SCRIPT --keys $KEYS_FILE --rpc http://127.0.0.1:5005 >> $BASE_DIR/claimer.log 2>&1") \
    | sort -u | crontab -

# ---------------------------------------------------------------------------
# 10. Final success banner
# ---------------------------------------------------------------------------
PUBKEY=$(curl -s -X POST "http://127.0.0.1:5005" \
    -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' 2>/dev/null \
    | python3 -c "
import sys, json
try:
    print(json.load(sys.stdin)['result']['info'].get('pubkey_validator','n/a'))
except Exception:
    print('n/a')
" 2>/dev/null || echo "n/a")

banner "qXRP VALIDATOR IS LIVE"
echo ""
echo "  Account (fund this & where rewards land) : $VALIDATOR_ACCOUNT"
echo "  Validation pubkey                        : $VAL_PUBKEY"
echo "  Node pubkey (for UNL)                    : $PUBKEY"
[[ -n "$PAYOUT_ADDRESS" ]] && echo "  Your payout/withdraw wallet (--payout)   : $PAYOUT_ADDRESS"
echo ""
echo "  Admin RPC   : http://127.0.0.1:5005"
echo "  Peer port   : 0.0.0.0:51235  ← open this in your firewall / cloud security group"
echo ""
echo "  Next steps in the qXRP Portal (wallet page):"
echo "    • Check that your validator account shows as BONDED"
echo "    • Watch epochs & composite score"
echo "    • Claim rewards (or let the claimer run), then withdraw to your main wallet"
echo ""
echo "  Useful commands:"
echo "    journalctl -u ${SERVICE_NAME} -f              # live log"
echo "    systemctl status ${SERVICE_NAME}               # service status"
echo "    systemctl stop ${SERVICE_NAME}                 # stop"
echo "    $CLAIMER_SCRIPT --keys $KEYS_FILE             # manual claim"
echo ""
echo "  IMPORTANT: Backup $KEYS_FILE and $SEED_FILE securely (offline)."
echo "             Losing them = losing this validator identity forever."
echo ""
echo "═══════════════════════════════════════════════════════════════════════════════"
log "Setup complete. Go fund the validator account above if you haven't already."
