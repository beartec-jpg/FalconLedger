#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Deploy and run stablecoin issuance on the coordinator full node.
#
# Usage (on coordinator as root):
#   bash scripts/deploy-issue-stables.sh
#   bash scripts/deploy-issue-stables.sh --dry-run

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ADMIN_RPC="${ADMIN_RPC_URL:-http://127.0.0.1:5005}"
PUBLIC_RPC="${PUBLIC_RPC_URL:-http://46.224.0.140:6005}"
CONTAINER="${DOCKER_CONTAINER:-qxrp-full}"
STATE_DIR="${STABLES_STATE_DIR:-/var/lib/qxrp-stables}"
STATE_FILE="${STABLES_STATE_FILE:-${STATE_DIR}/stables_state.json}"
MANIFEST="${STABLES_MANIFEST:-${REPO_ROOT}/config/testnet-stables.json}"

DRY_RUN=""
BRIDGE_ONLY=""
for arg in "$@"; do
  case "${arg}" in
    --dry-run) DRY_RUN="--dry-run" ;;
    --bridge-only) BRIDGE_ONLY="--bridge-only" ;;
  esac
done

# Mainnet / production launch: always bridge-only (F-USDC only from Sepolia locks).
if [[ "${FALCON_BRIDGE_ONLY_REQUIRED:-}" =~ ^(1|true|yes)$ && -z "${BRIDGE_ONLY}" ]]; then
  echo "ERROR: FALCON_BRIDGE_ONLY_REQUIRED is set — re-run with --bridge-only" >&2
  exit 1
fi

mkdir -p "${STATE_DIR}"

echo "==> Deploying stables scripts to ${STATE_DIR}"
install -m 0755 "${REPO_ROOT}/scripts/issue-testnet-stables.py" "${STATE_DIR}/issue-testnet-stables.py"
install -m 0644 "${REPO_ROOT}/scripts/launch-guards.py" "${STATE_DIR}/launch-guards.py"

echo "==> Running stablecoin bootstrap"
python3 "${STATE_DIR}/issue-testnet-stables.py" \
  --admin-rpc "${ADMIN_RPC}" \
  --public-rpc "${PUBLIC_RPC}" \
  --container "${CONTAINER}" \
  --state-file "${STATE_FILE}" \
  --manifest "${MANIFEST}" \
  ${BRIDGE_ONLY} \
  ${DRY_RUN}

if [[ -z "${DRY_RUN}" && -f "${MANIFEST}" ]]; then
  echo ""
  echo "==> Issuer manifest:"
  python3 -m json.tool "${MANIFEST}"
fi

echo ""
echo "Done. Copy config/testnet-stables.json env block into qXRP-faucet-wallet .env / Vercel."