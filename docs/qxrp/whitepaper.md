# qXRP White Paper

**Version 1.0 — May 2026**

---

<div align="center">

![qXRP Coin](https://github.com/user-attachments/assets/b5edf8bb-690f-42c3-ba1a-c2f25a6711e3)

*qXRP — Quantum-Resistant. Validator-Rewarding. No Escrow. No Dumps.*

</div>

---

## Executive Summary

qXRP is a quantum-resistant payment protocol forked from the XRP Ledger.
It keeps RPCA consensus and sub-second finality and replaces everything
that made XRP a compromised bet: the company-controlled supply, the zero
validator incentives, and the classical cryptography that a quantum computer
will eventually break.

**Fixed supply. 200 billion qXRP. Hard cap. No exceptions.**

98% of the supply is locked in a protocol treasury with no private key.
It is released only by on-chain consensus rules — one epoch at a time,
according to a halving schedule, to the validators who keep the network alive.
Validators get paid for doing the work. Fees burn. The supply shrinks.
No company can dump on you.

Falcon-512 lattice signatures are built in at the protocol level, not
retrofitted later. Hybrid Falcon plus ed25519 support allows a smooth
migration. This chain is built to be secure in 2026 and in 2046.

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

### 1.4 No On-Chain Governance

Protocol changes on the XRP Ledger happen through Ripple's amendment process.
Validators vote, but the proposals originate from and are largely controlled
by Ripple. There is no on-chain mechanism for bonded participants to propose,
debate, and ratify changes to economic parameters without relying on off-chain
social coordination and a company's goodwill.

---

## 2. Why Now

- NIST Post-Quantum Cryptography standards finalized in 2024: ML-DSA
  (CRYSTALS-Dilithium) and Falcon-512 are the approved signature schemes.
- The era of VC-controlled "decentralized" protocols is ending under
  regulatory scrutiny. SEC actions against centralized token issuers are
  accelerating globally.
- Bitcoin's halving model proved that predictable emission schedules build
  long-term holder confidence. No equivalent model exists in the XRP
  ecosystem.
- The validator incentive problem is unsolved in every ledger-based chain
  that does not issue a staking reward. This problem has a known solution:
  pay validators on-chain, deterministically, from a protocol-controlled
  supply.

---

## 3. What qXRP Is

qXRP is a protocol, not a company product. It is a fork of the XRP Ledger
with three fundamental upgrades applied to the layers that XRP left broken:

| Layer | What qXRP Changes |
|---|---|
| Supply | Protocol treasury replaces company wallet |
| Incentives | Validators earn rewards every epoch |
| Cryptography | Falcon-512 and hybrid signatures |
| Governance | Bonded validator supermajority on-chain |
| Fees | Burn + validator split, no dead-end fee destruction |

The consensus model is unchanged. RPCA stays. Finality stays sub-second.
Fees stay low. The economic and cryptographic layers are replaced entirely.

---

## 4. Quantum Security

### 4.1 The Threat

Shor's algorithm, running on a cryptographically relevant quantum computer,
can recover a private key from its public key for any ECDSA or ed25519
identity. Once a public key is exposed on-chain — which happens the moment
a wallet sends a transaction — it is theoretically vulnerable.

Every transaction ever sent on the XRP Ledger has exposed the sender's
public key. Those keys, and the wallets they control, will be attackable
once the hardware catches up to the math.

### 4.2 Falcon-512

qXRP uses Falcon-512 as the default signature scheme for new identities.
Falcon is a NIST-approved lattice-based signature algorithm. Its security
is based on the hardness of the NTRU lattice problem, which has no known
quantum speedup. Falcon-512 signatures are approximately 666 bytes — small
enough for high-throughput consensus and compatible with the existing
transaction framing.

Falcon verification is deterministic, fully local, and requires no external
services or network calls.

### 4.3 Hybrid Migration Path

Forcing an immediate network-wide migration to Falcon would break existing
tooling and validator key infrastructure. qXRP solves this with native
hybrid signature support: a Falcon-512 signature combined with an ed25519
signature on the same transaction.

During the migration window, hybrid identities satisfy both the post-quantum
security requirement and interoperability with legacy infrastructure.
Pure Falcon identities are available from day one for new deployments.
The protocol enforces a migration timeline via amendment gating, not by
breaking existing participants.

### 4.4 Design Principles

- Quantum resistance is a protocol requirement, not an optional feature.
- Signature verification must remain deterministic and fully on-ledger.
- No external services are permitted in any consensus-critical path.
- The migration is gradual, not a forced flag day.

---

## 5. Tokenomics

### 5.1 Fixed Supply

**200,000,000,000 qXRP. Hard cap. No new issuance. Ever.**

The total supply is set at genesis and enforced by a compile-time
invariant. There is no mechanism in the protocol to create additional
supply. There is no team allocation that vests over time. There is no
foundation reserve that can be spent at will.

### 5.2 Genesis Allocation

| Account | qXRP | Share | Purpose |
|---|---:|---:|---|
| Genesis circulating | 4,000,000,000 | 2% | Exchange listings, development, emergency reserve |
| Protocol treasury | 196,000,000,000 | 98% | Emitted only by on-chain epoch rules |

The genesis circulating allocation is capped at 2% of total supply.
It is subdivided into time-locked tranches for liquidity bootstrap,
core development, and an emergency multisig reserve. No single entity
controls it outright.

The treasury holds 98% of the supply. It has no private key. Funds can
only leave the treasury via the `RewardEpoch` pseudo-transaction, which
is triggered automatically by the protocol at the close of each epoch.
No human, company, or foundation can authorize a treasury withdrawal.

### 5.3 Emission Schedule

Treasury emission follows a Bitcoin-style halving schedule.

```
halvingN     = (epochIndex - 1) / 208        (integer division)
emissionBps  = max(50 >> halvingN, 1)        (basis points, floor 1)
epochEmit    = treasuryBalance × emissionBps / 10,000
```

Each epoch spans 172,800 ledgers — approximately 7 days at 3.5 seconds
per ledger. The emission rate halves every 208 epochs, or roughly every
4 years. The floor is 1 basis point (0.01% of remaining treasury per
epoch), reached at approximately year 24 and held indefinitely.

| Halving | Years | Rate | ~Weekly Emission |
|---|---|---|---|
| 0 | 0–4 | 0.50% | 980,000,000 qXRP |
| 1 | 4–8 | 0.25% | 490,000,000 qXRP |
| 2 | 8–12 | 0.12% | 235,200,000 qXRP |
| 3 | 12–16 | 0.06% | 117,600,000 qXRP |
| 4 | 16–20 | 0.03% | 58,800,000 qXRP |
| 5+ | 20–24 | 0.01% (floor) | decreasing |

This is a **double decay**: the rate halves every 4 years, and the
treasury balance it applies to shrinks continuously. Absolute weekly
emission decreases every single epoch. Supply is always going down, never up.

### 5.4 Fee Burn

Every transaction fee is split into two components:

- **Burn** (40%–70% of fee): permanently destroyed. Gone from supply forever.
- **Reward pool** (30%–60% of fee): routed to active validators.

The burn fraction is computed dynamically from two on-chain signals:
treasury fill level and fee volume. When the treasury is large, more
is burned. When network usage is high, more goes to validators. The
split is bounded between 40% and 70% and can be adjusted by bonded
validator governance within those bounds.

The result: every transaction makes the supply smaller. The more the
network is used, the faster it deflates.

---

## 6. Proof of Participation

### 6.1 Validators Get Paid

Every epoch, the protocol emits a share of the treasury to qualifying
validators. The share is proportional to each validator's composite
performance score. There is no off-chain oracle. There is no human
judgment. The score is computed deterministically from on-ledger data
at the close of each epoch.

**You run the chain. You earn the chain.**

### 6.2 Composite Score

Each validator's share of epoch rewards is determined by their composite
score:

```
compositeScore = (uptime × 40)
               + (voteAccuracy × 30)
               + (latencyScore × 15)
               + (consistency × 10)
               + (slashMultiplier × 5)
               ÷ 100
```

All five components are derived entirely from on-chain state. No external
data source is involved.

- **Uptime** (40%): percentage of ledgers closed while the validator was
  responsive and participating.
- **Vote accuracy** (30%): fraction of validation votes that matched the
  consensus outcome.
- **Latency score** (15%): relative speed of proposal and vote submission.
- **Consistency** (10%): participation stability across rolling windows,
  penalizing intermittent operators.
- **Slash multiplier** (5%): reduced by any prior slashing events.

A validator must score at least 5% composite to be included in epoch
reward distribution. Validators below this threshold receive nothing
that epoch.

### 6.3 Bonding

A validator must bond a minimum of 1,000 qXRP to be eligible for rewards.
Bonded qXRP is locked on-chain in an `ltVALIDATOR_BOND` object controlled
by the protocol. To exit, a validator initiates a 30-day unbonding period.
The bond cannot be withdrawn early.

Bonding creates skin in the game. A validator who misbehaves risks losing
their bond, not just their rewards.

### 6.4 Slashing

Provable misbehavior is penalized on-chain:

| Offense | Penalty |
|---|---|
| Double-sign (equivocation) | 100% of bond — immediate forced unbond |
| Proven invalid vote | 50% of bond |
| Sustained absence (3+ epochs) | 25% of bond |

Slashed amounts go to the treasury, not to the reporter. There is no
bounty hunting incentive. Slashing requires cryptographic proof submitted
on-chain via a `ValidatorSlash` transaction. The protocol adjudicates it;
no human decides.

### 6.5 Claiming Rewards

Rewards are not automatically pushed to validators. Each validator submits
a `ClaimReward` transaction for each epoch they are eligible for. The payout
is drawn directly from the treasury. The per-validator share is computed from
the fixed emission rate recorded at epoch creation — the order in which
validators claim does not affect how much they receive.

---

## 7. On-Chain Governance

Protocol parameters can be updated by the bonded validator set without a
hard fork.

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

## 8. qXRP vs XRP

| | XRP | qXRP |
|---|---|---|
| **Supply control** | Ripple holds ~40B tokens | Protocol treasury, no private key |
| **Validator rewards** | None | Paid every epoch, on-chain |
| **Quantum resistance** | No — ed25519 only | Yes — Falcon-512 + hybrid |
| **Escrow / unlock schedule** | Yes — Ripple releases monthly | No — emission only by protocol rules |
| **Governance** | Ripple / XRPLF | On-chain bonded validator supermajority |
| **Supply curve** | Unpredictable monthly unlocks | Deterministic halving, continuous burn |
| **Company dependency** | High | Zero |
| **Fee model** | Burned, no beneficiary | Split: burned + paid to validators |
| **Slashing** | No | Yes — cryptographic proof on-chain |
| **Migration to PQ crypto** | Not planned | Falcon-512 live at genesis |

---

## 9. Milestones

### Completed

- ✅ Protocol treasury — deterministic account, no private key, locked by protocol
- ✅ Epoch emission with halving schedule — 7-day epochs, 4-year halvings
- ✅ Dynamic fee burn + validator reward split — 40%–70% burn, remainder to validators
- ✅ Validator registration, bonding, unbonding, and slashing
- ✅ Composite score computation — five-factor, fully on-chain
- ✅ ClaimReward transaction — per-epoch, pull-based, treasury-sourced
- ✅ On-chain governance — proposals, voting, supermajority enforcement
- ✅ Falcon-512 hybrid signature support
- ✅ Drop conservation invariant — enforced at every transaction
- ✅ Governance parameter object — burn BPS adjustable within hard bounds

### In Progress

- 🔲 Genesis validator set configuration and mainnet launch preparation
- 🔲 DEX integration and on-chain liquidity primitives
- 🔲 Bridge and interoperability layer for cross-chain assets
- 🔲 Full production audit packages
- 🔲 Operator tooling and validator onboarding documentation
- 🔲 Long-horizon reward model simulations and adversarial testing

---

## 10. Technical Summary

| Component | Detail |
|---|---|
| Consensus | RPCA — unchanged from XRP Ledger |
| Finality | Sub-second, deterministic |
| Signature default | Falcon-512 (NIST PQC standard) |
| Migration path | Hybrid Falcon + ed25519 supported |
| Total supply | 200,000,000,000 qXRP (hard cap) |
| Treasury | 196,000,000,000 qXRP (98%), no private key |
| Genesis circulating | 4,000,000,000 qXRP (2%), time-locked |
| Epoch length | 172,800 ledgers (~7 days) |
| Initial emission rate | 50 bps per epoch (0.50% of treasury) |
| Halving cadence | Every 208 epochs (~4 years) |
| Emission floor | 1 bps per epoch (0.01% of treasury) |
| Fee burn range | 40%–70% of every transaction fee |
| Validator reward | Remaining fee + epoch treasury share |
| Minimum bond | 1,000 qXRP |
| Unbonding period | 262,800 ledgers (~30 days) |
| Governance threshold | 67% of aggregate composite score |
| Slashing — double sign | 100% bond + forced unbond |
| Slashing — invalid vote | 50% bond |
| Slashing — absence | 25% bond (3+ epochs) |

---

## 11. The Bet

Ripple bet that speed was enough.

They were half right. Speed matters. But speed controlled by a company with
a 40-billion-token inventory, no validator rewards, no quantum plan, and no
on-chain governance is just a fast way to get rugged.

qXRP makes a different bet: that the chain worth building on for the next
50 years is the one where the math is unbreakable, the supply is fixed,
the validators are paid, and no single entity can move the market with a
monthly unlock.

**Same speed. Better economics. Quantum proof. No company. No escrow. No dumps.**

---

*Copyright © 2026 qXRP Team. Protocol code is licensed under AGPL-3.0 for
the first two years after first public release, then MIT. See LICENSE and
COPYRIGHT.md for full terms.*
