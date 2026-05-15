#!/usr/bin/env bash
# =============================================================================
# upgrade-validator.sh — safely upgrade xrpld on a running validator
# =============================================================================
#
# Procedure:
#   1. Build new binary (or accept pre-built path)
#   2. Verify new binary starts and returns a version
#   3. Wait for current ledger to close cleanly
#   4. Gracefully stop the service (SIGTERM → waits → SIGKILL fallback)
#   5. Backup old binary
#   6. Install new binary
#   7. Start service
#   8. Health check (30s window)
#   9. Automatic rollback if health check fails
#
# Usage:
#   bash upgrade-validator.sh --service qxrp-v1 --binary /tmp/xrpld.new
#   bash upgrade-validator.sh --service qxrp-v1 --build   # build from current source
#
# Options:
#   --service  <name>   Systemd service name (default: qxrp-v1)
#   --binary   <path>   Pre-built binary to install
#   --build             Build from source before upgrading
#   --src-dir  <path>   Source directory for --build (default: /opt/qxrp/src)
#   --rpc-url  <url>    Node RPC URL for health checks (default: http://127.0.0.1:5005)
#   --dry-run           Show what would happen without doing it
#
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
SERVICE="qxrp-v1"
NEW_BINARY=""
DO_BUILD=0
SRC_DIR="/opt/qxrp/src"
RPC_URL="http://127.0.0.1:5005"
DRY_RUN=0
INSTALL_DIR="/opt/qxrp/bin"

# ---------------------------------------------------------------------------
# Colours
# ---------------------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
log()  { echo -e "${GREEN}[upgrade]${NC} $*"; }
warn() { echo -e "${YELLOW}[upgrade WARN]${NC} $*"; }
die()  { echo -e "${RED}[upgrade ERROR]${NC} $*" >&2; exit 1; }
step() { echo -e "\n${CYAN}── $* ──${NC}"; }
dry()  { [[ "$DRY_RUN" -eq 1 ]] && echo -e "${YELLOW}[dry-run]${NC} $*" && return 0; return 1; }

# ---------------------------------------------------------------------------
# Parse args
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --service)   SERVICE="$2";    shift 2 ;;
    --binary)    NEW_BINARY="$2"; shift 2 ;;
    --build)     DO_BUILD=1;      shift   ;;
    --src-dir)   SRC_DIR="$2";    shift 2 ;;
    --rpc-url)   RPC_URL="$2";    shift 2 ;;
    --dry-run)   DRY_RUN=1;       shift   ;;
    *) die "Unknown option: $1" ;;
  esac
done

[[ "$DO_BUILD" -eq 0 && -z "$NEW_BINARY" ]] && die "Provide --binary <path> or --build"
[[ "$DO_BUILD" -eq 1 && -n "$NEW_BINARY" ]] && die "Use --binary OR --build, not both"

CURRENT_BINARY="$INSTALL_DIR/xrpld"
BACKUP_BINARY="$INSTALL_DIR/xrpld.backup.$(date +%Y%m%d-%H%M%S)"

rpc_call() {
    curl -s --max-time 5 "$RPC_URL" \
        -H 'Content-Type: application/json' \
        -d "{\"method\":\"$1\",\"params\":[{}]}" 2>/dev/null
}

get_state() {
    rpc_call server_info | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info']['server_state'])" 2>/dev/null \
        || echo "unreachable"
}

get_ledger() {
    rpc_call server_info | python3 -c \
        "import sys,json; print(json.load(sys.stdin)['result']['info'].get('validated_ledger',{}).get('seq',0))" 2>/dev/null \
        || echo "0"
}

# ---------------------------------------------------------------------------
# 1. Build new binary if requested
# ---------------------------------------------------------------------------
if [[ "$DO_BUILD" -eq 1 ]]; then
    step "Building new binary from source"
    [[ -d "$SRC_DIR/.git" ]] || die "Source directory not found: $SRC_DIR"

    log "Pulling latest commits..."
    dry git -C "$SRC_DIR" pull --ff-only || git -C "$SRC_DIR" pull --ff-only

    NEW_BINARY="$SRC_DIR/.build/xrpld.new"
    BUILD_DIR="$SRC_DIR/.build"

    if ! dry "cmake --build $BUILD_DIR --target xrpld -j$(nproc)"; then
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        conan install "$SRC_DIR" --output-folder . --build missing \
            --settings build_type=Release -c "tools.build:jobs=$(nproc)"
        cmake "$SRC_DIR" \
            -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -Dxrpld=ON -Dtests=OFF -G Ninja \
            -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
        cmake --build . -j"$(nproc)"
        cd -
    fi

    # Find the built binary
    BUILT=$(find "$BUILD_DIR" -name "xrpld" -newer "$CURRENT_BINARY" -type f 2>/dev/null | head -1 || true)
    [[ -x "$BUILT" ]] || BUILT=$(find "$BUILD_DIR" -name "xrpld" -type f | head -1)
    [[ -x "$BUILT" ]] || die "Build completed but no xrpld binary found in $BUILD_DIR"
    NEW_BINARY="$BUILT"
fi

# ---------------------------------------------------------------------------
# 2. Verify the new binary
# ---------------------------------------------------------------------------
step "Verifying new binary"
[[ -x "$NEW_BINARY" ]] || die "Binary not found or not executable: $NEW_BINARY"

NEW_VERSION=$("$NEW_BINARY" --version 2>&1 | head -1 || echo "unknown")
OLD_VERSION=$("$CURRENT_BINARY" --version 2>&1 | head -1 || echo "unknown")
log "Current version : $OLD_VERSION"
log "New version     : $NEW_VERSION"

if [[ "$NEW_VERSION" == "$OLD_VERSION" ]]; then
    warn "Versions are identical. Proceeding anyway."
fi

# ---------------------------------------------------------------------------
# 3. Check current service health
# ---------------------------------------------------------------------------
step "Pre-upgrade health check"
CURRENT_STATE=$(get_state)
CURRENT_LEDGER=$(get_ledger)
log "Current state  : $CURRENT_STATE"
log "Current ledger : $CURRENT_LEDGER"

if [[ "$CURRENT_STATE" == "unreachable" ]]; then
    warn "Node is not responding. Will still attempt upgrade."
fi

# ---------------------------------------------------------------------------
# 4. Wait for a clean ledger close
# ---------------------------------------------------------------------------
if [[ "$CURRENT_STATE" != "unreachable" ]]; then
    step "Waiting for clean ledger close"
    log "Waiting up to 15s for next ledger..."
    START_LEDGER="$CURRENT_LEDGER"
    for i in $(seq 1 15); do
        sleep 1
        NEW_LEDGER=$(get_ledger)
        if [[ "$NEW_LEDGER" -gt "$START_LEDGER" ]]; then
            log "Ledger advanced $START_LEDGER → $NEW_LEDGER. Safe to upgrade."
            break
        fi
        [[ $i -eq 15 ]] && warn "Ledger did not advance in 15s. Proceeding anyway."
    done
fi

# ---------------------------------------------------------------------------
# 5. Stop the service
# ---------------------------------------------------------------------------
step "Stopping service $SERVICE"
if dry "systemctl stop $SERVICE"; then
    log "[dry-run] Would stop $SERVICE"
else
    sudo systemctl stop "$SERVICE" || warn "Stop command returned non-zero"
    # Wait up to 60s for graceful shutdown
    for i in $(seq 1 12); do
        STATE=$(systemctl is-active "$SERVICE" 2>/dev/null || echo "inactive")
        [[ "$STATE" == "inactive" || "$STATE" == "failed" ]] && break
        [[ $i -eq 12 ]] && {
            warn "Service did not stop in 60s. Sending SIGKILL."
            PID=$(systemctl show --property=MainPID --value "$SERVICE" 2>/dev/null || echo "")
            [[ -n "$PID" && "$PID" != "0" ]] && sudo kill -9 "$PID" || true
        }
        sleep 5
    done
    log "Service stopped."
fi

# ---------------------------------------------------------------------------
# 6. Backup and install
# ---------------------------------------------------------------------------
step "Backing up and installing new binary"
if dry "cp $CURRENT_BINARY $BACKUP_BINARY && cp $NEW_BINARY $CURRENT_BINARY"; then
    log "[dry-run] Would backup to $BACKUP_BINARY and install $NEW_BINARY"
else
    cp "$CURRENT_BINARY" "$BACKUP_BINARY"
    log "Backup: $BACKUP_BINARY"
    cp "$NEW_BINARY" "$CURRENT_BINARY"
    chmod +x "$CURRENT_BINARY"
    log "Installed: $CURRENT_BINARY"
fi

# ---------------------------------------------------------------------------
# 7. Start the service
# ---------------------------------------------------------------------------
step "Starting service $SERVICE"
if dry "systemctl start $SERVICE"; then
    log "[dry-run] Would start $SERVICE"
else
    sudo systemctl start "$SERVICE"
    log "Service started."
fi

# ---------------------------------------------------------------------------
# 8. Health check with automatic rollback
# ---------------------------------------------------------------------------
step "Post-upgrade health check (30s window)"
HEALTHY=0

if [[ "$DRY_RUN" -eq 0 ]]; then
    for i in $(seq 1 10); do
        sleep 3
        STATE=$(get_state)
        LEDGER=$(get_ledger)
        log "  Attempt $i/10 — state=$STATE ledger=$LEDGER"

        if [[ "$STATE" == "proposing" || "$STATE" == "full" || "$STATE" == "tracking" ]]; then
            HEALTHY=1
            break
        fi
    done
else
    HEALTHY=1
    log "[dry-run] Skipping health check"
fi

if [[ "$HEALTHY" -eq 1 ]]; then
    echo ""
    echo -e "${GREEN}══════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Upgrade successful!${NC}"
    echo -e "${GREEN}══════════════════════════════════════════════${NC}"
    log "Service  : $(systemctl is-active $SERVICE 2>/dev/null || echo 'unknown')"
    log "State    : $(get_state)"
    log "Version  : $NEW_VERSION"
    log "Backup   : $BACKUP_BINARY"
    echo ""
    log "To remove backup when confident: rm $BACKUP_BINARY"
else
    echo ""
    echo -e "${RED}══════════════════════════════════════════════${NC}"
    echo -e "${RED}  Health check FAILED — rolling back!${NC}"
    echo -e "${RED}══════════════════════════════════════════════${NC}"

    sudo systemctl stop "$SERVICE" 2>/dev/null || true
    cp "$BACKUP_BINARY" "$CURRENT_BINARY"
    chmod +x "$CURRENT_BINARY"
    sudo systemctl start "$SERVICE"

    log "Rolled back to $BACKUP_BINARY"
    die "Upgrade failed. Node restored to previous version."
fi
