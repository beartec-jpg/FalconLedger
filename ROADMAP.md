# Falcon Ledger Roadmap

**Chain:** Falcon Ledger · **Token ticker:** qXRP · **Last updated:** June 2026

Falcon Ledger is a quantum-resistant fork of the XRP Ledger. It is named after the
**Falcon** post-quantum signature scheme it runs as standard, and after its heritage
as a fork of the XRP Ledger. The chain is **Falcon Ledger**; the token keeps the
ticker **qXRP**.

This roadmap summarizes what has been accomplished so far and the path to the end
goal: a mainnet launch where a **faucet**, a **wallet**, and **in-wallet swaps to
USDC and USDT** (via the built-in DEX/AMM) mean **no centralized exchange is
required** — validators can sell their rewards on-chain from day one.

For the full protocol rationale, see the [White Paper](docs/qxrp/whitepaper.md).

---

## North Star

> A new validator can spin up a Falcon Ledger node, get funded by a faucet, earn
> qXRP rewards every epoch from a protocol-controlled treasury (no company, no
> grants), and swap those rewards to USDC/USDT inside their wallet — without ever
> touching a centralized exchange.

---

## What's Accomplished (Done)

These were validated on the testnet (see
[QXRP_CHAIN_REPORT_2026-05-29.md](QXRP_CHAIN_REPORT_2026-05-29.md) for the detailed
chain report) and form the protocol foundation.

### Cryptography — Falcon as standard

- [x] Falcon-512 (NIST PQC, Level 1) integrated as the standard validator signature.
- [x] All testnet validators register an 898-byte Falcon public key (`0xFB` prefix) on-chain.
- [x] Validators proposing/validating with Falcon-derived identities (`n9...` preserved for compatibility).
- [ ] Hybrid Falcon + ed25519 signature path for migration/interoperability. **Not implemented.** Transaction signing/verification (`STTx::checkSign` → `verify()` in `src/libxrpl/protocol/PublicKey.cpp`) currently supports only secp256k1 and ed25519. Falcon is used for validator node keys (ProofOfParticipation) only — there is no transaction-level Falcon or hybrid signature path yet.

### Supply & treasury — no company control

- [x] Fixed 200B qXRP supply, hard-capped at genesis.
- [x] 2% genesis circulating (4B) / 98% protocol treasury (196B), treasury has no private key.
- [x] Treasury emits only via the protocol's `RewardEpoch` rules — no human/company withdrawal.

### Emission & fees — protocol-controlled rewards

- [x] Epoch-based emission with Bitcoin-style halving (every ~208 epochs / ~4 years).
- [x] First halving confirmed live on-chain (50 → 25 bps).
- [x] Dynamic fee split: 40%–70% burned, remainder routed to active validators.
- [x] Drop-conservation invariant enforced at every transaction.

### Proof of Participation — validators get paid

- [x] `ValidatorRegister`, `ValidatorBond`, `UnbondValidator`, `ReleaseBond` transactions.
- [x] Five-factor composite score (uptime, vote accuracy, latency, consistency, slash) computed fully on-chain each epoch.
- [x] `ClaimReward` — pull-based, treasury-sourced; equal-split, proportional, sole-validator, and post-slash exclusion all tested.
- [x] `SlashValidator` — double-sign slashing (100% bond + forced unbond) enforced and tested.

### Governance — on-chain, no company gatekeeper

- [x] `GovernanceProposal` / `GovernanceVote` with composite-score-weighted voting.
- [x] 67% supermajority enforced; a burn-BPS change executed on-chain end-to-end.
- [x] Hard bounds that governance cannot cross (supply cap, treasury lock, consensus rules).

### Network & reliability

- [x] Direct XRPL fork on network ID 999 (mainnet/testnet replay protection).
- [x] Sustained testnet load test: 850k+ payments over 71+ hours, ~3s convergence, 0 stalls.
- [x] First testnet third-party security audit completed and remediated.

---

## In Progress / Near Term

### Liquidity foundation (toward no-exchange swaps)

- [ ] Exercise the inherited DEX order books and AMM under the live amendment set.
- [ ] Establish qXRP↔USDC and qXRP↔USDT liquidity (order books and/or AMM pools).
- [ ] Validate swap routing and pricing for validator reward conversion.

### Proof of Participation hardening

- [ ] Implement real latency measurement (currently hard-floored at 5,000 bps).
- [ ] Enable absence (25%) and invalid-vote (50%) slashing (defined but currently `temDISABLED`).
- [ ] Address bond-minimum grandfathering edge cases surfaced in testnet.

### Test & audit

- [ ] Expand reward-scoring, bonding, and slashing test/simulation coverage.
- [ ] Long-horizon emission and treasury-depletion simulations.

---

## Mainnet Launch Goal: Faucet · Wallet · In-Wallet Swaps

The launch experience that removes the need for any centralized exchange:

### 1. Faucet

- [ ] Faucet service that funds new wallets with qXRP for reserves and fees.
- [ ] Rate limiting / abuse protection suitable for mainnet.

### 2. Wallet

- [ ] Wallet that holds qXRP and manages a Falcon validator identity/keys.
- [ ] Claim epoch rewards (`ClaimReward`) from the wallet UI.
- [ ] Bonding / unbonding management from the wallet.

### 3. In-wallet swaps (no exchange required)

- [ ] Swap qXRP → **USDC** and qXRP → **USDT** directly via the built-in DEX/AMM.
- [ ] Execute swaps entirely on-chain, inside the wallet, with no third party.
- [ ] One-click "claim rewards → swap to stablecoin" flow for validators.

### Launch readiness

- [ ] Genesis validator set configuration and mainnet parameters finalized.
- [ ] Production security audit packages completed.
- [ ] Operator tooling and validator onboarding documentation published.

**Definition of done for launch:** at mainnet genesis, a validator can be funded by
the faucet, earn qXRP rewards, and swap them to USDC/USDT in-wallet — with no
centralized exchange anywhere in the loop.

---

## Long Term

- [ ] Bridge / interoperability layer for cross-chain stablecoins and assets.
- [ ] Performance-weighted validator influence replacing static trust assumptions.
- [ ] Expanded on-chain governance over additional bounded protocol parameters.
- [ ] Adversarial testing and continuous reward-model simulation.
- [ ] Migration tooling for hybrid identities and legacy validator operators.

---

## Why This Beats the XRP Ledger

| Benefit                   | XRP Ledger                    | Falcon Ledger                               |
| ------------------------- | ----------------------------- | ------------------------------------------- |
| Post-quantum signatures   | None                          | Falcon-512 standard, always on              |
| Validator pay             | None                          | Paid every epoch from protocol treasury     |
| Company control of supply | High (escrow unlocks)         | None — treasury has no private key          |
| Ecosystem grants          | Company/foundation discretion | Protocol emission, no grant gatekeeper      |
| Reward distribution       | N/A                           | Protocol-controlled, deterministic on-chain |
| Governance                | Company-gated amendments      | On-chain bonded supermajority               |
| Economic security         | None                          | Bonding + slashing with cryptographic proof |
| Selling rewards           | Requires an exchange          | In-wallet swaps to USDC/USDT, no exchange   |
