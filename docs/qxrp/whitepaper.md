# Falcon Ledger White Paper

**Version 2.1 — July 2026**

---

<div align="center">

![Falcon Ledger](https://q-xrp-faucet.vercel.app/icon-512.png)

_Falcon Ledger — Quantum-Resistant. Validator-Rewarding. No Company. No Escrow. No Dumps._

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
98% of the supply is locked in a protocol treasury with no private key. It is
released only by on-chain consensus rules — one epoch at a time, according to a
halving schedule, to the validators who keep the network alive. Validators get
paid for doing the work. Fees burn. The supply shrinks. No company can dump on you,
and no foundation decides who gets a grant.

**No exchange required.** Falcon Ledger ships with a built-in DEX and AMM. The
launch target is an in-wallet experience — faucet, wallet, and swaps — that lets a
validator convert qXRP rewards to **USDC** and **USDT** on-chain from day one of
mainnet, with no centralized exchange in the loop.

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
- Bitcoin's halving model proved that predictable emission schedules build
  long-term holder confidence. No equivalent model exists in the XRP
  ecosystem.
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
| Incentives   | Validators earn rewards every epoch                         |
| Cryptography | Falcon-512 as the standard signature scheme for all keys and transactions |
| Governance   | Bonded validator supermajority on-chain                     |
| Fees         | Burn + validator split, no dead-end fee destruction         |
| Liquidity    | Built-in DEX/AMM for in-wallet qXRP↔USDC/USDT swaps         |

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

**P2P overlay only:** Validators may configure a separate `node_seed` for
peer-to-peer overlay identity and handshakes. This key does not sign consensus
proposals, validations, or rewards — it is not part of validator authority or
bonding.

There is no hybrid mode for consensus or account authority — the protocol is
Falcon-native for every security-critical path.

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
supply. There is no team allocation that vests over time. There is no
foundation reserve that can be spent at will.

### 5.2 Genesis Allocation

| Account             |            qXRP | Share | Purpose                                           |
| ------------------- | --------------: | ----: | ------------------------------------------------- |
| Genesis circulating |   4,000,000,000 |    2% | Exchange listings, development, emergency reserve |
| Protocol treasury   | 196,000,000,000 |   98% | Emitted only by on-chain epoch rules              |

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

| Halving | Years | Rate          | ~Weekly Emission |
| ------- | ----- | ------------- | ---------------- |
| 0       | 0–4   | 0.50%         | 980,000,000 qXRP |
| 1       | 4–8   | 0.25%         | 490,000,000 qXRP |
| 2       | 8–12  | 0.12%         | 235,200,000 qXRP |
| 3       | 12–16 | 0.06%         | 117,600,000 qXRP |
| 4       | 16–20 | 0.03%         | 58,800,000 qXRP  |
| 5+      | 20–24 | 0.01% (floor) | decreasing       |

This is a **double decay**: the rate halves every 4 years, and the
treasury balance it applies to shrinks continuously. Absolute weekly
emission decreases every single epoch. Supply is always going down, never up.

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
is drawn directly from the treasury. The per-validator share is computed from
the fixed emission rate recorded at epoch creation — the order in which
validators claim does not affect how much they receive.

---

## 7. On-Chain Governance

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

## 8. Built-In Liquidity: Faucet, Wallet, and No-Exchange Swaps

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

## 9. Falcon Ledger vs XRP

|                              | XRP                           | Falcon Ledger (qXRP)                      |
| ---------------------------- | ----------------------------- | ----------------------------------------- |
| **Supply control**           | Ripple holds ~40B tokens      | Protocol treasury, no private key         |
| **Validator rewards**        | None                          | Paid every epoch, on-chain                |
| **Quantum resistance**       | No — ed25519 only             | Yes — Falcon-512 for all keys and transactions |
| **Escrow / unlock schedule** | Yes — Ripple releases monthly | No — emission only by protocol rules      |
| **Governance**               | Ripple / XRPLF                | On-chain bonded validator supermajority   |
| **Ecosystem grants**         | Company/foundation discretion | Protocol emission, no grant gatekeeper    |
| **Supply curve**             | Unpredictable monthly unlocks | Deterministic halving, continuous burn    |
| **Company dependency**       | High                          | Zero                                      |
| **Fee model**                | Burned, no beneficiary        | Split: burned + paid to validators        |
| **Slashing**                 | No                            | Yes — cryptographic proof on-chain        |
| **Migration to PQ crypto**   | Not planned                   | Not needed — Falcon-only from genesis     |
| **Selling rewards**          | Requires an exchange          | In-wallet swaps to USDC/USDT, no exchange |

---

## 10. Milestones

### Completed

- ✅ Direct fork of XRPL reference implementation (replay-protected; testnet network ID 1001)
- ✅ Falcon-512 transaction signing — all wallets and transactions use Falcon keys (verified on testnet)
- 🔲 Full Falcon validator consensus fleet upgrade (`validation_falcon_secret`, Falcon hex UNL) — rolling out July 2026
- ✅ Protocol treasury — deterministic account, no private key, locked by protocol
- ✅ Epoch emission with halving schedule — first halving confirmed on-chain (50→25 bps)
- ✅ Dynamic fee burn + validator reward split — 40%–70% burn, remainder to validators
- ✅ Validator registration, bonding, unbonding, and re-bonding
- ✅ Composite score computation — five-factor, fully on-chain
- ✅ ClaimReward transaction — per-epoch, pull-based, treasury-sourced (all distribution modes tested)
- ✅ Double-sign slashing — 100% bond burn + forced unbond, verified on multiple nodes
- ✅ On-chain governance — proposals, voting, supermajority enforcement executed on-chain
- ✅ Drop conservation invariant — enforced at every transaction
- ✅ Sustained testnet load — 850k+ payments over 71+ hours, 0 consensus stalls

### In Progress

- 🔲 Built-in DEX/AMM exercise under the live amendment set (qXRP↔USDC/USDT)
- 🔲 Faucet + wallet + in-wallet swap experience for mainnet launch
- 🔲 Latency scoring (currently hard-floored) and absence/invalid-vote slashing enablement
- 🔲 Genesis validator set configuration and mainnet launch preparation
- 🔲 Bridge and interoperability layer for cross-chain stablecoins
- 🔲 Full production audit packages
- 🔲 Operator tooling and validator onboarding documentation
- 🔲 Long-horizon reward model simulations and adversarial testing

See [ROADMAP.md](../../ROADMAP.md) for the full, status-tracked plan.

---

## 11. Technical Summary

| Component               | Detail                                               |
| ----------------------- | ---------------------------------------------------- |
| Chain name              | Falcon Ledger                                        |
| Token ticker            | qXRP                                                 |
| Consensus               | RPCA — unchanged from XRP Ledger                     |
| Finality                | Sub-second, deterministic                            |
| Validator signature     | Falcon-512 (NIST PQC standard) — standard, always on |
| Transaction signature   | Falcon-512 — all wallets and transactions use Falcon  |
| Total supply            | 200,000,000,000 qXRP (hard cap)                      |
| Treasury                | 196,000,000,000 qXRP (98%), no private key           |
| Genesis circulating     | 4,000,000,000 qXRP (2%), time-locked                 |
| Epoch length            | 172,800 ledgers (~7 days)                            |
| Initial emission rate   | 50 bps per epoch (0.50% of treasury)                 |
| Halving cadence         | Every 208 epochs (~4 years)                          |
| Emission floor          | 1 bps per epoch (0.01% of treasury)                  |
| Fee burn range          | 40%–70% of every transaction fee                     |
| Validator reward        | Remaining fee + epoch treasury share                 |
| Minimum bond            | 1,000 qXRP                                           |
| Unbonding period        | 262,800 ledgers (~30 days)                           |
| Governance threshold    | 67% of aggregate composite score                     |
| Slashing — double sign  | 100% bond + forced unbond                            |
| Slashing — invalid vote | 50% bond (defined, currently disabled)               |
| Slashing — absence      | 25% bond (3+ epochs; defined, currently disabled)    |
| Liquidity               | Built-in DEX order books + AMM for qXRP↔USDC/USDT    |

---

## 12. The Bet

Ripple bet that speed was enough.

They were half right. Speed matters. But speed controlled by a company with
a 40-billion-token inventory, no validator rewards, no quantum plan, and no
on-chain governance is just a fast way to get rugged.

Falcon Ledger makes a different bet: that the chain worth building on for the
next 50 years is the one where the math is unbreakable, the supply is fixed,
the validators are paid, no single entity can move the market with a monthly
unlock — and where you can sell what you earn on the same network, with no
exchange in the way.

**Same speed. Better economics. Quantum proof. No company. No escrow. No dumps.
No exchange required.**

---

_Copyright © 2026 Falcon Ledger Team. Protocol code is licensed under AGPL-3.0 for
the first two years after first public release, then MIT. See LICENSE and
COPYRIGHT.md for full terms._
