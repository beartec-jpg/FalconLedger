[![codecov](https://codecov.io/gh/XRPLF/rippled/graph/badge.svg?token=WyFr5ajq3O)](https://codecov.io/gh/XRPLF/rippled)

# qXRP

Copyright (c) 2026 qXRP Team. This repository contains upstream XRPL code under its original ISC license and qXRP-original work under the project policy described in [LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md).

qXRP is a quantum-resistant fork of the XRP Ledger that keeps RPCA consensus, fast finality, and low operating costs while adding post-quantum signatures, validator-aligned incentives, and a fixed-supply token model built for long-term sustainability.

## Vision

qXRP is designed to combine three properties that usually conflict:

1. Fast consensus and low fees from the XRP Ledger architecture.
2. Strong post-quantum cryptography, with Falcon as the default recommended signature scheme.
3. Sustainable tokenomics that reward reliable validators instead of relying on large premine allocations.

The goal is a network where security, decentralization, and participation incentives reinforce each other on-chain.

## Key Differentiators

- Post-quantum security with Falcon support as a first-class signature type.
- Hybrid signature support during migration, including Falcon plus ed25519 where needed.
- Proof-of-Participation style rewards for active validators with deterministic, on-ledger scoring.
- Fixed total supply of 200 billion qXRP.
- Minimal genesis allocation, with the protocol-controlled treasury holding the bulk of supply.
- Dynamic fee splitting that burns part of the fee and routes part to validators.
- On-chain bonding, slashing, and governance for bounded protocol parameters.
- RPCA consensus remains in place; qXRP evolves the economic and cryptographic layers around it.

## Tokenomics

qXRP uses a fixed supply and a treasury-first emission design.

| Parameter | Value | Notes |
| --- | ---:| --- |
| Total supply | 200,000,000,000 qXRP | Fixed at genesis |
| Genesis allocation cap | 4,000,000,000 qXRP | Maximum 2% of supply |
| Treasury allocation | 196,000,000,000 qXRP | 98% controlled by protocol emission rules |
| Initial issuance | High bootstrap rate | Intended to bootstrap validator participation |
| Emission schedule | Halving every 4 years | Roughly 31.5 million ledgers per period |
| Long-tail emission horizon | About 60 to 80 years | Emissions taper toward negligible levels |
| Base fee split | 50% to 60% burned | Deflationary component |
| Base fee remainder | 40% to 50% to validators | Distributed to active, qualifying validators |

### Genesis Allocation Targets

| Category | Target share | Purpose |
| --- | ---:| --- |
| Liquidity bootstrap | Small portion of the 2% cap | Exchange listings and initial market depth |
| Core development | Small portion of the 2% cap | Engineering, audits, infrastructure |
| Emergency reserve | Small portion of the 2% cap | Time-locked, multi-sig controlled reserve |
| Reward treasury | At least 98% of supply | Emitted only by protocol rules to qualifying validators |

### Reward Eligibility Signals

Validator rewards should be based only on deterministic on-ledger metrics such as:

- Uptime percentage over rolling windows.
- Vote accuracy.
- Proposal latency.
- Participation consistency.
- Slashing history.
- Bond status where bonding is enabled.

These signals should be aggregated on-chain so rewards can be calculated without off-chain oracles or manual intervention.

## Running a Validator

The current target workflow is:

1. Build the server from source using [BUILD.md](BUILD.md).
2. Configure a validator node with durable storage, stable connectivity, and secure key handling.
3. Generate a validator identity using the post-quantum key type selected for your deployment.
4. Register the validator for reward eligibility once the `ValidatorRewards` or `ProofOfParticipation` amendment is available.
5. Keep the node online, synced, and responsive so it can accumulate uptime, participation, and vote-quality metrics.
6. Monitor reward and slashing status on-ledger.

During early development, this section describes the intended operating model rather than a finalized production guide.

## Quantum Security

qXRP treats quantum resistance as a protocol requirement, not an optional add-on.

### Signature Strategy

- Falcon is the recommended default signature scheme for new qXRP identities.
- Hybrid Falcon plus ed25519 support can be used during migration and interoperability periods.
- Signature verification must remain deterministic and fully local to the node.
- The protocol should preserve backwards compatibility where possible during the transition period.

### Design Goals

- Reduce long-term exposure to future quantum attacks.
- Keep verification fast enough for consensus-critical paths.
- Avoid dependence on external services for key translation or validation.
- Support gradual migration instead of a forced network-wide flag day.

## Technical Architecture

qXRP keeps the XRP Ledger consensus model and layers new economic and cryptographic behavior around it.

| Layer | Responsibility |
| --- | --- |
| Consensus | Keep RPCA and existing validator communication patterns |
| Cryptography | Add Falcon and hybrid signature verification |
| Genesis | Create protocol treasury and initial distribution objects |
| Fees | Apply dynamic base-fee burn and validator reward split |
| Rewards | Compute validator emissions from on-chain performance metrics |
| Bonding | Allow optional validator bond locking for full rewards and voting weight |
| Slashing | Penalize provable misbehavior and sustained underperformance |
| Governance | Allow bounded, automated parameter updates by bonded validators |

The primary implementation goal is to make each new system deterministic, auditable, and fully enforceable by ledger state.

## Roadmap

### Short Term

- Add first-class Falcon support.
- Define the `ValidatorRewards` or `ProofOfParticipation` amendment.
- Introduce the treasury account and reward emission plumbing.
- Add fee split logic for burn plus validator distribution.
- Build test coverage for reward scoring, bonding, and slashing rules.

### Long Term

- Replace static trust assumptions with performance-weighted validator influence.
- Expand on-chain governance for bounded protocol parameters.
- Harden the reward model with long-horizon simulations and adversarial testing.
- Add migration tooling for hybrid identities and legacy validator operators.
- Prepare production audit packages and operator tooling.

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
- All new qXRP code (Falcon support, ProofOfParticipation transactors, treasury, rewards, slashing, governance, etc.) is under **AGPL-3.0-only** as noted in the file headers.

This has implications for downstream projects, exchanges, and node operators. See [COPYRIGHT.md](COPYRIGHT.md) for details.

qXRP-original files and qXRP-specific modifications are intended to follow the protection policy described in [LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md):

- New qXRP work is intended to be distributed under AGPL-3.0 terms for the first 2 years after first public release.
- After that period, the project intends to offer the same material under MIT terms.
- File headers should clearly identify qXRP-original contributions.

This notice is a project policy summary. Contributors should review the full policy text in [LICENSE](LICENSE) before submitting work.

## Links

- Documentation: https://docs.qxrp.example/
- GitHub: https://github.com/XRPLF/rippled
- Discord: https://discord.gg/qxrp
- Telegram: https://t.me/qxrp

## Existing XRPL Resources

The upstream XRP Ledger project documentation remains a useful reference for architecture, deployment, and background reading:

- [XRP Ledger Dev Portal](https://xrpl.org/)
- [Setup and Installation](https://xrpl.org/install-rippled.html)
- [Source Documentation (Doxygen)](https://xrplf.github.io/rippled/)
- [Clio API Server for the XRP Ledger](https://github.com/XRPLF/clio)
