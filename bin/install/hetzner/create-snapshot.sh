#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Snapshot a running qXRP Hetzner validator for rapid multi-region deployment.
#
# Requirements:
#   - hcloud CLI installed and authenticated (hcloud auth login)
#   - HCLOUD_SERVER_NAME env var set (or pass as first argument)
#
# Usage:
#   HCLOUD_SERVER_NAME=my-validator ./bin/install/hetzner/create-snapshot.sh
#   ./bin/install/hetzner/create-snapshot.sh my-validator

set -euo pipefail

SERVER="${1:-${HCLOUD_SERVER_NAME:-}}"
[[ -n "$SERVER" ]] || { echo "Usage: $0 <server-name>"; exit 1; }

command -v hcloud &>/dev/null || { echo "ERROR: hcloud CLI not found. Install from https://github.com/hetznercloud/cli"; exit 1; }

TIMESTAMP=$(date +%Y%m%d-%H%M%S)
SNAPSHOT_NAME="qxrp-validator-${TIMESTAMP}"

echo "Creating snapshot '$SNAPSHOT_NAME' of server '$SERVER'..."
hcloud server create-image \
    --type snapshot \
    --description "$SNAPSHOT_NAME" \
    "$SERVER"

echo "Snapshot '$SNAPSHOT_NAME' created successfully."
echo "Use it when creating new servers with: hcloud server create --image <snapshot-id> ..."
