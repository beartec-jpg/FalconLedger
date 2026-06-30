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
DOCKER_IMAGE="${QXRP_XRPLD_IMAGE:-qxrp/xrpld:latest}"

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

# Setup directories
echo "Setting up directories..."
mkdir -p /var/lib/qxrp-validator/config
mkdir -p /var/lib/qxrp-validator/db /var/lib/qxrp-validator/nudb
chown -R 1001:1001 /var/lib/qxrp-validator

cd /var/lib/qxrp-validator

# Write docker-compose.yml (public hub ships :latest; override with QXRP_XRPLD_IMAGE)
cat > docker-compose.yml << EOC
version: "3.8"
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

# Bootstrap peer: the 8GB full history node
[ips_fixed]
46.224.0.140 51235

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

# Live testnet UNL (classical n9 validator keys)
TRUSTED_KEYS="n9M1HThMraVC5Fa5xqk7AHpZdjm15HioyVneKVq3JdohwGMQ4pZ9,n9K7bH53ZhNVU5LVsTBwCchEJvTX8vTVXYyF5EvGGYTE4nn9GtMF,n9KtL7AC4C62QvPxixj3EKYUki2i5TNFk8SLrussW7LmuHusSsgT,n9LrpfYjS4MJhvCEPUEPDAheto4NMrpmbuuFfU1uLUsBbn9sZdU5"
PUBLIC_RPC="${QXRP_PUBLIC_RPC:-http://46.224.0.140:6005}"
MIN_FUND_DROPS=1100000000
MIN_BOND_DROPS=1000000000

{
  echo "[validators]"
  IFS=',' read -ra KEYS <<< "$TRUSTED_KEYS"
  for k in "${KEYS[@]}"; do echo "$k"; done
} > config/validators.txt

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

echo "Starting validator container..."
(cd /var/lib/qxrp-validator && dc up -d)

echo "Waiting for RPC to be ready (up to 90s)..."
for i in $(seq 1 30); do
  if docker exec qxrp-validator curl -sf --max-time 3 -X POST -d '{"method":"server_info"}' http://127.0.0.1:5005 > /dev/null 2>&1; then
    echo "RPC ready!"
    break
  fi
  sleep 3
done

echo "Generating validator keys (classical consensus + Falcon identity + node peer key)..."

rpc_local() {
  docker exec qxrp-validator curl -sf -X POST http://127.0.0.1:5005 \
    -H 'Content-Type: application/json' \
    -d "{\"method\":\"$1\",\"params\":[$2]}"
}

VAL_JSON=$(rpc_local validation_create '{"key_type":"secp256k1"}')
VAL_SEED=$(echo "$VAL_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['validation_seed'])")
VAL_PUBKEY=$(echo "$VAL_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['validation_public_key'])")

WP_JSON=$(rpc_local wallet_propose "{\"seed\":\"${VAL_SEED}\",\"key_type\":\"secp256k1\"}")
ACCOUNT=$(echo "$WP_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r['account_id'])")
CONSENSUS_KEY=$(echo "$WP_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print((r.get('public_key_hex') or r.get('public_key','')).upper())")

NODE_JSON=$(rpc_local wallet_propose '{"key_type":"secp256k1"}')
NODE_SEED=$(echo "$NODE_JSON" | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['master_seed'])")

FALCON_JSON=$(rpc_local wallet_propose '{"key_type":"falcon512"}')
FALCON_PK=$(echo "$FALCON_JSON" | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print((r.get('public_key_hex') or r.get('public_key','')).upper())")

KEYS_FILE=/var/lib/qxrp-validator/validator-keys.json
python3 - <<PY
import json, os
data = {
    "validation_seed": "${VAL_SEED}",
    "validation_public_key": "${VAL_PUBKEY}",
    "consensus_key_hex": "${CONSENSUS_KEY}",
    "account_address": "${ACCOUNT}",
    "falcon_public_key_hex": "${FALCON_PK}",
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

[validation_seed]
${VAL_SEED}

[node_seed]
${NODE_SEED}
EOC

echo "$VAL_PUBKEY" >> /var/lib/qxrp-validator/config/validators.txt
echo "$ACCOUNT" > /var/lib/qxrp-validator/validator-r-address
echo "$VAL_SEED" > /var/lib/qxrp-validator/validator-master-seed

echo "Restarting validator with peer + validation keys..."
(cd /var/lib/qxrp-validator && dc up -d --force-recreate)

echo ""
echo "=== FUND THIS VALIDATOR ADDRESS (≥1,100 qXRP) ==="
echo "Validator r-address: $ACCOUNT"
echo "Validation public key: $VAL_PUBKEY"
echo "Payout address (rewards): $PAYOUT"
echo ""
echo "Keys saved: $KEYS_FILE"
echo ""

# Bond when funded (Python — avoids bash JSON limits with long Falcon hex keys)
BOND_SCRIPT=/var/lib/qxrp-validator/bond-if-funded.py
cat > "$BOND_SCRIPT" << 'BOND'
#!/usr/bin/env python3
"""Wait for validator funding, then ValidatorRegister + ValidatorBond."""
import json
import subprocess
import sys
import time
import urllib.request

KEYS_FILE = "/var/lib/qxrp-validator/validator-keys.json"
PUBLIC_RPC = __import__("os").environ.get("QXRP_PUBLIC_RPC", "http://46.224.0.140:6005")
MIN_FUND = 1_100_000_000
MIN_BOND = 1_000_000_000


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


def submit_tx(tx_json, secret):
    r = rpc_local("submit", {"tx_json": tx_json, "secret": secret})
    result = r.get("result", r)
    if "error" in result:
        raise RuntimeError(f"{result.get('error')}: {result.get('error_message', '')}")
    return result


def main():
    keys = json.load(open(KEYS_FILE))
    account = keys["account_address"]
    secret = keys["validation_seed"]
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
        except Exception:
            bal = 0
        if i % 6 == 0:
            print(f"  … balance {bal} drops (need {MIN_FUND})")
        time.sleep(10)
    else:
        print("Timed out — run again after funding.")
        return

    print("Submitting ValidatorRegister...")
    reg = submit_tx({
        "TransactionType": "ValidatorRegister",
        "Account": account,
        "PublicKey": falcon_pk,
        "ConsensusKey": consensus,
        "Fee": "12",
    }, secret)
    eng = reg.get("engine_result", "unknown")
    print(f"  ValidatorRegister: {eng}")
    if eng not in ("tesSUCCESS", "tecDUPLICATE", "terQUEUED"):
        print(f"  message: {reg.get('engine_result_message', '')}")
        sys.exit(1)
    time.sleep(4)

    print("Submitting ValidatorBond (1,000 qXRP)...")
    bond = submit_tx({
        "TransactionType": "ValidatorBond",
        "Account": account,
        "ConsensusKey": consensus,
        "BondedAmount": str(MIN_BOND),
        "Fee": "12",
    }, secret)
    eng = bond.get("engine_result", "unknown")
    print(f"  ValidatorBond: {eng}")
    if eng == "tecNO_PERMISSION":
        print("  Already bonded.")
    elif eng not in ("tesSUCCESS", "terQUEUED"):
        print(f"  message: {bond.get('engine_result_message', '')}")
        sys.exit(1)
    else:
        print("Bond complete.")


if __name__ == "__main__":
    main()
BOND
chmod +x "$BOND_SCRIPT"

echo "Starting auto-bond watcher (logs: /var/lib/qxrp-validator/bond.log)..."
nohup python3 "$BOND_SCRIPT" > /var/lib/qxrp-validator/bond.log 2>&1 &

echo "=== FINAL OUTPUT ==="
echo "Validator r-address (FUND THIS): $ACCOUNT"
echo "validation_public_key: $VAL_PUBKEY"
if [ -n "$PAYOUT" ]; then echo "Payout address: $PAYOUT"; fi
echo "Auto-bond running in background once funded."
echo "Bootstrap complete."
