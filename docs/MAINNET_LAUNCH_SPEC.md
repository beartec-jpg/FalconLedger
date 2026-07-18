# Falcon Ledger — Mainnet Launch Spec Sheet

**Status:** Draft for mainnet planning (July 2026)  
**Scope:** Every adjustable launch parameter, proposed genesis split, airdrop program, tracker product, anti-sybil  
**Sources of truth (code today):** `include/xrpl/protocol/QXRPConstants.h`, `SystemParameters.h`, lending/bootstrap scripts, portal env

---

## 1. Supply model (current code)

| Parameter | Value | Code / notes |
|-----------|--------|----------------|
| **Total supply** | **200,000,000,000** FALCON (200B) | `kINITIAL_XRP` |
| **Drop granularity** | 1 FALCON = 1,000,000 drops | standard XRPL |
| **Genesis circulating** | **4,000,000,000** (2% of total) | `kQXRP_GENESIS_ALLOCATION` |
| **Protocol treasury** | **196,000,000,000** (98% of total) | `kQXRP_TREASURY_ALLOCATION` — no private key; emissions only |
| **Treasury seed** | public well-known | `kQXRP_TREASURY_SEED` |

**Yes — genesis at 2% = 4B tokens is already the protocol design.** That matches what you want at the top level.

Treasury remains **98%** unless you change code. Your airdrop/faucet/dev split should come **out of the 4B genesis wallet**, not the treasury (recommended), so:

| Bucket | % of total supply | FALCON | Source |
|--------|-------------------|--------|--------|
| **Community airdrop** | 1.0% | **2,000,000,000** | From genesis 4B |
| **Mainnet faucet** | 0.5% | **1,000,000,000** | From genesis 4B |
| **Development / ops** | 0.5% | **1,000,000,000** | From genesis 4B |
| **Protocol treasury** | 98.0% | **196,000,000,000** | On-chain, keyless |
| **Total** | 100% | **200B** | |

That is a **fair, clean split**: half of launch float to community via airdrop, a quarter each to faucet and development, majority locked in protocol rules. Recommend **time-locked or multi-sig** for the 0.5% dev wallet (not a hot key).

---

## 2. Every adjustable mainnet component (inventory)

### 2.1 Supply & genesis

| Knob | Current | Adjustable via | Launch decision |
|------|---------|----------------|-----------------|
| Total supply | 200B | Protocol constant / genesis | **Lock** unless hard fork |
| Genesis % | 2% (4B) | `kQXRP_GENESIS_ALLOCATION` | Keep 2% |
| Treasury % | 98% (196B) | `kQXRP_TREASURY_ALLOCATION` | Keep 98% |
| Genesis account seed | masterpassphrase (dev/test) | Ops secret management | **Mainnet: unique genesis + offline cold process** |
| Split of 4B (airdrop/faucet/dev) | Not coded (all “genesis”) | **Launch scripts + wallets** | **This spec §1** |

### 2.2 Epoch & emission (CID + PoPL)

| Knob | Current (production default) | Notes |
|------|------------------------------|--------|
| Ledgers per epoch | **172,800** (~7 days @ ~3.5s) | `kQXRP_LEDGERS_PER_EPOCH`; overridable with `-Dqxrp_epoch_override` |
| Epochs per year | **52** | `kQXRP_EPOCHS_PER_YEAR` |
| CID year-1 avg emission | **12% of treasury / year** (1200 bps) | `kQXRP_CID_YEAR1_AVG_BPS` |
| CID year-5 avg | **4.5% / year** (450 bps) | `kQXRP_CID_YEAR5_AVG_BPS` |
| CID yearly floor | **1.5% / year** (150 bps) | `kQXRP_CID_YEARLY_FLOOR_BPS` |
| Per-epoch floor | **3 bps** of treasury | `kQXRP_CID_EPOCH_FLOOR_BPS` |
| Decline curve | num **750** / den **10816** | linear per-epoch decline |
| LP share of emission | **+1% per active vault LP**, cap **50%** of epoch | **Today: vault LPs only** — **mainnet must also include AMM LPs** (see readiness audit §B) |
| Who is “LP” | Vault share MPT holders | **Must expand:** vault + AMM; claim-based pay to wallets, no pool top-ups |
| Min score to claim rewards | **5%** composite (500 bps) | `kMIN_COMPOSITE_SCORE_BPS` |
| Legacy halving constants | 50 bps initial / 208 epochs / 1 bps floor | marked unused; CID is live path |
| **First emission epoch** | **Today: epoch 1 emits** | **Mainnet: no pool until epoch ≥ 8** (see §3b) |

### 2.2b Bootstrap quiet period (mainnet)

| Item | Value | Rationale |
|------|--------|-----------|
| **First emitting epoch** | **8** | ~7 weeks at 7-day epochs for network bootstrap (UNL, bridges, faucet, airdrop scoring) without large treasury unlocks |
| Epochs 1–7 | `emissionBps = 0` / skip pool create or zero pool | Validators still bond/score; no ClaimReward emissions |
| Epoch 8+ | Normal `cidEmissionBps(epochNum)` | Or optionally shift schedule so “economic epoch 1” = ledger epoch 8 |

**Implementation (protocol):** in `applyRewardEpoch` / `cidEmissionBps`:

```text
if (epochNum < kQXRP_FIRST_EMISSION_EPOCH)  // = 8
    emissionBps = 0;  // or skip creating claimable pool
```

**Calendar (approx @ 7-day epochs):**

| Epoch | ~Time after genesis | Emission |
|-------|---------------------|----------|
| 1–7 | weeks 0–7 | **0** |
| 8 | ~week 8 | first CID emission |
| … | … | normal CID curve |

*If mainnet uses a shorter epoch override during bootstrap, redefine “epoch 8” as “after N ledgers / calendar days” so operators aren’t surprised.*

### 2.3 Fee burn / treasury refill

| Knob | Current | Notes |
|------|---------|--------|
| Burn floor | **40%** of fees | `kFEE_BURN_MIN_BPS` |
| Burn ceiling | **70%** | `kFEE_BURN_MAX_BPS` |
| Default burn | **55%** | `kFEE_BURN_DEFAULT_BPS` |
| Treasury fill sensitivity | 1000 bps | `kFEE_TREASURY_SENSITIVITY_BPS` |
| Usage sensitivity | 500 bps | `kFEE_USAGE_SENSITIVITY_BPS` |
| Governance of burn | supermajority vote | proposal type `kPROPOSAL_TYPE_BURN_BPS` |

### 2.4 Validators & bonding

| Knob | Current | Notes |
|------|---------|--------|
| Min bond | **1,000 FALCON** | `kQXRP_MIN_BOND_DROPS` (UI often asks ≥1,100 for reserve) |
| Unbond lock | **262,800 ledgers** (~30 days) | `kUNBONDING_LOCK_LEDGERS` |
| Score weights | Uptime 40 / Vote 30 / Latency 15 / Consistency 10 / Slash 5 | must sum 100 |
| Slash double-sign | **100%** of bond | |
| Slash absence | **25%** | detection partially disabled |
| Slash invalid vote | **50%** | detection partially disabled |
| Governance supermajority | **67%** of aggregate score | |
| Governance voting window | **172,800 ledgers** (~7 days) | |

### 2.5 Network identity

| Knob | Testnet today | Mainnet decision |
|------|---------------|------------------|
| Network ID | **1001** | Pick **≠ 1001** (e.g. 1025+ if you want NetworkID field required) |
| NetworkID in txs | **Must NOT include** at 1001 | Document signing rule carefully |
| Peer port default | 2459 | ops |
| Amendment majority time | 2 weeks | `kDEFAULT_AMENDMENT_MAJORITY_TIME` |

### 2.6 Lending (product economics — not supply)

| Knob | Testnet typical | Notes |
|------|-----------------|--------|
| Min HF at borrow | **1.5** (15000 bps) | permissionless |
| Liquidation HF | **1.1** (11000 bps) | |
| Interest rate example | **5% APR** (500 tenth-bps) | broker / LoanSet |
| Loan duration unit | **7-day epoch**, 1–52 | portal + `PaymentInterval` |
| Cover rates | bootstrap `COVER_RATE_*` | permissionless path less dependent |
| Liquidation recovery | FALCON to LP claim pool (v4) | no auto-sell |

### 2.7 Portal / ops (off-chain adjustable)

| Knob | Testnet | Mainnet |
|------|---------|---------|
| Faucet drip amount | often **2,000 FALCON** | fund from **1B faucet wallet**; drip size TBD |
| Faucet max claims / window | env `RATE_LIMIT_REQUESTS` default **5** / `RATE_LIMIT_WINDOW_SECONDS` default **3600** (sliding) | **Mainnet target: 5 / calendar day + 1h min spacing** (see §5.3) |
| RPC endpoints | coordinator public RPC | multi-endpoint + status page |
| Stablecoin model | testnet QUC / bridge-only mode for mainnet path | **bridge-only** stables recommended |
| Image pin | `qxrp/xrpld:lending-v5` | pin digest at launch |

---

## 3. Fairness of the proposed 2% / 1% / 0.5% / 0.5%

**Assessment: fair and coherent.**

| Pros | Watch-outs |
|------|------------|
| Aligns with existing 2% genesis / 98% treasury story | Dev 0.5% should be multi-sig + public schedule |
| Large community airdrop (2B) without touching treasury | Sybil can drain fairness if points are farmable |
| Faucet 1B is enough for years of onboarding if drip is modest | Faucet ≠ airdrop; keep separate wallets |
| No “foundation whale” beyond transparent 0.5% | Publish vesting if any dev unlock over time |

**Recommendation:** keep **treasury 98% untouched** for emissions; only **partition the 4B genesis** into three cold-to-hot wallets as above.

---

## 4. Genesis day procedure (wallets & scripts)

### 4.1 Wallets to create (offline ceremony)

| Wallet | Purpose | Balance after genesis |
|--------|---------|------------------------|
| `GENESIS` | Protocol genesis account (temporary) | 4B then drained to sub-wallets |
| `AIRDROP` | Community claim / batch release | 2B |
| `FAUCET` | Mainnet faucet hot/warm | 1B (or 1B cold + refill hot) |
| `DEV` | Development / infrastructure | 1B (multi-sig preferred) |
| `TREASURY` | Keyless protocol account | 196B (automatic) |

### 4.2 Release scripts (to build)

| Script | Responsibility |
|--------|----------------|
| `scripts/mainnet-genesis-split.py` | After genesis: pay 2B / 1B / 1B from GENESIS → AIRDROP / FAUCET / DEV |
| `scripts/airdrop-compute-scores.py` | Snapshot testnet metrics → score table → allocations |
| `scripts/airdrop-publish-manifest.py` | Publish signed CSV/JSON merkle root of entitlements |
| `scripts/airdrop-release-batch.py` | Batch `Payment`s from AIRDROP per claim window / merkle proof |
| `scripts/faucet-mainnet-fund.py` | Fund portal faucet hot wallet from FAUCET cold |
| `scripts/launch-guards.py` | Already exists — enforce bridge-only / no bootstrap mint |

### 4.3 Launch checklist (high level)

1. Freeze protocol constants; tag `mainnet-v1` image + digest  
2. Ceremony: genesis ledger, UNL, network id  
3. Split genesis 4B per §1  
4. Enable amendments in order (MPTokens, SAV, Lending, PoPL, …)  
5. Publish airdrop merkle root + tracker  
6. Open faucet with rate limits + anti-sybil  
7. Bridge-only stables deploy  
8. Public RPC + explorer + status  

---

## 5. Airdrop program design

### 5.1 Goals

- Reward **real testnet contribution**, not pure farming  
- Transparent **points → FALCON** mapping  
- **Anti-sybil** enough to stop bulk wallets  
- Simple **claim UX** + public **tracker page**

### 5.1b Contribution window (mainnet bootstrap)

| Item | Value |
|------|--------|
| **Start** | **Mainnet genesis** (ledger 1 / genesis close time) |
| **Duration** | **60 calendar days** |
| **End** | genesis_time + 60d (snapshot cutoff) |
| **What counts** | Activity **during this window only** — not pre-mainnet testnet history |
| **After day 60** | Freeze scores → publish allocations → claim/release |

This matches a **bootstrap mainnet**: network goes live, community earns airdrop by participating for two months, emissions stay off until epoch 8 (~aligned with that window if epochs are ~7 days).

### 5.2 Point categories (mainnet contribution window)

| Category | What to measure | Why fair | Data source |
|----------|-----------------|----------|-------------|
| **A. Validator operator** | Bonded on mainnet during window (time-weighted) | Highest cost signal | On-chain bond objects + UNL |
| **B. Validator setup complete** | Bond + linked payout / portal link during window | “Did the work” | Bond txs + portal |
| **C. DEX LP (not lend)** | Time-weighted AMM LP ownership in window | Trading liquidity | Daily AMM LP snapshots |
| **D. Faucet engagement** | **Days active + daily claim intensity** (see §5.3) | Rewards logging in repeatedly | Faucet success logs |
| **E. Lend** | **Exclude** (or tiny weight) | Focus on DEX LP | — |

**Recommended base weights (tunable):**

| Category | Weight of airdrop pool | Cap per identity |
|----------|------------------------|------------------|
| Validator bonded (time-weighted) | **40%** | 1 identity / operator |
| Validator setup complete | **10%** | binary once |
| DEX LP (time-weighted TVL share) | **35%** | soft cap e.g. 2% of LP bucket |
| Faucet engagement (days × daily intensity) | **10%** | see caps below |
| Residual / buffer | **5%** | sybil / corrections |

Total airdrop = **2B FALCON**.

### 5.3 Faucet limits + scoring (not “max 5 claims ever”)

**Problem with “max 5 claims score”:** the faucet already allows **5 successful drips per rate-limit window** (today often 5 / hour via env). Almost everyone hits 5 quickly; that does **not** distinguish daily returners.

**Mainnet faucet policy (product):**

| Rule | Value | Purpose |
|------|--------|---------|
| **Max successful claims / UTC day** | **5** | Daily ceiling |
| **Min spacing between claims** | **1 hour** | Forces real return visits for max daily score |
| **Max theoretical claims / day** | 5 (with ≥1h gaps) | ~spread over ≥4 hours |
| **Max theoretical claims / 60-day window** | **5 × 60 = 300** | Engagement ceiling |

**Logging (required for airdrop):** each successful faucet drip stores:

```text
{ address, ts_utc, day_utc, ip_hash, device_hash, tx_hash, amount }
```

**Faucet score (recommended):**

```text
# Per UTC day d:
claims_d = number of successful drips that day (0..5)
day_intensity_d = claims_d / 5          # 0..1  (5 claims that day = full intensity)

# Over the 60-day window:
active_days = count of days with claims_d >= 1
intensity_sum = Σ day_intensity_d       # 0..60 if perfect every day

# Composite (tunable weights):
score_faucet =
    0.6 × (active_days / 60)            # showing up most days
  + 0.4 × (intensity_sum / 60)          # filling the 5/day when present

# Optional streak bonus (small):
streak_bonus = min(max_consecutive_active_days / 60, 1) × 0.1
score_faucet = min(1.0, score_faucet + streak_bonus)
```

**Interpretation:**

| Behavior | Approx faucet score |
|----------|---------------------|
| Claim 5 times on day 1, never return | ~low (1/60 active day) |
| Claim once a day for 60 days | high on “active_days”, medium intensity |
| Claim 5×/day every day (1h cooldowns) | **maximum** faucet score |
| Bot that burns 5 slots in 5 minutes then sleeps | low (1 day only) |

**Implementation notes (portal):**

- Today: `RATE_LIMIT_REQUESTS=5`, `RATE_LIMIT_WINDOW_SECONDS=3600` (sliding 1h window) — that is **5 per hour**, not 5 per day.  
- Mainnet needs **two limits**:
  1. **Cooldown limit:** 5 / UTC day (address + optional IP)  
  2. **Cooldown spacing:** 1 hour since last **successful** claim for that address  
- Log every success into durable store (not only Redis counters) for the airdrop pipeline.

### 5.3b Other scoring formulas (sketch)

**Validator score**

```
score_val = bond_epochs_in_window × quality_mult
quality_mult = f(uptime, validations) ∈ [0.5, 1.5]
```

Only one airdrop identity per bonded validator account (and optional payout address link).

**DEX LP score**

```
For each daily snapshot t in [genesis, genesis+60d]:
  share_i(t) = LP_tokens_i(t) / LP_tokens_total(t)
score_lp_i = Σ_t share_i(t)   # time-weighted ownership over window
```

### 5.4 Allocation formula

```
points_i = Σ (normalized category scores × category weight)
allocation_i = 2B × points_i / Σ points
```

Publish:

- raw metrics  
- points  
- FALCON entitlement  
- merkle proof leaf  

### 5.5 Claim / release mechanism

**Option A — Merkle claim (recommended)**  
1. Off-chain compute + publish merkle root on website + optional on-chain memo  
2. User proves entitlement; portal or batch script pays from AIRDROP wallet  
3. One claim per identity  

**Option B — Fully scripted airdrop**  
Batch payments over N days (no user claim). Simpler ops, worse UX for unclaimed dust.

**Vesting (optional):** e.g. 50% at T+0, 50% linear over 90 days — reduces dump risk; increases product work.

---

## 6. Airdrop tracker page (product)

### 6.1 Routes

| Route | Purpose |
|-------|---------|
| `/airdrop` | Program overview, rules, timelines, pool sizes |
| `/airdrop/leaderboard` | Ranked identities (pseudonymous addresses) |
| `/airdrop/me` | Connected wallet: metrics, points, entitlement, claim status |
| `/airdrop/docs` | Methodology, anti-sybil, appeals |

### 6.2 UI modules

1. **Pool summary:** 2B total · allocated · claimed · remaining  
2. **Category breakdown** for selected address  
3. **Validator card:** bonded? epochs? score  
4. **DEX LP card:** pools, time-weighted share  
5. **Faucet card:** claim count (capped display)  
6. **Allocation result:** FALCON amount + % of airdrop  
7. **Claim CTA** (when open)  
8. **Export CSV** of public allocations (transparency)

### 6.3 Backend

| Component | Role |
|-----------|------|
| `airdrop_snapshots` DB | Daily on-chain scrapes (bonds, AMM LP) |
| `airdrop_faucet_events` | From faucet API logs |
| `airdrop_scores` | Computed scores per address |
| `airdrop_allocations` | Final FALCON amounts + merkle indices |
| `GET /api/airdrop/overview` | Totals |
| `GET /api/airdrop/address?address=` | Per-wallet detail |
| `POST /api/airdrop/claim` | Claim (when live) |

### 6.4 Release scripts ↔ tracker

```
compute scores → write allocations table → publish merkle root
     ↓
tracker reads allocations (read-only)
     ↓
claim / batch release updates claim_status
```

---

## 7. Anti-sybil (basic but real)

### 7.1 Hard rules

| Rule | Detail |
|------|--------|
| **One airdrop identity** | Merge validator payout address + bonded account + passkey wallet when linked in portal |
| **Validator uniqueness** | One bonded validator key → one airdrop recipient |
| **LP soft cap** | No single address > X% of LP category (e.g. 2–5%) |
| **Faucet limits** | **5 claims / UTC day** + **1h cooldown**; score by **days + intensity**, not total-5 cap |
| **Window** | Only activity in **[genesis, genesis+60d]** counts |
| **Minimum activity cost** | e.g. bond ≥ 1000 FALCON **or** non-trivial fee burn / LP notional |

### 7.2 Soft signals (down-weight, don’t ban alone)

- Many accounts funded only from faucet in a tight time window  
- Identical behavior graphs  
- LP opened minutes before snapshot and closed after  

### 7.3 Optional stronger checks (phase 2)

- GitHub / Discord account age (privacy tradeoff)  
- Human challenge (captcha) for claim only  
- Small mainnet “activation fee” burned on claim  

### 7.4 Appeals

Publish a short appeals window for false positives (validator operators mis-linked addresses).

---

## 8. Data collection plan (starts at mainnet genesis)

| Signal | When | Retention |
|--------|------|-----------|
| Daily bond / validator list | **From genesis, 60 days** | Until airdrop paid |
| Daily AMM LP ownership (FALCON pairs) | **From genesis, 60 days** | Until airdrop paid |
| Faucet success events (address, day, ts) | **From genesis** | Until airdrop paid; hash IPs |
| Portal “validator linked” | Optional from genesis | |
| Pre-mainnet testnet history | **Not used for airdrop points** | Optional analytics only |
| Lend positions | **Exclude** | — |

**Pipeline goes live on genesis day** — scrapers + faucet logging must be ready **before** mainnet starts.

---

## 9. Suggested launch timeline (bootstrap mainnet)

| Phase | Work | Timing |
|-------|------|--------|
| **P0 — Spec freeze** | This document + emission delay PR (`FIRST_EMISSION_EPOCH=8`) | Now |
| **P1 — Launch readiness** | Genesis split scripts, faucet 5/day+1h cooldown, logging schema | Days |
| **P2 — Tracker UI** | `/airdrop` live from day 0 (scores update daily) | Days |
| **P3 — Genesis ceremony** | Mainnet start; split 4B → 2B / 1B / 1B | Launch |
| **P4 — Contribution window** | Days 0–60: bond, LP, faucet engagement | 60 days |
| **P5 — First emission** | Epoch **8** (~week 8 if 7-day epochs) | Aligned with window |
| **P6 — Score freeze + claim** | Day 60+: publish merkle / claim | After window |
| **P7 — Steady state** | Normal CID emissions, faucet continues from 1B bucket | Ongoing |

---

## 10. Open decisions (remaining)

1. **Vesting** on airdrop and/or dev wallet?  
2. **Claim UX** vs automatic batch send after day 60?  
3. **Mainnet network id** value  
4. **Dev wallet** multi-sig + public schedule  
5. **Exact epoch length** on mainnet (keep 172,800 or temporary shorter bootstrap epochs)?  
6. **FIRST_EMISSION_EPOCH = 8** vs calendar “no emissions for 56 days” if epoch length changes  
7. **Whether to retag** Docker `latest` to launch image  

**Closed in this revision:**

- ✅ Airdrop window = **60 days from mainnet genesis** (not long testnet farming)  
- ✅ Faucet score = **active days + daily intensity**, not max-5 lifetime  
- ✅ Faucet policy = **5/day + 1h cooldown**  
- ✅ DEX LP yes, lend excluded (default)  
- ✅ Bootstrap mainnet soon; first emission at **epoch 8**  

---

## 11. Fairness summary (your proposal)

| Your idea | Spec position |
|-----------|----------------|
| Genesis 2% = 4B | ✅ Already in protocol |
| 1% community airdrop | ✅ 2B from genesis float |
| 0.5% faucet | ✅ 1B |
| 0.5% development | ✅ 1B, multi-sig recommended |
| Track validators + setup | ✅ Primary weight |
| Track DEX LP not lend | ✅ Primary liquidity weight |
| Faucet: daily login engagement | ✅ Days + intensity; 5/day + 1h cooldown |
| 60-day window from mainnet genesis | ✅ |
| Emissions after bootstrap | ✅ First emission **epoch 8** |
| Anti-sybil | ✅ Identity merge + caps + cost signals |

---

## 12. Next implementation tickets (when you greenlight)

1. Snapshot jobs: validators + AMM LP  
2. Score engine + dry-run CSV  
3. Portal `/airdrop` tracker  
4. Genesis split script + checklist  
5. Claim/release path  
6. Mainnet faucet funding from FAUCET wallet  

---

*This document is the working mainnet launch spec. Protocol constants remain authoritative until a freeze PR updates `QXRPConstants.h` and related ops scripts.*
