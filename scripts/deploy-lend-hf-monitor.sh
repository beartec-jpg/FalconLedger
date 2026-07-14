#!/usr/bin/env bash
# Deploy lending HF enforcement daemon on the coordinator full node.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
STATE_DIR="${LEND_STATE_DIR:-/var/lib/qxrp-lending}"
STABLES_STATE="${STABLES_STATE_FILE:-/var/lib/qxrp-stables/stables_state.json}"
CONTAINER="${DOCKER_CONTAINER:-qxrp-full}"
LENDING_MANIFEST="${LENDING_MANIFEST:-${REPO_ROOT}/../qXRP-faucet-wallet/public/config/lending.json}"

echo "==> Installing lend-hf-monitor to ${STATE_DIR}"
install -d -m 0750 "${STATE_DIR}"
install -m 0755 "${SCRIPT_DIR}/lend-hf-monitor.py" "${STATE_DIR}/lend-hf-monitor.py"
if [[ -f "${LENDING_MANIFEST}" ]]; then
  install -m 0644 "${LENDING_MANIFEST}" "${STATE_DIR}/lending.json"
fi

echo "==> Installing systemd unit qxrp-lend-hf-monitor.service"
cat > /etc/systemd/system/qxrp-lend-hf-monitor.service <<EOF
[Unit]
Description=qXRP lending health-factor LoanManage enforcement
After=docker.service network-online.target qxrp-bridge-relay.service
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${STATE_DIR}
Environment=PUBLIC_RPC_URL=http://127.0.0.1:6005
Environment=ADMIN_RPC_URL=http://127.0.0.1:5005
Environment=DOCKER_CONTAINER=${CONTAINER}
Environment=STABLES_STATE_FILE=${STABLES_STATE}
Environment=LENDING_MANIFEST=${STATE_DIR}/lending.json
Environment=LEND_HF_MONITOR_STATE=${STATE_DIR}/hf_monitor_state.json
# TESTNET_LENDING_BROKER_SECRET in override.conf (same as portal co-sign)
ExecStart=/usr/bin/python3 ${STATE_DIR}/lend-hf-monitor.py --loop --interval 60
Restart=on-failure
RestartSec=15

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable qxrp-lend-hf-monitor.service
systemctl restart qxrp-lend-hf-monitor.service
systemctl --no-pager status qxrp-lend-hf-monitor.service || true

echo "==> One-shot dry-run scan"
python3 "${STATE_DIR}/lend-hf-monitor.py" --once --dry-run \
  --manifest "${STATE_DIR}/lending.json" \
  --stables-state "${STABLES_STATE}" \
  --container "${CONTAINER}" || true

echo "Done. Logs: journalctl -u qxrp-lend-hf-monitor -f"
echo "Set broker secret: systemctl edit qxrp-lend-hf-monitor.service"
echo "  [Service]"
echo "  Environment=TESTNET_LENDING_BROKER_SECRET=<broker falcon_secret>"