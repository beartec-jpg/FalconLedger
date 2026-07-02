#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. SPDX-License-Identifier: AGPL-3.0-only
#
# Migrate the qXRP testnet fleet to full Falcon-only validators.
# Consensus-breaking: wipes validator DBs, regenerates Falcon keys, rebuilds UNL.
#
# Usage (on build/coordinator host with SSH to all fleet nodes):
#   export FLEET_HOSTS="46.224.0.140,167.233.55.43,204.168.175.194,89.167.109.241"
#   export DOCKER_IMAGE="qxrp/xrpld:falcon-only"
#   bash bin/install/migrate-falcon-fleet.sh
#
set -euo pipefail

FLEET_HOSTS="${FLEET_HOSTS:-46.224.0.140,167.233.55.43,204.168.175.194,89.167.109.241}"
DOCKER_IMAGE="${DOCKER_IMAGE:-qxrp/xrpld:falcon-only}"
NETWORK_ID=1001
QUORUM=3
PUBLIC_RPC="${QXRP_PUBLIC_RPC:-http://46.224.0.140:6005}"
BOOTSTRAP_PEERS="46.224.0.140:51235,167.233.55.43:51235,204.168.175.194:51235,89.167.109.241:51235"

log() { echo "[migrate] $*"; }
die() { echo "[migrate] ERROR: $*" >&2; exit 1; }

IFS=',' read -ra HOSTS <<< "$FLEET_HOSTS"
[[ ${#HOSTS[@]} -ge 3 ]] || die "Need at least 3 fleet hosts"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

log "Phase 1: generate Falcon keys on each host"
declare -a FALCON_PKS=()
for i in "${!HOSTS[@]}"; do
  host="${HOSTS[$i]}"
  log "  $host — keygen"
  out=$(ssh -o StrictHostKeyChecking=no "root@${host}" bash -s <<REMOTE
set -euo pipefail
IMG="${DOCKER_IMAGE}"
BOOT=$(mktemp -d)
trap 'rm -rf "\$BOOT"' EXIT
cat > "\$BOOT/xrpld.cfg" <<CFG
[server]
port_rpc
[port_rpc]
port = 5997
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = \$BOOT/db
[database_path]
\$BOOT
[debug_logfile]
\$BOOT/debug.log
CFG
docker run --rm -d --name qxrp_migrate_keygen "\$IMG" --conf /data/xrpld.cfg --standalone -a /data >/dev/null 2>&1 || \
  docker run --rm -d --name qxrp_migrate_keygen -v "\$BOOT:/data" "\$IMG" --conf /data/xrpld.cfg --standalone >/dev/null
for j in \$(seq 1 30); do
  docker exec qxrp_migrate_keygen curl -sf -X POST http://127.0.0.1:5997 \
    -H 'Content-Type: application/json' -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
  sleep 1
done
FALCON=\$(docker exec qxrp_migrate_keygen curl -sf -X POST http://127.0.0.1:5997 \
  -H 'Content-Type: application/json' -d '{"method":"wallet_propose","params":[{"key_type":"falcon512"}]}')
docker stop qxrp_migrate_keygen >/dev/null 2>&1 || true
python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r['falcon_secret']); print((r.get('public_key_hex') or '').upper())" <<< "\$FALCON"
REMOTE
) || die "Keygen failed on $host"
  secret=$(echo "$out" | sed -n '1p')
  pk=$(echo "$out" | sed -n '2p')
  [[ -n "$secret" && -n "$pk" ]] || die "Empty key from $host"
  echo "$secret" > "$WORK/host-${i}.secret"
  echo "$pk" > "$WORK/host-${i}.pk"
  FALCON_PKS+=("$pk")
  log "    pk=${pk:0:20}…"
done

UNL_FILE="$WORK/validators.txt"
{
  echo "[validators]"
  for pk in "${FALCON_PKS[@]}"; do echo "$pk"; done
} > "$UNL_FILE"

IPS_BLOCK=""
IFS=',' read -ra PEERS <<< "$BOOTSTRAP_PEERS"
for peer in "${PEERS[@]}"; do IPS_BLOCK+="${peer/:/ }"$'\n'; done

log "Phase 2: reset validator data + deploy Falcon config on each host"
for i in "${!HOSTS[@]}"; do
  host="${HOSTS[$i]}"
  secret=$(cat "$WORK/host-${i}.secret")
  pk=$(cat "$WORK/host-${i}.pk")
  log "  $host — reconfigure"

  scp -o StrictHostKeyChecking=no "$UNL_FILE" "root@${host}:/tmp/falcon-validators.txt" >/dev/null

  ssh -o StrictHostKeyChecking=no "root@${host}" bash -s <<REMOTE
set -euo pipefail
IMG="${DOCKER_IMAGE}"
SECRET='${secret}'
PK='${pk}'
DATA_DIR=/var/lib/qxrp-validator
CFG_DIR=\$DATA_DIR/config

docker stop qxrp-validator qxrp-full qxrp-val2 2>/dev/null || true
rm -rf "\$DATA_DIR/db" "\$DATA_DIR/data" "\$DATA_DIR/nudb" 2>/dev/null || true
mkdir -p "\$CFG_DIR"

# node peer key (P2P only — not consensus)
NODE_JSON=\$(docker run --rm "\$IMG" --conf /dev/null 2>/dev/null || true)
BOOT=\$(mktemp -d)
cat > "\$BOOT/xrpld.cfg" <<CFG
[server]
port_rpc
[port_rpc]
port = 5996
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = \$BOOT/db
[database_path]
\$BOOT
[debug_logfile]
\$BOOT/debug.log
CFG
docker run --rm -d --name qxrp_node_keygen -v "\$BOOT:/data" "\$IMG" --conf /data/xrpld.cfg --standalone >/dev/null
for j in \$(seq 1 20); do
  docker exec qxrp_node_keygen curl -sf http://127.0.0.1:5996 -X POST -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
  sleep 1
done
NODE_SEED=\$(docker exec qxrp_node_keygen curl -sf -X POST http://127.0.0.1:5996 \
  -H 'Content-Type: application/json' \
  -d '{"method":"wallet_propose","params":[{"key_type":"secp256k1"}]}' \
  | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['master_seed'])")
docker stop qxrp_node_keygen >/dev/null 2>&1 || true
rm -rf "\$BOOT"

cp /tmp/falcon-validators.txt "\$CFG_DIR/validators.txt"
cat > "\$CFG_DIR/xrpld.cfg" <<CFG
[network_id]
${NETWORK_ID}
[node_size]
medium
[ledger_history]
256
[validation_quorum]
${QUORUM}
[validation_falcon_secret]
\${SECRET}
[node_seed]
\${NODE_SEED}
[validators_file]
/cfg/validators.txt
[features]
ProofOfParticipation
[server]
port_rpc_admin_local
port_peer
[port_rpc_admin_local]
port = 5005
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[port_peer]
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
chmod 600 "\$CFG_DIR/xrpld.cfg"
echo "Falcon migration config written on \$(hostname)"
REMOTE
done

log "Phase 3: pull new image and restart validators"
for host in "${HOSTS[@]}"; do
  ssh -o StrictHostKeyChecking=no "root@${host}" \
    "docker pull ${DOCKER_IMAGE} && cd /var/lib/qxrp-validator 2>/dev/null && docker compose up -d --force-recreate || true"
done

log "Migration complete. Fresh ledger will sync from peers."
log "Next: fund each validator Falcon account and re-bond (ValidatorRegister + ValidatorBond)."
log "UNL written with ${#FALCON_PKS[@]} Falcon hex keys."
log "Public keys:"
for pk in "${FALCON_PKS[@]}"; do echo "  $pk"; done