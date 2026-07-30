#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# BitcoinSPVBridge — AMENDMENT ONLY (never [features] force-enable).
#
# Correct sequence for public testnet network_id 1001:
#   1) Deploy xrpld built from feature/btc-spv-light-client to ALL validators
#   2) Confirm feature is supported=true (still enabled=false)
#   3) This script: append hash to [amendments] + un-veto vote + majority wait
#   4) Wait amendment_majority_time (default 15 minutes) with supermajority votes
#
# NEVER add BitcoinSPVBridge under [features] on 1001 — that force-enables rules
# and can diverge the network.
#
# Usage:
#   bash scripts/enable-btc-spv-fleet.sh                 # status + dry-run
#   bash scripts/enable-btc-spv-fleet.sh --execute --wait # real vote (after binary deploy)
#
# Single validator (other ops), after new binary is running:
#   See docs/btc-spv/TESTNET_AMENDMENT_ROLLOUT.md § single-val commands

set -euo pipefail

EXECUTE=false
WAIT=false
PUBLIC_RPC="${PUBLIC_RPC_URL:-http://46.224.0.140:6005}"
FEATURE="BitcoinSPVBridge"
# Known hash from this branch's binary (feature name registration)
AMEND_HASH="${BTC_SPV_AMEND_HASH:-76DAF975D0E23358239AED3C74A8600600A0FBEBB7E12B1FEA314C41469F3D33}"
MAJORITY_TIME="${AMENDMENT_MAJORITY_TIME:-15 minutes}"

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
    -h|--help) sed -n '1,30p' "$0"; exit 0 ;;
  esac
done

echo "=== BitcoinSPVBridge AMENDMENT enable (no [features] force) ==="
echo "PUBLIC_RPC=$PUBLIC_RPC"
echo "AMEND_HASH=$AMEND_HASH"
echo "MAJORITY_TIME=$MAJORITY_TIME"
echo "EXECUTE=$EXECUTE WAIT=$WAIT"
echo

feature_status() {
  curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
    -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${FEATURE}\"}]}" \
    | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result') or {}
feats = r.get('features') if isinstance(r.get('features'), dict) else r
found = None
for k, v in (feats or {}).items():
    if not isinstance(v, dict):
        continue
    if v.get('name') == '${FEATURE}':
        found = (k, v)
        break
if not found:
    print('MISSING')
    sys.exit(2)
k, v = found
print(k)
print(f\"supported={v.get('supported')} enabled={v.get('enabled')} vetoed={v.get('vetoed')} count={v.get('count')} threshold={v.get('threshold')} majority={v.get('majority')}\")
"
}

echo "-- Public RPC status --"
if STATUS=$(feature_status 2>/dev/null); then
  HASH_LINE=$(echo "$STATUS" | head -1)
  echo "$STATUS" | tail -n +1
  if [[ "$HASH_LINE" != "MISSING" && ${#HASH_LINE} -eq 64 ]]; then
    AMEND_HASH="$HASH_LINE"
    echo "Using live hash: $AMEND_HASH"
  fi
else
  code=$?
  if [[ $code -eq 2 ]] || echo "$STATUS" | grep -q MISSING; then
    echo "BitcoinSPVBridge is NOT in the feature table on public testnet."
    echo "That means validators are still on an OLD binary without this amendment."
    echo
    echo "REQUIRED FIRST (all validators):"
    echo "  1. Build/pull image from feature/btc-spv-light-client (or merged main with SPV)"
    echo "  2. Restart every bonded validator on that binary"
    echo "  3. Re-run this script — feature must show supported=true, enabled=false"
    echo
    echo "Do NOT add BitcoinSPVBridge under [features] — that force-enables and can fork the net."
    if $EXECUTE; then
      echo "ERROR: refusing --execute until the amendment exists on the network binary."
      exit 1
    fi
  else
    echo "WARN: could not query public RPC"
  fi
fi
echo

AMEND_LINE="${AMEND_HASH} ${FEATURE}"

echo "-- Mode: amendment vote only --"
echo "  [amendments] line: $AMEND_LINE"
echo "  [amendment_majority_time]: $MAJORITY_TIME"
echo "  RPC un-veto: feature BitcoinSPVBridge vetoed=false"
echo "  NEVER touches [features]"
echo

if ! $EXECUTE; then
  echo "DRY-RUN. No SSH, no config patch, no votes."
  echo
  echo "When ALL vals run the new binary and you are ready to start the majority clock:"
  echo "  bash scripts/enable-btc-spv-fleet.sh --execute --wait"
  echo
  echo "Single-validator commands for other operators (after binary upgrade):"
  cat <<'SINGLE'

  # On each validator host (adjust paths/container):
  # 1) Ensure binary includes BitcoinSPVBridge (feature RPC shows the name)
  # 2) Append amendment (vote yes) — NOT [features]:
  HASH=76DAF975D0E23358239AED3C74A8600600A0FBEBB7E12B1FEA314C41469F3D33
  CFG=/var/lib/qxrp-validator/config/xrpld.cfg   # or /var/lib/qxrp-val2/config/xrpld.cfg
  grep -q "$HASH" "$CFG" || {
    grep -q '\[amendment_majority_time\]' "$CFG" || printf '\n[amendment_majority_time]\n15 minutes\n' >> "$CFG"
    grep -q '\[amendments\]' "$CFG" || printf '\n[amendments]\n' >> "$CFG"
    echo "$HASH BitcoinSPVBridge" >> "$CFG"
  }
  # 3) Restart validator container, then un-veto:
  docker restart qxrp-validator   # or qxrp-val2
  docker exec qxrp-validator curl -sf -X POST http://127.0.0.1:5005 \
    -H 'Content-Type: application/json' \
    -d '{"method":"feature","params":[{"feature":"BitcoinSPVBridge","vetoed":false}]}'

  # 4) Wait majority_time (15m) with enough validators voting yes, then:
  curl -s -X POST http://46.224.0.140:6005 -H 'Content-Type: application/json' \
    -d '{"method":"feature","params":[{"feature":"BitcoinSPVBridge"}]}' | jq .

SINGLE
  exit 0
fi

echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo "EXECUTE: will patch [amendments] + un-veto on fleet (NOT [features])"
echo "Ctrl+C within 8s to abort..."
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
sleep 8

patch_validator_cfg() {
  local host="$1" key="$2" user="$3" container="$4" data_dir="$5"
  local line="$6" maj="$7"
  ssh -o StrictHostKeyChecking=no -i "$key" "${user}@${host}" bash -s <<PATCH
set -euo pipefail
CFG="${data_dir}/config/xrpld.cfg"
[[ -f "\$CFG" ]] || { echo "  missing \$CFG"; exit 1; }
SUDO=""
[[ "\$(id -u)" -ne 0 ]] && command -v sudo >/dev/null && SUDO="sudo"
\$SUDO cp "\$CFG" "\$CFG.bak-btc-spv-\$(date +%Y%m%d%H%M%S)"
LINE='${line}'
MAJ='${maj}'
\$SUDO python3 - <<'PY'
from pathlib import Path
cfg = Path("${data_dir}/config/xrpld.cfg")
text = cfg.read_text()
line = """${line}"""
maj = """${maj}"""
h = line.split()[0]
# SAFETY: strip if someone wrongly put it under [features]
if "[features]" in text:
    parts = text.split("[features]")
    head, rest = parts[0], parts[1]
    # rest until next section
    if "\n[" in rest:
        feat_body, after = rest.split("\n[", 1)
        after = "[" + after
    else:
        feat_body, after = rest, ""
    lines_f = [ln for ln in feat_body.splitlines() if "BitcoinSPVBridge" not in ln and h not in ln]
    if len(lines_f) != len(feat_body.splitlines()):
        print("  REMOVED BitcoinSPVBridge from [features] (must not force-enable)")
        text = head + "[features]" + "\n".join(lines_f) + ("\n" if lines_f else "") + (after if after.startswith("[") else after)
if h in text and "[amendments]" in text:
    # already listed somewhere — ensure under amendments only
    print("  amendment hash already present in cfg")
else:
    if "[amendments]" not in text:
        text = text.rstrip() + f"\n\n[amendment_majority_time]\n{maj}\n\n[amendments]\n{line}\n"
        print("  created [amendments] + majority time + BitcoinSPVBridge")
    else:
        if "[amendment_majority_time]" not in text:
            text = text.replace("[amendments]", f"[amendment_majority_time]\n{maj}\n\n[amendments]", 1)
            print("  added [amendment_majority_time]")
        text = text.rstrip() + "\n" + line + "\n"
        print("  appended BitcoinSPVBridge to [amendments]")
    cfg.write_text(text)
# final write if we only stripped features
cfg.write_text(text)
PY
cd "${data_dir}" && (\$SUDO docker compose restart "${container}" 2>/dev/null \
  || \$SUDO docker restart "${container}")
echo "  restarted ${container}"
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
print('  vote', r.get('status', 'error'), end='')
for k,v in r.items():
    if isinstance(v, dict) and v.get('name') == '${FEATURE}':
        print(f\" enabled={v.get('enabled')} vetoed={v.get('vetoed')} count={v.get('count')}/{v.get('threshold')}\")
        break
else:
    print()
" 2>/dev/null || echo "  FAILED vote on ${host}"
}

for entry in "${NODES[@]}"; do
  IFS='|' read -r host key user container data_dir <<<"$entry"
  echo "Node $host ($container)"
  if [[ ! -f "$key" ]]; then
    echo "  SKIP missing key $key"
    continue
  fi
  patch_validator_cfg "$host" "$key" "$user" "$container" "$data_dir" "$AMEND_LINE" "$MAJORITY_TIME" || echo "  patch failed"
  sleep 5
  accept_feature "$host" "$key" "$user" "$container" || true
done

if $WAIT; then
  echo
  echo "-- Waiting for amendment majority (enabled=true) --"
  echo "   majority_time is $MAJORITY_TIME after enough validators vote yes"
  for i in $(seq 1 80); do
    if out=$(feature_status 2>/dev/null); then
      echo "[$(date -u +%H:%M:%S)] $out" | tr '\n' ' '
      echo
      if echo "$out" | grep -q 'enabled=True\|enabled=true'; then
        echo "SUCCESS: BitcoinSPVBridge ENABLED on public testnet via amendment"
        exit 0
      fi
    fi
    sleep 15
  done
  echo "TIMEOUT — check votes: feature count/threshold; ensure all vals run new binary"
  exit 1
fi

echo "Done. Monitor with:"
echo "  curl -s -X POST $PUBLIC_RPC -H 'Content-Type: application/json' \\"
echo "    -d '{\"method\":\"feature\",\"params\":[{\"feature\":\"BitcoinSPVBridge\"}]}'"
