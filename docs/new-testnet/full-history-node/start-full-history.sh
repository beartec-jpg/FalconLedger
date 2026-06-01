#!/bin/bash
#
# start-full-history.sh
# One-command starter for a qXRP Full History Node on the new clean testnet.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/docs/new-testnet/full-history-node/start-full-history.sh | bash
#
# Or run locally after cloning the repo.

set -e

echo "=== qXRP Full History Node Setup (Clean Testnet) ==="
echo ""

# Default values for new clean testnet
NETWORK_ID=${NETWORK_ID:-1001}
DATA_DIR=${DATA_DIR:-/var/lib/qxrp}
COMPOSE_DIR="$HOME/qxrp-full-history"

echo "Network ID     : $NETWORK_ID"
echo "Data Directory : $DATA_DIR"
echo ""

# Create data directory
echo "[1/4] Creating data directory..."
sudo mkdir -p "$DATA_DIR"
sudo chown $USER:$USER "$DATA_DIR"

# Clone or update the setup files
echo "[2/4] Downloading latest clean setup files..."
if [ -d "$COMPOSE_DIR" ]; then
    cd "$COMPOSE_DIR"
    git pull --quiet
else
    git clone --depth 1 https://github.com/beartec-jpg/qXRP.git /tmp/qxrp-clone
    mkdir -p "$COMPOSE_DIR"
    cp -r /tmp/qxrp-clone/docs/new-testnet/full-history-node/* "$COMPOSE_DIR/"
    rm -rf /tmp/qxrp-clone
    cd "$COMPOSE_DIR"
fi

# Make sure docker-compose.yml and xrpld.cfg exist
if [ ! -f "docker-compose.yml" ] || [ ! -f "xrpld.cfg" ]; then
    echo "ERROR: Required files not found in the setup directory."
    exit 1
fi

# Update network ID in config if different from default
if [ "$NETWORK_ID" != "1001" ]; then
    echo "[3/4] Updating Network ID to $NETWORK_ID..."
    sed -i "s/1001/$NETWORK_ID/" xrpld.cfg
else
    echo "[3/4] Using default Network ID 1001"
fi

# Start the node
echo "[4/4] Starting Full History Node..."
docker compose up -d

echo ""
echo "✅ qXRP Full History Node is starting!"
echo ""
echo "Useful commands:"
echo "  View logs     : docker logs -f qxrp-full-history"
echo "  Restart       : docker compose restart"
echo "  Stop          : docker compose down"
echo "  Check status  : docker ps | grep qxrp"
echo ""
echo "Public RPC will be available on port 6005 once synced."
echo "This can take many hours for a true full history node."
echo ""