#!/bin/bash
#
# start-validator.sh
# One-command starter for a qXRP Validator on the new clean testnet.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/docs/new-testnet/validator/start-validator.sh | bash

set -e

echo "=== qXRP Validator Setup (Clean Testnet) ==="
echo ""

NETWORK_ID=${NETWORK_ID:-1001}
DATA_DIR=${DATA_DIR:-/var/lib/qxrp-validator}
COMPOSE_DIR="$HOME/qxrp-validator"

echo "Network ID     : $NETWORK_ID"
echo "Data Directory : $DATA_DIR"
echo ""

echo "[1/3] Creating data directory..."
sudo mkdir -p "$DATA_DIR"
sudo chown $USER:$USER "$DATA_DIR"

echo "[2/3] Downloading latest clean validator setup..."
if [ -d "$COMPOSE_DIR" ]; then
    cd "$COMPOSE_DIR"
    git pull --quiet
else
    git clone --depth 1 https://github.com/beartec-jpg/qXRP.git /tmp/qxrp-clone
    mkdir -p "$COMPOSE_DIR"
    cp -r /tmp/qxrp-clone/docs/new-testnet/validator/* "$COMPOSE_DIR/"
    rm -rf /tmp/qxrp-clone
    cd "$COMPOSE_DIR"
fi

if [ "$NETWORK_ID" != "1001" ]; then
    sed -i "s/1001/$NETWORK_ID/" xrpld.cfg
fi

echo "[3/3] Starting Validator..."
docker compose up -d

echo ""
echo "✅ qXRP Validator is starting!"
echo ""
echo "Useful commands:"
echo "  Logs    : docker logs -f qxrp-validator"
echo "  Restart : docker compose restart"
echo "  Stop    : docker compose down"
echo ""
echo "Remember to generate validator keys and bond at least 1,000 qXRP."
echo ""