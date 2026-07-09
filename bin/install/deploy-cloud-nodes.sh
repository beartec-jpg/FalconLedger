#!/usr/bin/env bash
# =============================================================================
# deploy-cloud-nodes.sh — Build qXRP from source and run 3 full nodes
# =============================================================================
#
# Tested on: Ubuntu 22.04 LTS, Ubuntu 24.04 LTS
#
# Recommended cloud spec (per server):
#   CPU   : 4+ vCPU
#   RAM   : 16 GB  (8 GB minimum)
#   Disk  : 100 GB SSD (NVMe preferred)
#   OS    : Ubuntu 22.04 or 24.04
#
# Usage:
#   # Run as a non-root user with sudo privileges
#   bash deploy-cloud-nodes.sh [OPTIONS]
#
# Options:
#   --repo    <url>         Git repo URL  (default: https://github.com/beartec-jpg/qXRP)
#   --branch  <name>        Branch to build (default: develop)
#   --nodes   <1|2|3>       How many nodes to run on this server (default: 3)
#   --network-id <id>       Network ID (default: 1001 for new clean testnets)
#   --peers   <ip:port,...> Comma-separated bootstrap peer list (validators/other nodes)
#   --install-dir <path>    Root install directory (default: /opt/qxrp)
#   --data-dir <path>       Node data root (default: /var/lib/qxrp)
#   --skip-build            Re-use existing binary at --install-dir/bin/xrpld
#   --node-size <size>      xrpld node_size (tiny|small|medium|large|huge)
#   --jobs <n>              Parallel build jobs (default: nproc)
#
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
REPO_URL="https://github.com/beartec-jpg/qXRP"
BRANCH="develop"
NUM_NODES=3
NETWORK_ID=1001
BOOTSTRAP_PEERS=""        # e.g. "1.2.3.4:51235,5.6.7.8:51235"
INSTALL_DIR="/opt/qxrp"
DATA_ROOT="/var/lib/qxrp"
SKIP_BUILD=0
RELEASE_URL=""          # e.g. https://github.com/beartec-jpg/qXRP/releases/download/v1.0.0/xrpld-linux-x86_64
NODE_SIZE="medium"
BUILD_JOBS="$(nproc)"
BASE_RPC_PORT=5005
BASE_PEER_PORT=51235

# ---------------------------------------------------------------------------
# Colours
# ---------------------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
log()  { echo -e "${GREEN}[qxrp]${NC} $*"; }
warn() { echo -e "${YELLOW}[qxrp WARN]${NC} $*"; }
die()  { echo -e "${RED}[qxrp ERROR]${NC} $*" >&2; exit 1; }
step() { echo -e "\n${CYAN}══ $* ══${NC}"; }

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)         REPO_URL="$2";     shift 2 ;;
    --branch)       BRANCH="$2";       shift 2 ;;
    --nodes)        NUM_NODES="$2";    shift 2 ;;
    --network-id)   NETWORK_ID="$2";   shift 2 ;;
    --peers)        BOOTSTRAP_PEERS="$2"; shift 2 ;;
    --install-dir)  INSTALL_DIR="$2";  shift 2 ;;
    --data-dir)     DATA_ROOT="$2";    shift 2 ;;
    --skip-build)    SKIP_BUILD=1;        shift   ;;
    --release-url)  RELEASE_URL="$2";   shift 2 ;;
    --node-size)    NODE_SIZE="$2";      shift 2 ;;
    --jobs)         BUILD_JOBS="$2";   shift 2 ;;
    *) die "Unknown option: $1" ;;
  esac
done

[[ "$NUM_NODES" =~ ^[1-3]$ ]] || die "--nodes must be 1, 2, or 3"

BINARY="$INSTALL_DIR/bin/xrpld"
SRC_DIR="$INSTALL_DIR/src"

# ---------------------------------------------------------------------------
# Pre-flight
# ---------------------------------------------------------------------------
step "Pre-flight checks"

[[ "$(id -u)" -ne 0 ]] || die "Do not run as root. Run as a sudo-capable user."
command -v sudo >/dev/null || die "'sudo' not found."

RAM_MB=$(awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo)
DISK_GB=$(df --output=avail -BG "$HOME" | tail -1 | tr -d 'G ')
CPU_N=$(nproc)

log "CPU: $CPU_N cores | RAM: ${RAM_MB} MB | Disk: ${DISK_GB} GB available"

[[ "$RAM_MB" -ge 7500 ]]  || warn "Low RAM (${RAM_MB} MB). Recommend ≥8 GB. Build may OOM."
[[ "$DISK_GB" -ge 50 ]]   || die  "Insufficient disk: ${DISK_GB} GB. Need ≥50 GB."

# Detect OS
. /etc/os-release 2>/dev/null || die "Cannot read /etc/os-release"
log "OS: $PRETTY_NAME"
[[ "$ID" == "ubuntu" || "$ID" == "debian" ]] \
    || warn "Only Ubuntu/Debian tested. Proceeding anyway."

# ---------------------------------------------------------------------------
# 1. Install system dependencies
# ---------------------------------------------------------------------------
step "Installing system dependencies"

sudo apt-get update -qq

# Detect best available GCC (prefer 13, fall back to 12)
if apt-cache show gcc-13 &>/dev/null 2>&1; then
    GCC_VER=13
elif apt-cache show gcc-12 &>/dev/null 2>&1; then
    GCC_VER=12
else
    die "GCC 12 or 13 required but not found in apt. Add the toolchain PPA first:
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test && sudo apt-get update"
fi
log "Using GCC $GCC_VER"

sudo apt-get install -y --no-install-recommends \
    gcc-${GCC_VER} g++-${GCC_VER} \
    cmake ninja-build \
    git curl wget ca-certificates \
    python3 python3-pip python3-dev python-is-python3 \
    libssl-dev pkg-config \
    ccache \
    lsof htop jq \
    logrotate

# Set GCC as default compiler
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VER} 100 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-${GCC_VER} \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-${GCC_VER} \
    --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-${GCC_VER} \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-${GCC_VER}
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 99
sudo update-alternatives --auto gcc

log "GCC: $(gcc --version | head -1)"

# Install / upgrade mold linker for faster linking
if ! command -v mold &>/dev/null; then
    log "Installing mold linker..."
    MOLD_TAG=$(curl -fsSL "https://api.github.com/repos/rui314/mold/releases/latest" \
        | python3 -c "import sys,json; print(json.load(sys.stdin)['tag_name'])" 2>/dev/null || echo "v2.34.1")
    MOLD_URL="https://github.com/rui314/mold/releases/download/${MOLD_TAG}/mold-${MOLD_TAG#v}-x86_64-linux.tar.gz"
    TMP_MOLD=$(mktemp -d)
    curl -fsSL "$MOLD_URL" | tar -xz -C "$TMP_MOLD" --strip-components=1
    sudo cp "$TMP_MOLD/bin/mold" /usr/local/bin/mold
    sudo cp "$TMP_MOLD/lib/mold/mold-wrapper.so" /usr/local/lib/ 2>/dev/null || true
    rm -rf "$TMP_MOLD"
fi
log "Mold: $(mold --version 2>&1 | head -1)"

# Install Conan
if ! command -v conan &>/dev/null; then
    log "Installing Conan..."
    pip3 install --user --break-system-packages conan 2>/dev/null \
        || pip3 install --user conan
    export PATH="$HOME/.local/bin:$PATH"
fi
log "Conan: $(conan --version)"

# ---------------------------------------------------------------------------
# 2. Clone / update the repo
# ---------------------------------------------------------------------------
step "Cloning / updating repository"

sudo mkdir -p "$INSTALL_DIR"
sudo chown "$USER:$USER" "$INSTALL_DIR"

if [[ -d "$SRC_DIR/.git" ]]; then
    log "Repo already cloned. Fetching latest..."
    git -C "$SRC_DIR" fetch origin
    git -C "$SRC_DIR" checkout "$BRANCH"
    git -C "$SRC_DIR" reset --hard "origin/$BRANCH"
else
    log "Cloning $REPO_URL ($BRANCH)..."
    git clone --branch "$BRANCH" --depth 1 "$REPO_URL" "$SRC_DIR"
fi

COMMIT=$(git -C "$SRC_DIR" rev-parse --short HEAD)
log "Source: $REPO_URL @ $BRANCH ($COMMIT)"

# ---------------------------------------------------------------------------
# 3. Configure Conan
# ---------------------------------------------------------------------------
step "Configuring Conan"

export PATH="$HOME/.local/bin:$PATH"

# Import project profiles
conan config install "$SRC_DIR/conan/profiles/" \
    -tf "$(conan config home)/profiles/" 2>/dev/null || true

# Add XRPLF patched recipes remote (must be index 0)
if ! conan remote list | grep -q "xrplf"; then
    conan remote add --index 0 xrplf https://conan.ripplex.io
fi

# Ensure profile has C++20 and libstdc++11
PROFILE_FILE="$(conan config home)/profiles/default"
if [[ ! -f "$PROFILE_FILE" ]]; then
    conan profile detect
fi
sed -i.bak 's|^compiler\.cppstd=.*$|compiler.cppstd=20|'     "$PROFILE_FILE" 2>/dev/null || true
sed -i.bak 's|^compiler\.libcxx=.*$|compiler.libcxx=libstdc++11|' "$PROFILE_FILE" 2>/dev/null || true

log "Conan profile:"; conan profile show 2>/dev/null | head -15

# ---------------------------------------------------------------------------
# 4. Build xrpld
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
    step "Building xrpld (this takes 20-60 minutes on first run)"

    BUILD_DIR="$SRC_DIR/.build"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    log "Running conan install (resolving & building dependencies)..."
    conan install "$SRC_DIR" \
        --output-folder . \
        --build missing \
        --settings build_type=Release \
        -c "tools.build:jobs=${BUILD_JOBS}"

    log "Running cmake configure..."
    cmake "$SRC_DIR" \
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -Dxrpld=ON \
        -Dtests=OFF \
        -Duse_mold=ON \
        -G Ninja \
        -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"

    log "Compiling with $BUILD_JOBS parallel jobs..."
    cmake --build . -j"${BUILD_JOBS}"

    cd "$SRC_DIR"

    # Install binary
    mkdir -p "$INSTALL_DIR/bin"
    BUILT_BIN=$(find "$BUILD_DIR" -name "xrpld" -type f | head -1)
    [[ -x "$BUILT_BIN" ]] || die "Build produced no xrpld binary"
    cp "$BUILT_BIN" "$BINARY"
    chmod +x "$BINARY"
fi

log "Binary: $(ls -lh $BINARY | awk '{print $5, $9}')"
log "Version: $($BINARY --version 2>&1 | head -1 || echo 'N/A')"

# ---------------------------------------------------------------------------
# 5. Create data directories and configs for N nodes
# ---------------------------------------------------------------------------
step "Creating node configurations ($NUM_NODES nodes)"

sudo mkdir -p "$DATA_ROOT"
sudo chown "$USER:$USER" "$DATA_ROOT"

# Build peer list for [ips_fixed]: other nodes on this server
LOCAL_PEERS=""
for i in $(seq 1 "$NUM_NODES"); do
    PORT=$((BASE_PEER_PORT + i - 1))
    # Each node peers with all other nodes on this host
    for j in $(seq 1 "$NUM_NODES"); do
        [[ $i -ne $j ]] || continue
        PEER_PORT=$((BASE_PEER_PORT + j - 1))
        LOCAL_PEERS+="127.0.0.1 ${PEER_PORT}"$'\n'
    done
done

for i in $(seq 1 "$NUM_NODES"); do
    NODE_DIR="$DATA_ROOT/node${i}"
    RPC_PORT=$((BASE_RPC_PORT + i - 1))
    PEER_PORT=$((BASE_PEER_PORT + i - 1))

    mkdir -p "$NODE_DIR/db" "$NODE_DIR/nudb"

    # ---- Build [ips_fixed] block ----
    IPS_BLOCK="[ips_fixed]"$'\n'
    # Peer with other local nodes
    for j in $(seq 1 "$NUM_NODES"); do
        [[ $i -ne $j ]] || continue
        J_PEER_PORT=$((BASE_PEER_PORT + j - 1))
        IPS_BLOCK+="127.0.0.1 ${J_PEER_PORT}"$'\n'
    done
    # Add external bootstrap peers
    if [[ -n "$BOOTSTRAP_PEERS" ]]; then
        IFS=',' read -ra PEER_LIST <<< "$BOOTSTRAP_PEERS"
        for peer in "${PEER_LIST[@]}"; do
            peer_ip="${peer%%:*}"
            peer_port="${peer##*:}"
            IPS_BLOCK+="${peer_ip} ${peer_port}"$'\n'
        done
    fi

    cat > "$NODE_DIR/xrpld.cfg" <<CFG
# Node ${i} — qXRP full node
# Generated by deploy-cloud-nodes.sh on $(date -u +%Y-%m-%dT%H:%M:%SZ)
# Branch: ${BRANCH} @ ${COMMIT}

[network_id]
${NETWORK_ID}

[node_size]
${NODE_SIZE}

[ledger_history]
full

[validation_quorum]
4

# Full nodes do NOT have a [validation_seed] — they track only.

[features]
ProofOfParticipation
SingleAssetVault
LendingProtocol

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

[node_db]
type = NuDB
path = ${NODE_DIR}/nudb
advisory_delete = 0
online_delete = 512

[database_path]
${NODE_DIR}/db

[debug_logfile]
${NODE_DIR}/debug.log

[sntp_servers]
time.windows.com
time.apple.com
time.nist.gov
pool.ntp.org

${IPS_BLOCK}
[transaction_queue]
minimum_txn_in_ledger = 100
target_txn_in_ledger = 1000
ledgers_in_queue = 30
minimum_queue_size = 10000
maximum_txn_per_account = 100
CFG

    log "Node $i config → $NODE_DIR/xrpld.cfg  (RPC :${RPC_PORT}, Peer :${PEER_PORT})"
done

# ---------------------------------------------------------------------------
# 6. Create systemd service units
# ---------------------------------------------------------------------------
step "Installing systemd services"

for i in $(seq 1 "$NUM_NODES"); do
    NODE_DIR="$DATA_ROOT/node${i}"
    SERVICE="qxrp-node${i}"

    sudo tee /etc/systemd/system/${SERVICE}.service > /dev/null <<UNIT
[Unit]
Description=qXRP Full Node ${i}
Documentation=https://github.com/beartec-jpg/qXRP
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=300
StartLimitBurst=5

[Service]
Type=simple
User=${USER}
Group=${USER}
ExecStart=${BINARY} --conf ${NODE_DIR}/xrpld.cfg
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
ReadWritePaths=${DATA_ROOT}
ProtectHome=read-only

[Install]
WantedBy=multi-user.target
UNIT

    log "Created /etc/systemd/system/${SERVICE}.service"
done

# ---------------------------------------------------------------------------
# 7. Log rotation
# ---------------------------------------------------------------------------
step "Configuring log rotation"

sudo tee /etc/logrotate.d/qxrp > /dev/null <<LOGROTATE
${DATA_ROOT}/node*/debug.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
    dateext
    dateformat -%Y%m%d
}
LOGROTATE

log "Log rotation: daily, 14-day retention, compressed"

# ---------------------------------------------------------------------------
# 8. Firewall rules (ufw)
# ---------------------------------------------------------------------------
step "Configuring firewall"

if command -v ufw &>/dev/null; then
    for i in $(seq 1 "$NUM_NODES"); do
        PEER_PORT=$((BASE_PEER_PORT + i - 1))
        PUBLIC_RPC=$((BASE_RPC_PORT + i - 1 + 1000))
        PUBLIC_WS=$((BASE_RPC_PORT + i - 1 + 2000))
        sudo ufw allow "${PEER_PORT}/tcp" comment "qxrp-node${i} peer" 2>/dev/null || true
        sudo ufw allow "${PUBLIC_WS}/tcp"  comment "qxrp-node${i} websocket" 2>/dev/null || true
        # Public RPC only if you want external JSON-RPC access — commented out by default
        # sudo ufw allow "${PUBLIC_RPC}/tcp" comment "qxrp-node${i} rpc-public"
    done
    log "Firewall rules added (peer + websocket ports opened)"
else
    warn "ufw not found. Open these ports manually:"
    for i in $(seq 1 "$NUM_NODES"); do
        PEER_PORT=$((BASE_PEER_PORT + i - 1))
        PUBLIC_WS=$((BASE_RPC_PORT + i - 1 + 2000))
        echo "  Node $i — peer: $PEER_PORT/tcp, ws: $PUBLIC_WS/tcp"
    done
fi

# ---------------------------------------------------------------------------
# 9. Install health-check monitor
# ---------------------------------------------------------------------------
step "Installing health monitor"

MONITOR="$INSTALL_DIR/bin/health-check.sh"
mkdir -p "$INSTALL_DIR/bin"

cat > "$MONITOR" <<'MONITOR'
#!/usr/bin/env bash
# qXRP node health check — run via cron or manually
BINARY_PLACEHOLDER
DATA_ROOT_PLACEHOLDER

DATA_ROOT="DATA_ROOT_VALUE"
NUM_NODES="NUM_NODES_VALUE"
BASE_RPC_PORT="BASE_RPC_PORT_VALUE"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
ok()   { echo -e "${GREEN}[OK]${NC}    $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $*"; }
bad()  { echo -e "${RED}[FAIL]${NC}  $*"; }

echo "═══════════════════════════════════════════"
echo "  qXRP Node Health  $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "═══════════════════════════════════════════"

for i in $(seq 1 "$NUM_NODES"); do
    RPC_PORT=$((BASE_RPC_PORT + i - 1))
    SERVICE="qxrp-node${i}"
    echo ""
    echo "  Node ${i}  (RPC :${RPC_PORT})"

    # Systemd state
    STATE=$(systemctl is-active "$SERVICE" 2>/dev/null || echo "inactive")
    [[ "$STATE" == "active" ]] && ok "systemd: $STATE" || bad "systemd: $STATE"

    # RPC response
    RESP=$(curl -s --max-time 3 "http://127.0.0.1:${RPC_PORT}" \
        -H 'Content-Type: application/json' \
        -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo "{}")

    SERVER_STATE=$(echo "$RESP" | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info']['server_state'])" 2>/dev/null || echo "unreachable")
    SEQ=$(echo "$RESP" | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info'].get('validated_ledger',{}).get('seq',0))" 2>/dev/null || echo "0")
    PEERS=$(echo "$RESP" | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info'].get('peers',0))" 2>/dev/null || echo "0")
    LOAD=$(echo "$RESP" | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info'].get('load_factor',1))" 2>/dev/null || echo "?")

    [[ "$SERVER_STATE" == "proposing" || "$SERVER_STATE" == "full" ]] \
        && ok "state: $SERVER_STATE" || warn "state: $SERVER_STATE"
    ok "ledger: $SEQ  |  peers: $PEERS  |  load_factor: $LOAD"

    # Disk usage
    NODE_DIR="$DATA_ROOT/node${i}"
    USED=$(du -sh "$NODE_DIR" 2>/dev/null | cut -f1 || echo "?")
    ok "disk used: $USED ($NODE_DIR)"

    # Log errors in last 60s
    LOG="$NODE_DIR/debug.log"
    if [[ -f "$LOG" ]]; then
        ERRORS=$(tail -500 "$LOG" | grep -c "Fatal\|Panic\|terminate\|segfault" 2>/dev/null || echo 0)
        [[ "$ERRORS" -eq 0 ]] && ok "log: no fatal errors" || bad "log: $ERRORS fatal error(s) — check $LOG"
    fi
done

echo ""
echo "═══════════════════════════════════════════"
MONITOR

# Substitute actual values
sed -i "s|DATA_ROOT_VALUE|${DATA_ROOT}|g" "$MONITOR"
sed -i "s|NUM_NODES_VALUE|${NUM_NODES}|g" "$MONITOR"
sed -i "s|BASE_RPC_PORT_VALUE|${BASE_RPC_PORT}|g" "$MONITOR"
sed -i '/BINARY_PLACEHOLDER\|DATA_ROOT_PLACEHOLDER/d' "$MONITOR"
chmod +x "$MONITOR"

# Add cron job for health check every 5 minutes
CRON_LINE="*/5 * * * * $MONITOR >> /var/log/qxrp-health.log 2>&1"
(crontab -l 2>/dev/null | grep -v "health-check.sh"; echo "$CRON_LINE") | crontab -
log "Health monitor: $MONITOR (runs every 5 min via cron)"

# ---------------------------------------------------------------------------
# 10. Enable and start services
# ---------------------------------------------------------------------------
step "Enabling and starting services"

sudo systemctl daemon-reload

for i in $(seq 1 "$NUM_NODES"); do
    SERVICE="qxrp-node${i}"
    sudo systemctl enable "$SERVICE"
    sudo systemctl restart "$SERVICE"
    log "Started $SERVICE"
done

# ---------------------------------------------------------------------------
# 11. Wait for nodes to respond and print status
# ---------------------------------------------------------------------------
step "Waiting for nodes to come online (up to 60s)"

sleep 5
ALL_OK=1
for i in $(seq 1 "$NUM_NODES"); do
    RPC_PORT=$((BASE_RPC_PORT + i - 1))
    for attempt in $(seq 1 20); do
        CODE=$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 \
            "http://127.0.0.1:${RPC_PORT}" \
            -H 'Content-Type: application/json' \
            -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
        [[ "$CODE" == "200" ]] && break
        [[ $attempt -eq 20 ]] && { warn "Node $i not responding after 60s"; ALL_OK=0; }
        sleep 3
    done
done

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  qXRP Cloud Nodes Deployed Successfully!${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════${NC}"
echo ""
echo "  Binary    : $BINARY"
echo "  Source    : $SRC_DIR ($BRANCH @ $COMMIT)"
echo "  Data      : $DATA_ROOT"
echo "  Network   : $NETWORK_ID"
echo ""
for i in $(seq 1 "$NUM_NODES"); do
    RPC_PORT=$((BASE_RPC_PORT + i - 1))
    PEER_PORT=$((BASE_PEER_PORT + i - 1))
    echo "  Node $i    : RPC=http://127.0.0.1:${RPC_PORT}"
    echo "             Peer=0.0.0.0:${PEER_PORT}"
    echo "             Data=$DATA_ROOT/node${i}"
done
echo ""
echo "  Manage:"
echo "    Status  : $MONITOR"
echo "    Logs    : journalctl -fu qxrp-node1"
echo "    Stop    : sudo systemctl stop qxrp-node1 qxrp-node2 qxrp-node3"
echo "    Restart : sudo systemctl restart qxrp-node1 qxrp-node2 qxrp-node3"
echo "    Update  : bash $0 --skip-build=0 (rebuilds + restarts)"
echo ""
if [[ -n "$BOOTSTRAP_PEERS" ]]; then
    echo "  Bootstrap peers: $BOOTSTRAP_PEERS"
else
    warn "No --peers specified. Nodes will only peer with each other."
    echo "  Add validator/peer IPs with: --peers <ip:port,...>"
fi
echo -e "${CYAN}══════════════════════════════════════════════════════${NC}"
