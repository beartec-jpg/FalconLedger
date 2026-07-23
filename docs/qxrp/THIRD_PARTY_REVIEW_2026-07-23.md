# Independent Third-Party Review  
## Falcon Ledger Protocol + Web Portal  
**(Rewritten after public wording update — honest 2% bootstrap)**

| | |
|---|---|
| **Repos** | [beartec-jpg/FalconLedger](https://github.com/beartec-jpg/FalconLedger) · [beartec-jpg/Falcon-faucet-wallet](https://github.com/beartec-jpg/Falcon-faucet-wallet) |
| **Review date** | 2026-07-23 |
| **Protocol tip** | `develop` @ `50da94c05` (docs wording commit) |
| **Portal tip** | `main` @ `d02addf` |
| **Classification** | Testnet / pre-mainnet — architecture, tokenomics, security, product surface |
| **Live product** | [falcon-ledger.com](https://falcon-ledger.com) · Testnet network ID **1001** · Planned mainnet **1026** |

---

## 1. Executive verdict

| Layer | Maturity | Summary |
|-------|----------|---------|
| **Protocol (FalconLedger)** | Strong for pre-mainnet | XRPL fork + real PQ/economics delta; freeze pin `mainnet-v2` in progress |
| **Tokenomics design** | Coherent and now **honestly documented** | Fixed 200B; 98% keyless treasury; public 2% bootstrap |
| **Cryptography** | Strong | Falcon-512 for accounts; consensus proposal path Falcon-aware in current tree |
| **Portal** | Good testnet product | Full DeFi surface; non-custodial wallet; API hardening |
| **Mainnet readiness** | Not live | Soak, ceremony, mainnet bridge lock still open |
| **Trust model** | Clearer after wording update | Founder custody of 2% is stated; bulk supply is protocol |

**Bottom line:** Falcon Ledger is a **credible post-quantum XRPL-class network** with a working testnet portal. After the wording update, public claims match the real model: **98% cannot be withdrawn by a company key**; **2% is a public founder-held bootstrap** (airdrop / faucet / builder pay) — not “zero human control of everything.” Suitable for **technical evaluation and community play** on testnet; real-value mainnet still needs soak, ceremony discipline, and careful bridge/issuer ops.

---

## 2. What these repos are

### FalconLedger (protocol)
- Quantum-resistant fork of XRPLF/`rippled`
- Native ticker **qXRP** (UI often **FALCON**)
- RPCA consensus retained
- Custom layer: keyless treasury, CID emission, bonding, fluid scoring, ClaimReward, double-sign slash, bounded governance, Account Names, PoPL LP rewards, vault/lending, Sepolia-oriented USDC lock contract

### Falcon-faucet-wallet (product)
- Next.js portal: faucet, passkey Falcon wallet, swap, pool, lend, bridge, explorer, rewards, board, arcade, whitepaper
- Client-side Falcon-512 signing (WASM); server holds faucet secrets only for drip paths

---

## 3. Tokenomics (updated public model)

### 3.1 Supply (protocol-enforced)

| Parameter | Value |
|-----------|------:|
| Total supply | **200,000,000,000** qXRP |
| Protocol treasury | **196B (98%)** — no private key |
| Genesis circulating | **4B (2%)** — human-held bootstrap keys |

### 3.2 Public split of the 2% (4B)

| Bucket | Amount | % of total | Purpose |
|--------|-------:|-----------:|---------|
| **Community airdrop** | 2,000,000,000 | 1.0% | Community / mainnet contributors |
| **Mainnet faucet** | 1,000,000,000 | 0.5% | Free claims, onboarding |
| **Builder pot** | 1,000,000,000 | 0.5% | Payment for core work + helpers (code, audits, outreach) |
| **Treasury** | 196,000,000,000 | 98.0% | Epoch emission only |

**Third-party fairness read:** This is a **reasonable, community-heavy** launch float. Half of the 2% goes to airdrop; a quarter each to faucet and builder. Getting paid from the 1B builder pot for building the chain — and paying contributors/auditors from the same pot — is **normal and defensible** when published.

### 3.3 Custody of the 2% (founder-controlled by design)

Docs now state intentionally:

- Airdrop / faucet / builder wallets are **founder-controlled** (single-operator cold/offline preferred).
- Rationale: **skin in the game**; multi-sig with untrusted co-signers can collude and seize the launch float.
- This is **not** the same as a company escrowing ~40% of supply with monthly unlocks.
- **Mitigation for transparency:** publish ceremony addresses so the split is auditable on-chain.

**Reviewer position:** Founder custody of a **capped, disclosed 2%** is a valid operational choice. It does not remove the technical fact that those keys can move funds — it **discloses** that fact instead of hiding it behind “no dumps / no company control” absolute language. That is an improvement.

### 3.4 Emission (CID + PoPL)

- Epoch ~172,800 ledgers (~7 days)
- Quiet period: epochs 1–7 zero claimable emission; first unlock **epoch 8**
- Year-1 avg ~12% of remaining treasury; long-term floor ~1.5%/yr
- PoPL split: vault LPs + AMM LPs (caps 25% each) + validators remainder
- Validators: **all bonded** paid ∝ composite score (ActiveSet K=32 cut removed)
- Fee burn 40–70% dynamic; min bond 1,000 qXRP; unbond ~30 days

### 3.5 What “no company escrow of bulk supply” correctly means

| Claim | Accurate? |
|-------|-----------|
| 98% cannot be drained by a company private key | **Yes** |
| No Ripple-style monthly escrow unlock of bulk supply | **Yes** |
| Ongoing emissions go by protocol rules to participants | **Yes** |
| Nobody can ever sell any tokens | **No** — 2% and later claimable emissions can move |
| Zero human power at launch | **No** — founder holds 2% keys by design |

---

## 4. Protocol functions

| Area | Status |
|------|--------|
| Falcon tx + validation signing | Live / core |
| Consensus proposal Falcon verify | Fixed in tree post earlier audit gap (verify on freeze pin) |
| Validator bond / claim / slash (double-sign) | Implemented |
| CID emission + PoPL LP claims | Implemented |
| Account Names | Implemented |
| AMM / DEX / lending vaults | Exercised on test/rehearsal |
| On-chain governance (burn BPS) | Narrow but live |
| Open/rotating UNL | Future amendment; bootstrap UNL now |
| EVM bridge multi-sig lock | Code + Sepolia e2e; ETH mainnet not deployed |

---

## 5. Security summary

### Protocol strengths
- Amendment-gated economic features
- Integer-only emission/score math (`muldiv64`)
- Drop conservation invariant
- ClaimReward: owner-only, min score, no double-claim, pool hard-cap
- Double-sign slash with cryptographic STValidation evidence; bond burned (no slasher bounty)
- liboqs pinned; PQ key types with length/prefix checks
- SECURITY.md + prior remediation work

### Protocol residual risks
| Topic | Severity | Note |
|-------|----------|------|
| Bootstrap UNL | Medium | Consensus trust still operator-set until open-UNL |
| Founder 2% keys | Accepted / disclosed | Ops + transparency (publish addresses, cold storage) |
| Bridge / F-USDC issuer | Medium–High for mainnet | Mint/release still ops trust; multi-sig for **other people’s USDC** is different from genesis float |
| Full ASAN/coverage campaigns | Medium | Ongoing assurance gap |
| Only DOUBLE_SIGN slash live | Low | Intentional staging |
| GitHub code scanning | Info | Not enabled on either repo via API |

### Portal strengths
- Non-custodial client-side Falcon signing
- Fail-closed propose/sign/derive endpoints
- Faucet rate limits; arcade claim uses server score only
- Production HTTPS enforcement for RPC paths
- Prior remediation of High swap/pool/bridge UX issues (in-repo reports)

### Portal residual risks
- Passkey PRF fallback; CSP `unsafe-inline` residual
- Faucet/game farming (limits, not identity)
- Server faucet secret = high-value target
- Bridge relay still a liveness/value trust anchor for stables

---

## 6. Architecture (trust layers)

```
98% TREASURY  ── protocol only (keyless)
2% BOOTSTRAP  ── founder keys: AIRDROP | FAUCET | BUILDER  (disclosed)
PORTAL        ── non-custodial user keys; server faucet drip
F-USDC/BRIDGE ── issuer + relay + (mainnet) lock multi-sig for USDC
UNL           ── bootstrap list until open-UNL amendment
```

These layers are **different**. Confusing “keyless treasury” with “trustless bridge” or “trustless UNL” is how overclaims happen. Current docs largely avoid that after the wording update.

---

## 7. Mainnet readiness (still open)

From project readiness materials (not a launch announcement):

1. Multi-day freeze-pin soak  
2. Genesis ceremony: split 4B → 2B/1B/1B, publish addresses  
3. ETH **mainnet** lock with cold multi-sig for **USDC custody** (user funds)  
4. Fund mainnet faucet from FAUCET wallet; portal `LIVE=false` until green  
5. Optional external freeze-scope audit  
6. Flip mainnet portal only after consensus + split are solid  

Recent scoring/ClaimReward fixes on `mainnet-v2` show active pre-launch debugging — positive engineering signal.

---

## 8. Updated third-party position on “risks”

### Genesis 2% — reassessed after disclosure

| Earlier concern | After honest wording + 2/1/1 plan |
|-----------------|-----------------------------------|
| Vague “small portion” genesis | **Fixed** — public 2B/1B/1B |
| “No dumps / no company” overclaim | **Fixed** — bulk vs bootstrap separated |
| Multi-sig required for fairness | **Not required** — founder custody is valid if disclosed |
| Builder pay unethical | **No** — 1B builder pot for work + helpers is fair |

**Remaining advice (ops hygiene, not ideology):**
- Keep DEV/AIRDROP **cold**; faucet warm only for drip refills  
- Publish addresses at T0  
- Airdrop rules + unclaimed policy public  
- Don’t mix builder keys with bridge owner keys  

### Bridge / issuer — still separate

Founder custody of **your** 2% does not secure **users’ bridged USDC**. For mainnet stables, multi-sig / cold owners on the **lock contract** remains best practice because that is **other people’s collateral**, not your builder compensation.

---

## 9. Scores (subjective, pre-mainnet)

| Dimension | Score (1–10) | Comment |
|-----------|:------------:|---------|
| Vision / differentiation | 8.5 | PQ + paid validators + keyless bulk treasury |
| Tokenomics design | **8.0** | ↑ honesty of 2% improves third-party read |
| Protocol engineering | 8.0 | Strong XRPL-grade patterns |
| Cryptography | 8.0–8.5 | Falcon real; pin-verify consensus path |
| Economic security | 6.5 | Double-sign only; bootstrap UNL |
| Bridge security | 6.0 | Model good; mainnet custody TBD |
| Portal security | 7.0 | Solid model; residual web crypto/CSP |
| Documentation honesty | **8.5** | ↑ after wording update |
| Decentralization today | 4.5–5 | Testnet / bootstrap UNL expected |
| Mainnet readiness | 5.5 | Close operationally; not go-live |
| **Composite** | **~7.4** | Credible; clearer trust story |

---

## 10. Recommendations (priority)

1. **Ceremony publish:** AIRDROP / FAUCET / DEV addresses + balances after split  
2. **Cold storage** for builder + airdrop; rate-limited faucet  
3. **Bridge mainnet:** multi-sig cold for **USDC lock only** (not “give away your chain”)  
4. **External freeze audit** before real capital  
5. **Open-UNL roadmap** with timeline  
6. Portal: tighten CSP, PRF-or-block weak passkey path  
7. Enable GitHub secret scanning / Dependabot  

---

## 11. Closing

**Falcon Ledger** is a serious, actively developed post-quantum XRPL fork with fixed supply, a keyless 98% treasury, and a small **public** 2% bootstrap: **2B airdrop, 1B faucet, 1B builder pot**. Founder custody of that 2% is **documented and intentional** — compensation and skin in the game, not multi-sig theater with untrusted parties.

As a third party: **testnet is suitable for technical evaluation and community play.** Marketing that says **no company escrow of bulk supply** is accurate. Marketing that implied **no human control anywhere** was overstated; current docs fix that. Bridge and F-USDC issuer remain **separate** trust surfaces for mainnet user funds.

**Same speed heritage. Better economics. Quantum-first. 98% protocol treasury. Honest 2% bootstrap.**

---

_Reviewer note: This is an independent architecture/security-style review based on direct repository access, not a formal penetration test or guarantee of fitness for real-value mainnet._
