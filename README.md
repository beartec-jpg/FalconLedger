[![codecov](https://codecov.io/gh/beartec-jpg/qXRP/graph/badge.svg)](https://codecov.io/gh/beartec-jpg/qXRP)

# Falcon Ledger

<div align="center">

![Falcon Ledger Coin](docs/qxrp/Screenshot_20260524-223407.png)

_Falcon Ledger — Quantum-Resistant. Validator-Rewarding. No Company. No Escrow. No Dumps._

</div>

Copyright (c) 2026 Falcon Ledger Team. This repository contains upstream XRPL code under its original ISC license and Falcon Ledger-original work under the project policy described in [LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md).

**Falcon Ledger** is a quantum-resistant fork of the XRP Ledger. It is named after its signature scheme — **Falcon** post-quantum lattice signatures, used as standard for every validator identity and every transaction — and after its heritage as a fork of the XRP Ledger.

> **Name vs. ticker:** the chain is called **Falcon Ledger**. The native token keeps the ticker **qXRP**. Throughout this repository, "Falcon Ledger" refers to the network/protocol and "qXRP" refers to the token (the unit of account, balances, fees, and rewards).

Falcon Ledger keeps RPCA consensus, fast finality, and low operating costs while adding post-quantum signatures, validator-aligned incentives, and a fixed-supply token model built for long-term sustainability.

## Vision

Falcon Ledger is designed to combine three properties that usually conflict:

1. Fast consensus and low fees from the XRP Ledger architecture.
2. Strong post-quantum cryptography, with Falcon as the standard signature scheme for all keys and transactions.
3. Sustainable tokenomics that reward reliable validators instead of relying on large premine allocations.

The goal is a network where security, decentralization, and participation incentives reinforce each other on-chain — with **no company in control** of supply, governance, or grants.

## Why Falcon Ledger Over the XRP Ledger

Falcon Ledger keeps everything that makes the XRP Ledger fast and replaces the parts that left holders and validators exposed:

- **Post-quantum signatures as standard, all the time.** Falcon (NIST PQC standard) is the signature scheme for validators and transactions from genesis — not a future retrofit. XRPL validators sign with classical ed25519/secp256k1 that Shor's algorithm will eventually break.
- **Validators get paid.** Running an XRPL validator earns nothing. Falcon Ledger pays validators every epoch from a protocol-controlled treasury, proportional to **fluid on-ledger scoring** (EMA-smoothed composite — all bonded, pay ∝ score).
- **No company control over the ecosystem.** There is no company holding tens of billions of tokens, no monthly escrow unlocks, and no foundation that can dump on holders. 98% of supply sits in a protocol treasury with no private key.
- **No company-controlled grants.** Emissions and incentives are released only by on-chain consensus rules (CID declining schedule + PoPL LP split), not by a foundation's discretionary grant program.
- **Protocol-controlled rewards.** Reward emission follows a continuous declining schedule enforced by the protocol — no human, company, or foundation can authorize a treasury withdrawal.
- **Fees fund the network instead of vanishing.** Each fee is split: part is burned (deflationary) and part is routed to active validators.
- **On-chain governance.** Bonded validators propose and ratify bounded parameter changes on-chain via supermajority, with no off-chain company gatekeeping.
- **Economic security via bonding and slashing.** Validators lock a bond and lose it for provable misbehavior — skin in the game that XRPL has no equivalent for.
- **Optional human account names.** Claim a bonded name (e.g. `alice.bob`) that resolves to your `r…` address — 100 qXRP bond, one name per account, one-epoch release cooldown.

## Key Differentiators

- Post-quantum security with Falcon as the standard, always-on signature type for validators, transactions, and P2P identity.
- Proof-of-Participation rewards with **fluid scoring**: independent uptime / vote / latency / consistency signals, EMA-smoothed composite, **pay ∝ score for all bonded** (joiners included when validations are seen).
- Fixed total supply of 200 billion qXRP; **CID** continuous emission decline (first unlock at epoch 8).
- Minimal genesis allocation, with the protocol-controlled treasury holding the bulk of supply.
- Dynamic fee splitting that burns part of the fee and routes part to validators.
- On-chain bonding, slashing, governance, and optional **Account Names** (`NameSet` / `NameUnbond` / `NameRelease`).
- Built-in DEX, AMM, and lending vaults — path to in-wallet qXRP↔USDC/USDT without a centralized exchange.
- RPCA consensus remains in place; Falcon Ledger evolves the economic and cryptographic layers around it.

## Tokenomics

Falcon Ledger uses a fixed supply and a treasury-first emission design. The token ticker is **qXRP**.

| Parameter                  |                    Value | Notes                                         |
| -------------------------- | -----------------------: | --------------------------------------------- |
| Total supply               |     200,000,000,000 qXRP | Fixed at genesis                              |
| Genesis allocation cap     |       4,000,000,000 qXRP | Maximum 2% of supply                          |
| Treasury allocation        |     196,000,000,000 qXRP | 98% controlled by protocol emission rules     |
| Initial issuance           |      CID bootstrap rate | ~12% of treasury / year average in year 1     |
| Emission schedule          | Continuous decline (CID) | First claimable pool at epoch 8; ~1.5%/yr floor |
| Long-tail emission horizon | Multi-decade taper       | Rate and treasury both decline each epoch     |
| Base fee split             |        40% to 70% burned | Dynamic from treasury fill + fee volume       |
| Base fee remainder         | Remainder to validators  | Bonded score-proportional (min composite floor) |

### Genesis Allocation Targets

| Category            |                Target share | Purpose                                                 |
| ------------------- | --------------------------: | ------------------------------------------------------- |
| Liquidity bootstrap | Small portion of the 2% cap | Exchange listings and initial market depth              |
| Core development    | Small portion of the 2% cap | Engineering, audits, infrastructure                     |
| Emergency reserve   | Small portion of the 2% cap | Time-locked, multi-sig controlled reserve               |
| Reward treasury     |      At least 98% of supply | Emitted only by protocol rules to qualifying validators |

### Reward Eligibility Signals

Validator rewards use deterministic on-ledger metrics only (256-ledger windows,
re-scored every flag ledger):

- **Uptime** — presence (any trusted full validation) over the window.
- **Vote accuracy** — correct canonical-hash votes / votes cast (independent of uptime).
- **Latency** — continuous relative score vs the earliest correct signer (−1 bps / 10 ms lag).
- **Consistency** — penalizes max consecutive absence streak (outages hurt more than scatter).
- **Slash multiplier** — applied after the weighted blend; then **EMA** with prior composite (35% new / 65% history).
- **All bonded** validators with a composite can share epoch rewards ∝ score (min floor applies at claim).
- Bond status — must be bonded; consensus UNL is separate (bootstrap list until open-UNL amendment).

No off-chain oracles or manual intervention.

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
3. Generate a validator identity using Falcon, the standard signature scheme for the entire protocol.
4. Register the validator for reward eligibility via the `ProofOfParticipation` amendment.
5. Keep the node online, synced, and responsive so it can accumulate uptime, participation, and vote-quality metrics.
6. Monitor reward and slashing status on-ledger.

During early development, this section describes the intended operating model rather than a finalized production guide.

## Quantum Security

Falcon Ledger treats quantum resistance as a protocol requirement, not an optional add-on.

### Signature Strategy

- Falcon is the standard, always-on signature scheme for validator identities and transactions.
- All wallets are created with Falcon key pairs; all transactions are signed and verified with Falcon.
- Signature verification remains deterministic and fully local to the node.

### Design Goals

- Eliminate exposure to future quantum attacks from genesis — no classical keys anywhere in the protocol.
- Keep verification fast enough for consensus-critical paths.
- Avoid dependence on external services for key generation or validation.

## Technical Architecture

Falcon Ledger keeps the XRP Ledger consensus model and layers new economic and cryptographic behavior around it.

| Layer        | Responsibility                                                           |
| ------------ | ------------------------------------------------------------------------ |
| Consensus    | Keep RPCA and existing validator communication patterns                  |
| Cryptography | Falcon signature scheme for all keys and transactions                    |
| Genesis      | Create protocol treasury and initial distribution objects                |
| Fees         | Apply dynamic base-fee burn and validator reward split                   |
| Rewards      | CID emission + fluid EMA scoring; pay ∝ composite for all bonded         |
| Bonding      | Validator bond locking for rewards and voting weight                     |
| Slashing     | Penalize provable misbehavior (double-sign live; others defined)         |
| Governance   | Bounded, automated parameter updates by bonded validators                |
| Names        | Optional Account Names — bonded human handles resolving to `r…`          |
| Liquidity    | Built-in DEX/AMM + lending vaults for in-wallet qXRP↔USDC/USDT path      |

The primary implementation goal is to make each new system deterministic, auditable, and fully enforceable by ledger state.

## Roadmap

A detailed, status-tracked roadmap lives in [ROADMAP.md](ROADMAP.md). In summary:

### Short Term (toward public mainnet)

- Protocol freeze pin `mainnet-v1` soak + registry digest publish.
- Portal Account Names UX; mainnet faucet/wallet/swap go-live.
- ETH mainnet multi-sig bridge lock with cold owners.
- Enable additional slash offenses when detection is production-ready.
- External audit of freeze scope; ceremony keys offline.

### Long Term

- Performance-weighted influence beyond ActiveSet reward weight.
- Expand on-chain governance for additional bounded parameters.
- Long-horizon emission simulations and continuous adversarial testing.
- Name transfer / marketplace (explicitly non-goal for v1).

## Documentation

- [Falcon Ledger White Paper](docs/qxrp/whitepaper.md) — full protocol rationale and design (v2.6).
- [Test & verification](docs/qxrp/TEST_AND_VERIFICATION.md) — public summary of what was proven (safe to share).
- [Mainnet readiness summary](docs/qxrp/MAINNET_READINESS_SUMMARY.md) — short progress vs T0 (no ops secrets).
- [Public docs index](docs/qxrp/PUBLIC_DOCS_INDEX.md) — what to share on X / blog.
- [Roadmap](ROADMAP.md) — what's accomplished and the path to mainnet.
- [Account Names](docs/qxrp/NAME_SERVICE.md) — bonded human handles (`NameSet` / unbond / release).
- [Validator Lifecycle](docs/qxrp/validator-lifecycle.md) — bonding, fluid scoring, ActiveSet.
- [Supply Model](docs/qxrp/supply-model.md), [Epoch Emission](docs/qxrp/epoch-emission.md), [Fee Split](docs/qxrp/fee-split.md), [Governance](docs/qxrp/governance.md).
- [Mainnet security freeze](docs/MAINNET_SECURITY_FREEZE.md) · [Dress rehearsal](docs/MAINNET_REHEARSAL.md) · [Bridge](docs/MAINNET_BRIDGE.md) (ops — not for public dump).

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
