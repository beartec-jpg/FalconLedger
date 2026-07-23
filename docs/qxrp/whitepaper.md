# Falcon Ledger White Paper

**Version 2.7 — July 2026**

---

<div align="center">

![Falcon Ledger](https://q-xrp-faucet.vercel.app/icon-512.png)

_Falcon Ledger — Quantum-Resistant. Validator-Rewarding. 98% Protocol Treasury. Honest Bootstrap._

</div>

---

> **Naming.** The chain is **Falcon Ledger** — named after the **Falcon** post-quantum
> signature scheme it uses as standard, and after its heritage as a fork of the XRP
> Ledger. The native token keeps the ticker **qXRP**. In this document, "Falcon Ledger"
> means the network/protocol; "qXRP" means the token (balances, fees, rewards, supply).

---

## Executive Summary

Falcon Ledger is a quantum-resistant payment protocol forked from the XRP Ledger.
It keeps RPCA consensus and sub-second finality and replaces everything that made
XRP a compromised bet: the company-controlled supply, the zero validator incentives,
the company-gated grants and governance, and the classical cryptography that a
quantum computer will eventually break.

**Falcon signatures, everywhere, from genesis.** Falcon-512 lattice signatures are the
standard signature scheme for validator identities and all transactions, built in at
the protocol level from genesis — not retrofitted later. Every wallet is created with
Falcon keys, every transaction is signed and verified with Falcon. This chain is built
to be secure in 2026 and in 2046.

**Fixed supply. 200 billion qXRP. Hard cap. No exceptions.**
**98%** of the supply is locked in a **protocol treasury with no private key**.
It is released only by on-chain consensus rules — one epoch at a time, according
to a continuous declining emission schedule (CID), shared between validators and
lending/AMM liquidity providers under Proof of Participation. Validators get
paid for doing the work. Fees burn. The supply shrinks. There is **no
company-style escrow of tens of billions** and **no monthly foundation unlock**
of the bulk supply.

**Honest 2% bootstrap.** The remaining **4B (2%)** is a **public, fixed launch
float** — not a hidden premine and not a discretionary treasury grant program:

| Bucket | qXRP | Share of total | Role |
| ------ | ---: | -------------: | ---- |
| Community airdrop | 2,000,000,000 | 1.0% | Community / mainnet contributors |
| Mainnet faucet | 1,000,000,000 | 0.5% | Free claims for onboarding |
| Builder pot | 1,000,000,000 | 0.5% | Pay for core work + helpers (code, audits, outreach) |

Those three wallets are **founder-controlled by design**. The builder has skin in
the game and will not hand multi-sig control of the launch float to untrusted
third parties who could collude and seize it. That is **custody of a known 2%**,
not “company control of half the chain.” The **98% treasury remains keyless**.

**No exchange required.** Falcon Ledger ships with a built-in DEX and AMM. The
launch target is an in-wallet experience — faucet, wallet, and swaps — that lets a
validator convert qXRP rewards to **USDC** and **USDT** on-chain from day one of
mainnet, with no centralized exchange in the loop.

**Human names, still self-custody.** Optional on-ledger account names map a
readable handle (e.g. `alice.bob`) to an `r…` address with a 100 qXRP bond —
payments always settle to the cryptographic address.

---

## 1. The Problem

### 1.1 XRP Is Controlled by a Company

Ripple, Inc. created 100 billion XRP at genesis and retained roughly 60% of
the supply. Through a series of escrow agreements, Ripple releases up to 1
billion XRP per month into the market. The releases are real. The selling
pressure is real. Holders of XRP are permanently exposed to a counterparty
that has a nine-figure inventory of the same token they are holding.

This is not decentralization. It is a public company with a vesting schedule
that happens to run on a fast ledger.

### 1.2 Validators Have No Reason to Participate

Running an XRP Ledger validator costs money — hardware, bandwidth, operations.
The protocol pays validators nothing. The incentive to run a validator is
reputational at best, and non-existent at worst. The result is a validator
set that skews heavily toward Ripple-affiliated entities and large institutions
with indirect business reasons to keep the network running.

A network where most validators serve at the pleasure of one company is not
a decentralized network.

### 1.3 Classical Cryptography Has an Expiry Date

ECDSA and ed25519 are secure today because factoring large integers and
solving the discrete logarithm problem is computationally infeasible on
classical hardware. Shor's algorithm solves both problems in polynomial time
on a sufficiently large quantum computer.

NIST finalized its post-quantum cryptography standards in 2024. The timeline
for cryptographically relevant quantum computers is measured in years, not
decades. Financial infrastructure built today needs to be secure for 20 to
40 years. A payment chain that waits until quantum computers arrive to begin
a migration will not survive it.

### 1.4 No On-Chain Governance, and Company-Gated Grants

Protocol changes on the XRP Ledger happen through Ripple's amendment process.
Validators vote, but the proposals originate from and are largely controlled
by Ripple. There is no on-chain mechanism for bonded participants to propose,
debate, and ratify changes to economic parameters without relying on off-chain
social coordination and a company's goodwill. Ecosystem funding, too, flows
through company- and foundation-controlled grant programs that pick winners
off-chain.

### 1.5 You Still Need an Exchange to Get Paid

Even where a chain pays participants, realizing that value usually means moving
tokens to a centralized exchange — KYC, listing politics, withdrawal limits,
and counterparty risk. The reward only matters if you can spend it.

---

## 2. Why Now

- NIST Post-Quantum Cryptography standards finalized in 2024: ML-DSA
  (CRYSTALS-Dilithium) and Falcon-512 are the approved signature schemes.
- The era of VC-controlled "decentralized" protocols is ending under
  regulatory scrutiny. SEC actions against centralized token issuers are
  accelerating globally.
- Predictable, declining emission schedules (Bitcoin halvings; Falcon's CID
  continuous decline) build long-term holder confidence. No equivalent model
  exists in the XRP ecosystem.
- The validator incentive problem is unsolved in every ledger-based chain
  that does not issue a staking reward. This problem has a known solution:
  pay validators on-chain, deterministically, from a protocol-controlled
  supply.
- On-chain DEX/AMM liquidity and stablecoins are mature enough to let a chain
  offer native swaps to USDC/USDT without depending on a centralized exchange.

---

## 3. What Falcon Ledger Is

Falcon Ledger is a protocol, not a company product. It is a fork of the XRP Ledger
with fundamental upgrades applied to the layers that XRP left broken:

| Layer        | What Falcon Ledger Changes                                  |
| ------------ | ----------------------------------------------------------- |
| Supply       | Protocol treasury replaces company wallet                   |
| Incentives   | Validators earn rewards every epoch (CID + fluid scoring)   |
| Cryptography | Falcon-512 as the standard signature scheme for all keys and transactions |
| Governance   | Bonded validator supermajority on-chain                     |
| Fees         | Burn + validator split, no dead-end fee destruction         |
| Liquidity    | Built-in DEX/AMM for in-wallet qXRP↔USDC/USDT swaps         |
| Identity UX  | Optional Account Names (`alice.bob` → `r…`) with bonded claim |

The consensus model is unchanged. RPCA stays. Finality stays sub-second.
Fees stay low. The economic and cryptographic layers are replaced entirely.

---

## 4. Quantum Security — Falcon as Standard

### 4.1 The Threat

Shor's algorithm, running on a cryptographically relevant quantum computer,
can recover a private key from its public key for any ECDSA or ed25519
identity. Once a public key is exposed on-chain — which happens the moment
a wallet sends a transaction — it is theoretically vulnerable.

Every transaction ever sent on the XRP Ledger has exposed the sender's
public key. Those keys, and the wallets they control, will be attackable
once the hardware catches up to the math.

### 4.2 Falcon-512, Always On

Falcon Ledger uses Falcon-512 as the **standard, always-on** signature scheme
for validator identities. Falcon is a NIST-approved lattice-based signature
algorithm. Its security is based on the hardness of the NTRU lattice problem,
which has no known quantum speedup. Falcon-512 provides NIST Level 1 security
and signatures small enough for high-throughput consensus and compatible with
the existing transaction framing.

Every validator on Falcon Ledger registers an 898-byte Falcon public key
(1-byte `0xFB` prefix + 897 raw bytes) on-chain and proposes with a
Falcon-derived identity. Falcon verification is deterministic, fully local,
and requires no external services or network calls.

### 4.3 Falcon-Native Authority — No Classical Signing Paths

Falcon Ledger does not use classical signature schemes (secp256k1 or ed25519) for
account authority, validator consensus, on-chain validator transactions, or
trusted validator list (UNL) identity. Every wallet is created with a Falcon key
pair. Every transaction is signed with Falcon. Every validator consensus message
is signed and verified with Falcon.

**P2P overlay:** Peer identity is Falcon as well (`validation_falcon_secret` by
default, or optional separate `node_falcon_secret`). Classical `node_seed` is
**disabled** — config will refuse to start if present.

There is no hybrid mode for consensus, accounts, or peer identity — the protocol
is Falcon-native for every security-critical path including P2P handshakes.

### 4.4 Design Principles

- Quantum resistance is a protocol requirement, not an optional feature.
- Falcon is the only signature scheme — not an opt-in for the security-conscious.
- Signature verification must remain deterministic and fully on-ledger.
- No external services are permitted in any consensus-critical path.

---

## 5. Tokenomics

### 5.1 Fixed Supply

**200,000,000,000 qXRP. Hard cap. No new issuance. Ever.**

The total supply is set at genesis and enforced by a compile-time
invariant. There is no mechanism in the protocol to create additional
supply beyond the fixed 200B. There is no company escrow of tens of
billions with monthly unlocks. Ongoing network incentives after launch
come from the **keyless protocol treasury** under CID + PoPL rules.

### 5.2 Genesis Allocation

| Account             |            qXRP | Share | Purpose                                           |
| ------------------- | --------------: | ----: | ------------------------------------------------- |
| Genesis circulating |   4,000,000,000 |    2% | Public bootstrap (airdrop + faucet + builder)     |
| Protocol treasury   | 196,000,000,000 |   98% | Emitted only by on-chain epoch rules              |

#### 5.2.1 Split of the 2% (4B) at ceremony

After genesis, the circulating 4B is partitioned into **three published wallets**:

| Wallet | qXRP | % of total | Purpose |
| ------ | ---: | ---------: | ------- |
| **AIRDROP** | 2,000,000,000 | 1.0% | Community airdrop / mainnet contribution rewards |
| **FAUCET** | 1,000,000,000 | 0.5% | Free claim faucet for onboarding (rate-limited) |
| **DEV / BUILDER** | 1,000,000,000 | 0.5% | Payment for core engineering + contributors who code, audit, or help grow the network |

**Builder pot.** Building a chain is real work. The 1B builder allocation is
**compensation for that work** and for people who help (development, audits,
outreach). It is **not** a silent “foundation discretionary grant machine”
funded from the 98% treasury.

**Custody (intentional single-operator control).** Airdrop, faucet, and builder
keys are held by the **project founder** under cold/offline practices. This is
deliberate: multi-sig with untrusted co-signers creates a collusion and theft
risk over the launch float the builder is accountable for. Founder custody of a
**known, capped 2%** is transparent skin-in-the-game; it is **not** the same as
a company controlling ~40% of supply via escrow. Ceremony addresses should be
published so anyone can audit the split on-chain.

The treasury holds 98% of the supply. It has no private key. Funds can
only leave the treasury via the `RewardEpoch` pseudo-transaction, which
is triggered automatically by the protocol at the close of each epoch.
No human can authorize a treasury withdrawal.

### 5.3 Emission Schedule (CID)

Treasury emission uses **Continuous Inflationary Decline (CID)** — a smooth
per-epoch schedule rather than discrete multi-year halvings.

```
// Per-epoch basis points of remaining treasury (integer math only)
// Year-1 average ≈ 12% of treasury / year (~52 epochs)
// Year-5 average ≈ 4.5% / year
// Long-term floor ≈ 1.5% / year (≈ 3 bps per epoch)
emissionBps  = cidEmissionBps(epochIndex)   // linear micro-decline per epoch
epochEmit    = treasuryBalance × emissionBps / 10,000
```

Each epoch spans 172,800 ledgers — approximately 7 days at 3.5 seconds
per ledger (~52 epochs per year). Bootstrap: epochs **1–7** schedule **zero**
claimable emission; the first non-zero pool unlocks at **epoch 8**.

| Period   | Approx. yearly avg of remaining treasury | Shape |
| -------- | ---------------------------------------- | ----- |
| Year 1   | ~12%                                     | starts higher, declines each epoch |
| Year 5   | ~4.5%                                    | continued linear decline |
| Long term| ~1.5% floor                              | per-epoch floor ~3 bps |

**PoPL split.** Each epoch’s emission is shared by participation:

- **Lending vault LPs** — +1% of the epoch pool per distinct active vault
  depositor (capped), proportional to vault MPT share within the LP basket.
- **AMM LPs** — participation-based allocation when AMM LP counting is active.
- **Validators** — remainder of the epoch pool, proportional to composite score
  among bonded validators by composite score (see §6).

Claims are pull-based (`ClaimReward`, `ClaimLPReward`, `ClaimAmmLpReward`) and
hard-capped to the epoch’s remaining pool balance. Absolute emission declines
every epoch as both the rate and the treasury balance fall — supply never
inflates beyond the 200B hard cap.

### 5.4 Fee Burn

Every transaction fee is split into two components:

- **Burn** (40%–70% of fee): permanently destroyed. Gone from supply forever.
- **Reward pool** (30%–60% of fee): routed to active validators.

The burn fraction is computed dynamically from on-chain signals: treasury
fill level and fee volume. When the treasury is large, more is burned. When
network usage is high, more goes to validators. The split is bounded between
40% and 70% and can be adjusted by bonded validator governance within those
bounds.

The result: every transaction makes the supply smaller. The more the
network is used, the faster it deflates.

---

## 6. Proof of Participation

### 6.1 Validators Get Paid

Every epoch, the protocol emits a share of the treasury to qualifying
validators. The share is proportional to each validator's composite
performance score. There is no off-chain oracle. There is no human
judgment. The score is computed deterministically from on-ledger data
every flag ledger (256 ledgers), then used at epoch claim time.

**You run the chain. You earn the chain.**

### 6.2 Fluid / Smoothed Composite Score

Rewards use **independent continuous signals** and a **smoothed composite** —
not a flat demerit shared by everyone who misses a single threshold.

```
// 256-ledger window; all values in basis points (0–10_000)
rawScore = (uptime × 40
         +  voteAccuracy × 30
         +  latencyScore × 15
         +  consistency  × 10) / 100

rawSlashed     = rawScore × slashMultiplier / 10_000
compositeScore = EMA(rawSlashed, previousComposite)
               // 35% new window  /  65% history  (kSCORE_EMA_NEW_BPS = 3500)
```

| Signal | Weight | Measurement (per 256-ledger window) |
| ------ | ------ | ----------------------------------- |
| **Uptime** | 40% | Presence: any trusted full validation for the sequence / 256 |
| **Vote accuracy** | 30% | Correct (canonical-hash) votes / votes cast — independent of uptime |
| **Latency** | 15% | Continuous relative score vs the **earliest correct signer** on each ledger (−1 bps per 10 ms lag; earliest → 10_000) |
| **Consistency** | 10% | `10_000 − max_absence_streak × 10_000 / 256` — long outages hurt more than scatter |
| **Slash multiplier** | applied after | Multiplicative; not an additive “fifth weight” in the raw blend |

**Why smoothing matters.** A one-window dip no longer zeros rewards forever or
snaps back to perfect in a single step. The EMA blends 35% of the new raw
composite with 65% of the previous on-bond composite, so recovery after a
knockdown is gradual and gaming a single window is less effective.

**Pay ∝ score (all bonded).** Every bonded validator is scored from observed
full validations — including joiners not yet on the bootstrap UNL (untrusted
validations are relayed by default). Composite is **not** wiped by a top‑K
rank cut. Epoch rewards are:

```
share = validatorPot × composite / sum(all composites)
```

for each claimer with composite ≥ **5%** (500 bps). Better operators earn more;
mediocre operators still earn; idle keys score near zero. **UNL membership is
separate** (who closes ledgers). Bootstrap UNL is operator-published; a future
amendment can move trust to a score-based rotating set.

### 6.3 Bonding

A validator must bond a minimum of 1,000 qXRP to be eligible for rewards.
Bonded qXRP is locked on-chain in an `ltVALIDATOR_BOND` object controlled
by the protocol. To exit, a validator initiates a 30-day unbonding period.
The bond cannot be withdrawn early.

Bonding creates skin in the game. A validator who misbehaves risks losing
their bond, not just their rewards.

### 6.4 Slashing

Provable misbehavior is penalized on-chain:

| Offense                       | Penalty                                |
| ----------------------------- | -------------------------------------- |
| Double-sign (equivocation)    | 100% of bond — immediate forced unbond |
| Proven invalid vote           | 50% of bond                            |
| Sustained absence (3+ epochs) | 25% of bond                            |

Slashed amounts go to the treasury, not to the reporter. There is no
bounty hunting incentive. Slashing requires cryptographic proof submitted
on-chain via a `SlashValidator` transaction. The protocol adjudicates it;
no human decides.

> **Status note:** double-sign slashing is enforced today. Absence and
> invalid-vote penalties are defined but intentionally disabled (`temDISABLED`)
> pending further testing — see the [Roadmap](../../ROADMAP.md).

### 6.5 Claiming Rewards

Rewards are not automatically pushed to validators. Each validator submits
a `ClaimReward` transaction for each epoch they are eligible for. The payout
is drawn directly from the treasury (hard-capped to remaining epoch pool).
The per-validator share is computed from the fixed emission rate recorded at
epoch creation — the order in which validators claim does not affect how much
they receive. Vault and AMM LPs claim via `ClaimLPReward` / `ClaimAmmLpReward`
when those paths are allocated.

---

## 7. Account Names (Human Addresses)

Falcon wallets remain **`r…` AccountIDs** under Falcon-512 keys. On top of that,
the protocol supports optional **Account Names** — a bonded, on-ledger map from
a human-readable name to an account.

### 7.1 Why names exist

Long base58 addresses are hard to share safely. Names give wallets and portals
a resolve path (`alice.bob` → owner `r…`) without changing settlement: every
Payment still targets an AccountID. Names are **not** a separate key system and
do not replace Falcon signatures.

### 7.2 Rules (protocol)

| Rule | Value |
| ---- | ----- |
| Bond | **100 qXRP** locked while the name is held |
| Ownership | **One** active or releasing name per account |
| Claim | `NameSet` — name free, account funded, no existing name |
| Release start | `NameUnbond` — status → releasing; name reserved to owner |
| Cooldown | **1 epoch** (`kQXRP_LEDGERS_PER_EPOCH` ledgers) after unbond |
| Finalize | `NameRelease` — bond returned, object deleted, name free |
| While releasing | Name-routed resolution **rejects**; raw `r…` payments still work |
| Format | Normalized lowercase ASCII; recommended `label.tld`-style (e.g. `scott.reynolds`) |

Duplicate claims and second names on the same account fail with `tecDUPLICATE`.
Releasing before the cooldown fails with `tecTOO_SOON`.

### 7.3 Economics and anti-squat

Holding a name has opportunity cost (100 qXRP bond). Unbonding frees the name
after one epoch so squatters cannot lock handles forever without capital. Bulk
hoarding still requires many funded accounts — acceptable for v1; no name
marketplace or multi-name portfolios in the first release.

### 7.4 Amendment and UX

Enabled via the **`AccountNames`** amendment. Wallets: create Falcon key → fund
→ optional **Claim username** → send to `r…` or resolve name first. Profile
shows active / releasing status. Full design notes: [NAME_SERVICE.md](NAME_SERVICE.md).

---

## 8. On-Chain Governance

Protocol parameters can be updated by the bonded validator set without a
hard fork — and without a company in the loop.

The governance mechanism:

1. Any bonded validator proposes a parameter change via `GovernanceProposal`.
2. A 7-day voting window opens.
3. Bonded validators submit `GovernanceVote` transactions. Each vote is
   weighted by the validator's composite score at submission time.
4. If yes votes exceed 67% of aggregate composite score, the proposal passes
   and the parameter is updated automatically on the next ledger close.
5. If the threshold is not met, the proposal expires and nothing changes.

**What can governance change:** fee burn fraction (within the 40%–70% hard
bounds), and additional bounded parameters as added by future amendments.

**What governance cannot change:** the 200B supply cap, the treasury lock,
RPCA consensus rules, or any parameter outside the defined governance bounds.
The protocol is not infinitely malleable. The core guarantees are immutable.

---

## 9. Built-In Liquidity: Faucet, Wallet, and No-Exchange Swaps

A validator reward only matters if it can be realized. Falcon Ledger inherits
the XRP Ledger's native decentralized exchange (offers/order books) and AMM,
and builds the launch experience around them so that **no centralized exchange
is required** at mainnet.

The target launch stack:

- **Faucet** — funds a new wallet with qXRP for reserves and fees.
- **Wallet** — holds qXRP, manages a Falcon validator identity, and claims
  epoch rewards.
- **In-wallet swaps** — converts qXRP to **USDC** and **USDT** through the
  built-in DEX order books and AMM pools, entirely on-chain and inside the
  wallet UI.

The outcome: a validator is paid in qXRP each epoch and can immediately swap
those rewards to stablecoins on the same network — from day one of mainnet,
with no listing, KYC gatekeeper, or third-party exchange. Implementation status
and remaining work are tracked in the [Roadmap](../../ROADMAP.md).

---

## 10. Falcon Ledger vs XRP

|                              | XRP                           | Falcon Ledger (qXRP)                      |
| ---------------------------- | ----------------------------- | ----------------------------------------- |
| **Supply control**           | Ripple holds ~40B tokens      | 98% keyless treasury; 2% public bootstrap |
| **Genesis float**            | Company / founders dominate   | 2B airdrop + 1B faucet + 1B builder       |
| **Validator rewards**        | None                          | Paid every epoch, on-chain                |
| **Quantum resistance**       | No — ed25519 only             | Yes — Falcon-512 for all keys and transactions |
| **Escrow / unlock schedule** | Yes — Ripple releases monthly | No company escrow of bulk supply          |
| **Governance**               | Ripple / XRPLF                | On-chain bonded validator supermajority   |
| **Ecosystem funding**        | Company/foundation discretion | Protocol emission + disclosed builder pot |
| **Supply curve**             | Unpredictable monthly unlocks | CID declining emission + continuous burn  |
| **Human addresses**          | No protocol names             | Optional Account Names (bonded)           |
| **Scoring**                  | N/A                           | Fluid EMA; pay ∝ score (all bonded)       |
| **Bulk-supply company escrow** | High                        | None on the 98% treasury                  |
| **Fee model**                | Burned, no beneficiary        | Split: burned + paid to validators        |
| **Slashing**                 | No                            | Yes — cryptographic proof on-chain        |
| **Migration to PQ crypto**   | Not planned                   | Not needed — Falcon-only from genesis     |
| **Selling rewards**          | Requires an exchange          | In-wallet swaps to USDC/USDT, no exchange |

---

## 11. Milestones

### Completed

- ✅ Direct fork of XRPL reference implementation (replay-protected; testnet network ID 1001)
- ✅ Falcon-512 transaction signing — all wallets and transactions use Falcon keys (verified on testnet)
- ✅ Full Falcon validator consensus fleet — `validation_falcon_secret`, Falcon hex UNL; classical `node_seed` banned
- ✅ Protocol treasury — deterministic account, no private key, locked by protocol
- ✅ CID epoch emission — continuous decline, first claimable unlock at epoch 8; PoPL LP participation split
- ✅ Dynamic fee burn + validator reward split — 40%–70% burn, remainder to validators
- ✅ Validator registration, bonding, unbonding, and re-bonding
- ✅ Fluid composite scoring — independent signals, relative latency, EMA smoothing; pay ∝ score for all bonded
- ✅ ClaimReward / ClaimLPReward / ClaimAmmLpReward — pull-based, pool hard-caps
- ✅ Double-sign slashing — 100% bond burn + forced unbond (pure burn path; re-proven on rehearsal)
- ✅ On-chain governance — proposals, voting, supermajority enforcement executed on-chain
- ✅ Account Names — `NameSet` / `NameUnbond` / `NameRelease`; 100 qXRP bond; freeze-pin smoke PASS
- ✅ Built-in AMM + SingleAssetVault + LendingProtocol (permissionless borrow path exercised)
- ✅ USDC bridge multi-sig lock (N-of-M) — Sepolia 2-of-3 e2e; mainnet redeploy pending
- ✅ Drop conservation invariant — enforced at every transaction
- ✅ Sustained testnet load — 850k+ payments over 71+ hours, 0 consensus stalls
- ✅ Mainnet protocol freeze pin — `mainnet-v1` @ `1789d2fb4` private-net smoke 14/14

### In Progress

- 🔲 Faucet + wallet + in-wallet swap polish for public mainnet launch
- 🔲 Absence / invalid-vote slashing enablement (defined; `temDISABLED` until detection ready)
- 🔲 Genesis validator set keys + public mainnet (network id **1026**) ceremony
- 🔲 ETH mainnet multi-sig lock deploy (new cold owners; Circle USDC)
- 🔲 Full production audit packages / external review of freeze scope
- 🔲 Operator one-liner + validator onboarding for public joiners
- 🔲 Long-horizon reward model simulations and continued adversarial testing

See [ROADMAP.md](../../ROADMAP.md) for the full, status-tracked plan.  
Public test summary: [TEST_AND_VERIFICATION.md](TEST_AND_VERIFICATION.md) · [MAINNET_READINESS_SUMMARY.md](MAINNET_READINESS_SUMMARY.md).

---

## 12. Technical Summary

| Component               | Detail                                               |
| ----------------------- | ---------------------------------------------------- |
| Chain name              | Falcon Ledger                                        |
| Token ticker            | qXRP                                                 |
| Consensus               | RPCA — unchanged from XRP Ledger                     |
| Finality                | Sub-second, deterministic                            |
| Validator signature     | Falcon-512 (NIST PQC standard) — standard, always on |
| Transaction signature   | Falcon-512 — all wallets and transactions use Falcon  |
| P2P identity            | Falcon-only (`node_seed` refused)                    |
| Total supply            | 200,000,000,000 qXRP (hard cap)                      |
| Treasury                | 196,000,000,000 qXRP (98%), no private key           |
| Genesis circulating     | 4,000,000,000 qXRP (2%): 2B airdrop / 1B faucet / 1B builder |
| Epoch length            | 172,800 ledgers (~7 days)                            |
| Emission model          | CID continuous decline; first unlock epoch 8         |
| Year-1 emission target  | ~12% of treasury / year (declining each epoch)       |
| Long-term emission floor| ~1.5% of treasury / year (~3 bps/epoch)              |
| PoPL split              | Validators + vault LPs + AMM LPs (participation)     |
| Fee burn range          | 40%–70% of every transaction fee                     |
| Scoring                 | Fluid EMA (35/65); pay ∝ score all bonded; relative latency |
| Minimum bond            | 1,000 qXRP                                           |
| Unbonding period        | 262,800 ledgers (~30 days)                           |
| Account name bond       | 100 qXRP; 1 name/account; 1-epoch release cooldown   |
| Governance threshold    | 67% of aggregate composite score                     |
| Mainnet network id      | 1026 (ceremony pack)                                 |
| Slashing — double sign  | 100% bond + forced unbond                            |
| Slashing — invalid vote | 50% bond (defined, currently disabled)               |
| Slashing — absence      | 25% bond (3+ epochs; defined, currently disabled)    |
| Liquidity               | Built-in DEX order books + AMM + lending vaults      |

---

## 13. The Bet

Ripple bet that speed was enough.

They were half right. Speed matters. But speed controlled by a company with
a 40-billion-token inventory, no validator rewards, no quantum plan, and no
on-chain governance is just a fast way to get rugged.

Falcon Ledger makes a different bet: that the chain worth building on for the
next 50 years is the one where the math is unbreakable, the supply is fixed,
the validators are paid, **bulk supply is not a company escrow with monthly
unlocks**, the launch float is a **small, public 2% bootstrap** — and where you
can sell what you earn on the same network, with no exchange in the way.

**Same speed. Better economics. Quantum proof. 98% protocol treasury. Honest 2%
bootstrap. No exchange required.**

---

_Copyright © 2026 Falcon Ledger Team. Protocol code is licensed under AGPL-3.0 for
the first two years after first public release, then MIT. See LICENSE and
COPYRIGHT.md for full terms._
