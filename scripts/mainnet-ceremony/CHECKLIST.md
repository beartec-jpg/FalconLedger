# Mainnet go-live checklist

**Use with:** `MAINNET_LAUNCH_SPEC.md`, `MAINNET_COLD_START.md`, `ops/MAINNET_OPS_RUNBOOK.md`  
**Model:** Cold prep → single T0 start (do not leave a public half-live chain sitting).

Tick boxes as you complete them.

---

## Phase 0 — Cold prep (before any public ledger)

### Protocol / image

- [ ] Release commit frozen (include: epoch-8, AMM+vault emissions, ClaimAmmLpReward, pool InterestRate)
- [ ] Image built and tagged (e.g. `mainnet-v1` or `lending-v6`)
- [ ] `docker run … xrpld --version` shows expected **git commit**
- [ ] Image pushed to Docker Hub (or private registry)
- [ ] **Digest** recorded: `qxrp/xrpld@sha256:________________`
- [ ] Installer / bootstrap default pin = that digest (not floating `:latest`)
- [ ] Security freeze notes / known issues published internally

### Keys & wallets (offline)

- [ ] Mainnet **genesis** secret generated (not testnet seed) — Falcon on freeze image
- [ ] **AIRDROP** wallet address + secret (cold → warm for batch pay)
- [ ] **FAUCET** wallet address + secret (1B allocation after split)
- [x] **DEV** multi-sig / custody plan documented → `scripts/mainnet-ceremony/DEV_CUSTODY.md`
- [ ] **DEV** wallet address + secret generated offline
- [ ] Initial **validator** keys (N operators) + bond plan
- [ ] Secrets backed up offline; never committed to git

### Network config

- [x] **Network ID** chosen and documented → **1026** (`scripts/mainnet-ceremony/NETWORK_ID.txt`)
- [x] Genesis / validators config templates filled → `cfg/mainnet/*.example` + ceremony pack
- [ ] UNL / validator public key list prepared (unpublished until T0) — generate Falcon keys offline
- [ ] Public RPC hostnames reserved (DNS ready) — template `scripts/mainnet-ceremony/DNS_RPC.md`

### Portal / data

- [ ] Neon (or Postgres) provisioned
- [ ] `docs/sql/airdrop-schema.sql` applied
- [ ] Vercel (or host) env staged:
  - [ ] `MAINNET_RPC_URL` / public RPC
  - [ ] `MAINNET_FAUCET_ACCOUNT` / `MAINNET_FAUCET_SECRET`
  - [ ] `MAINNET_DRIP_AMOUNT_QXRP=100` (or `MAINNET_DRIP_QXRP`)
  - [ ] `FAUCET_CLAIMS_PER_DAY=5`, `FAUCET_COOLDOWN_SECONDS=3600`
  - [ ] `DATABASE_URL`
  - [ ] `AIRDROP_ADMIN_TOKEN`
  - [ ] `NEXT_PUBLIC_MAINNET_LIVE=false` until T0
  - [ ] `NEXT_PUBLIC_MAINNET_NETWORK_ID`, `NEXT_PUBLIC_MAINNET_DRIP_QXRP=100`
- [ ] Portal `main` deployed with airdrop **mainnet-only** guards
- [ ] Airdrop cron script installed with `NETWORK=mainnet` only (do not start until T0)

### Scripts dry-run

- [ ] `mainnet-genesis-split.py --dry-run` with planned addresses
- [ ] `airdrop-batch-pay.py` dry-run path understood
- [ ] `set-broker-pool-rate.py` ready (after lend bootstrap on live chain)
- [ ] HF monitor unit file ready (mainnet RPC + broker secret)

### Ceremony people

- [ ] Who runs genesis node(s)
- [ ] Who holds UNL keys
- [ ] Who runs portal / faucet
- [ ] Who announces T0

---

## Phase 1 — T0 (turn-on day)

### Chain

- [ ] Start validators **together** from same image + genesis + UNL
- [ ] Confirm consensus / `server_state` healthy
- [ ] Confirm network id on peer handshakes
- [ ] Publish RPC endpoints
- [ ] **Do not** set airdrop `genesis_at` until this moment is intentional

### Economics

- [ ] Run genesis split: 2B AIRDROP / 1B FAUCET / 1B DEV
- [ ] Verify balances on three wallets
- [ ] Fund portal faucet hot wallet from FAUCET bucket (bounded amount)

### Portal

- [ ] Flip `NEXT_PUBLIC_MAINNET_LIVE=true`
- [ ] Point mainnet RPC at public endpoint
- [ ] Confirm faucet drip = **100**, limits 5/day + 1h
- [ ] Confirm faucet claims log with `network=mainnet`
- [ ] Set `airdrop_config.genesis_at` = T0 UTC
- [ ] Start daily airdrop snapshot cron (`NETWORK=mainnet`)

### Lending (same day or shortly after)

- [ ] Deploy lend vault + broker on mainnet
- [ ] `LoanBroker.InterestRate = 5000` (5%) via `set-broker-pool-rate.py`
- [ ] Bridge-only stables if required
- [x] Bridge multi-sig model proven on Sepolia (2-of-3) — see `dry-runs/REHEARSAL_RESULTS.md` + `docs/MAINNET_BRIDGE.md`
- [ ] ETH mainnet lock deploy: OWNERS multi-sig, REQUIRED≥2, Circle USDC `0xA0b8…eB48` (new keys; not Sepolia test owners)
- [ ] Bridge relays on mainnet Falcon issuer + no owner private keys on relay host
- [ ] HF monitor live

### Communications

- [ ] Publish install one-liner + image digest
- [ ] Publish network id, RPC, faucet rules, airdrop rules (mainnet-only, 60 days, batch pay 2B)
- [ ] Explicit: testnet does **not** earn mainnet airdrop

---

## Phase 2 — Days 1–60

- [ ] Snapshot cron green daily
- [ ] DEX LP snapshot quality reviewed (fix if stub)
- [ ] Faucet abuse / IP sybil review
- [ ] Emissions remain **0** until epoch **8**
- [ ] No testnet rows in `airdrop_allocations`

---

## Phase 3 — Day 60+

- [ ] `POST /api/airdrop/freeze` (mainnet only)
- [ ] Export allocation list; verify sum ≈ 2B FALCON
- [ ] Batch pay from AIRDROP wallet (scripted)
- [ ] Retry failures; log residual &lt; 1 FALCON if any
- [ ] Public “airdrop complete” note

---

## Phase 4 — Epoch 8+

- [ ] Confirm non-zero epoch pool
- [ ] Validators claim (`ClaimReward`)
- [ ] Vault LPs claim (`ClaimLPReward`)
- [ ] AMM LPs claim (`ClaimAmmLpReward`)
- [ ] Portal `/rewards` works against mainnet

---

## Explicit non-goals for T0

- Waiting for a pre-started genesis to “warm up” for weeks  
- Paying airdrop for **testnet** activity  
- Floating image tags without digest  
- Leaving genesis secrets on a public hot node  

---

## Quick “ready?” gate

| Gate | Required for T0 |
|------|-----------------|
| Image digest pinned | Yes |
| Keys + UNL | Yes |
| Split wallets + dry-run | Yes |
| Portal + Neon + faucet 100 | Yes |
| Airdrop mainnet-only scoring | Yes |
| Full DEX LP scan perfect | No (improve in window) |
| Epoch 8 emissions | No (later) |
| Batch airdrop paid | No (day 60) |
