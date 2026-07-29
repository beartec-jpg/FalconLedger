#!/usr/bin/env bash
# Drop-in bitcoin-cli for Docker regtest (matches vault.py / e2e_regtest.py).
set -euo pipefail
NAME="${BTC_REGTEST_NAME:-btc-spv-regtest}"
exec docker exec "$NAME" bitcoin-cli -regtest -rpcuser=spv -rpcpassword=spv "$@"
