#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# BitcoinSPVBridge — fleet prepare / optional execute for Falcon testnet.
#
# DEFAULT: dry-run only. Does NOT patch configs, restart nodes, or cast votes.
# Activation requires explicit --execute after product/security approval.
#
# Usage:
#   bash scripts/enable-btc-spv-fleet.sh              # prepare / dry-run
#   bash scripts/enable-btc-spv-fleet.sh --check-rpc  # query public RPC for feature
#   bash scripts/enable-btc-spv-fleet.sh --execute --wait   # REAL enable (dangerous)
#
# Never force-enable via [features] on network_id 1001.

set -euo pipefail

EXECUTE=false
WAIT=false
CHECK_RPC=true
PUBLIC_RPC="${PUBLIC_RPC_URL:-http://46.224.0.140:6005}"
FEATURE="BitcoinSPVBridge"

# Same node list pattern as enable-lending-fleet.sh (override via env if needed)
NODES=(
  "46.224.0.140|${HOME}/.ssh/id_ed25519|root|qxrp-val2|/var/lib/qxrp-val2"
  "167.233.55.43|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "204.168.175.194|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "89.167.109.241|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "5.78.142.246|${HOME}/.ssh/id_val5|root|qxrp-validator|/var/lib/qxrp-validator"
  "192.241.247.158|${HOME}/.ssh/id_digitalocean|root|qxrp-validator|/var/lib/qxrp-validator"
)

for arg in "$@"; do
  case "$arg" in
    --execute) EXECUTE=true ;;
    --wait) WAIT=true ;;
    --check-rpc) CHECK_RPC=true ;;
    --no-check-rpc) CHECK_RPC=false ;;
    -h|--help)
      sed -n '1,25p' "$0"
      exit 0
      ;;
  esac
done

echo "=== BitcoinSPVBridge fleet helper ==="
echo "PUBLIC_RPC=$PUBLIC_RPC"
echo "EXECUTE=$EXECUTE  WAIT=$WAIT"
echo

if $CHECK_RPC; then
  echo "-- Public RPC feature probe --"
  if ! curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
      -d '{"method":"server_info","params":[{}]}' >/dev/null; then
    echo "WARN: public RPC unreachable (expected if offline). Continue."
  else
    curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
      -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${FEATURE}\"}]}" \
      | python3 -c "
import sys, json
try:
    r = json.load(sys.stdin).get('result') or {}
except Exception as e:
    print('  parse error', e); sys.exit(0)
# shape: { hash: {name, enabled, supported, vetoed}, status }
found = False
for k, v in r.items():
    if not isinstance(v, dict):
        continue
    if v.get('name') == '${FEATURE}' or k == '${FEATURE}':
        found = True
        print(f\"  name={v.get('name')} supported={v.get('supported')} enabled={v.get('enabled')} vetoed={v.get('vetoed')} count={v.get('count')} threshold={v.get('threshold')}\")
if not found:
    print('  BitcoinSPVBridge not in feature table yet — fleet binary may predate this branch.')
    print('  Next: deploy xrpld built from feature/btc-spv-light-client (amendment still OFF).')
" || echo "  feature query failed"
  fi
  echo
fi

echo "-- Prepare summary (no network mutation) --"
echo "  1. Build: cmake --build .build -j1 --target xrpld  (or fleet Release image)"
echo "  2. Deploy binary to ALL bonded validators (same generation)"
echo "  3. Confirm feature supported=true, enabled=false on each node"
echo "  4. Do NOT add BitcoinSPVBridge to [features] on 1001"
echo "  5. When approved: re-run with --execute --wait"
echo "  Docs: docs/btc-spv/TESTNET_AMENDMENT_ROLLOUT.md"
echo

if ! $EXECUTE; then
  echo "DRY-RUN complete. No configs patched, no votes cast."
  echo "To activate later (after approval): $0 --execute --wait"
  exit 0
fi

echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo "EXECUTE mode: will un-veto / vote BitcoinSPVBridge on fleet"
echo "Ctrl+C within 5s to abort..."
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
sleep 5

# Resolve amendment hash from a live node if possible
AMEND_LINE=""
if HASH_JSON=$(curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
    -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${FEATURE}\"}]}"); then
  AMEND_LINE=$(echo "$HASH_JSON" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result') or {}
for k, v in r.items():
    if isinstance(v, dict) and v.get('name') == '${FEATURE}':
        print(k + ' ${FEATURE}')
        break
")
fi
if [[ -z "$AMEND_LINE" ]]; then
  echo "ERROR: cannot resolve amendment hash from RPC. Deploy binary first."
  exit 1
fi
echo "Using: $AMEND_LINE"

patch_validator_cfg() {
  local host="$1" key="$2" user="$3" container="$4" data_dir="$5" line="$6"
  ssh -o StrictHostKeyChecking=no -i "$key" "${user}@${host}" bash -s <<PATCH
set -euo pipefail
CFG="${data_dir}/config/xrpld.cfg"
[[ -f "\$CFG" ]] || { echo "  missing \$CFG"; exit 1; }
SUDO=""
[[ "\$(id -u)" -ne 0 ]] && command -v sudo >/dev/null && SUDO="sudo"
\$SUDO cp "\$CFG" "\$CFG.bak-btc-spv"
LINE='${line}'
\$SUDO python3 - <<'PY'
from pathlib import Path
cfg = Path("${data_dir}/config/xrpld.cfg")
text = cfg.read_text()
line = """${line}"""
h = line.split()[0]
if h in text:
    print("  amendment already listed")
else:
    if "[amendments]" not in text:
        text = text.rstrip() + "\n\n[amendment_majority_time]\n15 minutes\n\n[amendments]\n" + line + "\n"
    else:
        text = text.rstrip() + "\n" + line + "\n"
    if "[amendment_majority_time]" not in text:
        text = text.replace("[amendments]", "[amendment_majority_time]\n15 minutes\n\n[amendments]", 1)
    cfg.write_text(text)
    print("  appended BitcoinSPVBridge to [amendments]")
# Safety: never inject into [features]
if "BitcoinSPVBridge" in text.split("[features]")[-1].split("[")[0] if "[features]" in text else "":
    # only warn if features section force-lists it
    feat = text.split("[features]")[1].split("[")[0] if "[features]" in text else ""
    if "BitcoinSPVBridge" in feat:
        print("  WARNING: BitcoinSPVBridge appears under [features] — remove for 1001!")
PY
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
" 2>/dev/null || echo "  FAILED ${host}"
}

echo "-- Patch + restart + vote --"
for entry in "${NODES[@]}"; do
  IFS='|' read -r host key user container data_dir <<<"$entry"
  echo "Node $host"
  patch_validator_cfg "$host" "$key" "$user" "$container" "$data_dir" "$AMEND_LINE" || echo "  patch failed"
  sleep 3
  accept_feature "$host" "$key" "$user" "$container" || true
done

if $WAIT; then
  echo "-- Waiting for enabled=true --"
  for i in $(seq 1 120); do
    if curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
        -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${FEATURE}\"}]}" \
        | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result') or {}
for v in r.values():
    if isinstance(v, dict) and v.get('name')=='${FEATURE}':
        print('enabled=', v.get('enabled'), 'count=', v.get('count'))
        raise SystemExit(0 if v.get('enabled') else 1)
raise SystemExit(1)
"; then
      echo "BitcoinSPVBridge ENABLED on public net"
      exit 0
    fi
    sleep 15
  done
  echo "TIMEOUT waiting for enable"
  exit 1
fi

echo "Done (execute without --wait). Check feature RPC manually."
