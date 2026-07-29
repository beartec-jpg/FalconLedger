#!/usr/bin/env bash
# Phase 2: bitcoin-only BitVM vault e2e (CSV + hashlock).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="$ROOT/scripts/btc-spv:$PATH"

# Prefer wrapper if host has no bitcoin-cli
if ! command -v bitcoin-cli >/dev/null 2>&1; then
  ln -sfn "$ROOT/scripts/btc-spv/bitcoin-cli-wrapper.sh" /tmp/bitcoin-cli
  export PATH="/tmp:$PATH"
  # also name bitcoin-cli on PATH via function-friendly copy
  install -m 755 "$ROOT/scripts/btc-spv/bitcoin-cli-wrapper.sh" /tmp/bitcoin-cli 2>/dev/null || true
fi

"$ROOT/scripts/btc-spv/start-bitcoin-regtest.sh"

# vault.py shells out to bitcoin-cli — ensure wrapper is first on PATH
if ! command -v bitcoin-cli >/dev/null 2>&1 || ! bitcoin-cli getblockchaininfo >/dev/null 2>&1; then
  # install shim in a private bin
  BINDIR="$ROOT/data/btc-spv-1101/bin"
  mkdir -p "$BINDIR"
  cp "$ROOT/scripts/btc-spv/bitcoin-cli-wrapper.sh" "$BINDIR/bitcoin-cli"
  chmod +x "$BINDIR/bitcoin-cli"
  export PATH="$BINDIR:$PATH"
fi

cd "$ROOT/scripts/btc-spv/bitvm"
python3 e2e_regtest.py --bitcoin-only "$@"
