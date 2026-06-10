[![codecov](https://codecov.io/gh/beartec-jpg/qXRP/graph/badge.svg)](https://codecov.io/gh/beartec-jpg/qXRP)

# Falcon Ledger

<div align="center">

![Falcon Ledger Coin](docs/qxrp/Screenshot_20260524-223407.png)

_Falcon Ledger — Quantum-Resistant. Validator-Rewarding. No Company. No Escrow. No Dumps._

</div>

Copyright (c) 2026 Falcon Ledger Team. This repository contains upstream XRPL code under its original ISC license and Falcon Ledger-original work under the project policy described in [LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md).

**Falcon Ledger** is a quantum-resistant fork of the XRP Ledger. It is named after its signature scheme — **Falcon** post-quantum lattice signatures, used as standard on every validator identity — and after its heritage as a fork of the XRP Ledger.

> **Name vs. ticker:** the chain is called **Falcon Ledger**. The native token keeps the ticker **qXRP**. Throughout this repository, "Falcon Ledger" refers to the network/protocol and "qXRP" refers to the token (the unit of account, balances, fees, and rewards).

Falcon Ledger keeps RPCA consensus, fast finality, and low operating costs while adding post-quantum signatures, validator-aligned incentives, and a fixed-supply token model built for long-term sustainability.

## Vision

Falcon Ledger is designed to combine three properties that usually conflict:

1. Fast consensus and low fees from the XRP Ledger architecture.
2. Strong post-quantum cryptography, with Falcon as the standard, always-on validator signature scheme.
3. Sustainable tokenomics that reward reliable validators instead of relying on large premine allocations.

The goal is a network where security, decentralization, and participation incentives reinforce each other on-chain — with **no company in control** of supply, governance, or grants.

## Why Falcon Ledger Over the XRP Ledger

Falcon Ledger keeps everything that makes the XRP Ledger fast and replaces the parts that left holders and validators exposed:

- **Post-quantum signatures as standard, all the time.** Falcon (NIST PQC standard) is the default validator signature scheme from genesis — not a future retrofit. XRPL validators sign with classical ed25519/secp256k1 that Shor's algorithm will eventually break.
- **Validators get paid.** Running an XRPL validator earns nothing. Falcon Ledger pays validators every epoch from a protocol-controlled treasury, proportional to deterministic, on-ledger performance.
- **No company control over the ecosystem.** There is no company holding tens of billions of tokens, no monthly escrow unlocks, and no foundation that can dump on holders. 98% of supply sits in a protocol treasury with no private key.
- **No company-controlled grants.** Emissions and incentives are released only by on-chain consensus rules, not by a foundation's discretionary grant program.
- **Protocol-controlled rewards.** Reward emission follows a fixed halving schedule enforced by the protocol — no human, company, or foundation can authorize a treasury withdrawal.
- **Fees fund the network instead of vanishing.** Each fee is split: part is burned (deflationary) and part is routed to active validators.
- **On-chain governance.** Bonded validators propose and ratify bounded parameter changes on-chain via supermajority, with no off-chain company gatekeeping.
- **Economic security via bonding and slashing.** Validators lock a bond and lose it for provable misbehavior — skin in the game that XRPL has no equivalent for.

## Key Differentiators

- Post-quantum security with Falcon as the standard, always-on validator signature type.
- Proof-of-Participation rewards for active validators with deterministic, on-ledger scoring.
- Fixed total supply of 200 billion qXRP.
- Minimal genesis allocation, with the protocol-controlled treasury holding the bulk of supply.
- Dynamic fee splitting that burns part of the fee and routes part to validators.
- On-chain bonding, slashing, and governance for bounded protocol parameters.
- A built-in DEX (and AMM) that will power an in-wallet path to swap qXRP for USDC and USDT — so validator rewards can be sold from day one of mainnet, no centralized exchange required.
- RPCA consensus remains in place; Falcon Ledger evolves the economic and cryptographic layers around it.

## Tokenomics

Falcon Ledger uses a fixed supply and a treasury-first emission design. The token ticker is **qXRP**.

| Parameter                  |                    Value | Notes                                         |
| -------------------------- | -----------------------: | --------------------------------------------- |
| Total supply               |     200,000,000,000 qXRP | Fixed at genesis                              |
| Genesis allocation cap     |       4,000,000,000 qXRP | Maximum 2% of supply                          |
| Treasury allocation        |     196,000,000,000 qXRP | 98% controlled by protocol emission rules     |
| Initial issuance           |      High bootstrap rate | Intended to bootstrap validator participation |
| Emission schedule          |    Halving every 4 years | Roughly 31.5 million ledgers per period       |
| Long-tail emission horizon |     About 60 to 80 years | Emissions taper toward negligible levels      |
| Base fee split             |        50% to 60% burned | Deflationary component                        |
| Base fee remainder         | 40% to 50% to validators | Distributed to active, qualifying validators  |

### Genesis Allocation Targets

| Category            |                Target share | Purpose                                                 |
| ------------------- | --------------------------: | ------------------------------------------------------- |
| Liquidity bootstrap | Small portion of the 2% cap | Exchange listings and initial market depth              |
| Core development    | Small portion of the 2% cap | Engineering, audits, infrastructure                     |
| Emergency reserve   | Small portion of the 2% cap | Time-locked, multi-sig controlled reserve               |
| Reward treasury     |      At least 98% of supply | Emitted only by protocol rules to qualifying validators |

### Reward Eligibility Signals

Validator rewards are based only on deterministic on-ledger metrics such as:

- Uptime percentage over rolling windows.
- Vote accuracy.
- Proposal latency.
- Participation consistency.
- Slashing history.
- Bond status where bonding is enabled.

These signals are aggregated on-chain so rewards can be calculated without off-chain oracles or manual intervention.

## End Goal: Faucet, Wallet, and In-Wallet Swaps

Falcon Ledger is built so that, **at mainnet launch, no centralized exchange is required** to use the network or realize validator rewards. The target user experience is:

1. **Faucet** — claim qXRP to fund a fresh wallet and pay reserves/fees.
2. **Wallet** — hold qXRP, run a validator, and claim epoch rewards.
3. **In-wallet swaps** — swap qXRP for **USDC** and **USDT** directly through the built-in DEX/AMM liquidity, inside the wallet, with no third-party exchange.

This means a validator can be paid in qXRP and immediately convert rewards to stablecoins on-chain from launch day. See [ROADMAP.md](ROADMAP.md) for status and the path to this milestone.

## Running a Validator

The current target workflow is:

1. Build the server from source using [BUILD.md](BUILD.md).
2. Configure a validator node with durable storage, stable connectivity, and secure key handling.
3. Generate a validator identity using Falcon, the standard post-quantum key type.
4. Register the validator for reward eligibility via the `ProofOfParticipation` amendment.
5. Keep the node online, synced, and responsive so it can accumulate uptime, participation, and vote-quality metrics.
6. Monitor reward and slashing status on-ledger.

During early development, this section describes the intended operating model rather than a finalized production guide.

## Quantum Security

Falcon Ledger treats quantum resistance as a protocol requirement, not an optional add-on.

### Signature Strategy

- Falcon is the standard, always-on signature scheme for validator identities.
- Hybrid Falcon plus ed25519 transaction signing is planned but **not yet implemented** (see ROADMAP).
- Signature verification remains deterministic and fully local to the node.
- The protocol preserves backwards compatibility where possible during the transition period.

### Design Goals

- Reduce long-term exposure to future quantum attacks.
- Keep verification fast enough for consensus-critical paths.
- Avoid dependence on external services for key translation or validation.

## Technical Architecture

Falcon Ledger keeps the XRP Ledger consensus model and layers new economic and cryptographic behavior around it.

| Layer        | Responsibility                                                           |
| ------------ | ------------------------------------------------------------------------ |
| Consensus    | Keep RPCA and existing validator communication patterns                  |
| Cryptography | Add Falcon and hybrid signature verification                             |
| Genesis      | Create protocol treasury and initial distribution objects                |
| Fees         | Apply dynamic base-fee burn and validator reward split                   |
| Rewards      | Compute validator emissions from on-chain performance metrics            |
| Bonding      | Allow optional validator bond locking for full rewards and voting weight |
| Slashing     | Penalize provable misbehavior and sustained underperformance             |
| Governance   | Allow bounded, automated parameter updates by bonded validators          |
| Liquidity    | Provide built-in DEX/AMM primitives for in-wallet qXRP↔USDC/USDT swaps   |

The primary implementation goal is to make each new system deterministic, auditable, and fully enforceable by ledger state.

## Roadmap

A detailed, status-tracked roadmap lives in [ROADMAP.md](ROADMAP.md). In summary:

### Short Term

- Maintain first-class Falcon support as the standard validator signature.
- Harden the `ProofOfParticipation` amendment (rewards, bonding, slashing, governance).
- Exercise the built-in DEX/AMM with qXRP↔USDC/USDT liquidity under the live amendment set.
- Expand test coverage for reward scoring, bonding, and slashing rules.

### Long Term

- Ship the faucet, wallet, and in-wallet swap experience for mainnet launch.
- Replace static trust assumptions with performance-weighted validator influence.
- Expand on-chain governance for bounded protocol parameters.
- Harden the reward model with long-horizon simulations and adversarial testing.
- Add migration tooling for hybrid identities and legacy validator operators.
- Prepare production audit packages and operator tooling.

## Documentation

- [Falcon Ledger White Paper](docs/qxrp/whitepaper.md) — full protocol rationale and design.
- [Roadmap](ROADMAP.md) — what's accomplished and the path to mainnet.
- [Supply Model](docs/qxrp/supply-model.md), [Epoch Emission](docs/qxrp/epoch-emission.md), [Fee Split](docs/qxrp/fee-split.md), [Validator Lifecycle](docs/qxrp/validator-lifecycle.md), [Governance](docs/qxrp/governance.md).

## Contributing

Contributions should focus on correctness, determinism, and protocol safety.

- Open a focused issue or design note before implementing consensus, reward, or signature changes.
- Prefer small, reviewable pull requests.
- Add tests for every protocol rule you touch.
- Keep cryptographic changes isolated and explicitly documented.
- Do not assume off-chain services can be part of consensus behavior.

Please read [COPYRIGHT.md](COPYRIGHT.md) before submitting code.

For security testing guidance (sanitizers, fuzzing, coverage targets), see [docs/security/security-testing.md](docs/security/security-testing.md).

### Security Audit Reports

- [1st Testnet Full Security Audit Report](docs/security/QXRP_1st_Testnet_Full_Security_Audit_Report.md) — Original third-party security audit (May 2026)
- [Security Audit Remediation Report](docs/security/QXRP_Security_Audit_Remediation_Report.md) — Summary of all fixes and improvements made in response to the audit

## Licensing And Protection Notice

**Important licensing split:**

- Upstream XRPL code (from XRPLF/rippled) remains under its original **ISC license** — see [LICENSE.md](LICENSE.md).
- All new Falcon Ledger code (Falcon support, ProofOfParticipation transactors, treasury, rewards, slashing, governance, etc.) is under **AGPL-3.0-only** as noted in the file headers.

This has implications for downstream projects, exchanges, and node operators. See [COPYRIGHT.md](COPYRIGHT.md) for details.

Falcon Ledger-original files and Falcon Ledger-specific modifications are intended to follow the protection policy described in [LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md):

- New Falcon Ledger work is intended to be distributed under AGPL-3.0 terms for the first 2 years after first public release.
- After that period, the project intends to offer the same material under MIT terms.
- File headers should clearly identify Falcon Ledger-original contributions.

This notice is a project policy summary. Contributors should review the full policy text in [LICENSE](LICENSE) before submitting work.

## Links

- Documentation: See the `docs/` folder in this repository
- GitHub: https://github.com/beartec-jpg/qXRP
- Discord / Telegram: (links to be added once communities are live)

## Existing XRPL Resources

The upstream XRP Ledger project documentation remains a useful reference for architecture, deployment, and background reading:

- [XRP Ledger Dev Portal](https://xrpl.org/)
- [Setup and Installation](https://xrpl.org/install-rippled.html)
- [Source Documentation (Doxygen)](https://xrplf.github.io/rippled/)
- [Clio API Server for the XRP Ledger](https://github.com/XRPLF/clio)
