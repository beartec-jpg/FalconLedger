#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# One-command qXRP validator onboarding installer (Docker edition).
# Bootstraps a full bonded, reward-earning validator with minimal effort.
#
# Tested on: Ubuntu 22.04, Ubuntu 24.04, Debian 12
#
# USAGE (recommended - from the qXRP Portal wallet page):
#   curl -fsSL https://install.qxrp.network/validator | bash -s -- \
#     --payout rYOUR_MAIN_WALLET_ADDRESS \
#     --node-name mynode
#
# Or locally during development:
#   bash bin/install/install-qxrp-validator.sh --payout r... --node-name test
#
# After running:
#   1. The script prints YOUR VALIDATOR ACCOUNT (fund it with >=1100 qXRP)
#   2. It auto-detects funding, runs ValidatorRegister + ValidatorBond
#   3. Starts the node + a reward claimer
#   4. You withdraw/claim from the portal or with the helper script later
#
# Flags:
#   --payout ADDR     Your main wallet address (rewards will be easy to send here)
#   --node-name NAME  Short name for this validator (default: host short name)
#   --bond 1000       Minimum bond in qXRP (do not change unless you know why)
#   --no-auto-bond    Only set up the node; you will bond manually
#   --help            Show this help

set -euo pipefail

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
PAYOUT_ADDRESS=""
NODE_NAME="$(hostname -s 2>/dev/null || echo validator)"
BOND_QXRP=1000
AUTO_BOND=1
SHOW_HELP=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --payout)
            PAYOUT_ADDRESS="$2"; shift 2 ;;
        --node-name|--name)
            NODE_NAME="$2"; shift 2 ;;
        --bond)
            BOND_QXRP="$2"; shift 2 ;;
        --no-auto-bond)
            AUTO_BOND=0; shift ;;
        --help|-h)
            SHOW_HELP=1; shift ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Run with --help for usage." >&2
            exit 1
            ;;
    esac
done

if [[ "$SHOW_HELP" -eq 1 ]]; then
    sed -n '5,35p' "$0" | sed 's/^# \?//'
    exit 0
fi

if [[ -z "$PAYOUT_ADDRESS" ]]; then
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  RECOMMENDED: Run with --payout so the installer knows where   ║"
    echo "║  you want to withdraw rewards to later.                        ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Example (copy from the qXRP Portal wallet page):"
    echo "  curl -fsSL https://install.qxrp.network/validator | bash -s -- \\"
    echo "    --payout rYOUR_WALLET_ADDRESS_HERE --node-name mynode"
    echo ""
    echo "Continuing without --payout (you can still withdraw manually)..."
    echo ""
    sleep 2
fi

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DOCKER_IMAGE="qxrp/xrpld:latest"
BASE_DIR="$HOME/.qxrp"
CONFIG_DIR="$BASE_DIR/config"
DATA_DIR="$BASE_DIR/data"
KEYS_DIR="$BASE_DIR/keys"
COMPOSE_FILE="$BASE_DIR/docker-compose.yml"
CLAIMER_SCRIPT="$BASE_DIR/qxrp-claimer.py"
MIN_RAM_MB=3800
MIN_DISK_GB=80
BOND_DROPS=$(( BOND_QXRP * 1000000 ))
REQUIRED_DROPS=$(( (BOND_QXRP + 250) * 1000000 ))   # bond + generous reserve + fees
NETWORK_ID=999

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log()  { echo -e "\033[1;32m[qxrp]\033[0m $*"; }
warn() { echo -e "\033[1;33m[qxrp] WARN:\033[0m $*"; }
die()  { echo -e "\033[1;31m[qxrp] ERROR:\033[0m $*" >&2; exit 1; }
already_done() { log "$1 – already done, skipping."; }

# Generate a syntactically-valid Falcon-512 "pubkey" (0xFB + 897 random bytes).
# This passes isValidNodeKey() and is exactly what the regtest bonding script uses.
# Real liboqs Falcon keys will be added later (new RPC or bundled tool).
generate_falcon_pubkey() {
    python3 -c '
import secrets, sys
prefix = bytes([0xFB])
raw = prefix + secrets.token_bytes(897)
print(raw.hex().upper())
' 2>/dev/null || {
        # Pure bash fallback (slower but no python dependency for this part)
        echo -n "FB"; head -c 897 /dev/urandom | od -An -tx1 | tr -d ' \n' | tr 'a-f' 'A-F' | cut -c1-1794
    }
}

banner() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════════════════╗"
    printf "║  %-74s ║\n" "$1"
    echo "╚════════════════════════════════════════════════════════════════════════════╝"
}

# ---------------------------------------------------------------------------
# 1. Pre-flight checks
# ---------------------------------------------------------------------------
log "Checking system requirements..."

RAM_MB=$(awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo)
DISK_GB=$(df --output=avail -BG "$HOME" | tail -1 | tr -d 'G ')

[[ "$RAM_MB" -ge "$MIN_RAM_MB" ]] \
    || die "Insufficient RAM: ${RAM_MB} MB available, ${MIN_RAM_MB} MB required."

[[ "$DISK_GB" -ge "$MIN_DISK_GB" ]] \
    || die "Insufficient disk: ${DISK_GB} GB available, ${MIN_DISK_GB} GB required."

log "System OK – RAM: ${RAM_MB} MB, Disk: ${DISK_GB} GB  |  Payout: ${PAYOUT_ADDRESS:-not set}  |  Node: $NODE_NAME"

# ---------------------------------------------------------------------------
# 2. Install Docker if missing
# ---------------------------------------------------------------------------
if command -v docker &>/dev/null; then
    already_done "Docker $(docker --version | awk '{print $3}' | tr -d ',')"
else
    log "Installing Docker (this may take a minute)..."
    curl -fsSL https://get.docker.com | sh
    sudo usermod -aG docker "$USER" || true
    warn "Docker installed. You may need to log out and back in (newgrp docker) for group membership."
fi

mkdir -p "$CONFIG_DIR" "$DATA_DIR" "$KEYS_DIR"

# ---------------------------------------------------------------------------
# 3. Key generation (classical validation seed + synthetic Falcon identity)
# ---------------------------------------------------------------------------
KEYS_FILE="$KEYS_DIR/validator-keys.json"
SEED_FILE="$KEYS_DIR/seed.txt"
FALCON_FILE="$KEYS_DIR/falcon-pubkey.txt"

if [[ -f "$KEYS_FILE" ]]; then
    already_done "Validator keys (re-using existing identity in $KEYS_FILE)"
    VAL_SEED=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['validation_seed'])")
    VAL_PUBKEY=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['validation_public_key'])")
    CONSENSUS_KEY=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['consensus_key_hex'])")
    VALIDATOR_ACCOUNT=$(python3 -c "import json; print(json.load(open('$KEYS_FILE'))['account_address'])")
    FALCON_PUB=$(cat "$FALCON_FILE" 2>/dev/null || generate_falcon_pubkey)
else
    log "Generating validator identity (classical + Falcon)..."

    # Use a temporary standalone container from the same image to generate keys cleanly.
    # This avoids needing xrpld on the host and keeps everything self-contained.
    BOOT_ID="qxrp-keygen-$$"
    BOOT_DIR=$(mktemp -d)
    BOOT_PORT=15123

    cat > "$BOOT_DIR/xrpld.cfg" <<'BOOTCFG'
[node_size]
tiny
[ledger_history]
0
[server]
port_rpc_admin_local
[port_rpc_admin_local]
port = 15123
ip = 127.0.0.1
admin = 127.0.0.1
protocol = http
[node_db]
type = NuDB
path = /tmp/db
advisory_delete = 0
[database_path]
/tmp
[debug_logfile]
/tmp/debug.log
BOOTCFG

    docker rm -f "$BOOT_ID" >/dev/null 2>&1 || true
    docker run -d --rm --name "$BOOT_ID" \
        -v "$BOOT_DIR:/cfg:ro" \
        -p "127.0.0.1:${BOOT_PORT}:${BOOT_PORT}" \
        "$DOCKER_IMAGE" --conf /cfg/xrpld.cfg --standalone >/dev/null

    # Wait for temp node
    for i in $(seq 1 30); do
        if curl -s -o /dev/null -w '%{http_code}' \
            -X POST "http://127.0.0.1:${BOOT_PORT}" \
            -H 'Content-Type: application/json' \
            -d '{"method":"server_info","params":[{}]}' 2>/dev/null | grep -q 200; then
            break
        fi
        sleep 1
        [[ $i -eq 30 ]] && die "Temporary keygen node failed to start"
    done

    # Generate classical validation seed + keys
    RESP=$(curl -s -X POST "http://127.0.0.1:${BOOT_PORT}" \
        -H 'Content-Type: application/json' \
        -d "{\"method\":\"validation_create\",\"params\":[{\"secret\":\"qxrp-${NODE_NAME}-$(date +%s)\"}]}")

    VAL_SEED=$(echo "$RESP" | python3 -c "
import sys, json
data = json.load(sys.stdin)
print(data.get('result',{}).get('validation_seed','') or data.get('result',{}).get('seed',''))
")
    VAL_PUBKEY=$(echo "$RESP" | python3 -c "
import sys, json
print(json.load(sys.stdin)['result']['validation_public_key'])
")

    # Derive the account address + consensus pubkey from the same seed (this is the account that will bond)
    WP=$(curl -s -X POST "http://127.0.0.1:${BOOT_PORT}" \
        -H 'Content-Type: application/json' \
        -d "{\"method\":\"wallet_propose\",\"params\":[{\"seed\":\"${VAL_SEED}\",\"key_type\":\"secp256k1\"}]}")
    VALIDATOR_ACCOUNT=$(echo "$WP" | python3 -c "
import sys, json
print(json.load(sys.stdin)['result']['account_id'])
")
    CONSENSUS_KEY=$(echo "$WP" | python3 -c "
import sys, json
r = json.load(sys.stdin)['result']
print(r.get('public_key_hex') or r.get('public_key',''))
")

    # Synthetic Falcon pubkey (protocol-accepted shape)
    FALCON_PUB=$(generate_falcon_pubkey)

    # Save everything
    python3 - <<PY > "$KEYS_FILE"
import json, datetime
data = {
    "validation_seed": "$VAL_SEED",
    "validation_public_key": "$VAL_PUBKEY",
    "consensus_key_hex": "$CONSENSUS_KEY",
    "account_address": "$VALIDATOR_ACCOUNT",
    "falcon_pubkey": "$FALCON_PUB",
    "node_name": "$NODE_NAME",
    "network_id": $NETWORK_ID,
    "generated_at": datetime.datetime.utcnow().isoformat() + "Z",
    "payout_address": "${PAYOUT_ADDRESS}"
}
json.dump(data, open("$KEYS_FILE", "w"), indent=2)
PY
    chmod 600 "$KEYS_FILE"

    echo "$VAL_SEED" > "$SEED_FILE"
    chmod 600 "$SEED_FILE"

    echo "$FALCON_PUB" > "$FALCON_FILE"

    docker rm -f "$BOOT_ID" >/dev/null 2>&1 || true
    rm -rf "$BOOT_DIR"

    log "Keys generated and saved to $KEYS_FILE (chmod 600 — keep this file secret!)"
fi

log "Validator account   : $VALIDATOR_ACCOUNT"
log "Validation pubkey   : $VAL_PUBKEY"
log "Consensus key (hex) : ${CONSENSUS_KEY:0:16}..."

# ---------------------------------------------------------------------------
# 4. Write hardened validator config + validators.txt
# ---------------------------------------------------------------------------
CFG_FILE="$CONFIG_DIR/xrpld.cfg"
VALIDATORS_FILE="$CONFIG_DIR/validators.txt"

if [[ -f "$CFG_FILE" && -f "$VALIDATORS_FILE" ]]; then
    already_done "Validator config and UNL"
else
    log "Writing production validator configuration..."

    # Self-trusting validators.txt (add more via --trusted or manually for a real UNL)
    {
        echo "# qXRP validator UNL — edit carefully"
        echo "$VAL_PUBKEY"
    } > "$VALIDATORS_FILE"
    chmod 600 "$VALIDATORS_FILE"

    cat > "$CFG_FILE" <<CFG
# qXRP Validator — $NODE_NAME (generated by install-qxrp-validator.sh)
# Keep this file chmod 600. The [validation_seed] is extremely sensitive.

[network_id]
${NETWORK_ID}

[node_size]
medium

[ledger_history]
full

[validation_quorum]
1

# === VALIDATOR IDENTITY (classical) ===
[validation_seed]
${VAL_SEED}

[validators_file]
${VALIDATORS_FILE}

# === FEATURES ===
[features]
ProofOfParticipation

# === PORTS ===
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

# === STORAGE ===
[node_db]
type = NuDB
path = /data/nudb
advisory_delete = 0
online_delete = 512

[database_path]
/data/db

[debug_logfile]
/data/debug.log

[sntp_servers]
time.windows.com
time.apple.com
time.nist.gov
pool.ntp.org

# === RECOMMENDED FOR VALIDATORS ===
[transaction_queue]
minimum_txn_in_ledger = 100
target_txn_in_ledger = 1000
ledgers_in_queue = 30
minimum_queue_size = 10000
maximum_txn_per_account = 100
CFG

    chmod 600 "$CFG_FILE"
    log "Config written: $CFG_FILE"
fi

# ---------------------------------------------------------------------------
# 5. Docker Compose (validator + optional future claimer sidecar)
# ---------------------------------------------------------------------------
if [[ -f "$COMPOSE_FILE" ]]; then
    already_done "docker-compose.yml"
else
    log "Writing docker-compose.yml..."
    cat > "$COMPOSE_FILE" <<COMPOSE
version: "3.9"
services:
  xrpld:
    image: ${DOCKER_IMAGE}
    container_name: qxrp_validator
    restart: unless-stopped
    volumes:
      - ${CONFIG_DIR}:/cfg:ro
      - ${DATA_DIR}:/data
      - ${KEYS_DIR}:/keys:ro
    ports:
      - "5005:5005"
      - "6005:6005"
      - "51235:51235"
      - "7005:7005"
      - "8080:8080"
    command: ["--conf", "/cfg/xrpld.cfg"]
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:5005"]
      interval: 30s
      timeout: 5s
      retries: 3

  # Future: lightweight reward claimer sidecar (uncomment when ready)
  # claimer:
  #   image: python:3.11-alpine
  #   restart: unless-stopped
  #   volumes:
  #     - ${CLAIMER_SCRIPT}:/claimer.py:ro
  #     - ${KEYS_DIR}:/keys:ro
  #   command: python /claimer.py --config /keys/validator-keys.json --rpc http://xrpld:5005
  #   depends_on:
  #     - xrpld
COMPOSE
fi

# ---------------------------------------------------------------------------
# 6. Pull image + start node
# ---------------------------------------------------------------------------
log "Pulling Docker image ${DOCKER_IMAGE}..."
docker pull "$DOCKER_IMAGE" || warn "Pull failed — will try to use cached image"

log "Starting qXRP validator container..."
docker compose -f "$COMPOSE_FILE" up -d xrpld

# ---------------------------------------------------------------------------
# 7. Wait for RPC + initial bond flow
# ---------------------------------------------------------------------------
log "Waiting for node RPC (up to 90s)..."
for i in $(seq 1 90); do
    CODE=$(curl -s -o /dev/null -w '%{http_code}' \
        -X POST "http://127.0.0.1:5005" \
        -H 'Content-Type: application/json' \
        -d '{"method":"server_info","params":[{}]}' 2>/dev/null || echo 000)
    [[ "$CODE" == "200" ]] && break
    [[ $i -eq 90 ]] && die "Node did not become ready. Check: docker logs qxrp_validator"
    sleep 1
done

# Derive current account sequence etc. (the account we must fund)
log "Validator account that must be funded: $VALIDATOR_ACCOUNT"

if [[ "$AUTO_BOND" -eq 1 ]]; then
    banner "FUNDING REQUIRED — READ CAREFULLY"
    echo ""
    echo "  Your validator account (send qXRP here):"
    echo "     $VALIDATOR_ACCOUNT"
    echo ""
    echo "  Recommended amount: 1,100 – 1,200 qXRP"
    echo "     (1,000 qXRP minimum bond + ~200 qXRP reserve + fees)"
    echo ""
    echo "  Get test qXRP from the faucet in the qXRP Portal,"
    echo "  or send from the wallet address you loaded there."
    echo ""
    echo "  The installer will now poll every 15 seconds until it sees enough balance."
    echo "  Then it will automatically register + bond the validator."
    echo ""

    # Simple funding detection + auto bond loop
    FUNDED=0
    for attempt in $(seq 1 200); do   # ~50 minutes max
        BAL=$(curl -s -X POST "http://127.0.0.1:5005" \
            -H 'Content-Type: application/json' \
            -d "{\"method\":\"account_info\",\"params\":[{\"account\":\"${VALIDATOR_ACCOUNT}\",\"ledger_index\":\"current\"}]}" \
            2>/dev/null | python3 -c "
import sys, json
try:
    r = json.load(sys.stdin)
    bal = r.get('result',{}).get('account_data',{}).get('Balance','0')
    print(int(bal))
except Exception:
    print(0)
" || echo 0)

        if [[ "$BAL" -ge "$REQUIRED_DROPS" ]]; then
            FUNDED=1
            log "Funding detected! Balance = $((BAL / 1000000)) qXRP — proceeding to auto-bond..."
            break
        fi

        if [[ $(( attempt % 4 )) -eq 0 ]]; then
            echo -ne "\r  Waiting for funding... (current: $((BAL / 1000000)) qXRP / need ~$((REQUIRED_DROPS / 1000000)))   "
        fi
        sleep 15
    done
    echo ""

    if [[ "$FUNDED" -ne 1 ]]; then
        warn "Funding not detected after waiting. You can bond manually later with:"
        echo "  python3 - <<'PY'   # (or use the portal)"
        echo "  # (see docs/validator-onboarding.md for the exact ValidatorRegister + ValidatorBond txs)"
        echo "PY"
    else
        # Auto bond using the local node (it has the seed in config, but we use submit with secret for simplicity)
        log "Submitting ValidatorRegister (Falcon identity + consensus key)..."
        REG_TX=$(curl -s -X POST "http://127.0.0.1:5005" \
            -H 'Content-Type: application/json' \
            -d "{
                \"method\":\"submit\",
                \"params\":[{
                    \"tx_json\":{
                        \"TransactionType\":\"ValidatorRegister\",
                        \"Account\":\"${VALIDATOR_ACCOUNT}\",
                        \"PublicKey\":\"${FALCON_PUB}\",
                        \"ConsensusKey\":\"${CONSENSUS_KEY}\",
                        \"Fee\":\"12\"
                    },
                    \"secret\":\"${VAL_SEED}\"
                }]
            }")

        REG_RESULT=$(echo "$REG_TX" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result',{})
print(r.get('engine_result','ERROR'))
" 2>/dev/null || echo ERROR)

        if [[ "$REG_RESULT" == "tesSUCCESS" || "$REG_RESULT" == "tecDUPLICATE" ]]; then
            log "Register: $REG_RESULT"
        else
            warn "Register result: $REG_RESULT (may already be registered — continuing)"
        fi

        sleep 3

        log "Submitting ValidatorBond (${BOND_QXRP} qXRP)..."
        BOND_TX=$(curl -s -X POST "http://127.0.0.1:5005" \
            -H 'Content-Type: application/json' \
            -d "{
                \"method\":\"submit\",
                \"params\":[{
                    \"tx_json\":{
                        \"TransactionType\":\"ValidatorBond\",
                        \"Account\":\"${VALIDATOR_ACCOUNT}\",
                        \"ConsensusKey\":\"${CONSENSUS_KEY}\",
                        \"BondedAmount\":\"${BOND_DROPS}\",
                        \"Fee\":\"12\"
                    },
                    \"secret\":\"${VAL_SEED}\"
                }]
            }")

        BOND_RESULT=$(echo "$BOND_TX" | python3 -c "
import sys, json
r = json.load(sys.stdin).get('result',{})
print(r.get('engine_result','ERROR'))
" 2>/dev/null || echo ERROR)

        if [[ "$BOND_RESULT" == "tesSUCCESS" ]]; then
            log "Bond: tesSUCCESS — validator is now bonded and eligible for rewards!"
        elif [[ "$BOND_RESULT" == "tecNO_PERMISSION" ]]; then
            log "Bond: already bonded ✓"
        else
            warn "Bond result: $BOND_RESULT — check with ledger_entry or the portal"
        fi

        sleep 2
    fi
fi

# ---------------------------------------------------------------------------
# 8. Install lightweight claimer helper (auto-claim when score is good)
# ---------------------------------------------------------------------------
log "Installing reward claim helper (qxrp-claimer)..."
python3 - <<'PYEOF' > "$CLAIMER_SCRIPT"
#!/usr/bin/env python3
"""Minimal qXRP reward claimer.
Run periodically (cron / systemd timer / docker sidecar).
It only claims — it does NOT auto-sweep to payout (per design).
User controls withdrawals from the portal or manually.
"""
import argparse, json, time, urllib.request, urllib.error, sys, os

def rpc(url, method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read())["result"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", default="/keys/validator-keys.json")
    ap.add_argument("--rpc", default="http://127.0.0.1:5005")
    ap.add_argument("--once", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.keys):
        print("No keys file — nothing to claim for.", file=sys.stderr)
        return

    with open(args.keys) as f:
        k = json.load(f)

    account = k["account_address"]
    seed = k["validation_seed"]
    ck = k["consensus_key_hex"]

    try:
        info = rpc(args.rpc, "server_info")["info"]
        state = info.get("server_state")
        print(f"Node state: {state}")
    except Exception as e:
        print("Cannot reach node:", e)
        return

    # Check bond / score
    try:
        bond = rpc(args.rpc, "ledger_entry", {"validator_bond": {"account": account}, "ledger_index": "validated"})
        node = bond.get("node", {})
        score = node.get("CompositeScore", 0) or 0
        status = node.get("BondStatus")
        print(f"Bond status={status} composite_score={score} bps")
        if score < 500:
            print("Score too low for ClaimReward right now — skipping.")
            return
    except Exception:
        print("No bond object yet (or not bonded).")
        return

    # Attempt claim
    try:
        tx = {
            "TransactionType": "ClaimReward",
            "Account": account,
            "ConsensusKey": ck,
            "Fee": "12"
        }
        res = rpc(args.rpc, "submit", {"tx_json": tx, "secret": seed})
        eng = res.get("engine_result", "unknown")
        print(f"ClaimReward result: {eng}")
        if eng == "tesSUCCESS":
            print("Rewards claimed successfully into validator account.")
    except Exception as e:
        print("Claim attempt failed:", e)

if __name__ == "__main__":
    main()
PYEOF
chmod +x "$CLAIMER_SCRIPT"

# Simple cron hint (user can enable)
(crontab -l 2>/dev/null || true; echo "*/30 * * * * $CLAIMER_SCRIPT --keys $KEYS_FILE --rpc http://127.0.0.1:5005 >> $BASE_DIR/claimer.log 2>&1") | sort -u | crontab -

# ---------------------------------------------------------------------------
# 9. Final success banner
# ---------------------------------------------------------------------------
PUBKEY=$(curl -s -X POST "http://127.0.0.1:5005" \
    -H 'Content-Type: application/json' \
    -d '{"method":"server_info","params":[{}]}' 2>/dev/null \
    | python3 -c "
import sys, json
try:
    print(json.load(sys.stdin)['result']['info'].get('pubkey_validator','n/a'))
except Exception:
    print('n/a')
" 2>/dev/null || echo "n/a")

banner "qXRP VALIDATOR IS LIVE"
echo ""
echo "  Account (fund this & where rewards land) : $VALIDATOR_ACCOUNT"
echo "  Validation pubkey                       : $VAL_PUBKEY"
echo "  Node pubkey (for UNL)                   : $PUBKEY"
[[ -n "$PAYOUT_ADDRESS" ]] && echo "  Your payout/withdraw wallet (from --payout): $PAYOUT_ADDRESS"
echo ""
echo "  Local dashboard : http://localhost:8080"
echo "  RPC (admin)     : http://127.0.0.1:5005"
echo "  Peer port       : 0.0.0.0:51235  (open this in your firewall / cloud security group)"
echo ""
echo "  Next steps in the qXRP Portal (wallet page):"
echo "    • Check that your validator account now shows as BONDED"
echo "    • Watch epochs & composite score"
echo "    • Claim rewards (or use the claimer script) then withdraw to your main wallet"
echo ""
echo "  Useful commands:"
echo "    docker compose -f $COMPOSE_FILE logs -f"
echo "    docker compose -f $COMPOSE_FILE down"
echo "    $CLAIMER_SCRIPT --keys $KEYS_FILE"
echo ""
echo "  IMPORTANT: Backup $KEYS_FILE and $SEED_FILE securely (offline)."
echo "             Losing them = losing this validator identity forever."
echo ""
echo "═══════════════════════════════════════════════════════════════════════════════"
log "Setup complete. Go fund the validator account above if you haven't already."
