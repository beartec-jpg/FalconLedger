#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Mainnet stablecoin setup: issuer accounts ONLY — no bootstrap mint.
# Every F-USDC on mainnet must enter via the Sepolia/Ethereum bridge (1:1 locked USDC).
#
# Usage (on coordinator as root):
#   FALCON_BRIDGE_ONLY_REQUIRED=1 bash scripts/deploy-mainnet-stables.sh
#   FALCON_BRIDGE_ONLY_REQUIRED=1 bash scripts/deploy-mainnet-stables.sh --dry-run

set -euo pipefail

export FALCON_BRIDGE_ONLY_REQUIRED=1
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/deploy-issue-stables.sh" --bridge-only "$@"