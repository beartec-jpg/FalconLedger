# Mainnet ops runbook — Neon, cron, genesis, fleet

**Date:** 2026-07-18  
Companion to `docs/MAINNET_LAUNCH_SPEC.md` and `docs/MAINNET_READINESS_AUDIT.md`.

---

## 1. Neon / Postgres (portal)

Portal faucet quota + airdrop tracker require Postgres (Neon recommended).

### Schema

From the portal repo (`Falcon-faucet-wallet` / `qXRP-faucet-wallet`):

```bash
# Neon SQL editor or psql
psql "$DATABASE_URL" -f docs/sql/airdrop-schema.sql
```

Creates:

- `faucet_claims` — durable claim log (5/day + 1h cooldown enforcement)
- `airdrop_config` — window, pool size, first emission epoch
- `airdrop_allocations` — per-address scores
- `airdrop_snapshots` — daily snapshot payloads

### Env (Vercel)

| Variable | Purpose |
|----------|---------|
| `DATABASE_URL` | Neon pooled connection string |
| `AIRDROP_ADMIN_TOKEN` | Bearer token for snapshot / freeze / recompute admin APIs |
| `FAUCET_SECRET` / existing faucet envs | Unchanged |

Without `DATABASE_URL`, faucet falls back to in-memory limits (not durable across restarts).

---

## 2. Daily airdrop snapshot cron

During the **60 days after mainnet genesis**, run a daily snapshot:

```bash
# Example: 02:00 UTC daily
0 2 * * * /opt/falcon/scripts/airdrop-snapshot-cron.sh >> /var/log/airdrop-snapshot.log 2>&1
```

Script lives in this repo: `scripts/airdrop-snapshot-cron.sh`.

It POSTs to the portal:

```
POST /api/airdrop/snapshot
Authorization: Bearer $AIRDROP_ADMIN_TOKEN
```

After window close: freeze scores, export, then run `scripts/airdrop-batch-pay.py`.

---

## 3. Genesis split dry-run

**Never** run `--execute` until keys and UNL ceremony are complete.

```bash
export PUBLIC_RPC='https://<mainnet-rpc>'
export ADMIN_RPC='http://127.0.0.1:5005'   # signing node
export GENESIS_ADDRESS='r...'              # or GENESIS_SECRET for derive
export AIRDROP_ADDRESS='r...'
export FAUCET_ADDRESS='r...'
export DEV_ADDRESS='r...'
# optional: CONTAINER=qxrp-full for docker-exec curl

python3 scripts/mainnet-genesis-split.py --dry-run
```

Dry-run prints the plan (2B / 1B / 1B of the 4B circulating) and checks genesis balance when reachable.  
`--execute` submits three Payments from the genesis circulating account. **Does not touch the 196B treasury.**

Testnet practice (optional):

```bash
export PUBLIC_RPC='http://46.224.0.140:6005'
export GENESIS_ADDRESS='rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh'
export AIRDROP_ADDRESS='rTEST1...'
export FAUCET_ADDRESS='rTEST2...'
export DEV_ADDRESS='rTEST3...'
python3 scripts/mainnet-genesis-split.py --dry-run
```

---

## 4. Fleet image (epoch-8 + AMM emissions)

Protocol image must include:

- `kQXRP_FIRST_EMISSION_EPOCH = 8`
- Vault/AMM LP emission split + `ClaimAmmLpReward` (tt 94)

```bash
# On build host (8GB coordinator)
cd /root/qXRP && git pull origin develop
DOCKER_BUILDKIT=1 docker build -t qxrp/xrpld:lending-v5 -f docker/Dockerfile .

docker tag qxrp/xrpld:lending-v5 qxrp/xrpld:latest
docker push qxrp/xrpld:lending-v5
# optional: retag Hub latest after soak
```

Rolling upgrade (existing validators — **not** wipe/genesis):

```bash
# Example compose pin
export QXRP_XRPLD_IMAGE=qxrp/xrpld:lending-v5
docker pull "$QXRP_XRPLD_IMAGE"
docker compose pull && docker compose up -d
```

Confirm:

```bash
docker exec <container> xrpld --version
# ledger_entry reward_epoch → AmmLPAllocationBps present after next epoch boundary
```

**Note:** Epoch-8 quiet period only applies to **new** ledgers after upgrade. Existing testnet epoch numbers continue from current seq / 172800.

---

## 5. Installer default image

Mainnet installers / one-liners should pin:

```bash
export QXRP_XRPLD_IMAGE=qxrp/xrpld:lending-v5
```

Do not leave external validators on historical `cid-popl` tags after launch.

---

## 6. Quick verification checklist

| Check | Command / UI |
|-------|----------------|
| Schema applied | `\dt` shows `faucet_claims`, `airdrop_*` |
| Faucet durable | Claim twice → log rows in Neon |
| Snapshot | Admin POST snapshot → row in `airdrop_snapshots` |
| Genesis plan | `--dry-run` shows 2B/1B/1B |
| Emissions | Epoch ≥ 8, pool > 0, ClaimReward / ClaimLPReward / ClaimAmmLpReward |
| Portal | `/rewards` shows all three claim cards |
