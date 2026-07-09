#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Enable SingleAssetVault + LendingProtocol on all bonded validators.
# Same pattern as enable-amm-fleet.sh: patch [amendments] in cfg, restart, vote.
#
# Usage:
#   bash scripts/enable-lending-fleet.sh
#   bash scripts/enable-lending-fleet.sh --wait

set -euo pipefail

WAIT=false
PATCH_CFG=true
for arg in "$@"; do
  [[ "$arg" == "--wait" ]] && WAIT=true
  [[ "$arg" == "--no-patch" ]] && PATCH_CFG=false
done

PUBLIC_RPC="${PUBLIC_RPC_URL:-http://46.224.0.140:6005}"
# SingleAssetVault must activate before LendingProtocol.
FEATURES=(SingleAssetVault LendingProtocol)

NODES=(
  "46.224.0.140|${HOME}/.ssh/id_ed25519|root|qxrp-val2|/var/lib/qxrp-val2"
  "167.233.55.43|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "204.168.175.194|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "89.167.109.241|${HOME}/.ssh/id_ed25519|qxrp|qxrp-validator|/var/lib/qxrp-validator"
  "5.78.142.246|${HOME}/.ssh/id_val5|root|qxrp-validator|/var/lib/qxrp-validator"
  "192.241.247.158|${HOME}/.ssh/id_digitalocean|root|qxrp-validator|/var/lib/qxrp-validator"
)

patch_validator_cfg() {
  local host="$1" key="$2" user="$3" container="$4" data_dir="$5"
  ssh -o StrictHostKeyChecking=no -i "$key" "${user}@${host}" bash -s <<PATCH
set -euo pipefail
CFG="${data_dir}/config/xrpld.cfg"
[[ -f "\$CFG" ]] || { echo "  missing \$CFG"; exit 1; }
SUDO=""
[[ "\$(id -u)" -ne 0 ]] && command -v sudo >/dev/null && SUDO="sudo"
\$SUDO cp "\$CFG" "\$CFG.bak-lending"
\$SUDO python3 - <<'PY'
from pathlib import Path
cfg = Path("${data_dir}/config/xrpld.cfg")
text = cfg.read_text()
lines = [
    "81BD2619B6B3C8625AC5D0BC01DE17F06C3F0AB95C7C87C93715B87A4FD240D8 SingleAssetVault",
    "565B90CA1AB2B9D42208ED10884188C64F9E19083DECB9634AAF06EB03299509 LendingProtocol",
]
hashes = [l.split()[0] for l in lines]
if "[amendments]" not in text:
    block = """
[amendment_majority_time]
15 minutes

[amendments]
2E2FB9CF8A44EB80F4694D38AADAE9B8B7ADAFD2F092E10068E61C98C4F092B0 fixUniversalNumber
8CC0774A3BF66D1D22E76BBDA8E8A232E6B6313834301B3B23E8601196AE6455 AMM
81BD2619B6B3C8625AC5D0BC01DE17F06C3F0AB95C7C87C93715B87A4FD240D8 SingleAssetVault
565B90CA1AB2B9D42208ED10884188C64F9E19083DECB9634AAF06EB03299509 LendingProtocol
"""
    cfg.write_text(text.rstrip() + block + "\n")
    print("  appended full [amendments] block incl. lending")
else:
    changed = False
    for line in lines:
        h = line.split()[0]
        if h not in text:
            text = text.rstrip() + "\n" + line + "\n"
            changed = True
    if "[amendment_majority_time]" not in text:
        text = text.replace("[amendments]", "[amendment_majority_time]\n15 minutes\n\n[amendments]", 1)
        changed = True
    if changed:
        cfg.write_text(text)
        print("  appended lending amendments")
    else:
        print("  lending amendments already present")
PY
cd "${data_dir}" && (\$SUDO docker compose restart "${container}" 2>/dev/null \
  || \$SUDO docker restart "${container}")
PATCH
}

accept_feature() {
  local host="$1" key="$2" user="$3" container="$4" feature="$5"
  local payload
  payload=$(printf '{"method":"feature","params":[{"feature":"%s","vetoed":false}]}' "$feature")
  ssh -o StrictHostKeyChecking=no -i "$key" "${user}@${host}" \
    "docker exec ${container} curl -sf -X POST http://127.0.0.1:5005 \
      -H 'Content-Type: application/json' -d '${payload}'" \
    | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result', {})
print('  ', r.get('status', 'error'), end='')
for k,v in r.items():
    if isinstance(v, dict) and v.get('name') == '${feature}':
        print(f\" vetoed={v.get('vetoed')} count={v.get('count')}/{v.get('threshold')}\")
        break
else:
    print()
" 2>/dev/null || echo "  FAILED ${host} ${feature}"
}

check_enabled() {
  local feature="$1"
  curl -sf -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
    -d "{\"method\":\"feature\",\"params\":[{\"feature\":\"${feature}\"}]}" \
    | python3 -c "
import sys, json
r = json.load(sys.stdin)['result']
for v in r.values():
    if isinstance(v, dict) and v.get('name')=='${feature}':
        print('enabled=', v.get('enabled'), 'count=', v.get('count'), 'threshold=', v.get('threshold'))
        raise SystemExit(0 if v.get('enabled') else 1)
raise SystemExit(1)
"
}

if $PATCH_CFG; then
  echo "==> Patching validator configs ([amendments] + lending) and restarting"
  for entry in "${NODES[@]}"; do
    IFS='|' read -r host key user container data_dir <<< "$entry"
    echo -n "${host}: "
    patch_validator_cfg "$host" "$key" "$user" "$container" "$data_dir" || echo "  PATCH FAILED"
  done
  echo "Waiting 45s for validators to rejoin..."
  sleep 45
fi

echo "==> Accepting lending amendments on ${#NODES[@]} validators"
for feature in "${FEATURES[@]}"; do
  echo "--- ${feature} ---"
  for entry in "${NODES[@]}"; do
    IFS='|' read -r host key user container data_dir <<< "$entry"
    echo -n "${host} (${container}): "
    accept_feature "$host" "$key" "$user" "$container" "$feature" || true
  done
done

echo ""
echo "==> Current lending status (public RPC)"
for feature in "${FEATURES[@]}"; do
  echo -n "${feature}: "
  if check_enabled "$feature" 2>/dev/null; then
    echo "ENABLED"
  else
    echo "pending"
  fi
done

if ! $WAIT; then
  echo "Re-run with --wait to poll until both are enabled (~15 min majority time)"
  exit 0
fi

echo "Waiting for lending activation (up to 30 min)..."
for i in $(seq 1 60); do
  sleep 30
  ok=true
  for feature in "${FEATURES[@]}"; do
    check_enabled "$feature" 2>/dev/null || ok=false
  done
  if $ok; then
    echo "Lending amendments ENABLED after ~$((i * 30))s"
    exit 0
  fi
  echo "  ... still pending ($((i * 30))s)"
done
echo "Timeout — check validator logs"
exit 1