#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# One-command qXRP validator installer.
# Tested on: Ubuntu 22.04, Ubuntu 24.04, Debian 12
#
# Usage:
#   curl -fsSL https://install.qxrp.network/validator | bash
#   # or locally:
#   bash bin/install/install-qxrp-validator.sh

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DOCKER_IMAGE="qxrp/xrpld:latest"
CONFIG_DIR="$HOME/.qxrp/config"
DATA_DIR="$HOME/.qxrp/data"
COMPOSE_FILE="$HOME/.qxrp/docker-compose.yml"
MIN_RAM_MB=3800
MIN_DISK_GB=80

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log()  { echo -e "\033[1;32m[qxrp]\033[0m $*"; }
warn() { echo -e "\033[1;33m[qxrp] WARN:\033[0m $*"; }
die()  { echo -e "\033[1;31m[qxrp] ERROR:\033[0m $*" >&2; exit 1; }
already_done() { log "$1 – already done, skipping."; }

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

log "System OK – RAM: ${RAM_MB} MB, Disk: ${DISK_GB} GB"

# ---------------------------------------------------------------------------
# 2. Install Docker if missing
# ---------------------------------------------------------------------------
if command -v docker &>/dev/null; then
    already_done "Docker $(docker --version | awk '{print $3}' | tr -d ',')"
else
    log "Installing Docker..."
    curl -fsSL https://get.docker.com | sh
    sudo usermod -aG docker "$USER"
    log "Docker installed. You may need to log out and back in for group membership."
fi

# ---------------------------------------------------------------------------
# 3. Create config and data directories
# ---------------------------------------------------------------------------
if [[ -d "$CONFIG_DIR" ]] && [[ -d "$DATA_DIR" ]]; then
    already_done "Config dirs ($CONFIG_DIR, $DATA_DIR)"
else
    log "Creating config directories..."
    mkdir -p "$CONFIG_DIR" "$DATA_DIR"
fi

# ---------------------------------------------------------------------------
# 4. Write default config if none exists
# ---------------------------------------------------------------------------
CFG_FILE="$CONFIG_DIR/xrpld.cfg"
if [[ -f "$CFG_FILE" ]]; then
    already_done "Config file $CFG_FILE"
else
    log "Writing default validator config..."
    cat > "$CFG_FILE" <<'CFG'
[node_size]
medium

[ledger_history]
256

[server]
port_rpc_admin_local
port_peer_public

[port_rpc_admin_local]
port = 5005
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[port_peer_public]
port = 51235
ip = 0.0.0.0
protocol = peer

[node_db]
type = NuDB
path = /data/nudb
advisory_delete = 0

[database_path]
/data/db

[debug_logfile]
/data/debug.log

[network_id]
1001

[features]
ProofOfParticipation
CFG
    log "Default config written to $CFG_FILE"
fi

# ---------------------------------------------------------------------------
# 5. Write docker-compose file
# ---------------------------------------------------------------------------
if [[ -f "$COMPOSE_FILE" ]]; then
    already_done "Compose file $COMPOSE_FILE"
else
    log "Writing docker-compose.yml..."
    cat > "$COMPOSE_FILE" <<COMPOSE
version: "3.9"
services:
  xrpld:
    image: ${DOCKER_IMAGE}
    container_name: qxrp_validator
    restart: unless-stopped
    volumes:
      - ${CONFIG_DIR}:/cfg:ro
      - ${DATA_DIR}:/data
    ports:
      - "5005:5005"
      - "51235:51235"
      - "8080:8080"
    command: ["--conf", "/cfg/xrpld.cfg"]
COMPOSE
fi

# ---------------------------------------------------------------------------
# 6. Pull image and start
# ---------------------------------------------------------------------------
log "Pulling Docker image $DOCKER_IMAGE..."
docker pull "$DOCKER_IMAGE"

log "Starting validator..."
docker compose -f "$COMPOSE_FILE" up -d

# ---------------------------------------------------------------------------
# 7. Wait for RPC to come up and print summary
# ---------------------------------------------------------------------------
log "Waiting for node to start (up to 60s)..."
for i in $(seq 1 60); do
    CODE=$(curl -s -o /dev/null -w '%{http_code}' \
                -X POST "http://127.0.0.1:5005" \
                -H 'Content-Type: application/json' \
                -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
    [[ "$CODE" == "200" ]] && break
    [[ $i -eq 60 ]] && die "Node did not start in time. Check: docker logs qxrp_validator"
    sleep 1
done

# Get validator public key
PUBKEY=$(curl -s -X POST "http://127.0.0.1:5005" \
    -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['info']['pubkey_validator'])" 2>/dev/null || echo "unavailable")

echo ""
echo "================================================="
echo "  qXRP Validator is running!"
echo "================================================="
echo "  Dashboard : http://localhost:8080"
echo "  RPC       : http://localhost:5005"
echo "  Peer      : 0.0.0.0:51235"
echo "  Pubkey    : $PUBKEY"
echo "  Config    : $CFG_FILE"
echo "  Data      : $DATA_DIR"
echo ""
echo "  Manage:"
echo "    Stop  : docker compose -f $COMPOSE_FILE down"
echo "    Logs  : docker logs -f qxrp_validator"
echo "    Update: docker pull $DOCKER_IMAGE && docker compose -f $COMPOSE_FILE up -d"
echo "================================================="
