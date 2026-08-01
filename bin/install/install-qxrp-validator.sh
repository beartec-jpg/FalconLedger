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

# Image runs as uid 1001 (xrpld). Host bind mounts must be readable/writable
# by that user. mktemp is 0700 by default → Permission denied inside the container.
# Parent dir must be writable too (peerfinder.sqlite, debug.log live next to db/).
# Prefer chmod 777 over chown 1001 — chown leaves files the host user cannot delete
# (breaks traps under set -e and prints "Operation not permitted").
prepare_xrpld_mount() {
  local dir="$1"
  mkdir -p "${dir}/db"
  chmod 777 "$dir" "${dir}/db" 2>/dev/null || true
}

# Safe cleanup when container created root/1001-owned files under a host mount.
# Always returns 0 — never abort the install under set -e.
safe_rm_rf() {
  local p="$1"
  [[ -n "$p" && -e "$p" ]] || return 0
  # Prefer sudo first when files may be uid 1001; never print errors.
  command sudo rm -rf "$p" >/dev/null 2>&1 || rm -rf "$p" >/dev/null 2>&1 || true
  return 0
}

# Verify the pulled image can sign/simulate Falcon txs (local).
smoke_test_local_image() {
  log "Falcon smoke test — local image (${DOCKER_IMAGE})..."
  local smoke_dir wp sim_result sim_code acct secret i
  smoke_dir=$(mktemp -d)

  # Paths must be container paths (/data/...), not host mktemp paths
  cat > "${smoke_dir}/xrpld.cfg" <<'CFG'
[server]
port_rpc
[port_rpc]
port = 5998
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = /data/db
[database_path]
/data
[debug_logfile]
/data/debug.log
CFG
  prepare_xrpld_mount "$smoke_dir"
  chmod 644 "${smoke_dir}/xrpld.cfg" 2>/dev/null || true

  docker rm -f qxrp_falcon_smoke >/dev/null 2>&1 || true
  docker run --rm -d --name qxrp_falcon_smoke \
    -v "${smoke_dir}:/data" \
    "${DOCKER_IMAGE}" \
    --conf /data/xrpld.cfg --standalone >/dev/null

  for i in $(seq 1 45); do
    docker exec qxrp_falcon_smoke curl -sf -X POST http://127.0.0.1:5998 \
      -H 'Content-Type: application/json' -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
    if [[ $i -eq 45 ]]; then
      warn "Smoke container logs:"
      docker logs qxrp_falcon_smoke 2>&1 | tail -30 || true
      docker rm -f qxrp_falcon_smoke >/dev/null 2>&1 || true
      safe_rm_rf "$smoke_dir"
      die "Falcon smoke test: ephemeral node did not start"
    fi
    sleep 1
  done

  wp=$(docker exec qxrp_falcon_smoke curl -sf -X POST http://127.0.0.1:5998 \
    -H 'Content-Type: application/json' \
    -d '{"method":"wallet_propose","params":[{"key_type":"falcon512"}]}')
  acct=$(echo "$wp" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['account_id'])")
  secret=$(echo "$wp" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['falcon_secret'])")

  sim_result=$(docker exec qxrp_falcon_smoke curl -sf -X POST http://127.0.0.1:5998 \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"simulate\",\"params\":[{\"tx_json\":{\"TransactionType\":\"Payment\",\"Account\":\"${acct}\",\"Destination\":\"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh\",\"Amount\":\"1\",\"Fee\":\"12\",\"Sequence\":1,\"LastLedgerSequence\":99999999},\"falcon_secret\":\"${secret}\"}]}")

  sim_code=$(echo "$sim_result" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r.get('engine_result') or r.get('error',''))")
  docker stop qxrp_falcon_smoke >/dev/null 2>&1 || true
  docker rm -f qxrp_falcon_smoke >/dev/null 2>&1 || true
  safe_rm_rf "$smoke_dir"

  if echo "$sim_code" | grep -qiE 'BAD.?SIGN|invalid.?sign|temBAD'; then
    die "Falcon smoke test FAILED (local): bad signature (${sim_code})"
  fi
  [[ -n "$sim_code" ]] || die "Falcon smoke test FAILED (local): empty simulate response"

  log "  local image OK (simulate → ${sim_code})"
}

# Optional: re-submit a known Falcon Payment against public RPC / peer admin ports.
# Local image smoke already proved Falcon sign/simulate. Fleet check needs outbound
# HTTP to PUBLIC_RPC (port 6005) — many home ISPs / firewalls block that. Do not
# hard-fail install when unreachable; node still peers on 51235 after start.
smoke_test_validator_fleet() {
  log "Falcon smoke test — validator fleet (bootstrap peers)..."
  local tx_json blob host port url result engine err msg

  tx_json=$(rpc_public account_tx "{\"account\":\"${FAUCET_SMOKE_ACCOUNT}\",\"limit\":1}") || {
    warn "Cannot reach public RPC (${PUBLIC_RPC}) from this network."
    warn "Skipping fleet smoke — local image already OK. Install will continue."
    warn "After start, your node connects to peers on TCP 51235 (not 6005)."
    return 0
  }
  local tx_hash
  tx_hash=$(echo "$tx_json" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result', {})
txs = r.get('transactions') or []
if not txs:
    raise SystemExit('no faucet transactions on ledger')
print(txs[0].get('tx', {}).get('hash') or txs[0].get('hash',''))
") || {
    warn "No faucet Payment found for smoke account — skipping fleet smoke"
    return 0
  }

  blob=$(rpc_public tx "{\"transaction\":\"${tx_hash}\",\"binary\":true}" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx'])") || {
    warn "Could not fetch tx blob — skipping fleet smoke"
    return 0
  }

  # Submit via public HTTP RPC (correct). Peer list ports are 51235 (binary P2P), not HTTP.
  result=$(rpc_to "${PUBLIC_RPC}" submit "{\"tx_blob\":\"${blob}\"}" 2>/dev/null) || {
    warn "Public RPC submit unreachable — skipping fleet smoke"
    return 0
  }
  engine=$(echo "$result" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('engine_result',''))" 2>/dev/null || echo "")
  err=$(echo "$result" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('error',''))" 2>/dev/null || echo "")
  msg=$(echo "$result" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('error_message',''))" 2>/dev/null || echo "")

  if [[ "$err" == "invalidTransaction" ]] || echo "$msg" | grep -qi 'invalid signature'; then
    die "Falcon smoke test FAILED on public RPC: ${err} ${msg} — image cannot verify Falcon signatures. See docs/fleet-image-pinning.md"
  fi
  log "  public RPC OK (${engine:-${err:-accepted}})"

  local peers_checked=0
  IFS=',' read -ra PEER_LIST <<< "$BOOTSTRAP_PEERS"
  for peer in "${PEER_LIST[@]}"; do
    host="${peer%%:*}"
    [[ -z "$host" ]] && continue
    # Try common HTTP admin/public ports (51235 is P2P only)
    for port in 6005 5005; do
      url="http://${host}:${port}"
      result=$(rpc_to "$url" server_info "{}" 2>/dev/null) || continue
      peers_checked=$((peers_checked + 1))
      log "  ${host}:${port} reachable"
      break
    done
  done

  if [[ "$peers_checked" -lt 1 ]]; then
    warn "No bootstrap peer HTTP ports reachable from here (OK on restricted networks)."
  else
    log "  fleet smoke test passed (${peers_checked} peer HTTP endpoint(s))"
  fi
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
# Data dir only — config stays owned by installing user so we can write keys/cfg.
prepare_xrpld_mount "$DATA_DIR"
# Config must be readable by container uid 1001
chmod 755 "$CONFIG_DIR" 2>/dev/null || true

log "Pulling ${DOCKER_IMAGE}..."
docker pull "$DOCKER_IMAGE" >/dev/null

run_falcon_smoke_tests

# ── Key generation (one-shot bootstrap container) ─────────────────────────────
if [[ -f "$KEYS_FILE" ]]; then
  log "Reusing existing keys at $KEYS_FILE"
else
  log "Generating validator keys..."
  BOOT_DIR=$(mktemp -d)
  # Expand path now (set -u safe); clear trap after successful keygen.
  trap "docker rm -f qxrp_keygen_boot >/dev/null 2>&1 || true; safe_rm_rf '${BOOT_DIR}' || true" EXIT

  cat > "${BOOT_DIR}/xrpld.cfg" <<'CFG'
[server]
port_rpc
[port_rpc]
port = 5999
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = /data/db
[database_path]
/data
[debug_logfile]
/data/debug.log
CFG
  prepare_xrpld_mount "$BOOT_DIR"
  chmod 644 "${BOOT_DIR}/xrpld.cfg" 2>/dev/null || true

  docker rm -f qxrp_keygen_boot >/dev/null 2>&1 || true
  docker run --rm -d --name qxrp_keygen_boot \
    -v "${BOOT_DIR}:/data" \
    "$DOCKER_IMAGE" \
    --conf /data/xrpld.cfg --standalone >/dev/null

  for i in $(seq 1 45); do
    docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
      -H 'Content-Type: application/json' -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
    if [[ $i -eq 45 ]]; then
      warn "Keygen container logs:"
      docker logs qxrp_keygen_boot 2>&1 | tail -30 || true
      die "Keygen bootstrap timed out"
    fi
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
  safe_rm_rf "$BOOT_DIR"
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

# ── validators.txt (FLEET UNL — never self-only or the node solo-forks) ───────
# Download official testnet UNL; optional TRUSTED_KEYS appends extras.
FLEET_UNL_URL="${QXRP_FLEET_UNL_URL:-https://raw.githubusercontent.com/beartec-jpg/FalconLedger/develop/bin/install/testnet-falcon-unl.txt}"
if [[ -f "$(dirname "${BASH_SOURCE[0]:-$0}")/testnet-falcon-unl.txt" ]]; then
  cp "$(dirname "${BASH_SOURCE[0]:-$0}")/testnet-falcon-unl.txt" "$VALIDATORS_FILE"
elif curl -fsSL "$FLEET_UNL_URL" -o "$VALIDATORS_FILE"; then
  log "Downloaded fleet UNL → $VALIDATORS_FILE"
else
  die "Cannot load fleet UNL (set QXRP_TRUSTED_FALCON_KEYS or fix network). Self-only UNL causes a solo chain."
fi
# Ensure [validators] header
if ! head -1 "$VALIDATORS_FILE" | grep -qi '\[validators\]'; then
  { echo "[validators]"; cat "$VALIDATORS_FILE"; } > "${VALIDATORS_FILE}.tmp"
  mv "${VALIDATORS_FILE}.tmp" "$VALIDATORS_FILE"
fi
if [[ -n "$TRUSTED_KEYS" ]]; then
  IFS=',' read -ra KEYS <<< "$TRUSTED_KEYS"
  for k in "${KEYS[@]}"; do
    k="${k//[[:space:]]/}"
    [[ -n "$k" ]] && echo "$k" >> "$VALIDATORS_FILE"
  done
fi
# Do NOT add only this node's key as the sole UNL entry.
nkeys=$(grep -cE '^FB[0-9A-F]+' "$VALIDATORS_FILE" 2>/dev/null || echo 0)
[[ "${nkeys}" -ge 3 ]] || die "Fleet UNL looks empty (${nkeys} keys). Aborting to avoid solo fork."
log "UNL keys loaded: ${nkeys}"

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
full

[validation_quorum]
${QUORUM}

# Do NOT enable validation_falcon_secret on first boot — a self-only or
# early-proposing node solo-forks (ledgers 5–10) and never sees network funds.
# Keys stay in validator-keys.json; finish-bond / post-sync enables proposing.
[#validation_falcon_secret_disabled_until_synced]
# ${FALCON_SECRET}

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
# Readable by container uid 1001; secrets live in validator-keys.json (0600)
chmod 644 "${CONFIG_DIR}/xrpld.cfg"

# ── Dashboard (same as cloud bootstrap — portal can link :8080) ───────────────
DASH_DIR="${HOME}/.qxrp/${NODE_NAME}/dashboard"
DASH_BASE="${QXRP_DASH_RAW_BASE:-https://raw.githubusercontent.com/beartec-jpg/FalconLedger/develop/tools/dashboard}"
mkdir -p "$DASH_DIR"
if [[ -f "$(dirname "${BASH_SOURCE[0]:-$0}")/../../tools/dashboard/server.py" ]]; then
  cp "$(dirname "${BASH_SOURCE[0]:-$0}")/../../tools/dashboard/server.py" "${DASH_DIR}/server.py"
  cp "$(dirname "${BASH_SOURCE[0]:-$0}")/../../tools/dashboard/requirements.txt" "${DASH_DIR}/requirements.txt"
else
  curl -fsSL "${DASH_BASE}/server.py" -o "${DASH_DIR}/server.py" \
    || die "Cannot download dashboard server.py"
  curl -fsSL "${DASH_BASE}/requirements.txt" -o "${DASH_DIR}/requirements.txt" \
    || die "Cannot download dashboard requirements.txt"
fi
# Written again after ACCOUNT is known (keys); placeholder for first compose up
printf 'VALIDATOR_ACCOUNT=%s\n' "${ACCOUNT:-}" > "${DASH_DIR}/.env"

# ── docker-compose (xrpld + challenger + dashboard) ───────────────────────────
DASH_NAME="qxrp-${NODE_NAME}-dashboard"
cat > "$COMPOSE_FILE" <<COMPOSE
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

  dashboard:
    image: python:3.13-slim
    container_name: ${DASH_NAME}
    restart: unless-stopped
    ports:
      - "8080:8080"
    environment:
      XRPLD_RPC_URL: http://xrpld:6005
      NETWORK_RPC_URL: ${PUBLIC_RPC}
      DASHBOARD_PORT: "8080"
      VALIDATOR_ACCOUNT: ${ACCOUNT:-}
    env_file:
      - ${DASH_DIR}/.env
    volumes:
      - ${DASH_DIR}:/app:ro
      - ${DATA_DIR}/dashboard-metrics:/var/lib/qxrp-dashboard
    working_dir: /app
    command: ["sh", "-c", "pip install -q -r requirements.txt && python3 server.py"]
    depends_on:
      - xrpld
    networks:
      - qxrp

networks:
  qxrp:
    driver: bridge
COMPOSE
mkdir -p "${DATA_DIR}/dashboard-metrics"
chmod 777 "${DATA_DIR}/dashboard-metrics" 2>/dev/null || true

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

# Prefer local node for balance + submit (home networks often cannot hit PUBLIC_RPC :6005).
account_balance_drops() {
  local acct="$1" bal=""
  bal=$(rpc_local account_info "{\"account\":\"${acct}\",\"ledger_index\":\"validated\"}" 2>/dev/null \
    | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('result',{}).get('account_data',{}).get('Balance','0'))" 2>/dev/null || true)
  if [[ -z "${bal}" || "${bal}" == "0" || "${bal}" == "None" ]]; then
    bal=$(rpc_public account_info "{\"account\":\"${acct}\",\"ledger_index\":\"validated\"}" 2>/dev/null \
      | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('result',{}).get('account_data',{}).get('Balance','0'))" 2>/dev/null || echo "0")
  fi
  echo "${bal:-0}"
}

submit_blob() {
  local blob="$1"
  # Local first (always available once container is up), then public
  rpc_local submit "{\"tx_blob\":\"${blob}\"}" >/dev/null 2>&1 \
    || rpc_public submit "{\"tx_blob\":\"${blob}\"}" >/dev/null 2>&1 \
    || warn "submit failed (local + public) — re-run bond when node is synced"
}

# Drop finish-bond helper next to the node (home nets often block public :6005).
FINISH_BOND="${HOME}/.qxrp/${NODE_NAME}/finish-bond.sh"
if [[ -f "$(dirname "${BASH_SOURCE[0]:-$0}")/finish-qxrp-bond.sh" ]]; then
  cp "$(dirname "${BASH_SOURCE[0]:-$0}")/finish-qxrp-bond.sh" "$FINISH_BOND"
else
  curl -fsSL "${INSTALL_RAW_BASE}/finish-qxrp-bond.sh" -o "$FINISH_BOND" 2>/dev/null || true
fi
chmod +x "$FINISH_BOND" 2>/dev/null || true

# ── Wait for funding + auto bond ──────────────────────────────────────────────
log "Waiting for funding on ${ACCOUNT} (local ledger first — home nets often block ${PUBLIC_RPC})..."
log "If you already paid and this spins forever: Ctrl+C then: bash ${FINISH_BOND} --node-name ${NODE_NAME}"
FUNDED=0
for i in $(seq 1 180); do
  BAL=$(account_balance_drops "${ACCOUNT}")
  if [[ "${BAL:-0}" -ge "$MIN_FUND_DROPS" ]]; then
    FUNDED=1
    log "Funded: $(python3 -c "print(${BAL}/1000000)") qXRP"
    break
  fi
  if [[ $((i % 6)) -eq 0 ]]; then
    # Show local sync so "0 drops" is not mysterious
    LSTATE=$(rpc_local server_info 2>/dev/null | python3 -c "import sys,json; print((json.load(sys.stdin).get('result') or {}).get('info',{}).get('server_state','?'))" 2>/dev/null || echo "?")
    LPEERS=$(rpc_local server_info 2>/dev/null | python3 -c "import sys,json; print((json.load(sys.stdin).get('result') or {}).get('info',{}).get('peers',0))" 2>/dev/null || echo 0)
    log "  … bal=${BAL} drops (need ${MIN_FUND_DROPS}) local_state=${LSTATE} peers=${LPEERS}"
  fi
  sleep 10
done

if [[ "$FUNDED" -eq 0 ]]; then
  warn "Timed out waiting for funding on local/public RPC."
  warn "If Payment already succeeded on-chain, finish bond after local sync:"
  warn "  bash ${FINISH_BOND} --node-name ${NODE_NAME}"
  warn "Docker tip: sudo usermod -aG docker \$USER && newgrp docker"
  exit 0
fi

log "Submitting ValidatorRegister..."
REG=$(rpc_local sign "{\"tx_json\":{\"TransactionType\":\"ValidatorRegister\",\"Account\":\"${ACCOUNT}\",\"PublicKey\":\"${FALCON_PK}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"12\"},\"falcon_secret\":\"${FALCON_SECRET}\"}")
REG_RESULT=$(echo "$REG" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result','error'))")
log "  ValidatorRegister: ${REG_RESULT}"

if [[ "$REG_RESULT" == "tesSUCCESS" || "$REG_RESULT" == "terQUEUED" ]]; then
  BLOB=$(echo "$REG" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
  submit_blob "$BLOB"
  sleep 4
fi

log "Submitting ValidatorBond (1,000 qXRP)..."
BOND=$(rpc_local sign "{\"tx_json\":{\"TransactionType\":\"ValidatorBond\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"BondedAmount\":\"${MIN_BOND_DROPS}\",\"Fee\":\"12\"},\"falcon_secret\":\"${FALCON_SECRET}\"}")
BOND_RESULT=$(echo "$BOND" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result','error'))")
log "  ValidatorBond: ${BOND_RESULT}"

if [[ "$BOND_RESULT" == "tesSUCCESS" || "$BOND_RESULT" == "terQUEUED" || "$BOND_RESULT" == "tecNO_PERMISSION" ]]; then
  if [[ "$BOND_RESULT" != "tecNO_PERMISSION" ]]; then
    BLOB=$(echo "$BOND" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
    submit_blob "$BLOB"
  else
    log "  Already bonded"
  fi
else
  warn "Bond failed: ${BOND_RESULT}"
fi

# ── Claim + auto-payout cron (node-side, not protocol) ────────────────────────
# ClaimReward → credits validator operator account.
# Then Payment sweeps excess free balance to payout_address from keys JSON.
# Protocol does NOT auto-pay the wallet; this cron does it for the operator.
CLAIM_SCRIPT="${HOME}/.qxrp/${NODE_NAME}/claim-rewards.sh"
cat > "$CLAIM_SCRIPT" <<'SCRIPT'
#!/usr/bin/env bash
# ClaimReward (if score allows) then Payment surplus → payout_address.
# tecINSUFF_FEE on claim = score below minimum (not a real fee error).
set -euo pipefail
KEYS_FILE="__KEYS_FILE__"
SERVICE="__SERVICE__"
# Keep this many drops free for fees/reserve (50 FALCON). Bond is already locked separately.
KEEP_DROPS="${QXRP_PAYOUT_KEEP_DROPS:-50000000}"
FEE_DROPS="${QXRP_TX_FEE_DROPS:-12}"
MIN_SEND="${QXRP_PAYOUT_MIN_DROPS:-1000000}"  # 1 FALCON dust floor

CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['consensus_key_hex'])")
ACCOUNT=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['account_address'])")
FALCON_SECRET=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['falcon_secret'])")
PAYOUT=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}')).get('payout_address') or '')")

docker_cmd() {
  if docker info >/dev/null 2>&1; then docker "$@"
  else sudo docker "$@"
  fi
}

rpc() {
  docker_cmd exec "${SERVICE}" curl -sf --max-time 20 -X POST http://127.0.0.1:5005 \
    -H 'Content-Type: application/json' \
    -d "$1"
}

logm() { echo "$(date -Is) $*"; }

# ── 1) ClaimReward (once per epoch when score is high enough) ───────────────
SIGN=$(rpc "{\"method\":\"sign\",\"params\":[{\"tx_json\":{\"TransactionType\":\"ClaimReward\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"${FEE_DROPS}\"},\"falcon_secret\":\"${FALCON_SECRET}\"}]}") || true
BLOB=$(echo "${SIGN:-}" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('tx_blob') or '')" 2>/dev/null || true)
if [[ -n "${BLOB}" ]]; then
  SUB=$(rpc "{\"method\":\"submit\",\"params\":[{\"tx_blob\":\"${BLOB}\"}]}") || true
  logm "claim=$(echo "${SUB:-}" | python3 -c "import sys,json; r=json.load(sys.stdin).get('result',{}); print(r.get('engine_result') or r.get('error') or '?')" 2>/dev/null || echo '?')"
else
  logm "claim=skip_no_blob"
fi

# ── 2) Sweep free balance → payout wallet ───────────────────────────────────
[[ -n "$PAYOUT" && "$PAYOUT" != "$ACCOUNT" ]] || { logm "payout=skip_no_payout_address"; exit 0; }

AI=$(rpc "{\"method\":\"account_info\",\"params\":[{\"account\":\"${ACCOUNT}\",\"ledger_index\":\"validated\"}]}") || true
BAL=$(echo "${AI:-}" | python3 -c "import sys,json; d=json.load(sys.stdin).get('result',{}); print(d.get('account_data',{}).get('Balance') or '0')" 2>/dev/null || echo 0)
SEND=$(python3 -c "b=int('${BAL}'); k=int('${KEEP_DROPS}'); f=int('${FEE_DROPS}'); m=int('${MIN_SEND}'); s=b-k-f; print(s if s>=m else 0)")
if [[ "${SEND}" -le 0 ]]; then
  logm "payout=skip_balance bal=${BAL} keep=${KEEP_DROPS}"
  exit 0
fi

PSIGN=$(rpc "{\"method\":\"sign\",\"params\":[{\"tx_json\":{\"TransactionType\":\"Payment\",\"Account\":\"${ACCOUNT}\",\"Destination\":\"${PAYOUT}\",\"Amount\":\"${SEND}\",\"Fee\":\"${FEE_DROPS}\"},\"falcon_secret\":\"${FALCON_SECRET}\"}]}") || true
PBLOB=$(echo "${PSIGN:-}" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('tx_blob') or '')" 2>/dev/null || true)
if [[ -z "${PBLOB}" ]]; then
  logm "payout=sign_failed"
  exit 0
fi
PSUB=$(rpc "{\"method\":\"submit\",\"params\":[{\"tx_blob\":\"${PBLOB}\"}]}") || true
logm "payout=$(echo "${PSUB:-}" | python3 -c "import sys,json; r=json.load(sys.stdin).get('result',{}); print(r.get('engine_result') or r.get('error') or '?')" 2>/dev/null || echo '?') amount_drops=${SEND} to=${PAYOUT}"
SCRIPT
sed -i "s|__KEYS_FILE__|${KEYS_FILE}|g; s|__SERVICE__|${SERVICE_NAME}|g" "$CLAIM_SCRIPT"
chmod +x "$CLAIM_SCRIPT"

# Daily at 06:17 UTC — claim once/day is enough (ClaimReward is once per epoch);
# same job sweeps surplus to payout. Override: QXRP_CLAIM_CRON='17 * * * *' for hourly.
CRON_SCHED="${QXRP_CLAIM_CRON:-17 6 * * *}"
CRON_LINE="${CRON_SCHED} ${CLAIM_SCRIPT} >> ${HOME}/.qxrp/${NODE_NAME}/claim.log 2>&1"
( crontab -l 2>/dev/null | grep -vF "$CLAIM_SCRIPT" || true; echo "$CRON_LINE" ) | crontab -
log "Claim+payout cron installed (${CRON_SCHED}) → ${PAYOUT_ADDRESS}"

# ── Done ──────────────────────────────────────────────────────────────────────
STATE=$(rpc_local server_info | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['info']['server_state'])" 2>/dev/null || echo "unknown")

# Ensure dashboard .env has validator account after keys exist
printf 'VALIDATOR_ACCOUNT=%s\n' "${ACCOUNT}" > "${HOME}/.qxrp/${NODE_NAME}/dashboard/.env" 2>/dev/null || true

big "VALIDATOR + CHALLENGER + DASHBOARD READY"
echo "  Falcon container : ${SERVICE_NAME}"
echo "  Challenger       : ${CHALLENGER_NAME}"
echo "  Dashboard        : qxrp-${NODE_NAME}-dashboard  →  http://<this-host-ip>:8080"
echo "  Falcon address   : ${ACCOUNT}"
echo "  BTC fee address  : ${BTC_FEE_ADDRESS} (${BTC_NETWORK})"
echo "  Falcon PK        : ${FALCON_PK}"
echo "  Payout           : ${PAYOUT_ADDRESS}"
echo "  State            : ${STATE}"
echo "  Funding sheet    : ${FUNDING_FILE}"
echo "  Logs             : docker logs -f ${SERVICE_NAME}"
echo "                     docker logs -f ${CHALLENGER_NAME}"
echo "                     docker logs -f qxrp-${NODE_NAME}-dashboard"
echo "  Config           : ${CONFIG_DIR}"
echo ""
echo "  Local dash:  http://127.0.0.1:8080"
echo "  LAN/Tailscale: http://\$(hostname -I | awk '{print \$1}'):8080"
echo "  Portal can link that host:8080 (needs TCP 8080 reachable from the internet for remote proxy)."
echo "  Portal guide: https://falcon-ledger.com/validator"
echo ""