#!/bin/bash
set -e

PAYOUT=""
NODE_NAME=""
SECRET_INPUT=""

# Parse arguments (supports --payout, --node-name from the web UI, and optional positional secret)
while [[ $# -gt 0 ]]; do
  case $1 in
    --payout)
      PAYOUT="$2"
      shift 2
      ;;
    --node-name)
      NODE_NAME="$2"
      shift 2
      ;;
    *)
      if [ -z "$SECRET_INPUT" ]; then
        SECRET_INPUT="$1"
      fi
      shift
      ;;
  esac
done

SECRET_INPUT="${SECRET_INPUT:-qxrp-val-fresh-$(date +%s)}"
# Pin fleet image — floating :latest may lack Falcon hex UNL support (see docs/fleet-image-pinning.md).
DOCKER_IMAGE="${QXRP_XRPLD_IMAGE:-qxrp/xrpld:cid-popl}"
PUBLIC_RPC="${QXRP_PUBLIC_RPC:-http://46.224.0.140:6005}"
FLEET_UNL_URL="${QXRP_FLEET_UNL_URL:-https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/testnet-falcon-unl.txt}"

write_validators_txt() {
  local own_key="${1:-}"
  local unl_file="$2"
  {
    echo "[validators]"
    if [[ -n "$own_key" ]]; then
      echo "$own_key"
    fi
    while IFS= read -r k || [[ -n "$k" ]]; do
      k="${k//[[:space:]]/}"
      [[ -z "$k" || "$k" == "$own_key" ]] && continue
      echo "$k"
    done < "$unl_file"
  } > /var/lib/qxrp-validator/config/validators.txt
}

echo "=== qXRP Validator Bootstrap (fresh server) ==="
echo "Using secret (wallet input): $SECRET_INPUT"
if [ -n "$PAYOUT" ]; then echo "Payout address: $PAYOUT"; fi
if [ -n "$NODE_NAME" ]; then echo "Node name: $NODE_NAME"; fi

# Install dependencies if missing (works on fresh Ubuntu)
if ! command -v docker &>/dev/null; then
  echo "Installing Docker..."
  apt-get update -qq
  apt-get install -y -qq curl docker.io python3 || true
fi

# Ubuntu docker.io ships without the Compose v2 plugin; install a compose CLI.
ensure_compose() {
  if docker compose version &>/dev/null 2>&1; then
    return 0
  fi
  if command -v docker-compose &>/dev/null; then
    return 0
  fi
  echo "Installing Docker Compose..."
  apt-get update -qq
  apt-get install -y -qq docker-compose-v2 2>/dev/null \
    || apt-get install -y -qq docker-compose 2>/dev/null \
    || true
}

dc() {
  if docker compose version &>/dev/null 2>&1; then
    docker compose "$@"
  elif command -v docker-compose &>/dev/null; then
    docker-compose "$@"
  else
    echo "ERROR: Docker Compose not found. Install docker-compose-v2 or docker-compose." >&2
    exit 1
  fi
}

ensure_compose

# Ensure qxrp user
if ! id qxrp &>/dev/null; then
  echo "Creating qxrp user..."
  useradd -m -s /bin/bash qxrp
  usermod -aG docker qxrp || true
fi

# Setup directories (wipe stale ledger data from any previous broken run)
echo "Setting up directories..."
mkdir -p /var/lib/qxrp-validator/config
mkdir -p /var/lib/qxrp-validator/db /var/lib/qxrp-validator/nudb
mkdir -p /var/lib/qxrp-validator/dashboard
rm -rf /var/lib/qxrp-validator/db/* /var/lib/qxrp-validator/nudb/*
DASH_BASE="https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/tools/dashboard"
curl -fsSL "${DASH_BASE}/server.py" -o /var/lib/qxrp-validator/dashboard/server.py
curl -fsSL "${DASH_BASE}/requirements.txt" -o /var/lib/qxrp-validator/dashboard/requirements.txt
echo "VALIDATOR_ACCOUNT=" > /var/lib/qxrp-validator/dashboard/.env
chown -R 1001:1001 /var/lib/qxrp-validator

cd /var/lib/qxrp-validator

# Write docker-compose.yml (public hub ships :latest; override with QXRP_XRPLD_IMAGE)
cat > docker-compose.yml << EOC
services:
  xrpld:
    image: ${DOCKER_IMAGE}
    container_name: qxrp-validator
    restart: unless-stopped
    mem_limit: 4g
    volumes:
      - ./config:/cfg:ro
      - /var/lib/qxrp-validator:/data
    ports:
      - "51235:51235"
    logging:
      driver: "json-file"
      options:
        max-size: "50m"
        max-file: "5"
    healthcheck:
      test: ["CMD", "curl", "-f", "-X", "POST", "-d", "{\"method\":\"server_info\"}", "http://localhost:6005"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 60s
    command: ["--conf", "/cfg/xrpld.cfg"]
    networks:
      - qxrp

  dashboard:
    image: python:3.13-slim
    container_name: qxrp-dashboard
    restart: unless-stopped
    ports:
      - "8080:8080"
    environment:
      XRPLD_RPC_URL: http://qxrp-validator:6005
      NETWORK_RPC_URL: ${PUBLIC_RPC}
    env_file:
      - ./dashboard/.env
    volumes:
      - ./dashboard:/app:ro
    working_dir: /app
    command: ["sh", "-c", "pip install -q -r requirements.txt && python3 server.py"]
    depends_on:
      - xrpld
    networks:
      - qxrp

networks:
  qxrp:
    driver: bridge
EOC

# Write xrpld.cfg (basic + features, no seed yet)
cat > config/xrpld.cfg << 'EOC'
# qXRP Validator (4GB server)
# Network ID 1001 for clean testnet restart
# This is a validator node.

[network_id]
1001

[node_size]
tiny

[ledger_history]
256

[validation_quorum]
3

[server]
port_rpc_admin_local
port_rpc_public
port_peer_public
port_ws_public

[port_rpc_admin_local]
port = 5005
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http

[port_rpc_public]
port = 6005
ip = 0.0.0.0
protocol = http

[port_peer_public]
port = 51235
ip = 0.0.0.0
protocol = peer

[port_ws_public]
port = 7005
ip = 0.0.0.0
protocol = ws

[validators_file]
validators.txt

[node_db]
type = NuDB
path = /data/nudb
advisory_delete = 0
online_delete = 512
cache_size = 256

[database_path]
/data/db

[debug_logfile]
/data/debug.log

[sntp_servers]
time.windows.com
time.apple.com
time.nist.gov
pool.ntp.org

# Bootstrap peers (full-history + fleet validators)
[ips_fixed]
46.224.0.140 51235
167.233.55.43 51235
204.168.175.194 51235
89.167.109.241 51235

[transaction_queue]
minimum_txn_in_ledger = 100
target_txn_in_ledger = 1000
ledgers_in_queue = 30
minimum_queue_size = 10000
maximum_txn_per_account = 100

[rpc_startup]
{ "command": "log_level", "severity": "warning" }

[features]
ProofOfParticipation
MultiSign
MultiSignReserve
Flow
FlowCross
FeeEscalation
TickSize
Escrow
DeletableAccounts
DepositAuth
DepositPreauth
AMM
XChainBridge
EOC

# Falcon hex UNL — bonded testnet fleet (not legacy n9 classical keys).
MIN_FUND_DROPS=1100000000
MIN_BOND_DROPS=1000000000
MIN_SYNC_SEQ=1000

FLEET_UNL_FILE=/var/lib/qxrp-validator/config/fleet-unl.txt
echo "Fetching bonded fleet UNL..."
curl -fsSL "$FLEET_UNL_URL" -o "$FLEET_UNL_FILE"
write_validators_txt "" "$FLEET_UNL_FILE"

chown -R 1001:1001 /var/lib/qxrp-validator

# Save payout and node name for the higher-level qXRP validator layer / bonding
if [ -n "$PAYOUT" ]; then
  echo "$PAYOUT" > /var/lib/qxrp-validator/payout-address
  echo "Saved payout address."
fi
if [ -n "$NODE_NAME" ]; then
  echo "$NODE_NAME" > /var/lib/qxrp-validator/node-name
  echo "Saved node name."
fi

echo "Pulling ${DOCKER_IMAGE}..."
docker pull "$DOCKER_IMAGE"

echo "Starting validator container..."
(cd /var/lib/qxrp-validator && dc up -d)

echo "Waiting for RPC to be ready (up to 90s)..."
RPC_READY=0
for i in $(seq 1 30); do
  if docker exec qxrp-validator curl -sf --max-time 3 -X POST -d '{"method":"server_info"}' http://127.0.0.1:5005 > /dev/null 2>&1; then
    echo "RPC ready!"
    RPC_READY=1
    break
  fi
  sleep 3
done
if [[ "$RPC_READY" -ne 1 ]]; then
  echo "ERROR: qxrp-validator RPC not ready. Check: docker logs qxrp-validator" >&2
  echo "  Common cause: wrong image tag (use qxrp/xrpld:cid-popl, not stale :latest)." >&2
  exit 1
fi

echo "Generating Falcon-512 validator keys (account + consensus + on-chain identity)..."

rpc_local() {
  docker exec qxrp-validator curl -sf -X POST http://127.0.0.1:5005 \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"$1\",\"params\":[$2]}"
}

FALCON_JSON=$(rpc_local wallet_propose '{"key_type":"falcon512"}')
ACCOUNT=$(echo "$FALCON_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['account_id'])")
FALCON_SECRET=$(echo "$FALCON_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['falcon_secret'])")
FALCON_PK=$(echo "$FALCON_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print((r.get('public_key_hex') or r.get('public_key','')).upper())")

# Classical wallet_propose is disabled on Falcon Ledger; generate P2P node_seed locally.
NODE_SEED=$(python3 - <<'PY'
import os, hashlib
ALPHABET = 'rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz'

def b58encode_xrpl(msg: bytes) -> str:
    zeroes = len(msg) - len(msg.lstrip(b'\0'))
    pbegin = msg.lstrip(b'\0')
    if not pbegin:
        return ALPHABET[0] * zeroes
    b58 = [0] * (len(msg) * 3)
    for ch in pbegin:
        carry = ch
        for i in range(len(b58) - 1, -1, -1):
            carry += 256 * b58[i]
            b58[i] = carry % 58
            carry //= 58
    start = 0
    while start < len(b58) and b58[start] == 0:
        start += 1
    return ALPHABET[0] * zeroes + ''.join(ALPHABET[d] for d in b58[start:])

raw = os.urandom(16)
payload = bytes([33]) + raw
chk = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
print(b58encode_xrpl(payload + chk))
PY
)

KEYS_FILE=/var/lib/qxrp-validator/validator-keys.json
python3 - <<PY
import json, os
data = {
    "falcon_secret": "${FALCON_SECRET}",
    "validation_public_key_hex": "${FALCON_PK}",
    "consensus_key_hex": "${FALCON_PK}",
    "falcon_public_key_hex": "${FALCON_PK}",
    "account_address": "${ACCOUNT}",
    "node_seed": "${NODE_SEED}",
    "payout_address": "${PAYOUT}",
    "node_name": "${NODE_NAME}",
    "network_id": 1001,
}
with open("${KEYS_FILE}", "w") as f:
    json.dump(data, f, indent=2)
os.chmod("${KEYS_FILE}", 0o600)
PY

CFG=/var/lib/qxrp-validator/config/xrpld.cfg
sed -i '/\[validation_seed\]/,+1d' "$CFG"
sed -i '/\[validation_falcon_secret\]/,+1d' "$CFG"
sed -i '/\[node_seed\]/,+1d' "$CFG"
cat >> "$CFG" << EOC

[node_seed]
${NODE_SEED}
EOC

write_validators_txt "$FALCON_PK" "$FLEET_UNL_FILE"
echo "$ACCOUNT" > /var/lib/qxrp-validator/validator-r-address

printf 'VALIDATOR_ACCOUNT=%s\n' "$ACCOUNT" > /var/lib/qxrp-validator/dashboard/.env

echo "Restarting validator with node peer key (tracking mode, no validation yet)..."
(cd /var/lib/qxrp-validator && dc up -d --force-recreate)

echo "Waiting for ledger sync before enabling validation (up to 10 min)..."
SYNCED=0
for i in $(seq 1 120); do
  INFO=$(docker exec qxrp-validator curl -sf -X POST http://127.0.0.1:5005 \
    -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo '{}')
  STATE=$(echo "$INFO" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('info',{}).get('server_state',''))" 2>/dev/null || echo "")
  SEQ=$(echo "$INFO" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('info',{}).get('validated_ledger',{}).get('seq',0))" 2>/dev/null || echo "0")
  LEDGERS=$(echo "$INFO" | python3 -c "import sys,json; print(json.load(sys.stdin).get('result',{}).get('info',{}).get('complete_ledgers',''))" 2>/dev/null || echo "")
  if [[ $((i % 6)) -eq 1 ]]; then
    echo "  … state=${STATE:-?} seq=${SEQ} ledgers=${LEDGERS:-?}"
  fi
  if [[ "${STATE}" == "full" || "${STATE}" == "proposing" ]] && [[ "${SEQ}" -gt "${MIN_SYNC_SEQ}" ]]; then
    SYNCED=1
    echo "Ledger sync OK (seq ${SEQ})."
    break
  fi
  sleep 5
done
if [[ "$SYNCED" -eq 0 ]]; then
  echo "WARNING: sync not confirmed — bonding may fail until the node catches up."
fi

cat >> "$CFG" << EOC

[validation_falcon_secret]
${FALCON_SECRET}
EOC

echo "Enabling Falcon validation and restarting..."
(cd /var/lib/qxrp-validator && dc up -d --force-recreate)

echo ""
echo "=== FUND THIS VALIDATOR ADDRESS (≥1,100 qXRP) ==="
echo "Validator r-address: $ACCOUNT"
echo "Falcon validator public key (hex): $FALCON_PK"
echo "Payout address (rewards): $PAYOUT"
echo ""
echo "Keys saved: $KEYS_FILE"
echo ""

# Bond when funded (Python — avoids bash JSON limits with long Falcon hex keys)
BOND_SCRIPT=/var/lib/qxrp-validator/bond-if-funded.py
cat > "$BOND_SCRIPT" << 'BOND'
#!/usr/bin/env python3
"""Wait for funding + local sync, then bond via sign (local) + submit (public RPC)."""
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request

KEYS_FILE = "/var/lib/qxrp-validator/validator-keys.json"
PUBLIC_RPC = __import__("os").environ.get("QXRP_PUBLIC_RPC", "http://46.224.0.140:6005")
MIN_FUND = 1_100_000_000
MIN_BOND = 1_000_000_000
MIN_SYNC_SEQ = 1000


def rpc(url, method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read())


def rpc_local(method, params=None):
    payload = json.dumps({"method": method, "params": [params or {}]})
    out = subprocess.check_output(
        ["docker", "exec", "qxrp-validator", "curl", "-sf", "-X", "POST",
         "http://127.0.0.1:5005", "-H", "Content-Type: application/json", "-d", payload],
        text=True,
    )
    return json.loads(out)


def local_validated_seq():
    try:
        info = rpc_local("server_info", {})
        return int(info.get("result", {}).get("info", {}).get("validated_ledger", {}).get("seq", 0))
    except Exception:
        return 0


def wait_for_local_sync():
    print("Waiting for local validated ledger (needed for signing)...")
    for i in range(120):
        seq = local_validated_seq()
        if seq > MIN_SYNC_SEQ:
            print(f"  Local ledger ready (seq {seq}).")
            return
        if i % 6 == 0:
            print(f"  … local seq {seq} (need > {MIN_SYNC_SEQ})")
        time.sleep(5)
    print("ERROR: local node has no validated ledger — check: docker logs qxrp-validator")
    sys.exit(1)


def sign_and_submit_public(tx_json, falcon_secret):
    """Sign on local admin RPC; broadcast signed blob via public RPC."""
    sign = rpc_local("sign", {"tx_json": tx_json, "falcon_secret": falcon_secret})
    result = sign.get("result", sign)
    if result.get("error"):
        err = result.get("error", "error")
        msg = result.get("error_message", "")
        raise RuntimeError(f"{err}: {msg}")

    blob = result["tx_blob"]
    sign_eng = result.get("engine_result", "")

    sub = rpc(PUBLIC_RPC, "submit", {"tx_blob": blob})
    sres = sub.get("result", sub)
    if sres.get("error") and not sres.get("engine_result"):
        raise RuntimeError(f"{sres.get('error')}: {sres.get('error_message', '')}")

    eng = sres.get("engine_result", sign_eng or "unknown")
    msg = sres.get("engine_result_message", result.get("engine_result_message", ""))
    return eng, msg


def main():
    keys = json.load(open(KEYS_FILE))
    account = keys["account_address"]
    falcon_secret = keys["falcon_secret"]
    consensus = keys["consensus_key_hex"]
    falcon_pk = keys["falcon_public_key_hex"]

    print(f"Waiting for ≥1,100 qXRP on {account}...")
    bal = 0
    for i in range(180):
        try:
            info = rpc(PUBLIC_RPC, "account_info", {"account": account, "ledger_index": "validated"})
            bal = int(info["result"]["account_data"]["Balance"])
            if bal >= MIN_FUND:
                print(f"Funded: {bal / 1_000_000} qXRP")
                break
        except (urllib.error.URLError, KeyError, ValueError):
            bal = 0
        if i % 6 == 0:
            print(f"  … balance {bal} drops (need {MIN_FUND})")
        time.sleep(10)
    else:
        print("Timed out — run again after funding.")
        return

    wait_for_local_sync()

    print("Submitting ValidatorRegister...")
    try:
        eng, msg = sign_and_submit_public({
            "TransactionType": "ValidatorRegister",
            "Account": account,
            "PublicKey": falcon_pk,
            "ConsensusKey": consensus,
            "Fee": "12",
        }, falcon_secret)
    except RuntimeError as e:
        print(f"  ValidatorRegister: error — {e}")
        sys.exit(1)
    print(f"  ValidatorRegister: {eng}" + (f" — {msg}" if msg else ""))
    if eng not in ("tesSUCCESS", "tecDUPLICATE", "terQUEUED"):
        sys.exit(1)
    time.sleep(5)

    print("Submitting ValidatorBond (1,000 qXRP)...")
    try:
        eng, msg = sign_and_submit_public({
            "TransactionType": "ValidatorBond",
            "Account": account,
            "ConsensusKey": consensus,
            "BondedAmount": str(MIN_BOND),
            "Fee": "12",
        }, falcon_secret)
    except RuntimeError as e:
        print(f"  ValidatorBond: error — {e}")
        sys.exit(1)
    print(f"  ValidatorBond: {eng}" + (f" — {msg}" if msg else ""))
    if eng == "tecNO_PERMISSION":
        print("  Already bonded.")
    elif eng not in ("tesSUCCESS", "terQUEUED"):
        sys.exit(1)
    else:
        print("Bond complete.")


if __name__ == "__main__":
    main()
BOND
chmod +x "$BOND_SCRIPT"

echo "Starting auto-bond watcher (logs: /var/lib/qxrp-validator/bond.log)..."
nohup python3 "$BOND_SCRIPT" > /var/lib/qxrp-validator/bond.log 2>&1 &

# Hourly ClaimReward cron (no-op until composite scores exist after epoch boundary)
CLAIM_SCRIPT="/var/lib/qxrp-validator/claim-rewards.sh"
cat > "$CLAIM_SCRIPT" <<'CLAIMEOF'
#!/usr/bin/env bash
set -euo pipefail
KEYS_FILE="/var/lib/qxrp-validator/validator-keys.json"
CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['consensus_key_hex'])")
ACCOUNT=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['account_address'])")
FALCON_SECRET=$(python3 -c "import json; print(json.load(open('${KEYS_FILE}'))['falcon_secret'])")
SIGN=$(docker exec qxrp-validator curl -sf -X POST http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d "{\"method\":\"sign\",\"params\":[{\"tx_json\":{\"TransactionType\":\"ClaimReward\",\"Account\":\"${ACCOUNT}\",\"ConsensusKey\":\"${CONSENSUS_KEY}\",\"Fee\":\"12\"},\"falcon_secret\":\"${FALCON_SECRET}\"}]}")
RESULT=$(echo "$SIGN" | python3 -c "import sys,json; print(json.load(sys.stdin)['result'].get('engine_result',''))")
[[ "$RESULT" == "tesSUCCESS" ]] || exit 0
BLOB=$(echo "$SIGN" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx_blob'])")
docker exec qxrp-validator curl -sf -X POST http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d "{\"method\":\"submit\",\"params\":[{\"tx_blob\":\"${BLOB}\"}]}" >/dev/null
CLAIMEOF
chmod +x "$CLAIM_SCRIPT"
CRON_LINE="17 * * * * ${CLAIM_SCRIPT} >> /var/lib/qxrp-validator/claim.log 2>&1"
( crontab -l 2>/dev/null | grep -vF "$CLAIM_SCRIPT"; echo "$CRON_LINE" ) | crontab -
echo "Reward claimer installed (hourly cron: ${CLAIM_SCRIPT})"

PUBLIC_IP=$(curl -sf -4 --max-time 5 ifconfig.me 2>/dev/null || curl -sf -4 --max-time 5 icanhazip.com 2>/dev/null || hostname -I 2>/dev/null | awk '{print $1}' || echo "YOUR_SERVER_IP")

echo "=== FINAL OUTPUT ==="
echo "Validator r-address (FUND THIS): $ACCOUNT"
echo "Falcon validator public key (hex): $FALCON_PK"
if [ -n "$PAYOUT" ]; then echo "Payout address: $PAYOUT"; fi
echo ""
echo "=== VIEW YOUR VALIDATOR ==="
echo "Dashboard (browser):  http://${PUBLIC_IP}:8080"
echo "  Open that URL from your laptop. IP alone is not enough — use port :8080."
echo "  Or paste this IP in the wallet → Run validator → I've started my node:"
echo "    ${PUBLIC_IP}"
echo "  If it does not load, open TCP 8080 in your cloud firewall (DigitalOcean → Networking → Firewalls)."
echo "Block explorer:       https://q-xrp-faucet.vercel.app/scan"
echo "Wallet / rewards:     https://q-xrp-faucet.vercel.app/wallet"
echo ""
echo "=== USEFUL COMMANDS (on this server) ==="
echo "  docker logs -f qxrp-validator          # live validator logs"
echo "  tail -f /var/lib/qxrp-validator/bond.log   # auto-bond progress"
echo "  docker logs -f qxrp-dashboard            # dashboard container"
echo "  docker ps | grep qxrp                    # container status"
echo ""
echo "Auto-bond running in background once funded."
echo "Bootstrap complete."
