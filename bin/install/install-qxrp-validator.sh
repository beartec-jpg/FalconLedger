#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# One-command qXRP Falcon validator installer for the public testnet (Network 1001).
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \
#     --payout rYourWalletAddress \
#     --node-name my-validator
#
# What it does:
#   1. Installs Docker (if needed) and pulls the pinned Falcon image (never :latest)
#   2. Falcon smoke tests (local image + validator fleet signature check)
#   3. Generates Falcon validator keys + Bitcoin challenger fee wallet
#   4. Writes config (UNL, peers, network 1001)
#   5. Starts xrpld + BitVM challenger sidecar (watch-only until fee wallet funded)
#   6. Prints ONLY two fund steps:
#        • FALCON (qXRP) → validator r-address for bond
#        • BTC (testnet) → challenger fee address
#   7. Auto-bonds when Falcon funded; challenger uses BTC float for dispute fees only
#
# Challenger BTC is NOT the shared reserve key — fee wallet only.
#
# Mainnet / launch: set QXRP_XRPLD_IMAGE to a digest from IMAGE_DIGEST.txt
#   export QXRP_XRPLD_IMAGE='qxrp/xrpld@sha256:…'

set -euo pipefail

# ── Defaults (Falcon testnet 1001 SPV bridge fleet) ───────────────────────────
# Pin to current public testnet SPV bridge binary. Never floating :latest.
# Override with QXRP_XRPLD_IMAGE only if you know you need a different build.
DOCKER_IMAGE="${QXRP_XRPLD_IMAGE:-qxrp/xrpld:btc-spv-v6}"
NETWORK_ID=1001
PUBLIC_RPC="${QXRP_PUBLIC_RPC:-http://46.224.0.140:6005}"
BOOTSTRAP_PEERS="46.224.0.140:51235,167.233.55.43:51235,204.168.175.194:51235,89.167.109.241:51235"
# Comma-separated Falcon validator public keys (uppercase hex). Required for UNL.
TRUSTED_KEYS="${QXRP_TRUSTED_FALCON_KEYS:-}"
PAYOUT_ADDRESS=""
NODE_NAME="my-qxrp-node"
MIN_FUND_DROPS=1100000000   # 1,100 qXRP (reserve + 1000 bond + fees)
MIN_BOND_DROPS=1000000000   # 1,000 qXRP bond
QUORUM=3
MIN_RAM_MB=3800
MIN_DISK_GB=40
FAUCET_SMOKE_ACCOUNT="${QXRP_FAUCET_ACCOUNT:-rwzhiWW4GYK2sQVR5Lw4iDpYLANB5krJXY}"
SKIP_SMOKE_TEST=0

CONFIG_DIR="${HOME}/.qxrp/${NODE_NAME}/config"
DATA_DIR="${HOME}/.qxrp/${NODE_NAME}/data"
COMPOSE_FILE="${HOME}/.qxrp/${NODE_NAME}/docker-compose.yml"
KEYS_FILE="${CONFIG_DIR}/validator-keys.json"
BTC_CHALLENGER_WALLET="${CONFIG_DIR}/btc-challenger-wallet.json"
CHALLENGER_DIR="${HOME}/.qxrp/${NODE_NAME}/bitvm-challenger"
VALIDATORS_FILE="${CONFIG_DIR}/validators.txt"
SERVICE_NAME="qxrp-${NODE_NAME}"
CHALLENGER_NAME="qxrp-${NODE_NAME}-challenger"
ADMIN_PORT=5005
# Raw install assets (works with curl | bash)
INSTALL_RAW_BASE="${QXRP_INSTALL_RAW_BASE:-https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install}"
BTC_NETWORK="${QXRP_BTC_NETWORK:-testnet}"

# ── Helpers ───────────────────────────────────────────────────────────────────
log()  { echo -e "\033[1;32m[qxrp]\033[0m $*"; }
warn() { echo -e "\033[1;33m[qxrp] WARN:\033[0m $*"; }
die()  { echo -e "\033[1;31m[qxrp] ERROR:\033[0m $*" >&2; exit 1; }
big()  { echo -e "\n\033[1;36m══════════════════════════════════════════════════════════════\033[0m"; echo -e "\033[1;37m  $*\033[0m"; echo -e "\033[1;36m══════════════════════════════════════════════════════════════\033[0m\n"; }

rpc_local() {
  local method="$1" params="${2:-{}}"
  docker exec "${SERVICE_NAME}" curl -sf -X POST "http://127.0.0.1:${ADMIN_PORT}" \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"${method}\",\"params\":[${params}]}"
}

rpc_public() {
  local method="$1" params="${2:-{}}"
  curl -sf --max-time 8 -X POST "${PUBLIC_RPC}" \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"${method}\",\"params\":[${params}]}"
}

rpc_to() {
  local url="$1" method="$2" params="${3:-{}}"
  curl -sf --max-time 10 -X POST "${url}" \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"${method}\",\"params\":[${params}]}"
}

# Verify the pulled image can sign/simulate Falcon txs (local).
smoke_test_local_image() {
  log "Falcon smoke test — local image (${DOCKER_IMAGE})..."
  local smoke_dir
  smoke_dir=$(mktemp -d)
  trap 'rm -rf "$smoke_dir"' RETURN

  cat > "${smoke_dir}/xrpld.cfg" <<CFG
[server]
port_rpc
[port_rpc]
port = 5998
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = ${smoke_dir}/db
[database_path]
${smoke_dir}
[debug_logfile]
${smoke_dir}/debug.log
CFG

  docker run --rm -d --name qxrp_falcon_smoke \
    -v "${smoke_dir}:/data" \
    "${DOCKER_IMAGE}" \
    --conf /data/xrpld.cfg --standalone >/dev/null

  for i in $(seq 1 30); do
    docker exec qxrp_falcon_smoke curl -sf -X POST http://127.0.0.1:5998 \
      -H 'Content-Type: application/json' -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
    [[ $i -eq 30 ]] && die "Falcon smoke test: ephemeral node did not start"
    sleep 1
  done

  local wp sim_result sim_code
  wp=$(docker exec qxrp_falcon_smoke curl -sf -X POST http://127.0.0.1:5998 \
    -H 'Content-Type: application/json' \
    -d '{"method":"wallet_propose","params":[{"key_type":"falcon512"}]}')
  local acct secret
  acct=$(echo "$wp" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['account_id'])")
  secret=$(echo "$wp" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['falcon_secret'])")

  sim_result=$(docker exec qxrp_falcon_smoke curl -sf -X POST http://127.0.0.1:5998 \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"simulate\",\"params\":[{\"tx_json\":{\"TransactionType\":\"Payment\",\"Account\":\"${acct}\",\"Destination\":\"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh\",\"Amount\":\"1\",\"Fee\":\"12\",\"Sequence\":1,\"LastLedgerSequence\":99999999},\"falcon_secret\":\"${secret}\"}]}")

  sim_code=$(echo "$sim_result" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r.get('engine_result') or r.get('error',''))")
  if echo "$sim_code" | grep -qiE 'BAD.?SIGN|invalid.?sign|temBAD'; then
    die "Falcon smoke test FAILED (local): bad signature (${sim_code})"
  fi
  [[ -n "$sim_code" ]] || die "Falcon smoke test FAILED (local): empty simulate response"

  docker stop qxrp_falcon_smoke >/dev/null 2>&1 || true
  log "  local image OK (simulate → ${sim_code})"
}

# Re-submit a known validated Falcon Payment to every bootstrap peer.
smoke_test_validator_fleet() {
  log "Falcon smoke test — validator fleet (bootstrap peers)..."
  local tx_json blob host port url result engine err msg

  tx_json=$(rpc_public account_tx "{\"account\":\"${FAUCET_SMOKE_ACCOUNT}\",\"limit\":1}") \
    || die "Falcon smoke test: cannot reach public RPC (${PUBLIC_RPC})"
  local tx_hash
  tx_hash=$(echo "$tx_json" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result', {})
txs = r.get('transactions') or []
if not txs:
    raise SystemExit('no faucet transactions on ledger')
print(txs[0].get('tx', {}).get('hash') or txs[0].get('hash',''))
") || die "Falcon smoke test: no faucet Payment tx found for ${FAUCET_SMOKE_ACCOUNT}"

  blob=$(rpc_public tx "{\"transaction\":\"${tx_hash}\",\"binary\":true}" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx'])") \
    || die "Falcon smoke test: could not fetch tx blob for ${tx_hash}"

  local peers_checked=0
  IFS=',' read -ra PEER_LIST <<< "$BOOTSTRAP_PEERS"
  for peer in "${PEER_LIST[@]}"; do
    host="${peer%%:*}"
    port="${peer##*:}"
    [[ -z "$host" || -z "$port" ]] && continue
    url="http://${host}:${port}"
    result=$(rpc_to "$url" submit "{\"tx_blob\":\"${blob}\"}" 2>/dev/null) || {
      warn "  ${host}: unreachable — skipping"
      continue
    }
    engine=$(echo "$result" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('engine_result',''))" 2>/dev/null || echo "")
    err=$(echo "$result" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('error',''))" 2>/dev/null || echo "")
    msg=$(echo "$result" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('error_message',''))" 2>/dev/null || echo "")
    peers_checked=$((peers_checked + 1))

    if [[ "$err" == "invalidTransaction" ]] || echo "$msg" | grep -qi 'invalid signature'; then
      die "Falcon smoke test FAILED on ${host}: ${err} ${msg} — validator image cannot verify Falcon signatures. See docs/fleet-image-pinning.md"
    fi
    if [[ -z "$engine" && -n "$err" ]]; then
      die "Falcon smoke test FAILED on ${host}: ${err} ${msg}"
    fi
    log "  ${host} OK (${engine:-accepted})"
  done

  [[ "$peers_checked" -ge 1 ]] || die "Falcon smoke test: no bootstrap peers reachable"
  log "  fleet smoke test passed (${peers_checked} peer(s))"
}

run_falcon_smoke_tests() {
  [[ "$SKIP_SMOKE_TEST" -eq 1 ]] && { warn "Skipping Falcon smoke tests (--skip-smoke-test)"; return 0; }
  big "FALCON SMOKE TEST"
  log "Pinned image: ${DOCKER_IMAGE}"
  log "Docs: https://github.com/beartec-jpg/qXRP/blob/develop/docs/fleet-image-pinning.md"
  smoke_test_local_image
  smoke_test_validator_fleet
  log "Falcon smoke tests passed — proceeding with validator install"
}

# ── Args ──────────────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --payout)       PAYOUT_ADDRESS="$2"; shift 2 ;;
    --node-name)    NODE_NAME="$2"; CONFIG_DIR="${HOME}/.qxrp/${NODE_NAME}/config"; DATA_DIR="${HOME}/.qxrp/${NODE_NAME}/data"; COMPOSE_FILE="${HOME}/.qxrp/${NODE_NAME}/docker-compose.yml"; KEYS_FILE="${CONFIG_DIR}/validator-keys.json"; BTC_CHALLENGER_WALLET="${CONFIG_DIR}/btc-challenger-wallet.json"; CHALLENGER_DIR="${HOME}/.qxrp/${NODE_NAME}/bitvm-challenger"; VALIDATORS_FILE="${CONFIG_DIR}/validators.txt"; SERVICE_NAME="qxrp-${NODE_NAME}"; CHALLENGER_NAME="qxrp-${NODE_NAME}-challenger"; shift 2 ;;
    --rpc-url)      PUBLIC_RPC="$2"; shift 2 ;;
    --peers)        BOOTSTRAP_PEERS="$2"; shift 2 ;;
    --trusted-keys) TRUSTED_KEYS="$2"; shift 2 ;;
    --skip-smoke-test) SKIP_SMOKE_TEST=1; shift ;;
    -h|--help)
      sed -n '2,23p' "$0"
      echo "  --skip-smoke-test   Skip Falcon image + fleet signature checks (not recommended)"
      exit 0
      ;;
    *) die "Unknown option: $1 (try --help)" ;;
  esac
done

[[ -n "$PAYOUT_ADDRESS" ]] || die "--payout <r-address> is required (your wallet address for reward withdrawals)"
[[ "$PAYOUT_ADDRESS" =~ ^r[1-9A-HJ-NP-Za-km-z]{24,34}$ ]] || die "Invalid --payout address"

# ── Pre-flight ────────────────────────────────────────────────────────────────
log "qXRP Falcon Validator Installer (network ${NETWORK_ID})"
log "Node name : ${NODE_NAME}"
log "Payout    : ${PAYOUT_ADDRESS}"
log "Public RPC: ${PUBLIC_RPC}"

RAM_MB=$(awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo)
DISK_GB=$(df --output=avail -BG "${HOME}" | tail -1 | tr -d 'G ')
[[ "$RAM_MB" -ge "$MIN_RAM_MB" ]]  || die "Need ${MIN_RAM_MB} MB RAM (have ${RAM_MB})"
[[ "$DISK_GB" -ge "$MIN_DISK_GB" ]] || die "Need ${MIN_DISK_GB} GB disk (have ${DISK_GB})"

if ! command -v docker &>/dev/null; then
  log "Installing Docker..."
  curl -fsSL https://get.docker.com | sh
fi

# Fresh Docker install: user is not in the docker group until re-login.
# usermod alone does not fix the current shell — use sudo docker for this run.
if ! docker info &>/dev/null 2>&1; then
  sudo usermod -aG docker "$USER" 2>/dev/null || true
  if sudo docker info &>/dev/null 2>&1; then
    log "Docker socket needs group access — using sudo docker for this install"
    log "After install finishes, run once: newgrp docker   (or log out/in)"
    docker() { command sudo docker "$@"; }
  else
    die "Cannot access Docker. Fix with:
  sudo usermod -aG docker \$USER
  newgrp docker
then re-run this installer."
  fi
fi

mkdir -p "$CONFIG_DIR" "$DATA_DIR"

log "Pulling ${DOCKER_IMAGE}..."
docker pull "$DOCKER_IMAGE" >/dev/null

run_falcon_smoke_tests

# ── Key generation (one-shot bootstrap container) ─────────────────────────────
if [[ -f "$KEYS_FILE" ]]; then
  log "Reusing existing keys at $KEYS_FILE"
else
  log "Generating validator keys..."
  BOOT_DIR=$(mktemp -d)
  trap 'rm -rf "$BOOT_DIR"' EXIT

  cat > "${BOOT_DIR}/xrpld.cfg" <<CFG
[server]
port_rpc
[port_rpc]
port = 5999
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = ${BOOT_DIR}/db
[database_path]
${BOOT_DIR}
[debug_logfile]
${BOOT_DIR}/debug.log
CFG

  docker run --rm -d --name qxrp_keygen_boot \
    -v "${BOOT_DIR}:/data" \
    "$DOCKER_IMAGE" \
    --conf /data/xrpld.cfg --standalone >/dev/null

  for i in $(seq 1 30); do
    docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
      -H 'Content-Type: application/json' -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
    [[ $i -eq 30 ]] && die "Keygen bootstrap timed out"
    sleep 1
  done

  FALCON_JSON=$(docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
    -H 'Content-Type: application/json' \
    -d '{"method":"wallet_propose","params":[{"key_type":"falcon512"}]}')
  ACCOUNT=$(echo "$FALCON_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['account_id'])")
  FALCON_SECRET=$(echo "$FALCON_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['falcon_secret'])")
  FALCON_PK=$(echo "$FALCON_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print((r.get('public_key_hex') or r.get('public_key','')).upper())")

  # ZERO classical keys — no node_seed generation.
  docker stop qxrp_keygen_boot >/dev/null 2>&1 || true
  rm -rf "$BOOT_DIR"
  trap - EXIT

  python3 - <<PY
import json, os
data = {
    "falcon_secret": "${FALCON_SECRET}",
    "validation_public_key_hex": "${FALCON_PK}",
    "consensus_key_hex": "${FALCON_PK}",
    "falcon_public_key_hex": "${FALCON_PK}",
    "account_address": "${ACCOUNT}",
    "payout_address": "${PAYOUT_ADDRESS}",
    "node_name": "${NODE_NAME}",
    "network_id": ${NETWORK_ID},
}
with open("${KEYS_FILE}", "w") as f:
    json.dump(data, f, indent=2)
os.chmod("${KEYS_FILE}", 0o600)
PY
  log "Keys saved to $KEYS_FILE (Falcon-only; no classical node_seed)"
fi

FALCON_SECRET=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['falcon_secret'])")
FALCON_PK=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['falcon_public_key_hex'])")
ACCOUNT=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['account_address'])")
CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['consensus_key_hex'])")

# ── Bitcoin challenger fee wallet (NOT reserve / NOT custody) ─────────────────
if [[ ! -f "$BTC_CHALLENGER_WALLET" ]]; then
  log "Generating Bitcoin challenger fee wallet..."
  python3 -m pip install -q ecdsa 2>/dev/null || true
  GEN_PY="${CHALLENGER_DIR}/generate-btc-challenger-wallet.py"
  mkdir -p "$CHALLENGER_DIR"
  if [[ -f "$(dirname "${BASH_SOURCE[0]:-$0}")/generate-btc-challenger-wallet.py" ]]; then
    cp "$(dirname "${BASH_SOURCE[0]:-$0}")/generate-btc-challenger-wallet.py" "$GEN_PY"
  else
    curl -fsSL "${INSTALL_RAW_BASE}/generate-btc-challenger-wallet.py" -o "$GEN_PY" \
      || die "Cannot download generate-btc-challenger-wallet.py"
  fi
  python3 "$GEN_PY" "$BTC_CHALLENGER_WALLET"
  log "BTC challenger wallet → $BTC_CHALLENGER_WALLET"
else
  log "Reusing BTC challenger wallet at $BTC_CHALLENGER_WALLET"
fi

if [[ "$BTC_NETWORK" == "mainnet" ]]; then
  BTC_FEE_ADDRESS=$(python3 -c "import json; print(json.load(open('${BTC_CHALLENGER_WALLET}'))['address_mainnet'])")
  BTC_FLOAT_HINT=$(python3 -c "import json; print(json.load(open('${BTC_CHALLENGER_WALLET}')).get('suggested_float_mainnet_btc','0.001'))")
else
  BTC_FEE_ADDRESS=$(python3 -c "import json; print(json.load(open('${BTC_CHALLENGER_WALLET}'))['address_testnet'])")
  BTC_FLOAT_HINT=$(python3 -c "import json; print(json.load(open('${BTC_CHALLENGER_WALLET}')).get('suggested_float_testnet_btc','0.001'))")
fi

# Challenger sidecar sources
mkdir -p "$CHALLENGER_DIR"
if [[ -f "$(dirname "${BASH_SOURCE[0]:-$0}")/bitvm-challenger/challenger.py" ]]; then
  cp "$(dirname "${BASH_SOURCE[0]:-$0}")/bitvm-challenger/challenger.py" "$CHALLENGER_DIR/"
  cp "$(dirname "${BASH_SOURCE[0]:-$0}")/bitvm-challenger/requirements.txt" "$CHALLENGER_DIR/" 2>/dev/null || true
else
  curl -fsSL "${INSTALL_RAW_BASE}/bitvm-challenger/challenger.py" -o "${CHALLENGER_DIR}/challenger.py" \
    || die "Cannot download bitvm-challenger/challenger.py"
fi

# ── validators.txt ────────────────────────────────────────────────────────────
{
  echo "[validators]"
  echo "$FALCON_PK"
  IFS=',' read -ra KEYS <<< "$TRUSTED_KEYS"
  for k in "${KEYS[@]}"; do
    k="${k//[[:space:]]/}"
    [[ -n "$k" && "$k" != "$FALCON_PK" ]] && echo "$k"
  done
} > "$VALIDATORS_FILE"

# ── xrpld.cfg ─────────────────────────────────────────────────────────────────
IPS_BLOCK=""
IFS=',' read -ra PEERS <<< "$BOOTSTRAP_PEERS"
for peer in "${PEERS[@]}"; do
  IPS_BLOCK+="${peer/:/ }"$'\n'
done

cat > "${CONFIG_DIR}/xrpld.cfg" <<CFG
[network_id]
${NETWORK_ID}

[node_size]
medium

[ledger_history]
256

[validation_quorum]
${QUORUM}

[validation_falcon_secret]
${FALCON_SECRET}

# P2P identity reuses Falcon validation key (classical [node_seed] forbidden)

[validators_file]
/cfg/validators.txt

[features]
ProofOfParticipation
SingleAssetVault
LendingProtocol

[server]
port_rpc_admin_local
port_peer_public

[port_rpc_admin_local]
port = ${ADMIN_PORT}
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[port_peer_public]
port = 51235
ip = 0.0.0.0
protocol = peer

[node_db]
type = NuDB
path = /data/nudb
advisory_delete = 0

[database_path]
/data/db

[debug_logfile]
/data/debug.log

[ips_fixed]
${IPS_BLOCK}
CFG
chmod 600 "${CONFIG_DIR}/xrpld.cfg"

# ── docker-compose (xrpld + BitVM challenger sidecar) ─────────────────────────
cat > "$COMPOSE_FILE" <<COMPOSE
version: "3.9"
services:
  xrpld:
    image: ${DOCKER_IMAGE}
    container_name: ${SERVICE_NAME}
    restart: unless-stopped
    volumes:
      - ${CONFIG_DIR}:/cfg:ro
      - ${DATA_DIR}:/data
    ports:
      - "${ADMIN_PORT}:${ADMIN_PORT}"
      - "51235:51235"
    command: ["--conf", "/cfg/xrpld.cfg"]
    networks:
      - qxrp

  bitvm-challenger:
    image: python:3.13-slim
    container_name: ${CHALLENGER_NAME}
    restart: unless-stopped
    environment:
      FALCON_RPC: http://xrpld:6005
      PUBLIC_RPC: ${PUBLIC_RPC}
      BTC_NETWORK: ${BTC_NETWORK}
      CHALLENGER_WALLET: /cfg/btc-challenger-wallet.json
      CHALLENGER_STATUS: /data/challenger-status.json
      CHALLENGER_POLL_SEC: "30"
    volumes:
      - ${CONFIG_DIR}:/cfg:ro
      - ${DATA_DIR}:/data
      - ${CHALLENGER_DIR}:/app:ro
    working_dir: /app
    command: ["python3", "-u", "/app/challenger.py"]
    depends_on:
      - xrpld
    networks:
      - qxrp

networks:
  qxrp:
    driver: bridge
COMPOSE

# Expose Falcon RPC on 6005 inside the compose network (cfg may use ADMIN_PORT only)
# Map peer + ensure challenger can reach: use host-published admin if needed
# Challenger uses service name xrpld — need port 6005 in container.
# If ADMIN_PORT is 5005 only, add public rpc in cfg or point FALCON_RPC to host.
# Prefer adding port_rpc on 6005 in container for sidecar:
if ! grep -q 'port = 6005' "${CONFIG_DIR}/xrpld.cfg" 2>/dev/null; then
  cat >> "${CONFIG_DIR}/xrpld.cfg" <<'RPC6005'

[port_rpc_public]
port = 6005
ip = 0.0.0.0
protocol = http
RPC6005
fi

log "Starting validator + BitVM challenger..."
docker compose -f "$COMPOSE_FILE" pull
docker compose -f "$COMPOSE_FILE" up -d

for i in $(seq 1 60); do
  rpc_local server_info >/dev/null 2>&1 && break
  [[ $i -eq 60 ]] && die "Validator RPC did not come up. Check: docker logs ${SERVICE_NAME}"
  sleep 2
done
log "Validator RPC is up"
log "Challenger: docker logs -f ${CHALLENGER_NAME}"

# Persist human funding sheet
FUNDING_FILE="${HOME}/.qxrp/${NODE_NAME}/FUNDING.txt"
cat > "$FUNDING_FILE" <<FUND
qXRP validator + BitVM challenger — FUNDING (only steps left)
==============================================================

1) FALCON BOND (required for validator)
   Send at least 1,100 FALCON/qXRP to:
   ${ACCOUNT}

   (1,000 bond + reserve/fees). Auto-bond runs once funded.

2) BITCOIN CHALLENGER FEES (required to challenge BTC bridge txs)
   Network: ${BTC_NETWORK}
   Send ~${BTC_FLOAT_HINT} BTC to (FEE WALLET ONLY — not the shared reserve):
   ${BTC_FEE_ADDRESS}

   Day-to-day cost ≈ 0 if no attacks. This pays rare challenge tx fees only.
   This address does NOT control the shared BTC reserve.

Payout (rewards): ${PAYOUT_ADDRESS}
Node: ${NODE_NAME}
Keys: ${KEYS_FILE}
BTC wallet: ${BTC_CHALLENGER_WALLET}
Status: ${DATA_DIR}/challenger-status.json
FUND

# ── Print funding addresses ───────────────────────────────────────────────────
big "ONLY TWO FUNDING STEPS"
echo -e "  \033[1;37m1) FALCON — bond / run validator\033[0m"
echo -e "     Send \033[1;33m≥ 1,100 FALCON\033[0m to:"
echo -e "     \033[1;33m${ACCOUNT}\033[0m"
echo ""
echo -e "  \033[1;37m2) BITCOIN — BitVM challenger fees\033[0m"
echo -e "     Network: \033[1;33m${BTC_NETWORK}\033[0m"
echo -e "     Send ~\033[1;33m${BTC_FLOAT_HINT} BTC\033[0m to (fee wallet only):"
echo -e "     \033[1;33m${BTC_FEE_ADDRESS}\033[0m"
echo ""
echo "  Challenger fee wallet ≠ shared reserve. No custody key for the bridge."
echo "  Saved: ${FUNDING_FILE}"
echo "  Payout (rewards): ${PAYOUT_ADDRESS}"
echo ""

# ── Wait for funding + auto bond ──────────────────────────────────────────────
log "Waiting for funding on ${ACCOUNT} (polling ${PUBLIC_RPC})..."
FUNDED=0
for i in $(seq 1 180); do
  BAL=$(rpc_public account_info "{\"account\":\"${ACCOUNT}\",\"ledger_index\":\"validated\"}" 2>/dev/null \
    | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('result',{}).get('account_data',{}).get('Balance','0'))" 2>/dev/null || echo "0")
  if [[ "${BAL:-0}" -ge "$MIN_FUND_DROPS" ]]; then
    FUNDED=1
    log "Funded: $(python3 -c "print(${BAL}/1000000)") qXRP"
    break
  fi
  [[ $((i % 6)) -eq 0 ]] && log "  … still waiting (${BAL} drops, need ${MIN_FUND_DROPS})"
  sleep 10
done

if [[ "$FUNDED" -eq 0 ]]; then
  warn "Timed out waiting for funding. Bond manually when ready:"
  warn "  python3 ${HOME}/.qxrp/${NODE_NAME}/bond-validator.py"
  exit 0
fi

log "Submitting ValidatorRegister..."
REG=$(rpc_local sign "{\"tx_json\":{\"TransactionType\":\"ValidatorRegister\",\"Account\":\"${ACCOUNT}\",\"PublicKey\":\"${FALCON_PK}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"12\"},\"falcon_secret\":\"${FALCON_SECRET}\"}")
REG_RESULT=$(echo "$REG" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result','error'))")
log "  ValidatorRegister: ${REG_RESULT}"

if [[ "$REG_RESULT" == "tesSUCCESS" || "$REG_RESULT" == "terQUEUED" ]]; then
  BLOB=$(echo "$REG" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
  rpc_public submit "{\"tx_blob\":\"${BLOB}\"}" >/dev/null
  sleep 4
fi

log "Submitting ValidatorBond (1,000 qXRP)..."
BOND=$(rpc_local sign "{\"tx_json\":{\"TransactionType\":\"ValidatorBond\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"BondedAmount\":\"${MIN_BOND_DROPS}\",\"Fee\":\"12\"},\"falcon_secret\":\"${FALCON_SECRET}\"}")
BOND_RESULT=$(echo "$BOND" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result','error'))")
log "  ValidatorBond: ${BOND_RESULT}"

if [[ "$BOND_RESULT" == "tesSUCCESS" || "$BOND_RESULT" == "terQUEUED" || "$BOND_RESULT" == "tecNO_PERMISSION" ]]; then
  if [[ "$BOND_RESULT" != "tecNO_PERMISSION" ]]; then
    BLOB=$(echo "$BOND" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
    rpc_public submit "{\"tx_blob\":\"${BLOB}\"}" >/dev/null
  else
    log "  Already bonded"
  fi
else
  warn "Bond failed: ${BOND_RESULT}"
fi

# ── Reward claimer cron ───────────────────────────────────────────────────────
CLAIM_SCRIPT="${HOME}/.qxrp/${NODE_NAME}/claim-rewards.sh"
cat > "$CLAIM_SCRIPT" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
KEYS_FILE="__KEYS_FILE__"
SERVICE="__SERVICE__"
CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['consensus_key_hex'])")
ACCOUNT=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['account_address'])")
FALCON_SECRET=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['falcon_secret'])")

SIGN=$(docker exec "${SERVICE}" curl -sf -X POST http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d "{\"method\":\"sign\",\"params\":[{\"tx_json\":{\"TransactionType\":\"ClaimReward\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"12\"},\"falcon_secret\":\"${FALCON_SECRET}\"}]}")

RESULT=$(echo "$SIGN" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result',''))")
[[ "$RESULT" == "tesSUCCESS" ]] || exit 0
BLOB=$(echo "$SIGN" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
docker exec "${SERVICE}" curl -sf -X POST http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d "{\"method\":\"submit\",\"params\":[{\"tx_blob\":\"${BLOB}\"}]}" >/dev/null
SCRIPT
sed -i "s|__KEYS_FILE__|${KEYS_FILE}|g; s|__SERVICE__|${SERVICE_NAME}|g" "$CLAIM_SCRIPT"
chmod +x "$CLAIM_SCRIPT"

CRON_LINE="17 * * * * ${CLAIM_SCRIPT} >> ${HOME}/.qxrp/${NODE_NAME}/claim.log 2>&1"
( crontab -l 2>/dev/null | grep -vF "$CLAIM_SCRIPT"; echo "$CRON_LINE" ) | crontab -
log "Reward claimer installed (hourly cron)"

# ── Done ──────────────────────────────────────────────────────────────────────
STATE=$(rpc_local server_info | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['info']['server_state'])" 2>/dev/null || echo "unknown")

big "VALIDATOR + CHALLENGER READY"
echo "  Falcon container : ${SERVICE_NAME}"
echo "  Challenger       : ${CHALLENGER_NAME}"
echo "  Falcon address   : ${ACCOUNT}"
echo "  BTC fee address  : ${BTC_FEE_ADDRESS} (${BTC_NETWORK})"
echo "  Falcon PK        : ${FALCON_PK}"
echo "  Payout           : ${PAYOUT_ADDRESS}"
echo "  State            : ${STATE}"
echo "  Funding sheet    : ${FUNDING_FILE}"
echo "  Logs             : docker logs -f ${SERVICE_NAME}"
echo "                     docker logs -f ${CHALLENGER_NAME}"
echo "  Config           : ${CONFIG_DIR}"
echo ""
echo "  Portal guide: https://q-xrp-faucet.vercel.app/validator"
echo ""