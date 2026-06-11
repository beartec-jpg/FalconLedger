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
#   1. Installs Docker (if needed) and pulls qxrp/xrpld:falcon (pinned Falcon build)
#   2. Falcon smoke tests (local image + validator fleet signature check) — see docs/fleet-image-pinning.md
#   3. Generates validator + node identity keys
#   4. Writes config (UNL, bootstrap peers, network 1001)
#   5. Starts the validator container
#   6. Prints the validator r-address to fund
#   7. Polls the public RPC until ≥1,100 qXRP, then ValidatorRegister + ValidatorBond(1000)
#   8. Installs an hourly reward-claim cron job
#
# Recommended: fund the validator address from the faucet (2,000 qXRP drip) BEFORE
# or AFTER running this script — bonding starts automatically once funded.

set -euo pipefail

# ── Defaults (Falcon testnet) ─────────────────────────────────────────────────
# Pin to the Falcon-capable build — never use floating :latest across a validator fleet.
DOCKER_IMAGE="${QXRP_XRPLD_IMAGE:-qxrp/xrpld:falcon}"
NETWORK_ID=1001
PUBLIC_RPC="${QXRP_PUBLIC_RPC:-http://46.224.0.140:6005}"
BOOTSTRAP_PEERS="46.224.0.140:51235,167.233.55.43:51235,204.168.175.194:51235,89.167.109.241:51235"
TRUSTED_KEYS="n9KvHaT7SJmratfNFhzktVasbFUjhMDnLPx6tgnuv3pR93BjMcRd,n94NpYCkXPLdmUDw76LHvXRkJ8EYpc3tduM7MnYdMgGLwKVnzMSw,n9MX4NgUkvgGLpr6qYyPNtyWpq8Vp7bcYBkhVCAqWAYR2a8Z4Xtn,n94wZUjfykCnpoejwvA97iDVdY9bhNCoyxBa4qahSbQH5hMEeBAa"
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
VALIDATORS_FILE="${CONFIG_DIR}/validators.txt"
SERVICE_NAME="qxrp-${NODE_NAME}"
ADMIN_PORT=5005

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
    --node-name)    NODE_NAME="$2"; CONFIG_DIR="${HOME}/.qxrp/${NODE_NAME}/config"; DATA_DIR="${HOME}/.qxrp/${NODE_NAME}/data"; COMPOSE_FILE="${HOME}/.qxrp/${NODE_NAME}/docker-compose.yml"; KEYS_FILE="${CONFIG_DIR}/validator-keys.json"; VALIDATORS_FILE="${CONFIG_DIR}/validators.txt"; SERVICE_NAME="qxrp-${NODE_NAME}"; shift 2 ;;
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
  sudo usermod -aG docker "$USER" 2>/dev/null || true
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

  VAL_JSON=$(docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
    -H 'Content-Type: application/json' \
    -d '{"method":"validation_create","params":[{"key_type":"secp256k1"}]}')
  VAL_SEED=$(echo "$VAL_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['validation_seed'])")
  VAL_PUBKEY=$(echo "$VAL_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['validation_public_key'])")

  WP_JSON=$(docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"wallet_propose\",\"params\":[{\"seed\":\"${VAL_SEED}\",\"key_type\":\"secp256k1\"}]}")
  ACCOUNT=$(echo "$WP_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r['account_id'])")
  CONSENSUS_KEY=$(echo "$WP_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print((r.get('public_key_hex') or r.get('public_key','')).upper())")

  NODE_JSON=$(docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
    -H 'Content-Type: application/json' \
    -d '{"method":"wallet_propose","params":[{"key_type":"secp256k1"}]}')
  NODE_SEED=$(echo "$NODE_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['master_seed'])")

  FALCON_JSON=$(docker exec qxrp_keygen_boot curl -sf -X POST http://127.0.0.1:5999 \
    -H 'Content-Type: application/json' \
    -d '{"method":"wallet_propose","params":[{"key_type":"falcon512"}]}')
  FALCON_PK=$(echo "$FALCON_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print((r.get('public_key_hex') or r.get('public_key','')).upper())")

  docker stop qxrp_keygen_boot >/dev/null 2>&1 || true
  rm -rf "$BOOT_DIR"
  trap - EXIT

  python3 - <<PY
import json, os
data = {
    "validation_seed": "${VAL_SEED}",
    "validation_public_key": "${VAL_PUBKEY}",
    "consensus_key_hex": "${CONSENSUS_KEY}",
    "account_address": "${ACCOUNT}",
    "falcon_public_key_hex": "${FALCON_PK}",
    "node_seed": "${NODE_SEED}",
    "payout_address": "${PAYOUT_ADDRESS}",
    "node_name": "${NODE_NAME}",
    "network_id": ${NETWORK_ID},
}
with open("${KEYS_FILE}", "w") as f:
    json.dump(data, f, indent=2)
os.chmod("${KEYS_FILE}", 0o600)
PY
  log "Keys saved to $KEYS_FILE"
fi

VAL_SEED=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['validation_seed'])")
VAL_PUBKEY=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['validation_public_key'])")
NODE_SEED=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['node_seed'])")
ACCOUNT=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['account_address'])")
CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['consensus_key_hex'])")
FALCON_PK=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['falcon_public_key_hex'])")

# ── validators.txt ────────────────────────────────────────────────────────────
{
  echo "[validators]"
  echo "$VAL_PUBKEY"
  IFS=',' read -ra KEYS <<< "$TRUSTED_KEYS"
  for k in "${KEYS[@]}"; do echo "$k"; done
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

[validation_seed]
${VAL_SEED}

[node_seed]
${NODE_SEED}

[validators_file]
/cfg/validators.txt

[features]
ProofOfParticipation

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

# ── docker-compose ────────────────────────────────────────────────────────────
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
COMPOSE

log "Starting validator container..."
docker compose -f "$COMPOSE_FILE" pull
docker compose -f "$COMPOSE_FILE" up -d

for i in $(seq 1 60); do
  rpc_local server_info >/dev/null 2>&1 && break
  [[ $i -eq 60 ]] && die "Validator RPC did not come up. Check: docker logs ${SERVICE_NAME}"
  sleep 2
done
log "Validator RPC is up"

# ── Print funding address ─────────────────────────────────────────────────────
big "FUND THIS VALIDATOR ADDRESS"
echo -e "  \033[1;33m${ACCOUNT}\033[0m"
echo ""
echo "  Send at least 1,100 qXRP (1,100,000,000 drops) to this address."
echo "  Recommended: claim 2,000 qXRP from the faucet to your wallet, then send 1,100+ here."
echo "  Payout / withdraw destination saved: ${PAYOUT_ADDRESS}"
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
REG=$(rpc_local sign "{\"tx_json\":{\"TransactionType\":\"ValidatorRegister\",\"Account\":\"${ACCOUNT}\",\"PublicKey\":\"${FALCON_PK}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"12\"},\"secret\":\"${VAL_SEED}\"}")
REG_RESULT=$(echo "$REG" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result','error'))")
log "  ValidatorRegister: ${REG_RESULT}"

if [[ "$REG_RESULT" == "tesSUCCESS" || "$REG_RESULT" == "terQUEUED" ]]; then
  BLOB=$(echo "$REG" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
  rpc_public submit "{\"tx_blob\":\"${BLOB}\"}" >/dev/null
  sleep 4
fi

log "Submitting ValidatorBond (1,000 qXRP)..."
BOND=$(rpc_local sign "{\"tx_json\":{\"TransactionType\":\"ValidatorBond\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"BondedAmount\":\"${MIN_BOND_DROPS}\",\"Fee\":\"12\"},\"secret\":\"${VAL_SEED}\"}")
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
VAL_SEED=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['validation_seed'])")

SIGN=$(docker exec "${SERVICE}" curl -sf -X POST http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d "{\"method\":\"sign\",\"params\":[{\"tx_json\":{\"TransactionType\":\"ClaimReward\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"12\"},\"secret\":\"${VAL_SEED}\"}]}")

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

big "VALIDATOR READY"
echo "  Container : ${SERVICE_NAME}"
echo "  Address   : ${ACCOUNT}"
echo "  Pubkey    : ${VAL_PUBKEY}"
echo "  Payout    : ${PAYOUT_ADDRESS}"
echo "  State     : ${STATE}"
echo "  Logs      : docker logs -f ${SERVICE_NAME}"
echo "  Config    : ${CONFIG_DIR}"
echo ""
echo "  Portal guide: https://q-xrp-faucet.vercel.app/validator"
echo ""