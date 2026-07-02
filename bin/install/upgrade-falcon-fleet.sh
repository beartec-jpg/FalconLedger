#!/usr/bin/env bash
# Upgrade qXRP testnet fleet to qxrp/xrpld:falcon-only (consensus-breaking reset).
set -euo pipefail

DOCKER_IMAGE="${DOCKER_IMAGE:-qxrp/xrpld:falcon-only}"
BUILD_HOST="${BUILD_HOST:-46.224.0.140}"
SSH_OPTS=(-o StrictHostKeyChecking=no)

log() { echo "[upgrade] $*"; }
die() { echo "[upgrade] ERROR: $*" >&2; exit 1; }

# host:data_dir:container:compose_dir
VALIDATORS=(
  "46.224.0.140:/var/lib/qxrp-val2:qxrp-val2:/var/lib/qxrp-val2"
  "167.233.55.43:/var/lib/qxrp-validator:qxrp-validator:/var/lib/qxrp-validator"
  "204.168.175.194:/var/lib/qxrp-validator:qxrp-validator:/var/lib/qxrp-validator"
  "89.167.109.241:/var/lib/qxrp-validator:qxrp-validator:/var/lib/qxrp-validator"
)
FULL_NODE="46.224.0.140:/var/lib/qxrp-full:qxrp-full:/var/lib/qxrp-full"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

remote() {
  local host=$1; shift
  ssh "${SSH_OPTS[@]}" "root@${host}" "$@"
}

log "Phase 1: distribute ${DOCKER_IMAGE} to fleet"
for entry in "${VALIDATORS[@]}"; do
  host="${entry%%:*}"
  [[ "$host" == "$BUILD_HOST" ]] && continue
  log "  loading image on $host (from ${BUILD_HOST})"
  ssh "${SSH_OPTS[@]}" "root@${BUILD_HOST}" "docker save ${DOCKER_IMAGE}" \
    | ssh "${SSH_OPTS[@]}" "root@${host}" docker load
done

log "Phase 2: generate Falcon validator keys"
declare -a FALCON_PKS=()
for i in "${!VALIDATORS[@]}"; do
  IFS=':' read -r host _ _ _ <<< "${VALIDATORS[$i]}"
  log "  keygen on $host"
  out=$(remote "$host" bash -s "$DOCKER_IMAGE" <<'REMOTE'
set -euo pipefail
IMG="$1"
BOOT=$(mktemp -d)
trap 'rm -rf "$BOOT"' EXIT
mkdir -p "$BOOT/db"
cat > "$BOOT/xrpld.cfg" <<CFG
[server]
port_rpc
[port_rpc]
port = 5997
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
docker rm -f qxrp_upgrade_keygen >/dev/null 2>&1 || true
docker run -d --user root --name qxrp_upgrade_keygen -v "$BOOT:/data" "$IMG" \
  --conf /data/xrpld.cfg --standalone >/dev/null
for j in $(seq 1 40); do
  docker exec qxrp_upgrade_keygen curl -sf -X POST http://127.0.0.1:5997 \
    -H 'Content-Type: application/json' -d '{"method":"server_info","params":[{}]}' >/dev/null 2>&1 && break
  sleep 1
done
OUT=$(docker exec qxrp_upgrade_keygen curl -sf -X POST http://127.0.0.1:5997 \
  -H 'Content-Type: application/json' \
  -d '{"method":"validation_create","params":[{"key_type":"falcon512"}]}')
docker rm -f qxrp_upgrade_keygen >/dev/null 2>&1 || true
python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r['falcon_secret']); print(r['validation_public_key_hex'].upper())" <<< "$OUT"
REMOTE
) || die "keygen failed on $host"
  secret=$(echo "$out" | sed -n '1p')
  pk=$(echo "$out" | sed -n '2p')
  [[ -n "$secret" && -n "$pk" ]] || die "empty key from $host"
  echo "$secret" > "$WORK/host-${i}.secret"
  echo "$pk" > "$WORK/host-${i}.pk"
  FALCON_PKS+=("$pk")
  log "    pk=${pk:0:24}…"
done

UNL_FILE="$WORK/validators.txt"
{
  echo "[validators]"
  for pk in "${FALCON_PKS[@]}"; do echo "$pk"; done
} > "$UNL_FILE"

log "Phase 3: reset validator data and deploy Falcon config"
for i in "${!VALIDATORS[@]}"; do
  IFS=':' read -r host data_dir container compose_dir <<< "${VALIDATORS[$i]}"
  secret=$(cat "$WORK/host-${i}.secret")
  log "  $host ($container)"
  scp "${SSH_OPTS[@]}" "$UNL_FILE" "root@${host}:/tmp/falcon-validators.txt" >/dev/null
  remote "$host" bash -s "$data_dir" "$container" "$compose_dir" "$DOCKER_IMAGE" "$secret" <<'REMOTE'
set -euo pipefail
DATA_DIR="$1"
CONTAINER="$2"
COMPOSE_DIR="$3"
IMG="$4"
SECRET="$5"
CFG="$DATA_DIR/config/xrpld.cfg"

docker stop "$CONTAINER" 2>/dev/null || true
rm -rf "$DATA_DIR/db" "$DATA_DIR/nudb"
truncate -s 0 "$DATA_DIR/debug.log" 2>/dev/null || true
cp /tmp/falcon-validators.txt "$DATA_DIR/config/validators.txt"

python3 - "$CFG" "$SECRET" <<'PY'
import pathlib, re, sys
cfg_path, secret = sys.argv[1], sys.argv[2]
text = pathlib.Path(cfg_path).read_text()
text = re.sub(r'\[validation_seed\]\s*\n[^\n]*\n?', '', text)
text = re.sub(r'\[validation_falcon_secret\]\s*\n[^\n]*\n?', '', text)
if '[validation_falcon_secret]' not in text:
    text = text.replace('[validators_file]\nvalidators.txt\n',
                        '[validators_file]\nvalidators.txt\n\n[validation_falcon_secret]\n' + secret + '\n')
else:
    text = re.sub(r'(\[validation_falcon_secret\]\s*\n)[^\n]*',
                  r'\1' + secret, text)
pathlib.Path(cfg_path).write_text(text)
PY

if [[ -f "$COMPOSE_DIR/docker-compose.yml" ]]; then
  sed -i "s|image: qxrp/xrpld:.*|image: ${IMG}|" "$COMPOSE_DIR/docker-compose.yml"
  cd "$COMPOSE_DIR" && docker compose up -d --force-recreate
else
  die_msg="missing compose at $COMPOSE_DIR"
  echo "$die_msg" >&2; exit 1
fi
REMOTE
done

log "Phase 4: upgrade full-history node (non-validator)"
IFS=':' read -r fhost fdata fcontainer fcompose <<< "$FULL_NODE"
scp "${SSH_OPTS[@]}" "$UNL_FILE" "root@${fhost}:/tmp/falcon-validators.txt" >/dev/null
remote "$fhost" bash -s "$fdata" "$fcontainer" "$fcompose" "$DOCKER_IMAGE" <<'REMOTE'
set -euo pipefail
DATA_DIR="$1"
CONTAINER="$2"
COMPOSE_DIR="$3"
IMG="$4"
docker stop "$CONTAINER" 2>/dev/null || true
rm -rf "$DATA_DIR/db" "$DATA_DIR/nudb"
truncate -s 0 "$DATA_DIR/debug.log" 2>/dev/null || true
cp /tmp/falcon-validators.txt "$DATA_DIR/config/validators.txt"
sed -i "s|image: qxrp/xrpld:.*|image: ${IMG}|" "$COMPOSE_DIR/docker-compose.yml"
cd "$COMPOSE_DIR" && docker compose up -d --force-recreate
REMOTE

log "Phase 5: wait for validators to become healthy"
sleep 30
for entry in "${VALIDATORS[@]}"; do
  IFS=':' read -r host _ container _ <<< "$entry"
  status=$(remote "$host" "docker inspect --format='{{.State.Health.Status}}' $container 2>/dev/null || echo unknown")
  log "  $host $container health=$status"
done

log "Done. Falcon-only fleet deployed with ${#FALCON_PKS[@]} validators."
log "Re-bond each validator on-chain (ValidatorRegister + ValidatorBond)."
for pk in "${FALCON_PKS[@]}"; do echo "  $pk"; done