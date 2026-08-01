# qXRP Falcon Validator Onboarding

Network ID **1001** · Falcon Ledger · Proof-of-Participation rewards

## Quick start (one command)

From the [Wallet → Run Validator](https://q-xrp-faucet.vercel.app/wallet) panel, copy the one-liner. It looks like:

```bash
# Defaults to qxrp/xrpld:btc-spv-v6 (live Falcon testnet 1001 SPV bridge fleet)
curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \
  --payout rYourWalletAddress \
  --node-name my-qxrp-node
```

### Before you run it

1. **Claim 2,000 qXRP** from the [faucet](https://q-xrp-faucet.vercel.app/) into your Falcon wallet.
2. **Open port 51235/TCP** on your VPS or home router (required for peering).
3. Use **Ubuntu 22.04/24.04** with ≥4 GB RAM and ≥40 GB disk.

### What the installer does

| Step | Action |
|------|--------|
| 1 | Installs Docker and pulls `qxrp/xrpld:btc-spv-v6` (pinned SPV fleet image — never floating `:latest`) |
| 2 | **Falcon smoke tests** — local image sign check + fleet signature check on all bootstrap peers |
| 3 | Generates validator keys (consensus + Falcon register key + node identity) |
| 4 | Connects to the live testnet UNL and bootstrap peers |
| 5 | Starts the validator container |
| 6 | Prints your **validator r-address** — fund it with ≥1,100 qXRP |
| 7 | Auto-submits `ValidatorRegister` + `ValidatorBond(1000)` when funded |
| 8 | Installs hourly `ClaimReward` cron |

If the smoke test fails, the installer **stops before** generating keys or bonding.
See [Fleet Image Pinning & Smoke Tests](./fleet-image-pinning.md) for manual checks
and mainnet launch guidance.

Your wallet address (`--payout`) is saved for future reward withdrawals.

## Funding math

| Item | Amount |
|------|--------|
| Account reserve | ~200 qXRP |
| Minimum bond | 1,000 qXRP |
| Fees buffer | ~100 qXRP |
| **Minimum to fund validator** | **1,100 qXRP** |
| **Faucet drip (recommended)** | **2,000 qXRP** |

## Useful commands (on your server)

Replace `my-qxrp-node` with your `--node-name`.

```bash
# Live logs
docker logs -f qxrp-my-qxrp-node

# Node status via RPC
curl -s -X POST http://127.0.0.1:5005 -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}' | python3 -m json.tool

# Validator account balance (public RPC)
curl -s -X POST http://46.224.0.140:6005 -H 'Content-Type: application/json' \
  -d '{"method":"account_info","params":[{"account":"rYOUR_VALIDATOR_ADDRESS","ledger_index":"validated"}]}'

# Restart validator
docker compose -f ~/.qxrp/my-qxrp-node/docker-compose.yml restart

# Stop validator
docker compose -f ~/.qxrp/my-qxrp-node/docker-compose.yml down

# Manual reward claim
~/.qxrp/my-qxrp-node/claim-rewards.sh

# View claim log
tail -f ~/.qxrp/my-qxrp-node/claim.log
```

## Network endpoints

| Service | URL |
|---------|-----|
| Public RPC (full history) | `http://46.224.0.140:6005` |
| Faucet + Wallet portal | `https://q-xrp-faucet.vercel.app` |
| Bootstrap peers | `46.224.0.140:51235`, `167.233.55.43:51235`, `204.168.175.194:51235`, `89.167.109.241:51235` |

## Image pinning & smoke tests

All validators must run the **same** `qxrp/xrpld:falcon` build. Mixed images cause
Falcon txs to show success on one node but never validate in consensus.

- **Docs:** [fleet-image-pinning.md](./fleet-image-pinning.md)
- **Override image:** `export QXRP_XRPLD_IMAGE='qxrp/xrpld@sha256:…'`
- **Skip smoke test (debug only):** add `--skip-smoke-test` to the installer

## Troubleshooting

- **`Invalid signature` on smoke test** — upgrade every validator to the same
  `qxrp/xrpld:falcon` digest, then re-run the installer (or manual fleet test).
- **Faucet success but zero balance** — usually fleet image skew; see fleet doc above.
- **`tracking` but not `proposing`** — node is syncing; wait for ledger catch-up.
- **`tecNO_PERMISSION` on bond** — already bonded, or insufficient balance.
- **No peers** — check port 51235 is open and `ips_fixed` peers are reachable.
- **Bad node public key** — ensure Falcon node identity (no classical `[node_seed]`; use `[validation_falcon_secret]` or auto Falcon identity).

## Files on your server

```
~/.qxrp/<node-name>/
  config/validator-keys.json   # SECRET — chmod 600
  config/xrpld.cfg
  config/validators.txt
  data/                        # ledger database
  docker-compose.yml
  claim-rewards.sh             # hourly reward claimer
  claim.log
```