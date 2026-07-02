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
if [[ "${1:-}" == "--dry-run" ]]; then
  DRY_RUN="--dry-run"
fi

mkdir -p "${STATE_DIR}"

echo "==> Deploying issue-testnet-stables.py to ${STATE_DIR}"
install -m 0755 "${REPO_ROOT}/scripts/issue-testnet-stables.py" "${STATE_DIR}/issue-testnet-stables.py"

echo "==> Running stablecoin bootstrap"
python3 "${STATE_DIR}/issue-testnet-stables.py" \
  --admin-rpc "${ADMIN_RPC}" \
  --public-rpc "${PUBLIC_RPC}" \
  --container "${CONTAINER}" \
  --state-file "${STATE_FILE}" \
  --manifest "${MANIFEST}" \
  ${DRY_RUN}

if [[ -z "${DRY_RUN}" && -f "${MANIFEST}" ]]; then
  echo ""
  echo "==> Issuer manifest:"
  python3 -m json.tool "${MANIFEST}"
fi

echo ""
echo "Done. Copy config/testnet-stables.json env block into qXRP-faucet-wallet .env / Vercel."