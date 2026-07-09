#!/usr/bin/env bash
# =============================================================================
# deploy-cloud-validator.sh — Build qXRP from source and run a validator node
# =============================================================================
#
# A VALIDATOR actively participates in consensus, signs ledger validations,
# and earns qXRP rewards.  A full/tracking node only follows the ledger.
# See the explanation at the bottom of this file for the full comparison.
#
# Tested on: Ubuntu 22.04 LTS, Ubuntu 24.04 LTS
#
# Recommended cloud spec:
#   CPU   : 4+ vCPU
#   RAM   : 16 GB  (8 GB minimum)
#   Disk  : 100 GB SSD (NVMe preferred)
#   OS    : Ubuntu 22.04 or 24.04
#   Net   : static public IP, port 51235 open
#
# Usage:
#   bash deploy-cloud-validator.sh [OPTIONS]
#
# Options:
#   --repo         <url>        Git repo URL  (default: https://github.com/beartec-jpg/qXRP)
#   --branch       <name>       Branch to build (default: develop)
#   --network-id   <id>         Network ID (default: 1001 for new clean testnets)
#   --peers        <ip:port,..> Comma-separated bootstrap peers (other validators)
#   --trusted-keys <key,key,..> Comma-separated validator public keys to trust
#   --quorum       <n>          Validation quorum (default: 4)
#   --install-dir  <path>       Root install directory (default: /opt/qxrp)
#   --data-dir     <path>       Validator data directory (default: /var/lib/qxrp/validator)
#   --node-name    <name>       Service name suffix (default: v1)
#   --skip-build                Re-use existing binary at --install-dir/bin/xrpld
#   --node-size    <size>       xrpld node_size: tiny|small|medium|large|huge
#   --jobs         <n>          Parallel build jobs (default: nproc)
#
# After running this script you MUST complete bonding — see STEP 3 in the
# printed summary.
#
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
REPO_URL="https://github.com/beartec-jpg/qXRP"
BRANCH="develop"
NETWORK_ID=1001
BOOTSTRAP_PEERS=""
TRUSTED_KEYS=""            # comma-separated validator pubkeys to add to validators.txt
QUORUM=4
INSTALL_DIR="/opt/qxrp"
DATA_DIR="/var/lib/qxrp/validator"
NODE_NAME="v1"
SKIP_BUILD=0
RELEASE_URL=""          # e.g. https://github.com/beartec-jpg/qXRP/releases/download/v1.0.0/xrpld-linux-x86_64
NODE_SIZE="medium"
BUILD_JOBS="$(nproc)"
RPC_PORT=5005
PEER_PORT=51235

# ---------------------------------------------------------------------------
# Colours / helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'
log()  { echo -e "${GREEN}[qxrp]${NC} $*"; }
warn() { echo -e "${YELLOW}[qxrp WARN]${NC} $*"; }
die()  { echo -e "${RED}[qxrp ERROR]${NC} $*" >&2; exit 1; }
step() { echo -e "\n${CYAN}══ $* ══${NC}"; }
rpc()  {
    local port="$1" method="$2" params="${3:-{}}"
    curl -s --max-time 5 "http://127.0.0.1:${port}" \
        -H 'Content-Type: application/json' \
        -d "{\"method\":\"${method}\",\"params\":[${params}]}"
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)          REPO_URL="$2";       shift 2 ;;
    --branch)        BRANCH="$2";         shift 2 ;;
    --network-id)    NETWORK_ID="$2";     shift 2 ;;
    --peers)         BOOTSTRAP_PEERS="$2"; shift 2 ;;
    --trusted-keys)  TRUSTED_KEYS="$2";   shift 2 ;;
    --quorum)        QUORUM="$2";         shift 2 ;;
    --install-dir)   INSTALL_DIR="$2";    shift 2 ;;
    --data-dir)      DATA_DIR="$2";       shift 2 ;;
    --node-name)     NODE_NAME="$2";      shift 2 ;;
    --skip-build)    SKIP_BUILD=1;        shift   ;;
    --release-url)  RELEASE_URL="$2";   shift 2 ;;
    --node-size)    NODE_SIZE="$2";      shift 2 ;;
    --jobs)          BUILD_JOBS="$2";     shift 2 ;;
    *) die "Unknown option: $1" ;;
  esac
done

BINARY="$INSTALL_DIR/bin/xrpld"
SRC_DIR="$INSTALL_DIR/src"
SERVICE="qxrp-${NODE_NAME}"

# ---------------------------------------------------------------------------
# Pre-flight
# ---------------------------------------------------------------------------
step "Pre-flight checks"

[[ "$(id -u)" -ne 0 ]] || die "Do not run as root. Run as a sudo-capable user."
command -v sudo >/dev/null || die "'sudo' not found."

RAM_MB=$(awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo)
DISK_GB=$(df --output=avail -BG "$HOME" | tail -1 | tr -d 'G ')
log "CPU: $(nproc) cores | RAM: ${RAM_MB} MB | Disk: ${DISK_GB} GB"
[[ "$RAM_MB" -ge 7500 ]] || warn "Low RAM (${RAM_MB} MB). Recommend ≥8 GB."
[[ "$DISK_GB" -ge 50 ]]  || die  "Insufficient disk: ${DISK_GB} GB. Need ≥50 GB."

. /etc/os-release 2>/dev/null || die "Cannot read /etc/os-release"
log "OS: $PRETTY_NAME"

# ---------------------------------------------------------------------------
# 1. Install system dependencies
# ---------------------------------------------------------------------------
step "Installing system dependencies"

sudo apt-get update -qq

if apt-cache show gcc-13 &>/dev/null 2>&1; then GCC_VER=13
elif apt-cache show gcc-12 &>/dev/null 2>&1; then GCC_VER=12
else die "GCC 12+ required. Try: sudo add-apt-repository ppa:ubuntu-toolchain-r/test"; fi

sudo apt-get install -y --no-install-recommends \
    gcc-${GCC_VER} g++-${GCC_VER} \
    cmake ninja-build git curl wget ca-certificates \
    python3 python3-pip python3-dev python-is-python3 \
    libssl-dev pkg-config ccache lsof htop jq logrotate

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VER} 100 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-${GCC_VER} \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-${GCC_VER} \
    --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-${GCC_VER} \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-${GCC_VER}
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 99
sudo update-alternatives --auto gcc

if ! command -v mold &>/dev/null; then
    MOLD_TAG=$(curl -fsSL "https://api.github.com/repos/rui314/mold/releases/latest" \
        | python3 -c "import sys,json; print(json.load(sys.stdin)['tag_name'])" 2>/dev/null || echo "v2.34.1")
    TMP_MOLD=$(mktemp -d)
    curl -fsSL "https://github.com/rui314/mold/releases/download/${MOLD_TAG}/mold-${MOLD_TAG#v}-x86_64-linux.tar.gz" \
        | tar -xz -C "$TMP_MOLD" --strip-components=1
    sudo cp "$TMP_MOLD/bin/mold" /usr/local/bin/mold
    rm -rf "$TMP_MOLD"
fi

if ! command -v conan &>/dev/null; then
    pip3 install --user --break-system-packages conan 2>/dev/null || pip3 install --user conan
    export PATH="$HOME/.local/bin:$PATH"
fi

log "GCC $(gcc --version | head -1 | awk '{print $NF}') | Conan $(conan --version)"

# ---------------------------------------------------------------------------
# 2. Clone / update repository
# ---------------------------------------------------------------------------
step "Cloning / updating repository"

sudo mkdir -p "$INSTALL_DIR"
sudo chown "$USER:$USER" "$INSTALL_DIR"

if [[ -d "$SRC_DIR/.git" ]]; then
    git -C "$SRC_DIR" fetch origin
    git -C "$SRC_DIR" checkout "$BRANCH"
    git -C "$SRC_DIR" reset --hard "origin/$BRANCH"
else
    git clone --branch "$BRANCH" --depth 1 "$REPO_URL" "$SRC_DIR"
fi

COMMIT=$(git -C "$SRC_DIR" rev-parse --short HEAD)
log "Source: $REPO_URL @ $BRANCH ($COMMIT)"

# ---------------------------------------------------------------------------
# 3. Configure Conan + Build
# ---------------------------------------------------------------------------
if [[ -n "$RELEASE_URL" ]]; then
    step "Downloading pre-built binary from release"
    mkdir -p "$INSTALL_DIR/bin"
    curl -fSL --progress-bar "$RELEASE_URL" -o "$BINARY"
    chmod +x "$BINARY"
    log "Downloaded: $(ls -lh $BINARY | awk '{print $5, $9}')"
elif [[ "$SKIP_BUILD" -eq 1 && -x "$BINARY" ]]; then
    log "Skipping build — using existing binary: $BINARY"
else
    step "Configuring Conan"
    export PATH="$HOME/.local/bin:$PATH"
    conan config install "$SRC_DIR/conan/profiles/" -tf "$(conan config home)/profiles/" 2>/dev/null || true
    if ! conan remote list | grep -q "xrplf"; then
        conan remote add --index 0 xrplf https://conan.ripplex.io
    fi
    PROFILE_FILE="$(conan config home)/profiles/default"
    [[ -f "$PROFILE_FILE" ]] || conan profile detect
    sed -i.bak 's|^compiler\.cppstd=.*$|compiler.cppstd=20|'       "$PROFILE_FILE" 2>/dev/null || true
    sed -i.bak 's|^compiler\.libcxx=.*$|compiler.libcxx=libstdc++11|' "$PROFILE_FILE" 2>/dev/null || true

    step "Building xrpld (~20-60 min first run)"
    BUILD_DIR="$SRC_DIR/.build"
    mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

    conan install "$SRC_DIR" --output-folder . --build missing \
        --settings build_type=Release -c "tools.build:jobs=${BUILD_JOBS}"

    cmake "$SRC_DIR" \
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -Dxrpld=ON -Dtests=OFF -Duse_mold=ON -G Ninja \
        -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"

    cmake --build . -j"${BUILD_JOBS}"
    cd "$SRC_DIR"

    mkdir -p "$INSTALL_DIR/bin"
    BUILT_BIN=$(find "$BUILD_DIR" -name "xrpld" -type f | head -1)
    [[ -x "$BUILT_BIN" ]] || die "Build produced no xrpld binary"
    cp "$BUILT_BIN" "$BINARY" && chmod +x "$BINARY"
fi

log "Binary: $(ls -lh $BINARY | awk '{print $5, $9}')"

# ---------------------------------------------------------------------------
# 4. Generate validator keys
# ---------------------------------------------------------------------------
step "Generating validator keys"

KEYS_FILE="$DATA_DIR/validator-keys.json"
SEEDS_FILE="$DATA_DIR/seeds.txt"
VALIDATORS_FILE="$DATA_DIR/validators.txt"

mkdir -p "$DATA_DIR/db" "$DATA_DIR/nudb"

if [[ -f "$KEYS_FILE" ]]; then
    log "Validator keys already exist — loading from $KEYS_FILE"
    VAL_SEED=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['validation_seed'])")
    VAL_PUBKEY=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['validation_public_key'])")
else
    log "Spinning up temporary standalone node to generate keys..."

    BOOT_DIR=$(mktemp -d)
    mkdir -p "$BOOT_DIR/db"
    BOOT_PORT=15100

    cat > "$BOOT_DIR/xrpld.cfg" <<CFG
[node_size]
tiny
[ledger_history]
0
[server]
port_rpc
[port_rpc]
port = $BOOT_PORT
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = $BOOT_DIR/db
[database_path]
$BOOT_DIR
[debug_logfile]
$BOOT_DIR/debug.log
CFG

    "$BINARY" --conf "$BOOT_DIR/xrpld.cfg" --standalone >> "$BOOT_DIR/stdout.log" 2>&1 &
    BOOT_PID=$!

    for i in $(seq 1 30); do
        CODE=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 \
            "http://127.0.0.1:${BOOT_PORT}" \
            -H 'Content-Type: application/json' \
            -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
        [[ "$CODE" == "200" ]] && break
        [[ $i -eq 30 ]] && { kill "$BOOT_PID" 2>/dev/null; die "Bootstrap node timed out"; }
        sleep 1
    done

    # Generate a unique key derived from hostname + timestamp
    KEY_SEED="qxrp-$(hostname)-$(date +%s)"
    RESP=$(rpc "$BOOT_PORT" "validation_create" "{\"secret\":\"${KEY_SEED}\"}")
    VAL_SEED=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['validation_seed'])")
    VAL_PUBKEY=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['validation_public_key'])")

    # Get the classical secp256k1 public key hex for consensus key registration
    RESP2=$(rpc "$BOOT_PORT" "wallet_propose" "{\"seed\":\"${VAL_SEED}\",\"key_type\":\"secp256k1\"}")
    CONSENSUS_KEY_HEX=$(echo "$RESP2" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['public_key_hex'])")
    ACCOUNT_ADDRESS=$(echo "$RESP2" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['account_id'])")

    kill "$BOOT_PID" 2>/dev/null || true
    wait "$BOOT_PID" 2>/dev/null || true
    rm -rf "$BOOT_DIR"

    # Save keys securely (chmod 600 — contains the seed!)
    python3 - <<PY
import json
data = {
    "validation_seed":       "$VAL_SEED",
    "validation_public_key": "$VAL_PUBKEY",
    "consensus_key_hex":     "$CONSENSUS_KEY_HEX",
    "account_address":       "$ACCOUNT_ADDRESS",
    "node_name":             "$NODE_NAME",
    "network_id":            $NETWORK_ID,
    "generated_at":          "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
with open("$KEYS_FILE", "w") as f:
    json.dump(data, f, indent=2)
PY
    chmod 600 "$KEYS_FILE"
    echo "$VAL_SEED" > "$SEEDS_FILE"
    chmod 600 "$SEEDS_FILE"

    log "Keys saved to $KEYS_FILE  (chmod 600 — keep this secret!)"
fi

log "Validator public key : $VAL_PUBKEY"
log "Consensus key (hex)  : $(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['consensus_key_hex'])" 2>/dev/null || echo 'n/a')"
log "Account address      : $(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['account_address'])" 2>/dev/null || echo 'n/a')"

# ---------------------------------------------------------------------------
# 5. Write validators.txt (trusted UNL)
# ---------------------------------------------------------------------------
step "Writing validators.txt (UNL)"

{
    echo "[validators]"
    echo "$VAL_PUBKEY"  # Trust ourselves
    if [[ -n "$TRUSTED_KEYS" ]]; then
        IFS=',' read -ra KEYS_LIST <<< "$TRUSTED_KEYS"
        for key in "${KEYS_LIST[@]}"; do
            echo "$key"
        done
    fi
} > "$VALIDATORS_FILE"

KEY_COUNT=$(grep -c '^\w' "$VALIDATORS_FILE" || true)
log "validators.txt: $KEY_COUNT trusted key(s)"
[[ "$KEY_COUNT" -lt "$QUORUM" ]] && \
    warn "You have $KEY_COUNT trusted keys but quorum=$QUORUM. Add more with --trusted-keys."

# ---------------------------------------------------------------------------
# 6. Write xrpld.cfg
# ---------------------------------------------------------------------------
step "Writing xrpld.cfg"

IPS_BLOCK="[ips_fixed]"$'\n'
if [[ -n "$BOOTSTRAP_PEERS" ]]; then
    IFS=',' read -ra PEER_LIST <<< "$BOOTSTRAP_PEERS"
    for peer in "${PEER_LIST[@]}"; do
        peer_ip="${peer%%:*}"
        peer_port="${peer##*:}"
        IPS_BLOCK+="${peer_ip} ${peer_port}"$'\n'
    done
fi

cat > "$DATA_DIR/xrpld.cfg" <<CFG
# qXRP Validator node — $NODE_NAME
# Generated by deploy-cloud-validator.sh on $(date -u +%Y-%m-%dT%H:%M:%SZ)
# Branch: ${BRANCH} @ ${COMMIT}
#
# SECURITY: This node has a [validation_seed] — treat this file as a secret.
# Restrict access: chmod 600 $DATA_DIR/xrpld.cfg

[network_id]
${NETWORK_ID}

[node_size]
${NODE_SIZE}

[ledger_history]
full

[validation_quorum]
${QUORUM}

# ── VALIDATOR IDENTITY ──────────────────────────────────────────────────────
# This seed derives the classical secp256k1 keypair used to sign consensus
# messages. Keep this secret — anyone with this seed can sign as this validator.
[validation_seed]
${VAL_SEED}

# Trusted validator list (UNL — Unique Node List)
[validators_file]
${VALIDATORS_FILE}

# ── FEATURES ────────────────────────────────────────────────────────────────
[features]
ProofOfParticipation
SingleAssetVault
LendingProtocol

# ── NETWORK PORTS ───────────────────────────────────────────────────────────
[server]
port_rpc_admin_local
port_rpc_public
port_peer_public
port_ws_public

[port_rpc_admin_local]
port = ${RPC_PORT}
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[port_rpc_public]
port = $((RPC_PORT + 1000))
ip = 0.0.0.0
protocol = http

[port_peer_public]
port = ${PEER_PORT}
ip = 0.0.0.0
protocol = peer

[port_ws_public]
port = $((RPC_PORT + 2000))
ip = 0.0.0.0
protocol = ws

# ── STORAGE ─────────────────────────────────────────────────────────────────
[node_db]
type = NuDB
path = ${DATA_DIR}/nudb
advisory_delete = 0
online_delete = 512

[database_path]
${DATA_DIR}/db

[debug_logfile]
${DATA_DIR}/debug.log

[sntp_servers]
time.windows.com
time.apple.com
time.nist.gov
pool.ntp.org

# ── PEER CONNECTIONS ─────────────────────────────────────────────────────────
${IPS_BLOCK}
# ── TRANSACTION QUEUE ───────────────────────────────────────────────────────
[transaction_queue]
minimum_txn_in_ledger = 100
target_txn_in_ledger = 1000
ledgers_in_queue = 30
minimum_queue_size = 10000
maximum_txn_per_account = 100
CFG

chmod 600 "$DATA_DIR/xrpld.cfg"
log "Config written: $DATA_DIR/xrpld.cfg  (chmod 600)"

# ---------------------------------------------------------------------------
# 7. Systemd service
# ---------------------------------------------------------------------------
step "Installing systemd service"

sudo tee /etc/systemd/system/${SERVICE}.service > /dev/null <<UNIT
[Unit]
Description=qXRP Validator Node ${NODE_NAME}
Documentation=https://github.com/beartec-jpg/qXRP
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=300
StartLimitBurst=5

[Service]
Type=simple
User=${USER}
Group=${USER}
ExecStart=${BINARY} --conf ${DATA_DIR}/xrpld.cfg
ExecReload=/bin/kill -HUP \$MAINPID
Restart=on-failure
RestartSec=10
TimeoutStopSec=60
LimitNOFILE=65535
LimitNPROC=65535

# Security hardening
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=${DATA_DIR}
ProtectHome=read-only

[Install]
WantedBy=multi-user.target
UNIT

sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE"

# ---------------------------------------------------------------------------
# 8. Log rotation
# ---------------------------------------------------------------------------
sudo tee /etc/logrotate.d/qxrp-${NODE_NAME} > /dev/null <<LOGROTATE
${DATA_DIR}/debug.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
    dateext
    dateformat -%Y%m%d
}
LOGROTATE

# ---------------------------------------------------------------------------
# 9. Firewall
# ---------------------------------------------------------------------------
if command -v ufw &>/dev/null; then
    sudo ufw allow "${PEER_PORT}/tcp" comment "qxrp-${NODE_NAME} peer" 2>/dev/null || true
    sudo ufw allow "$((RPC_PORT + 2000))/tcp" comment "qxrp-${NODE_NAME} websocket" 2>/dev/null || true
fi

# ---------------------------------------------------------------------------
# 10. Start
# ---------------------------------------------------------------------------
step "Starting validator"

sudo systemctl restart "$SERVICE"
sleep 5

STATE="unknown"
for attempt in $(seq 1 20); do
    RESP=$(rpc "$RPC_PORT" "server_info" 2>/dev/null || echo "{}")
    STATE=$(echo "$RESP" | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info']['server_state'])" 2>/dev/null || echo "starting")
    [[ "$STATE" != "starting" ]] && break
    sleep 3
done

log "Server state: $STATE"
[[ "$STATE" == "proposing" || "$STATE" == "full" || "$STATE" == "tracking" ]] \
    || warn "Unexpected state '$STATE' — check: journalctl -fu $SERVICE"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
CONSENSUS_KEY_HEX=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['consensus_key_hex'])" 2>/dev/null || echo "see $KEYS_FILE")
ACCOUNT_ADDR=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['account_address'])" 2>/dev/null || echo "see $KEYS_FILE")

echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  qXRP Validator ${NODE_NAME} Deployed!${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  ${BOLD}Identity${NC}"
echo "    Validator pubkey  : $VAL_PUBKEY"
echo "    Consensus key hex : $CONSENSUS_KEY_HEX"
echo "    Account address   : $ACCOUNT_ADDR"
echo "    Keys file         : $KEYS_FILE  ← KEEP SECRET"
echo ""
echo -e "  ${BOLD}Network${NC}"
echo "    RPC (local)       : http://127.0.0.1:${RPC_PORT}"
echo "    Peer (public)     : 0.0.0.0:${PEER_PORT}  ← share this with other validators"
echo "    WebSocket         : 0.0.0.0:$((RPC_PORT + 2000))"
echo "    Network ID        : $NETWORK_ID"
echo "    State now         : $STATE"
echo ""
echo -e "  ${BOLD}Manage${NC}"
echo "    Logs   : journalctl -fu $SERVICE"
echo "    Status : systemctl status $SERVICE"
echo "    Stop   : sudo systemctl stop $SERVICE"
echo ""
echo -e "  ${BOLD}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "  ${BOLD}║  REQUIRED NEXT STEPS TO ACTIVATE VALIDATION             ║${NC}"
echo -e "  ${BOLD}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "  STEP 1 — Add YOUR pubkey to every OTHER validator's validators.txt:"
echo "    echo \"$VAL_PUBKEY\" >> /path/to/other-validator/validators.txt"
echo "    sudo systemctl reload qxrp-<other>"
echo ""
echo "  STEP 2 — Fund your validator account with qXRP:"
echo "    You need ≥2000 qXRP in account: $ACCOUNT_ADDR"
echo "    (200 XRP reserve + 1000 qXRP minimum bond + fees)"
echo ""
echo "  STEP 3 — Register and Bond (on-ledger activation):"
echo "    python3 $SRC_DIR/scripts/bond-validators.py"
echo "    # Or manually submit:"
echo "    #   ValidatorRegister: PublicKey=<falcon512-key> ConsensusKey=$CONSENSUS_KEY_HEX"
echo "    #   ValidatorBond:     ConsensusKey=$CONSENSUS_KEY_HEX BondedAmount=1000000000"
echo ""
echo "  STEP 4 — Verify validation is active:"
echo "    curl -s http://127.0.0.1:${RPC_PORT} -H 'Content-Type: application/json' \\"
echo "      -d '{\"method\":\"server_info\",\"params\":[{}]}' | python3 -m json.tool | grep server_state"
echo "    # Must show: \"server_state\": \"proposing\""
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${YELLOW}NOTE: The node is now syncing ledger history. It will show${NC}"
echo -e "${YELLOW}'tracking' until it has caught up, then move to 'proposing'.${NC}"
