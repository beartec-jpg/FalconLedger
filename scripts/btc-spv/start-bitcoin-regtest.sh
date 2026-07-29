#!/usr/bin/env bash
# Start Bitcoin Core regtest for BitVM vault / SPV e2e (Docker).
# Does not touch Falcon testnet 1001 or mainnet soak.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATADIR="${BTC_REGTEST_DATADIR:-$ROOT/data/btc-spv-1101/bitcoin-regtest}"
NAME="${BTC_REGTEST_NAME:-btc-spv-regtest}"
RPC_PORT="${BTC_REGTEST_RPC_PORT:-18443}"
P2P_PORT="${BTC_REGTEST_P2P_PORT:-18444}"
# Official-ish multi-arch image used widely for regtest
IMAGE="${BTC_REGTEST_IMAGE:-ruimarinho/bitcoin-core:latest}"

mkdir -p "$DATADIR"

if docker ps --format '{{.Names}}' | grep -qx "$NAME"; then
  echo "Already running: $NAME"
  docker exec "$NAME" bitcoin-cli -regtest -rpcuser=spv -rpcpassword=spv getblockchaininfo | head -c 400
  echo
  exit 0
fi

if docker ps -a --format '{{.Names}}' | grep -qx "$NAME"; then
  echo "Starting existing container $NAME..."
  docker start "$NAME"
else
  echo "Pulling $IMAGE (if needed) and creating $NAME..."
  docker pull "$IMAGE"
  docker run -d --name "$NAME" \
    -p "127.0.0.1:${RPC_PORT}:18443" \
    -p "127.0.0.1:${P2P_PORT}:18444" \
    -v "$DATADIR:/home/bitcoin/.bitcoin" \
    "$IMAGE" \
    -regtest=1 \
    -server=1 \
    -rpcbind=0.0.0.0 \
    -rpcallowip=0.0.0.0/0 \
    -rpcuser=spv \
    -rpcpassword=spv \
    -fallbackfee=0.0002 \
    -txindex=1 \
    -printtoconsole=1
fi

echo "Waiting for RPC..."
for i in $(seq 1 60); do
  if docker exec "$NAME" bitcoin-cli -regtest -rpcuser=spv -rpcpassword=spv getblockchaininfo >/dev/null 2>&1; then
    echo "bitcoind regtest ready (container=$NAME rpc=127.0.0.1:$RPC_PORT)"
    docker exec "$NAME" bitcoin-cli -regtest -rpcuser=spv -rpcpassword=spv getblockchaininfo
    exit 0
  fi
  sleep 1
done
echo "ERROR: bitcoind RPC did not become ready" >&2
exit 1
