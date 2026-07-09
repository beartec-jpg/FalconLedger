#!/usr/bin/env bash
# Stop qxrp containers and wipe ledger DBs (keeps config/) across the testnet fleet.
# Usage: bash bin/install/wipe-fleet-for-genesis.sh
set -euo pipefail

SSH_OPTS=(-o StrictHostKeyChecking=no -o ConnectTimeout=20)
BUILD_HOST="${BUILD_HOST:-46.224.0.140}"

# host:compose_dir (repeat host for multiple dirs)
ENTRIES=(
  "46.224.0.140:/var/lib/qxrp-full"
  "46.224.0.140:/var/lib/qxrp-val2"
  "167.233.55.43:/var/lib/qxrp-validator"
  "204.168.175.194:/var/lib/qxrp-validator"
  "89.167.109.241:/var/lib/qxrp-validator"
  "5.78.142.246:/var/lib/qxrp-validator"
)

log() { echo "[wipe] $*"; }

remote_wipe() {
  local host=$1 dir=$2
  log "$host — wiping $dir"
  ssh "${SSH_OPTS[@]}" "root@${host}" bash -s "$dir" <<'REMOTE' || log "  WARN: failed $host $dir"
set -euo pipefail
DIR="$1"
[[ -d "$DIR" ]] || { echo "missing $DIR"; exit 0; }
cd "$DIR"
docker compose down --remove-orphans 2>/dev/null || docker stop "$(basename "$DIR")" 2>/dev/null || true
rm -rf db data nudb debug.log
mkdir -p db data nudb
chown -R 1001:1001 db data nudb 2>/dev/null || true
echo "  wiped $(basename "$DIR")"
REMOTE
}

log "Phase 1 — stop builds on ${BUILD_HOST}"
ssh "${SSH_OPTS[@]}" "root@${BUILD_HOST}" \
  'pkill -f "docker build.*qxrp" 2>/dev/null || true; pkill -f "ninja.*xrpld" 2>/dev/null || true; echo builds stopped; exit 0'

log "Phase 2 — wipe ledger data on fleet"
for entry in "${ENTRIES[@]}"; do
  host="${entry%%:*}"
  dir="${entry#*:}"
  remote_wipe "$host" "$dir"
done

log "Done. Nodes are stopped with empty db/ — ready for genesis image + compose up."