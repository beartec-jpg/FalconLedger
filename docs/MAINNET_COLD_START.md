# Mainnet cold-start model — prepare offline, turn on together

**Recommendation:** Do **not** “genesis and leave a live chain sitting” while you prep.  
**Do:** finish a **cold package**, then a single **T0 go-live** when validators start closing ledgers and you publish connect details.

---

## Can we genesis but not run ledgers?

### Short answer

| Idea | Works? | Notes |
|------|--------|--------|
| Prepare keys, UNL, image, portal, scripts offline | **Yes** | This is the main prep work |
| Create genesis ledger, then **stop all nodes** so no new ledgers | **Technically possible, poor ops** | Chain is “frozen” only if **nothing** is closing ledgers. Risky (leaked DB = someone can run private head). Hard to reason about. |
| Genesis on day 0 and leave one node running “idle” | **No** | XRPL-style nodes **keep closing ledgers**. Epochs advance by ledger count; wall-clock airdrop window is separate. |
| Public “connect to genesis” before validators are up | **No** | No healthy network until quorum can agree. |

### What actually works well

```text
PHASE 0 — Cold prep (now → T0−1)
  • Pin release image digest
  • Generate mainnet keys offline (genesis, faucet, airdrop, dev, validators)
  • Write UNL + validator list (unpublished)
  • Portal Neon + env staged (mainnet live=false)
  • Dry-run genesis split math + batch-pay scripts
  • Ceremony checklist signed off

PHASE 1 — T0 go-live (public day)
  • Start N validators from same genesis + same image + same UNL
  • Publish RPC URLs, network id, install one-liner, faucet
  • Run genesis split 2B / 1B / 1B
  • Enable mainnet in portal; start airdrop snapshot cron
  • Bootstrap lend pool InterestRate=5000 when ready

PHASE 2 — 60 days
  • Score mainnet only
  • Emissions quiet until epoch 8

PHASE 3 — Day 60+
  • Freeze scores → batch airdrop full 2B
```

**Epochs** count **ledgers**, not calendar days. If the network is not producing ledgers, epochs do not advance. The **airdrop 60-day window** is wall-clock from genesis time (portal/config). So “sit with no ledgers” freezes emissions/epochs but does **not** freeze calendar airdrop unless you also delay setting `genesis_at`.

**Best practice:** set `airdrop_config.genesis_at` only when T0 is real and public.

---

## Cold package contents (prep now)

Store offline / air-gapped as appropriate:

```text
mainnet-ceremony/
  IMAGE_DIGEST.txt          # qxrp/xrpld@sha256:…
  NETWORK_ID.txt            # chosen mainnet id
  unl/
    validators.txt          # public keys only
    validators-secrets/     # offline only — never publish
  wallets/
    GENESIS.address
    AIRDROP.address
    FAUCET.address
    DEV.address             # multi-sig plan documented
    # secrets offline
  portal/
    .env.mainnet.example    # no secrets in git
  scripts/
    mainnet-genesis-split.env.example
    airdrop-snapshot-cron.sh
  CHECKLIST.md              # copy of GO_LIVE checklist
```

---

## Why not “genesis early and wait”?

1. **Accidental liveness** — one running node produces history; peers join later at different heads → messy UNL/start.  
2. **Security** — genesis DB + secrets are crown jewels; sitting half-live increases leak surface.  
3. **Airdrop clock** — if you set genesis time early, the 60-day window burns while you’re still prepping.  
4. **Community trust** — announce connect details only when quorum is ready.

**Exception:** private **rehearsal** networks (throwaway genesis, wipe before public T0). Do not reuse rehearsal secrets on mainnet.

---

## T0 turn-on day (what you publish)

1. Network name + **network id**  
2. Image: `qxrp/xrpld@sha256:…`  
3. Seed/UNL or peer list for validators  
4. Public RPC URL(s)  
5. Portal URL + “mainnet live”  
6. Faucet limits (100 / 5/day / 1h)  
7. Airdrop rules (mainnet-only, 60 days, full 2B batch later)

New validators **join after T0** using the same image + published UNL/peers — they do **not** need a pre-genesis sit period.
