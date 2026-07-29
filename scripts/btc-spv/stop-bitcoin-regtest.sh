#!/usr/bin/env bash
set -euo pipefail
NAME="${BTC_REGTEST_NAME:-btc-spv-regtest}"
if docker ps --format '{{.Names}}' | grep -qx "$NAME"; then
  docker exec "$NAME" bitcoin-cli -regtest -rpcuser=spv -rpcpassword=spv stop 2>/dev/null || true
  sleep 2
  docker stop "$NAME" >/dev/null
  echo "Stopped $NAME"
else
  echo "Not running: $NAME"
fi
