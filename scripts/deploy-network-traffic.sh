#!/usr/bin/env bash
# Deploy traffic generator + upgraded dashboard to coordinator and fleet.
set -euo pipefail

COORD="46.224.0.140"
SSH=(ssh -i "${HOME}/.ssh/id_ed25519" -o StrictHostKeyChecking=no)
SCP=(scp -i "${HOME}/.ssh/id_ed25519" -o StrictHostKeyChecking=no)
REPO="${REPO:-/home/scott/qXRP}"

log() { echo "[deploy] $*"; }

log "Sync scripts + dashboard to coordinator"
"${SSH[@]}" "root@${COORD}" "mkdir -p /opt/qxrp/scripts /opt/qxrp/dashboard /var/lib/qxrp-traffic /var/lib/qxrp-dashboard"
"${SCP[@]}" "${REPO}/scripts/network-traffic.py" "root@${COORD}:/opt/qxrp/scripts/"
"${SCP[@]}" "${REPO}/tools/dashboard/server.py" "root@${COORD}:/opt/qxrp/dashboard/"
"${SCP[@]}" "${REPO}/tools/dashboard/requirements.txt" "root@${COORD}:/opt/qxrp/dashboard/"

log "Install systemd traffic service on coordinator"
"${SSH[@]}" "root@${COORD}" bash -s <<'REMOTE'
set -euo pipefail
cat > /etc/systemd/system/qxrp-traffic.service <<'UNIT'
[Unit]
Description=qXRP testnet traffic generator
After=docker.service
Wants=docker.service

[Service]
Type=simple
User=root
WorkingDirectory=/opt/qxrp/scripts
Environment=TRAFFIC_DATA_DIR=/var/lib/qxrp-traffic
Environment=XRPLD_CONTAINER=qxrp-full
Environment=ADMIN_RPC_URL=http://127.0.0.1:5005
Environment=PUBLIC_RPC_URL=http://46.224.0.140:6005
ExecStart=/usr/bin/python3 /opt/qxrp/scripts/network-traffic.py --daemon
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable qxrp-traffic.service
systemctl restart qxrp-traffic.service
sleep 3
systemctl --no-pager status qxrp-traffic.service | head -12
REMOTE

log "Update coordinator dashboards (full + val2)"
for dir in /var/lib/qxrp-full /var/lib/qxrp-val2; do
  "${SSH[@]}" "root@${COORD}" "
    mkdir -p ${dir}/dashboard ${dir}/dashboard-data
    cp /opt/qxrp/dashboard/server.py ${dir}/dashboard/
    cp /opt/qxrp/dashboard/requirements.txt ${dir}/dashboard/
  "
done

"${SSH[@]}" "root@${COORD}" bash -s <<'REMOTE'
set -euo pipefail
patch_dashboard() {
  local base="$1"
  local name="$2"
  local acct="$3"
  local hist="$4"
  local traffic_env="$5"
  python3 - "$base" "$name" "$acct" "$hist" "$traffic_env" <<'PY'
import sys, pathlib, re
base, name, acct, hist, traffic_env = sys.argv[1:6]
compose = pathlib.Path(base) / "docker-compose.yml"
text = compose.read_text()
if "TRAFFIC_STATS_FILE" not in text:
    text = text.replace(
        "NETWORK_RPC_URL: http://46.224.0.140:6005",
        "NETWORK_RPC_URL: http://46.224.0.140:6005\n      TRAFFIC_STATS_FILE: " + traffic_env + "\n      METRICS_HISTORY_FILE: " + hist,
    )
text = re.sub(r"VALIDATOR_ACCOUNT=.*", f"VALIDATOR_ACCOUNT={acct}", text)
if "dashboard-data:/data" not in text:
    text = text.replace(
        "- ./dashboard:/app:ro",
        "- ./dashboard:/app:ro\n      - ./dashboard-data:/data",
    )
compose.write_text(text)
PY
  cd "$base"
  docker compose up -d --force-recreate "$name"
}

VAL2_ACCT=$(python3 -c "
import hashlib
s=open('/var/lib/qxrp-val2/config/xrpld.cfg').read().split('[validation_falcon_secret]')[1].split()[0].strip()
raw=bytes.fromhex(s)
pub=raw[:898 if raw[0]==0xFB else 1794]
ab=hashlib.new('ripemd160', hashlib.sha256(pub).digest()).digest()
alpha='rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz'
payload=b'\\x00'+ab
cs=hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
data=payload+cs
num=int.from_bytes(data,'big')
chars=[]
while num:
    num,r=divmod(num,58); chars.append(alpha[r])
for b in data:
    if b==0: chars.append(alpha[0])
    else: break
print(''.join(reversed(chars)))
")

FULL_ACCT=$(python3 -c "print('')" 2>/dev/null || true)
patch_dashboard /var/lib/qxrp-full qxrp-full-dashboard "" /data/history.json /var/lib/qxrp-traffic/stats.json
patch_dashboard /var/lib/qxrp-val2 qxrp-val2-dashboard "$VAL2_ACCT" /data/history.json /var/lib/qxrp-traffic/stats.json
REMOTE

log "Deploy dashboard to validator fleet"
FLEET=(
  "qxrp@167.233.55.43:/var/lib/qxrp-validator"
  "qxrp@204.168.175.194:/var/lib/qxrp-validator"
  "qxrp@89.167.109.241:/var/lib/qxrp-validator"
)
for target in "${FLEET[@]}"; do
  host="${target%%:*}"
  path="${target##*:}"
  user="${host%%@*}"
  ip="${host##*@}"
  key=~/.ssh/id_ed25519
  log "  $ip"
  scp -i "$key" -o StrictHostKeyChecking=no \
    "${REPO}/tools/dashboard/server.py" \
    "${REPO}/tools/dashboard/requirements.txt" \
    "${user}@${ip}:${path}/dashboard/" 
  ssh -i "$key" -o StrictHostKeyChecking=no "${user}@${ip}" "
    mkdir -p ${path}/dashboard-data
    grep -q METRICS_HISTORY_FILE ${path}/docker-compose.yml 2>/dev/null || \
      sed -i 's|NETWORK_RPC_URL: http://46.224.0.140:6005|NETWORK_RPC_URL: http://46.224.0.140:6005\\n      METRICS_HISTORY_FILE: /data/history.json|' ${path}/docker-compose.yml
    grep -q dashboard-data ${path}/docker-compose.yml 2>/dev/null || \
      sed -i 's|- ./dashboard:/app:ro|- ./dashboard:/app:ro\\n      - ./dashboard-data:/data|' ${path}/docker-compose.yml
    cd ${path} && docker compose up -d --force-recreate qxrp-dashboard 2>/dev/null || docker compose up -d --force-recreate dashboard
  "
done

# val5 + nyc with their keys
for spec in "id_val5:root@5.78.142.246" "id_digitalocean:root@192.241.247.158"; do
  key=~/.ssh/${spec%%:*}
  host=${spec##*:}
  log "  ${host##*@}"
  scp -i "$key" -o StrictHostKeyChecking=no \
    "${REPO}/tools/dashboard/server.py" \
    "${REPO}/tools/dashboard/requirements.txt" \
    "${host}:/var/lib/qxrp-validator/dashboard/"
  ssh -i "$key" -o StrictHostKeyChecking=no "$host" "
    mkdir -p /var/lib/qxrp-validator/dashboard-data
    cd /var/lib/qxrp-validator && docker compose up -d --force-recreate qxrp-dashboard 2>/dev/null || docker compose up -d --force-recreate dashboard
  "
done

log "Done. Traffic stats: /var/lib/qxrp-traffic/stats.json"
log "Coordinator dashboard: http://${COORD}:8080"