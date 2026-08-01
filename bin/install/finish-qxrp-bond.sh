#!/usr/bin/env bash
# Finish ValidatorRegister + ValidatorBond after install when fund-wait
# could not see the balance (home networks blocking public RPC :6005).
#
# Usage (on the validator host):
#   curl -fsSL …/finish-qxrp-bond.sh | bash -s -- --node-name my-falcon-node
#
set -euo pipefail

NODE_NAME="my-falcon-node"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --node-name) NODE_NAME="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [--node-name NAME]"; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

ROOT="${HOME}/.qxrp/${NODE_NAME}"
KEYS="${ROOT}/config/validator-keys.json"
SERVICE="qxrp-${NODE_NAME}"
MIN_FUND=1100000000
MIN_BOND=1000000000

log()  { echo -e "\033[1;32m[qxrp]\033[0m $*"; }
warn() { echo -e "\033[1;33m[qxrp] WARN:\033[0m $*"; }
die()  { echo -e "\033[1;31m[qxrp] ERROR:\033[0m $*" >&2; exit 1; }

[[ -f "$KEYS" ]] || die "No keys at $KEYS — run the site install one-liner first"

# Docker without group membership (fresh install shells)
if ! docker info &>/dev/null 2>&1; then
  if sudo docker info &>/dev/null 2>&1; then
    docker() { command sudo docker "$@"; }
    log "Using sudo docker (run once later: newgrp docker)"
  else
    die "Cannot access Docker. Try: sudo usermod -aG docker \$USER && newgrp docker"
  fi
fi

docker ps --format '{{.Names}}' | grep -qx "$SERVICE" \
  || die "Container $SERVICE not running. Start: cd $ROOT && docker compose up -d"

ACCT=$(python3 -c "import json; print(json.load(open('$KEYS'))['account_address'])")
SECRET=$(python3 -c "import json; print(json.load(open('$KEYS'))['falcon_secret'])")
PK=$(python3 -c "import json; print(json.load(open('$KEYS')).get('public_key_hex') or json.load(open('$KEYS')).get('public_key',''))")
CK=$(python3 -c "import json; print(json.load(open('$KEYS'))['consensus_key_hex'])")

rpc() {
  local method="$1" params="${2:-{}}"
  docker exec "$SERVICE" curl -sf --max-time 12 -X POST "http://127.0.0.1:5005" \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"${method}\",\"params\":[${params}]}"
}

log "Node: $SERVICE  Account: $ACCT"
log "Waiting for local ledger + funding (this can take a while on first sync)..."

FUNDED=0
for i in $(seq 1 240); do
  INFO=$(rpc server_info 2>/dev/null || echo '{}')
  STATE=$(echo "$INFO" | python3 -c "import sys,json; print((json.load(sys.stdin).get('result') or {}).get('info',{}).get('server_state',''))" 2>/dev/null || true)
  PEERS=$(echo "$INFO" | python3 -c "import sys,json; print((json.load(sys.stdin).get('result') or {}).get('info',{}).get('peers',0))" 2>/dev/null || echo 0)
  SEQ=$(echo "$INFO" | python3 -c "
import sys,json
i=(json.load(sys.stdin).get('result') or {}).get('info',{})
v=i.get('validated_ledger') or {}
print(v.get('seq') or 0)
" 2>/dev/null || echo 0)

  BAL=$(rpc account_info "{\"account\":\"${ACCT}\",\"ledger_index\":\"validated\"}" 2>/dev/null \
    | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('result',{}).get('account_data',{}).get('Balance','0'))" 2>/dev/null || echo "0")

  if [[ "${BAL:-0}" -ge "$MIN_FUND" ]]; then
    FUNDED=1
    log "Funded: $(python3 -c "print(int('${BAL}')/1e6)") FALCON (local sees it)"
    break
  fi

  if [[ $((i % 6)) -eq 0 ]]; then
    log "  … bal=${BAL} drops  state=${STATE} peers=${PEERS} seq=${SEQ} (need ≥1100 FALCON on local ledger)"
  fi
  sleep 10
done

[[ "$FUNDED" -eq 1 ]] || die "Local node still cannot see funding after long wait.
Your payment may already be on-chain (check explorer). Leave the node syncing:
  docker logs -f $SERVICE
Then re-run this script."

sign_submit() {
  local tx_json="$1" label="$2"
  local sign res blob
  sign=$(rpc sign "{\"tx_json\":${tx_json},\"falcon_secret\":\"${SECRET}\"}") \
    || die "sign failed for $label"
  res=$(echo "$sign" | python3 -c "import sys,json; r=json.load(sys.stdin).get('result',{}); print(r.get('engine_result') or r.get('error') or 'error')")
  log "  $label sign: $res"
  if [[ "$res" != "tesSUCCESS" && "$res" != "terQUEUED" && "$res" != "tecDUPLICATE" && "$res" != "tecNO_PERMISSION" ]]; then
    # still try submit if we got a blob
    :
  fi
  blob=$(echo "$sign" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('tx_blob',''))" 2>/dev/null || true)
  [[ -n "$blob" ]] || die "No tx_blob for $label (result=$res)"
  sub=$(rpc submit "{\"tx_blob\":\"${blob}\"}") || die "submit failed for $label"
  echo "$sub" | python3 -c "import sys,json; r=json.load(sys.stdin).get('result',{}); print('  submit:', r.get('engine_result') or r.get('error') or r)"
}

log "ValidatorRegister..."
sign_submit "{\"TransactionType\":\"ValidatorRegister\",\"Account\":\"${ACCT}\",\"PublicKey\":\"${PK}\",\"ConsensusKey\":\"${CK}\",\"Fee\":\"12\"}" "ValidatorRegister"
sleep 5

log "ValidatorBond (1000 FALCON)..."
sign_submit "{\"TransactionType\":\"ValidatorBond\",\"Account\":\"${ACCT}\",\"ConsensusKey\":\"${CK}\",\"BondedAmount\":\"${MIN_BOND}\",\"Fee\":\"12\"}" "ValidatorBond"

log "Done. Check:"
log "  docker logs -f $SERVICE"
log "  # bond status via account objects / rewards page on portal"
