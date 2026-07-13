#!/usr/bin/env bash
# Enable LendingCollateral amendment on bonded validators (after new xrpld binary).
# Votes by feature name; patches [amendments] once hash is visible on RPC.
#
# Usage:
#   bash scripts/enable-lending-collateral-fleet.sh
#   bash scripts/enable-lending-collateral-fleet.sh --wait

set -euo pipefail

WAIT=false
PATCH_CFG=true
for arg in "$@"; do
  [[ "$arg" == "--wait" ]] && WAIT=true
  [[ "$arg" == "--no-patch" ]] && PATCH_CFG=false
done

PUBLIC_RPC="${PUBLIC_RPC_URL:-http://46.224.0.140:6005}"
FEATURE="LendingCollateral"

NODES=(
  "46.224.0.140|${HOME}/.ssh/id_ed25519|root|qxrp-val2|/var/lib/qxrp-val2"
  "167.233.55.43|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "204.168.175.194|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "89.167.109.241|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "5.78.142.246|${HOME}/.ssh/id_val5|root|qxrp-validator|/var/lib/qxrp-validator"
  "192.241.247.158|${HOME}/.ssh/id_digitalocean|root|qxrp-validator|/var/lib/qxrp-validator"
)

fetch_hash() {
  curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
    -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${FEATURE}\"}]}" \
    | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result', {})
for k, v in r.items():
    if k == 'status' or not isinstance(v, dict):
        continue
    if v.get('name') == '${FEATURE}':
        print(v.get('id') or k)
        raise SystemExit(0)
raise SystemExit(1)
"
}

patch_validator_cfg() {
  local host="$1" key="$2" user="$3" container="$4" data_dir="$5" hash="$6"
  ssh -o StrictHostKeyChecking=no -i "$key" "${user}@${host}" bash -s <<PATCH
set -euo pipefail
CFG="${data_dir}/config/xrpld.cfg"
[[ -f "\$CFG" ]] || { echo "  missing \$CFG"; exit 1; }
SUDO=""
[[ "\$(id -u)" -ne 0 ]] && command -v sudo >/dev/null && SUDO="sudo"
LINE="${hash} ${FEATURE}"
if grep -q "${hash}" "\$CFG" 2>/dev/null; then
  echo "  already patched"
else
  \$SUDO cp "\$CFG" "\$CFG.bak-lending-collateral"
  if grep -q '^\[amendments\]' "\$CFG"; then
    echo "\$LINE" | \$SUDO tee -a "\$CFG" >/dev/null
  else
    printf '\n[amendment_majority_time]\n15 minutes\n\n[amendments]\n%s\n' "\$LINE" | \$SUDO tee -a "\$CFG" >/dev/null
  fi
  echo "  appended \$LINE"
fi
cd "${data_dir}" && (\$SUDO docker compose restart "${container}" 2>/dev/null \
  || \$SUDO docker restart "${container}")
PATCH
}

accept_feature() {
  local host="$1" key="$2" user="$3" container="$4"
  local payload
  payload=$(printf '{"method":"feature","params":[{"feature":"%s","vetoed":false}]}' "$FEATURE")
  ssh -o StrictHostKeyChecking=no -i "$key" "${user}@${host}" \
    "docker exec ${container} curl -sf -X POST http://127.0.0.1:5005 \
      -H 'Content-Type: application/json' -d '${payload}'" \
    | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result', {})
print('  ', r.get('status', 'error'), end='')
for k,v in r.items():
    if isinstance(v, dict) and v.get('name') == '${FEATURE}':
        print(f\" vetoed={v.get('vetoed')} count={v.get('count')}/{v.get('threshold')}\")
        break
else:
    print()
" 2>/dev/null || echo "  FAILED ${host} ${FEATURE}"
}

check_enabled() {
  curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
    -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${FEATURE}\"}]}" \
    | python3 -c "
import sys, json
r = json.load(sys.stdin)['result']
for v in r.values():
    if isinstance(v, dict) and v.get('name')=='${FEATURE}':
        print('enabled=', v.get('enabled'), 'count=', v.get('count'), 'threshold=', v.get('threshold'))
        raise SystemExit(0 if v.get('enabled') else 1)
raise SystemExit(1)
"
}

echo "==> Resolve ${FEATURE} amendment hash from ${PUBLIC_RPC}"
HASH=""
for i in $(seq 1 12); do
  if HASH=$(fetch_hash 2>/dev/null) && [[ -n "$HASH" ]]; then
    echo "    id=${HASH}"
    break
  fi
  echo "    waiting for binary with ${FEATURE} ($i/12)..."
  sleep 10
done
[[ -n "$HASH" ]] || { echo "ERROR: ${FEATURE} not found on RPC — deploy new xrpld first"; exit 1; }

if $PATCH_CFG; then
  echo "==> Patching validator configs and restarting"
  for entry in "${NODES[@]}"; do
    IFS='|' read -r host key user container data_dir <<< "$entry"
    echo -n "${host}: "
    patch_validator_cfg "$host" "$key" "$user" "$container" "$data_dir" "$HASH" || echo "  PATCH FAILED"
  done
  echo "Waiting 45s for validators to rejoin..."
  sleep 45
fi

echo "==> Accepting ${FEATURE} on ${#NODES[@]} validators"
for entry in "${NODES[@]}"; do
  IFS='|' read -r host key user container data_dir <<< "$entry"
  echo -n "${host} (${container}): "
  accept_feature "$host" "$key" "$user" "$container" || true
done

echo ""
echo "==> Status (public RPC)"
echo -n "${FEATURE}: "
if check_enabled 2>/dev/null; then
  echo "ENABLED"
else
  echo "pending (~15 min majority after fleet agrees)"
fi

if ! $WAIT; then
  echo "Re-run with --wait to poll until enabled"
  exit 0
fi

for i in $(seq 1 60); do
  sleep 30
  if check_enabled 2>/dev/null; then
    echo "ENABLED after ~$((i * 30))s"
    exit 0
  fi
  echo "  ... still pending ($((i * 30))s)"
done
echo "Timeout — check validator logs"
exit 1