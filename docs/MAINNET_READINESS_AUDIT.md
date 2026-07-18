# Mainnet readiness audit — work to date + remaining gaps

**Date:** 2026-07-18  
**Purpose:** Single checklist of everything done today vs what **must** still land before mainnet bootstrap release.

---

## A. Today’s work — status matrix

### Protocol (`qXRP` / FalconLedger `develop`)

| Item | Commit | In GitHub | In **fleet image** | Mainnet ready? |
|------|--------|-----------|--------------------|----------------|
| Lending v3: forfeit collateral, no liquidator grab | `2ba718c68`… | ✅ | ✅ `lending-v4` base | ✅ |
| LP FALCON claim pool + `VaultClaimCollateral` | `13f5ae131` + compile fix `3b81ab7b2` | ✅ | ✅ **fleet on this** | ✅ (testnet proven path) |
| CollateralSurplus bookkeeping | earlier | ✅ | Partially superseded by claim pool | Docs aligned |
| E2E funding / surplus script fixes | `3920bbc19` | ✅ | scripts on coord if pulled | ✅ |
| **Mainnet launch spec** | `13fb4c124` | ✅ | n/a | Spec only |
| **First emission epoch = 8** | `1a482f6b8` | ✅ | ❌ **NOT in fleet** (still at `3b81ab7b2`) | ⚠️ **Must rebuild image** |
| Genesis split script 4B→2B/1B/1B | `a123b961f` | ✅ | n/a | ✅ script; needs ceremony keys |
| Airdrop batch pay script | `a123b961f` | ✅ | n/a | ✅ after freeze |

**Fleet reality (coordinator):** `qxrp/xrpld:lending-v4` @ **`3b81ab7b2`**  
→ Has LP claim collateral path.  
→ Does **not** yet have epoch-8 emission quiet period.

**Docker Hub:** `qxrp/xrpld:lending-v4` public (digest `3705e3d691d3…`).

---

### Portal (`Falcon-faucet-wallet` `main`)

| Item | Commit | Deployed? | Mainnet ready? |
|------|--------|-----------|----------------|
| Per-loan Positions cards | `13626e2` | likely | ✅ |
| Liquidation claim UI + sign | `d4e8954` | yes if latest | ⚠️ needs `lending-v4` chain |
| Repay preflight NetworkID fix | `d4e4759` / `ae13d41` | verified 200 OK | ✅ |
| Faucet **5/day + 1h cooldown** + claim log | `76802d2` | after Vercel | ⚠️ needs Neon schema |
| Airdrop page + overview/me APIs | `76802d2` | after Vercel | Scaffold only |
| Snapshot + freeze + recompute | `2caab6a` | after Vercel | Scaffold; DEX LP empty |
| Header **Airdrop** nav | `2caab6a` | after Vercel | ✅ |

---

## B. Critical product gap (you just confirmed)

### Emissions “LP” share ≠ AMM + lend

| Intended (product) | Code today |
|--------------------|------------|
| Pay **AMM/DEX LPs** | ❌ Not included |
| Pay **lend vault LPs** | ✅ `ClaimLPReward` by vault share MPT |
| Pay **validators** | ✅ `ClaimReward` |
| Top up collateral pool with emissions | ❌ (correct — collateral is liquidation, not emission) |
| Top up AMM with emissions | ❌ (should not auto-inject into AMM) |
| Manual claim | ✅ pull from treasury |

**Required for mainnet (design decision locked in conversation):**

```text
Epoch emission (epoch ≥ 8)
  ├─ Validators     — ClaimReward (score-weighted)
  ├─ Lend vault LPs — ClaimLPReward (vault share %)
  └─ AMM / DEX LPs  — NEW claim path (AMM LP token %)
```

- Basket size: grow with participation, **cap % of emission** (e.g. combined LP ≤ 50%), **not** “only 50 wallets.”
- Payment: **direct to wallets via claim**, not pool top-ups.
- Collateral pool stays for **liquidation FALCON claims only**.

**Status:** Spec gap → **protocol work not started** (blocker for “emissions fair to liquidity”).

---

## C. Must-have before mainnet bootstrap

### P0 — Blockers

| # | Work | Owner | Status |
|---|------|--------|--------|
| 1 | **Rebuild + roll fleet** image including `1a482f6b8` (epoch 8 quiet) | Ops | ❌ |
| 2 | **Emission redesign:** vault LP + AMM LP + validators; claim txs | Protocol | ❌ |
| 3 | Portal claim UI for all three reward types | Portal | Partial (vault only) |
| 4 | Mainnet **network id**, genesis keys, UNL ceremony | Ops | ❌ |
| 5 | Genesis **split ceremony** (script exists) + multi-sig DEV | Ops | Script ✅ |
| 6 | Bridge-only stables + no bootstrap mint guards | Ops/scripts | Partial |
| 7 | Neon: run `docs/sql/airdrop-schema.sql` | Ops | ❌ |
| 8 | Faucet mainnet fund from 1B bucket; env limits | Ops | Partial (code ✅) |
| 9 | Pin public image (`latest`/`falcon` retag?) + install one-liner uses launch tag | Portal/docs | ❌ defaults still `cid-popl` |
| 10 | Security freeze + audit of new lending/claim code | Security | ❌ |

### P1 — Airdrop (runs **during** 60 days post-genesis)

| # | Work | Status |
|---|------|--------|
| 11 | Daily snapshot cron (validators list complete) | Scaffold |
| 12 | **DEX LP holder scan** (ledger walk / holder index) | ❌ empty stub |
| 13 | Score freeze + CSV export | Freeze API ✅; CSV export ❌ |
| 14 | Merkle claim UX (optional vs batch pay) | Batch script ✅; merkle ❌ |
| 15 | Anti-sybil identity merge (payout ↔ bond ↔ passkey) | ❌ |

### P2 — Hardening / polish

| # | Work | Status |
|---|------|--------|
| 16 | Lending docs fully match two-pool + claim model | Partial |
| 17 | Installer default image = launch tag | ❌ |
| 18 | Explorer links, status page, multi-RPC | Partial |
| 19 | E2E suite on image that includes epoch-8 + AMM claim | Partial |
| 20 | Governance freeze of burn/emission params for launch | Ops |

---

## D. What is already “good enough” for bootstrap day

These can ship **if** you accept bootstrap with limited rewards UI:

| Area | OK for day 0? | Note |
|------|---------------|------|
| Supply 200B / 2% / 98% | ✅ | In constants |
| Epoch length 7d | ✅ | |
| No emission epochs 1–7 | ⚠️ | Code in git; **needs new image** |
| Lending borrow/repay/liquidate/claim collateral | ✅ | On `lending-v4` fleet |
| Faucet rate limits for airdrop scoring | ✅ | Code; need DB |
| Airdrop tracker page | ⚠️ | Shows rules + faucet days only until snapshots |
| Full AMM+vault emission split | ❌ | **Do before advertising PoPL LP rewards** |

**Recommendation:**  
- **Bootstrap mainnet** can start network, faucet, bond, DEX, lend, airdrop **scoring** with emissions quiet.  
- **Do not market “LP emissions”** until AMM+vault claim redesign ships.  
- Or delay mainnet until P0 items 1–3 complete.

---

## E. Suggested build order to finish “mainnet ready”

```text
1. Protocol: emission split (validator / vault LP / AMM LP) + constants
2. Protocol: ClaimAmmLpReward (or generalized claim) + epoch snapshot fields
3. Rebuild image (epoch-8 + emission split + lending claims) → Hub + fleet
4. Portal: unified Claim rewards UI
5. Ops: Neon schema, AIRDROP_ADMIN_TOKEN, cron snapshot
6. Ops: genesis ceremony checklist + split dry-run
7. Installer: QXRP_XRPLD_IMAGE default = launch tag
8. Freeze params + public launch notes
```

---

## F. Quick answers to open product questions

| Question | Decision for mainnet |
|----------|----------------------|
| Top up collateral pool on emission day? | **No** — liquidation custody only |
| Top up AMM with emissions? | **No** — dilutes/skews pool; pay wallets |
| Pay wallets? | **Yes** via **manual claim** |
| Cap 50 providers = only 50 paid? | **No** — cap is **% of emission**; all holders split by share |
| Who should get LP emissions? | **Vault LPs + AMM LPs** (code today: vault only) |

---

## G. Git / deploy hygiene

| Repo | Branch | Latest relevant tip |
|------|--------|---------------------|
| Protocol | `develop` | `a123b961f` scripts; emission quiet `1a482f6b8` |
| Portal | `main` | `2caab6a` airdrop + faucet |
| Fleet | image | **behind** `develop` by epoch-8 commit |

Uncommitted local junk on protocol (not part of launch): rolling-upgrade, lend-hf-monitor, etc. — leave or clean separately.

---

*This audit is the gate document. Next engineering focus: emission redesign (AMM + vault) + rebuild/roll image including `FIRST_EMISSION_EPOCH=8`.*
