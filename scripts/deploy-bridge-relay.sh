#!/usr/bin/env bash
# Deploy Sepolia → Falcon bridge mint relay on the coordinator full node.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
STATE_DIR="${BRIDGE_STATE_DIR:-/var/lib/qxrp-bridge}"
STABLES_STATE="${STABLES_STATE_FILE:-/var/lib/qxrp-stables/stables_state.json}"
CONTAINER="${DOCKER_CONTAINER:-qxrp-full}"
STABLES_MANIFEST="${STABLES_MANIFEST:-${REPO_ROOT}/config/testnet-stables.json}"

echo "==> Installing bridge relay scripts to ${STATE_DIR}"
install -d -m 0750 "${STATE_DIR}"
install -m 0755 "${SCRIPT_DIR}/bridge-deposit-relay.py" "${STATE_DIR}/bridge-deposit-relay.py"
install -m 0755 "${SCRIPT_DIR}/bridge-withdraw-relay.py" "${STATE_DIR}/bridge-withdraw-relay.py"
install -m 0755 "${SCRIPT_DIR}/bridge-sepolia-withdraw.js" "${STATE_DIR}/bridge-sepolia-withdraw.js"
install -m 0644 "${REPO_ROOT}/config/usdc-bridge.json" "${STATE_DIR}/usdc-bridge.json"

if [[ ! -f "${STABLES_STATE}" ]]; then
  echo "ERROR: ${STABLES_STATE} missing — run issue-testnet-stables.py first" >&2
  exit 1
fi

echo "==> Installing systemd unit qxrp-bridge-relay.service"
cat > /etc/systemd/system/qxrp-bridge-relay.service <<EOF
[Unit]
Description=qXRP Sepolia USDC bridge mint relay
After=docker.service network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${STATE_DIR}
Environment=STABLES_STATE_FILE=${STABLES_STATE}
Environment=BRIDGE_RELAY_STATE_FILE=${STATE_DIR}/relay_state.json
Environment=BRIDGE_MANIFEST=${STATE_DIR}/usdc-bridge.json
Environment=STABLES_MANIFEST=${STABLES_MANIFEST}
Environment=SEPOLIA_RPC_URL=https://ethereum-sepolia-rpc.publicnode.com
Environment=SEPOLIA_LOCK_CONTRACT=0x05712e9BC202cE3F1E601caCb1C82fc3AC9D8651
Environment=BRIDGE_WITHDRAW_STATE_FILE=${STATE_DIR}/withdraw_state.json
Environment=PUBLIC_RPC_URL=http://127.0.0.1:6005
Environment=ADMIN_RPC_URL=http://127.0.0.1:5005
Environment=DOCKER_CONTAINER=${CONTAINER}
# SEPOLIA_OWNER_PRIVATE_KEY must be set in /etc/systemd/system/qxrp-bridge-relay.service.d/override.conf
ExecStart=/bin/bash -c '/usr/bin/python3 ${STATE_DIR}/bridge-deposit-relay.py --loop --interval 30 & /usr/bin/python3 ${STATE_DIR}/bridge-withdraw-relay.py --loop --interval 30 & wait'
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable qxrp-bridge-relay.service
systemctl restart qxrp-bridge-relay.service
systemctl --no-pager status qxrp-bridge-relay.service || true

echo "==> One-shot catch-up (mint any pending deposits)"
python3 "${STATE_DIR}/bridge-deposit-relay.py" --once \
  --bridge-manifest "${STATE_DIR}/usdc-bridge.json" \
  --stables-state "${STABLES_STATE}" \
  --relay-state "${STATE_DIR}/relay_state.json" \
  --container "${CONTAINER}" || true

echo "Done. Logs: journalctl -u qxrp-bridge-relay -f"