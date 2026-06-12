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

echo "=== qXRP Validator Bootstrap (fresh server) ==="
echo "Using secret (wallet input): $SECRET_INPUT"
if [ -n "$PAYOUT" ]; then echo "Payout address: $PAYOUT"; fi
if [ -n "$NODE_NAME" ]; then echo "Node name: $NODE_NAME"; fi

# Install dependencies if missing (works on fresh Ubuntu)
if ! command -v docker &>/dev/null; then
  echo "Installing Docker..."
  apt-get update -qq
  apt-get install -y -qq curl docker.io python3 || apt-get install -y -qq curl docker.io docker-compose python3 || true
fi

# Ensure qxrp user
if ! id qxrp &>/dev/null; then
  echo "Creating qxrp user..."
  useradd -m -s /bin/bash qxrp
  usermod -aG docker qxrp || true
fi

# Setup directories
echo "Setting up directories..."
mkdir -p /var/lib/qxrp-validator/config
chown -R qxrp:qxrp /var/lib/qxrp-validator

cd /var/lib/qxrp-validator

# Write docker-compose.yml
cat > docker-compose.yml << 'EOC'
services:
  xrpld:
    image: qxrp/xrpld:latest
    container_name: qxrp-validator
    restart: unless-stopped
    deploy:
      resources:
        limits:
          memory: 4G
        reservations:
          memory: 2G
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

# Current good validators list (update as needed when adding more)
cat > config/validators.txt << 'EOC'
[validators]
n9M1HThMraVC5Fa5xqk7AHpZdjm15HioyVneKVq3JdohwGMQ4pZ9
n9K7bH53ZhNVU5LVsTBwCchEJvTX8vTVXYyF5EvGGYTE4nn9GtMF
n9KtL7AC4C62QvPxixj3EKYUki2i5TNFk8SLrussW7LmuHusSsgT
n9LrpfYjS4MJhvCEPUEPDAheto4NMrpmbuuFfU1uLUsBbn9sZdU5
EOC

chown -R qxrp:qxrp /var/lib/qxrp-validator

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
su - qxrp -c "cd /var/lib/qxrp-validator && docker compose up -d"

echo "Waiting for RPC to be ready (up to 90s)..."
for i in $(seq 1 30); do
  if su - qxrp -c "docker exec qxrp-validator curl -sf --max-time 3 -X POST -d '{\"method\":\"server_info\"}' http://127.0.0.1:5005 > /dev/null 2>&1"; then
    echo "RPC ready!"
    break
  fi
  sleep 3
done

echo "Running wallet one line command with secret: $SECRET_INPUT"
su - qxrp -c "
docker exec qxrp-validator curl -s -X POST -d '{\"method\":\"validation_create\",\"params\":[{\"secret\":\"$SECRET_INPUT\"}]}' http://127.0.0.1:5005
" > /tmp/wallet.json

cat /tmp/wallet.json

# Parse seed and pubkey
SEED=$(python3 -c '
import json,sys
try:
  d = json.load(open("/tmp/wallet.json"))
  print(d["result"]["validation_seed"])
except:
  print("FAIL")
' 2>/dev/null || echo "FAIL")

PUB=$(python3 -c '
import json,sys
try:
  d = json.load(open("/tmp/wallet.json"))
  print(d["result"]["validation_public_key"])
except:
  print("FAIL")
' 2>/dev/null || echo "FAIL")

if [ "$SEED" != "FAIL" ] && [ "$PUB" != "FAIL" ]; then
  echo "=== SUCCESS ==="
  echo "validation_seed: $SEED"
  echo "validation_public_key: $PUB"

  # Patch the config with the seed
  su - qxrp -c "
    CFG=/var/lib/qxrp-validator/config/xrpld.cfg
    sed -i '/\[validation_seed\]/,+1d' \$CFG
    cat >> \$CFG << 'EOC'

[validation_seed]
$SEED
EOC
    echo \"$PUB\" >> /var/lib/qxrp-validator/config/validators.txt
  "

  # Restart
  su - qxrp -c "cd /var/lib/qxrp-validator && docker compose up -d"

  echo "Validator is running with its own seed."
else
  echo "Failed to parse wallet output. Check /tmp/wallet.json and patch manually."
fi

# === Generate a regular XRPL account (r-address) for the node itself ===
# This is the address the user must fund with ≥1,100 qXRP for bonding.
# It is separate from the payout address.
echo ""
echo "Generating a fresh validator account (r-address) for bonding..."
su - qxrp -c "
docker exec qxrp-validator curl -s -X POST -d '{\"method\":\"wallet_propose\"}' http://127.0.0.1:5005
" > /tmp/node_account.json

NODE_R=$(python3 -c '
import json,sys
try:
  d = json.load(open("/tmp/node_account.json"))
  print(d["result"]["account"])
except:
  print("FAIL")
' 2>/dev/null || echo "FAIL")

NODE_SECRET=$(python3 -c '
import json,sys
try:
  d = json.load(open("/tmp/node_account.json"))
  print(d["result"]["master_seed"])
except:
  print("FAIL")
' 2>/dev/null || echo "FAIL")

if [ "$NODE_R" != "FAIL" ] && [ "$NODE_SECRET" != "FAIL" ]; then
  echo "$NODE_R" > /var/lib/qxrp-validator/validator-r-address
  echo "$NODE_SECRET" > /var/lib/qxrp-validator/validator-master-seed

  echo ""
  echo "=== FUNDING ADDRESS (send qXRP here for bonding) ==="
  echo "Validator r-address: $NODE_R"
  echo "Master seed (KEEP SECRET, never share): $NODE_SECRET"
  echo ""
  echo "Claim 2,000 qXRP from the faucet and send ≥1,100 qXRP to the r-address above."
  echo "The installer will auto-bond once it detects enough balance."
  echo "This is SEPARATE from your payout address ($PAYOUT)."
  echo ""
  echo "Files saved:"
  echo "  /var/lib/qxrp-validator/validator-r-address"
  echo "  /var/lib/qxrp-validator/validator-master-seed"
else
  echo "Failed to generate validator account. You will need to create one manually."
fi

echo ""
echo "=== FINAL OUTPUT ==="
echo "validation_public_key (add to UNL on other nodes): $PUB"
if [ -n "$PAYOUT" ]; then echo "Payout address (for rewards): $PAYOUT"; fi
if [ -n "$NODE_NAME" ]; then echo "Node name: $NODE_NAME"; fi
if [ "$NODE_R" != "FAIL" ]; then echo "Validator r-address (FUND THIS): $NODE_R"; fi
echo ""
echo "Bootstrap complete. Validator container is running."
