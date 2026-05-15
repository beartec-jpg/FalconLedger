# qXRP Deployment Guide

Complete instructions for deploying the qXRP network infrastructure: cloud nodes,
validators, faucet, UNL publisher, and monitoring.

---

## Contents

1. [Prerequisites](#1-prerequisites)
2. [Cloud Full Nodes](#2-cloud-full-nodes)
3. [Cloud Validator Node](#3-cloud-validator-node)
4. [Faucet (Vercel)](#4-faucet-vercel)
5. [UNL Publisher](#5-unl-publisher)
6. [Monitoring Stack](#6-monitoring-stack)
7. [Upgrading a Running Node](#7-upgrading-a-running-node)
8. [Quick-Reference Port Table](#8-quick-reference-port-table)

---

## 1. Prerequisites

### Source code

```bash
git clone https://github.com/beartec-jpg/qXRP
cd qXRP
git checkout develop
```

### Cloud server requirements (per server)

| Resource | Minimum | Recommended |
|---|---|---|
| CPU | 2 vCPU | 4+ vCPU |
| RAM | 8 GB | 16 GB |
| Disk | 50 GB SSD | 100 GB NVMe |
| OS | Ubuntu 22.04 | Ubuntu 24.04 |
| Network | 100 Mbit | 1 Gbit, static IP |

> Hetzner CX32 (4 vCPU / 8 GB / €13/mo) is a good starting point.
> A `user-data.yaml` for Hetzner cloud-init is at `bin/install/hetzner/user-data.yaml`.

---

## 2. Cloud Full Nodes

Full nodes sync and relay transactions. They do **not** validate or earn rewards.
Use them as public RPC/WebSocket endpoints.

### Deploy 3 nodes on one server

```bash
bash bin/install/deploy-cloud-nodes.sh \
  --nodes 3 \
  --network-id 999 \
  --peers "VALIDATOR_IP_1:51235,VALIDATOR_IP_2:51235"
```

### Options

| Flag | Default | Description |
|---|---|---|
| `--nodes` | `3` | Number of nodes on this server (1–3) |
| `--network-id` | `999` | qXRP network ID |
| `--peers` | _(none)_ | Comma-separated bootstrap peers `ip:port` |
| `--node-size` | `medium` | xrpld node_size (tiny/small/medium/large/huge) |
| `--install-dir` | `/opt/qxrp` | Where to install binary and source |
| `--data-dir` | `/var/lib/qxrp` | Where to store node databases |
| `--skip-build` | off | Skip build and reuse existing binary |
| `--jobs` | `nproc` | Parallel build jobs |

### What the script does

1. Installs GCC 13, CMake, Ninja, mold linker, Conan
2. Clones the repo and builds `xrpld` (~20–60 min first run)
3. Creates `xrpld.cfg` for each node under `/var/lib/qxrp/node{1,2,3}/`
4. Installs systemd services `qxrp-node1`, `qxrp-node2`, `qxrp-node3`
5. Configures log rotation (daily, 14-day retention)
6. Opens peer and WebSocket ports in `ufw`
7. Installs a health monitor cron job (`/opt/qxrp/bin/health-check.sh`)

### Managing nodes

```bash
# Status
/opt/qxrp/bin/health-check.sh

# Logs
journalctl -fu qxrp-node1

# Stop / start / restart
sudo systemctl stop qxrp-node1
sudo systemctl start qxrp-node1
sudo systemctl restart qxrp-node1
```

---

## 3. Cloud Validator Node

Validators actively participate in consensus, sign ledger proposals, and earn
qXRP rewards via Proof of Participation (PoP).

### Deploy a validator

```bash
bash bin/install/deploy-cloud-validator.sh \
  --network-id 999 \
  --peers "OTHER_VALIDATOR_IP:51235,ANOTHER_IP:51235" \
  --trusted-keys "nKEY1,nKEY2,nKEY3,nKEY4" \
  --quorum 4 \
  --node-name v1
```

### Options

| Flag | Default | Description |
|---|---|---|
| `--node-name` | `v1` | Suffix for systemd service name |
| `--peers` | _(none)_ | Bootstrap peers |
| `--trusted-keys` | _(none)_ | Comma-separated validator pubkeys to trust |
| `--quorum` | `4` | Minimum validations to close a ledger |
| `--data-dir` | `/var/lib/qxrp/validator` | Node data directory |
| `--node-size` | `medium` | xrpld node_size |
| `--skip-build` | off | Reuse existing binary |

### After the script runs — activation steps

The node will start in `tracking` state. Three steps are required to activate
validation:

#### Step 1 — Exchange public keys with other validators

Your validator public key is printed at the end of the script and saved in
`/var/lib/qxrp/validator/validator-keys.json`.

Send your key to every other validator operator and add theirs to your
`validators.txt`:

```bash
echo "nTHEIR_PUBKEY" >> /var/lib/qxrp/validator/validators.txt
sudo systemctl reload qxrp-v1
```

#### Step 2 — Fund the validator account

The validator's account address is in `validator-keys.json`. It needs at least
**2000 qXRP** to cover:

- 200 qXRP base reserve
- 1000 qXRP minimum bond
- Fees for registration and bonding transactions

#### Step 3 — Register and bond on-ledger

```bash
# From the qXRP source directory, with the regtest or mainnet running:
python3 scripts/bond-validators.py
```

Or submit the transactions manually:

```
ValidatorRegister:
  PublicKey    = <your Falcon-512 key, 1796 hex chars>
  ConsensusKey = <your classical key hex, from validator-keys.json>

ValidatorBond:
  ConsensusKey  = <same classical key hex>
  BondedAmount  = 1000000000   (1000 qXRP in drops)
```

#### Step 4 — Verify

```bash
curl -s http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}' \
  | python3 -m json.tool | grep server_state
# Expected: "server_state": "proposing"
```

### Key files (keep secret)

| File | Contents |
|---|---|
| `/var/lib/qxrp/validator/validator-keys.json` | Seed, pubkey, consensus key, account |
| `/var/lib/qxrp/validator/xrpld.cfg` | Config including `[validation_seed]` |
| `/var/lib/qxrp/validator/validators.txt` | Trusted UNL |

Both files are `chmod 600`. Back them up securely — losing the seed means losing
the validator identity.

---

## 4. Faucet (Vercel)

The faucet is a Next.js app that dispenses testnet qXRP. It lives in its own
GitHub repo and is deployed via Vercel for free serverless hosting.

### Create the repo

```bash
cp -r tools/faucet-web ~/qxrp-faucet
cd ~/qxrp-faucet
git init
git add .
git commit -m "feat: initial faucet app"
git remote add origin https://github.com/YOUR_USERNAME/qxrp-faucet.git
git push -u origin main
```

### Set up Upstash Redis (rate limiting)

1. Go to [console.upstash.com](https://console.upstash.com)
2. Create a new Redis database (free tier is sufficient)
3. Copy **REST URL** and **REST Token** — you'll need these below

### Deploy on Vercel

1. Go to [vercel.com/new](https://vercel.com/new)
2. Import the `qxrp-faucet` repository
3. Framework will auto-detect as **Next.js**
4. Add the following environment variables:

| Variable | Value |
|---|---|
| `XRPLD_RPC_URL` | `http://YOUR_NODE_IP:6005` |
| `FAUCET_ACCOUNT` | faucet account address (e.g. genesis account) |
| `FAUCET_SECRET` | faucet account seed |
| `DRIP_AMOUNT_QXRP` | `100` |
| `RATE_LIMIT_REQUESTS` | `1` |
| `RATE_LIMIT_WINDOW_SECONDS` | `86400` |
| `KV_REST_API_URL` | Upstash REST URL |
| `KV_REST_API_TOKEN` | Upstash REST Token |
| `NEXT_PUBLIC_NETWORK_NAME` | `qXRP Testnet` |
| `NEXT_PUBLIC_NETWORK_ID` | `999` |
| `NEXT_PUBLIC_EXPLORER_URL` | _(leave blank for now)_ |

5. Click **Deploy**

### Node firewall requirement

The faucet's serverless functions connect outbound to your node's public RPC port.
Make sure port `6005` is open on your cloud server:

```bash
sudo ufw allow 6005/tcp comment "qxrp public rpc"
```

### Local development

```bash
cd ~/qxrp-faucet
npm install
cp .env.example .env.local   # fill in your values
npm run dev
# Open http://localhost:3000
```

---

## 5. UNL Publisher

The UNL publisher serves a canonical list of trusted validator public keys over
HTTP. Nodes fetch this list so operators don't have to manually distribute
`validators.txt` to every participant.

### Start the publisher

```bash
cd tools/unl-publisher

# Set a strong admin token
ADMIN_TOKEN=your-secret-token docker compose up -d
```

The service starts on port **8090**.

### Add validator keys

```bash
curl -X POST http://localhost:8090/admin/validators \
  -H "X-Admin-Token: your-secret-token" \
  -H "Content-Type: application/json" \
  -d '{"key": "nVALIDATOR_PUBKEY_HERE"}'
```

### Remove a key

```bash
curl -X DELETE http://localhost:8090/admin/validators/nVALIDATOR_PUBKEY_HERE \
  -H "X-Admin-Token: your-secret-token"
```

### API endpoints

| Endpoint | Auth | Description |
|---|---|---|
| `GET /validators.txt` | None | Plaintext list for xrpld |
| `GET /validators.json` | None | JSON with sequence and metadata |
| `GET /health` | None | Service health check |
| `POST /admin/validators` | Token | Add a key |
| `DELETE /admin/validators/{key}` | Token | Remove a key |

### Configure nodes to auto-fetch the UNL

On each validator/full-node server, add a cron job to keep `validators.txt`
up to date:

```bash
# Edit crontab
crontab -e

# Add this line (fetches every 10 minutes):
*/10 * * * * UNL_URL=http://YOUR_UNL_SERVER:8090/validators.txt \
             LOCAL_FILE=/var/lib/qxrp/validator/validators.txt \
             bash /opt/qxrp/tools/unl-publisher/fetch-unl.sh
```

xrpld re-reads `validators_file` at each consensus round — no restart needed.

---

## 6. Monitoring Stack

Prometheus + Grafana + custom xrpld exporter, deployed via Docker Compose.

### Start the stack

```bash
cd tools/monitoring

# Set node URLs and Grafana password
NODES=http://NODE1_IP:6005,http://NODE2_IP:6005,http://NODE3_IP:6005 \
GRAFANA_PASSWORD=your-password \
docker compose up -d
```

### Access Grafana

Open **http://YOUR_SERVER:3000** in a browser.

- Username: `admin`
- Password: value of `GRAFANA_PASSWORD` (default: `qxrp-admin`)

The **qXRP Network** dashboard is pre-provisioned with:

- Node online/offline status
- Validated ledger sequence
- Proposing state per validator
- Peer counts
- Fee load factor
- Ledger close rate
- Node uptime

### Configure alert destinations

Edit `tools/monitoring/alertmanager.yml` and uncomment one of the receiver
blocks (Slack, PagerDuty, or email). Then restart:

```bash
docker compose restart alertmanager
```

### Default alert rules

| Alert | Trigger | Severity |
|---|---|---|
| `NodeDown` | Node unreachable for 2 min | Critical |
| `LedgerStall` | No new ledgers for 60s | Critical |
| `NotProposing` | Not proposing for 5 min | Warning |
| `LowPeers` | Fewer than 2 peers for 5 min | Warning |
| `HighLoadFactor` | Load factor > 10 for 2 min | Warning |
| `DiskFilling` | Less than 15% disk remaining | Warning |
| `HighMemory` | Memory > 90% for 5 min | Warning |

### Add more nodes

Edit `tools/monitoring/prometheus.yml` and add IPs to the `node` scrape config,
then:

```bash
docker compose restart prometheus
```

---

## 7. Upgrading a Running Node

The upgrade script stops the node safely between ledger closes, installs the
new binary, and automatically rolls back if the health check fails.

### Using a pre-built binary

```bash
bash bin/upgrade-validator.sh \
  --service qxrp-v1 \
  --binary /path/to/new/xrpld
```

### Rebuilding from source

```bash
bash bin/upgrade-validator.sh \
  --service qxrp-v1 \
  --build
```

### Dry run (preview without changes)

```bash
bash bin/upgrade-validator.sh \
  --service qxrp-v1 \
  --build \
  --dry-run
```

### Options

| Flag | Default | Description |
|---|---|---|
| `--service` | `qxrp-v1` | Systemd service to upgrade |
| `--binary` | _(none)_ | Path to pre-built binary |
| `--build` | off | Build from source before upgrading |
| `--src-dir` | `/opt/qxrp/src` | Source directory for `--build` |
| `--rpc-url` | `http://127.0.0.1:5005` | RPC URL for health checks |
| `--dry-run` | off | Show steps without executing |

### What happens during upgrade

1. Verifies the new binary is executable
2. Logs current state and ledger sequence
3. Waits up to 15s for a clean ledger close
4. Sends SIGTERM to the service (60s graceful window, then SIGKILL)
5. Backs up old binary to `/opt/qxrp/bin/xrpld.backup.YYYYMMDD-HHMMSS`
6. Installs the new binary
7. Starts the service
8. Polls for 30s — if the node reaches `proposing`, `full`, or `tracking` the upgrade succeeds
9. If health check fails: stops service, restores backup, restarts — no manual intervention needed

---

## 8. Quick-Reference Port Table

| Port | Protocol | Service | Exposure |
|---|---|---|---|
| 5005 | TCP | xrpld admin RPC (node 1) | localhost only |
| 5006 | TCP | xrpld admin RPC (node 2) | localhost only |
| 5007 | TCP | xrpld admin RPC (node 3) | localhost only |
| 6005 | TCP | xrpld public RPC (node 1) | public (needed by faucet) |
| 6006 | TCP | xrpld public RPC (node 2) | public |
| 6007 | TCP | xrpld public RPC (node 3) | public |
| 7005 | TCP | xrpld WebSocket (node 1) | public |
| 7006 | TCP | xrpld WebSocket (node 2) | public |
| 7007 | TCP | xrpld WebSocket (node 3) | public |
| 51235 | TCP | xrpld peer (node 1) | public |
| 51236 | TCP | xrpld peer (node 2) | public |
| 51237 | TCP | xrpld peer (node 3) | public |
| 3000 | TCP | Grafana dashboard | internal/VPN |
| 9090 | TCP | Prometheus | internal/VPN |
| 9093 | TCP | Alertmanager | internal/VPN |
| 9101 | TCP | xrpld Prometheus exporter | internal only |
| 8090 | TCP | UNL publisher | public |
