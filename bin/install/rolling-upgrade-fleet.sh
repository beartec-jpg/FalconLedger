#!/usr/bin/env bash
# Rolling upgrade: new xrpld image without wiping ledger data.
# Usage:
#   DOCKER_IMAGE=qxrp/xrpld:cid-popl bash bin/install/rolling-upgrade-fleet.sh
set -euo pipefail

DOCKER_IMAGE="${DOCKER_IMAGE:-qxrp/xrpld:cid-popl}"
BUILD_HOST="${BUILD_HOST:-46.224.0.140}"
SSH_OPTS=(-o StrictHostKeyChecking=no -o ConnectTimeout=15)

log() { echo "[rolling] $*"; }
die() { echo "[rolling] ERROR: $*" >&2; exit 1; }

# host:container:compose_dir
NODES=(
  "46.224.0.140:qxrp-full:/var/lib/qxrp-full"
  "46.224.0.140:qxrp-val2:/var/lib/qxrp-val2"
  "167.233.55.43:qxrp-validator:/var/lib/qxrp-validator"
  "204.168.175.194:qxrp-validator:/var/lib/qxrp-validator"
  "89.167.109.241:qxrp-validator:/var/lib/qxrp-validator"
  "5.78.142.246:qxrp-validator:/var/lib/qxrp-validator"
  "192.241.247.158:qxrp-validator:/var/lib/qxrp-validator"
)

remote() {
  local host=$1; shift
  ssh "${SSH_OPTS[@]}" "root@${host}" "$@"
}

log "Phase 1: distribute ${DOCKER_IMAGE} from ${BUILD_HOST}"
for entry in "${NODES[@]}"; do
  host="${entry%%:*}"
  [[ "$host" == "$BUILD_HOST" ]] && continue
  log "  loading image on $host"
  ssh "${SSH_OPTS[@]}" "root@${BUILD_HOST}" "docker save ${DOCKER_IMAGE}" \
    | ssh "${SSH_OPTS[@]}" "root@${host}" docker load
done

log "Phase 2: update configs and restart (preserve DB)"
for entry in "${NODES[@]}"; do
  IFS=':' read -r host container compose_dir <<< "$entry"
  log "  $host — $container"
  remote "$host" bash -s "$container" "$compose_dir" "$DOCKER_IMAGE" <<'REMOTE'
set -euo pipefail
CONTAINER="$1"
COMPOSE_DIR="$2"
IMG="$3"
CFG="$COMPOSE_DIR/config/xrpld.cfg"

if [[ -f "$CFG" ]] && ! grep -q '^SingleAssetVault$' "$CFG" 2>/dev/null; then
  awk '/^\[features\]/{print; print "SingleAssetVault"; print "LendingProtocol"; next} {print}' "$CFG" > "$CFG.tmp"
  mv "$CFG.tmp" "$CFG"
fi

if [[ -f "$COMPOSE_DIR/docker-compose.yml" ]]; then
  sed -i "s|image: qxrp/xrpld:.*|image: ${IMG}|" "$COMPOSE_DIR/docker-compose.yml"
  cd "$COMPOSE_DIR" && docker compose up -d --force-recreate
else
  echo "missing compose at $COMPOSE_DIR" >&2
  exit 1
fi
REMOTE
done

log "Phase 3: wait for coordinator RPC"
sleep 20
for i in $(seq 1 30); do
  state=$(curl -sf --max-time 5 -X POST "http://${BUILD_HOST}:6005" \
    -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['info']['server_state'])" 2>/dev/null || echo "")
  [[ -n "$state" && "$state" != "disconnected" ]] && { log "  server_state=$state"; break; }
  [[ $i -eq 30 ]] && die "coordinator not healthy"
  sleep 5
done

log "Done. Fleet running ${DOCKER_IMAGE} (ledger data preserved)."