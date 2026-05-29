# qXRP Security Policy

**This document applies to the qXRP project (beartec-jpg/qXRP).**

qXRP is a quantum-resistant fork of the XRP Ledger. While we inherit a large amount of upstream code from XRPLF/rippled, we have introduced significant new functionality including post-quantum cryptography (Falcon), a protocol-controlled treasury, validator bonding, on-chain Proof-of-Participation rewards, slashing, and governance.

## Supported Versions

We accept vulnerability reports for the following branches:

- `develop` (active development)
- Latest `release/*` branches
- Recent tagged releases

We generally do **not** accept reports against very old tags unless the issue also affects supported branches.

## Scope

### In Scope (qXRP-specific)
- Post-quantum Falcon signature implementation and integration
- New transaction types: `ValidatorRegister`, `ValidatorBond`, `ValidatorUnbond`, `ReleaseBond`, `ClaimReward`, `ValidatorSlash`, `GovernanceProposal`, `GovernanceVote`
- Treasury account and emission logic (`RewardEpoch`)
- Validator scoring and slashing mechanisms
- Fee splitting and burn logic
- Governance parameter updates
- Any code under `src/libxrpl/tx/transactors/qxrp/`, `src/libxrpl/protocol/{falcon,PQ*}.*`, `RewardEpoch.cpp`, `ValidatorScoring.cpp`, and related headers

### Out of Scope
- Pure upstream XRPLF/rippled issues (report those to the [Ripple Bugcrowd program](https://ripple.com) where applicable)
- Issues in third-party dependencies (liboqs, Boost, OpenSSL, etc.) unless they are triggered by qXRP-specific usage
- Social engineering, physical attacks, or attacks against specific node operators

## Reporting a Vulnerability

**Please do not open public GitHub issues for security vulnerabilities.**

### Preferred Method
Send an email to: **security@beartec.uk** (or the address published in the repository README if different).

Include the following information:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Any suggested fixes (optional but appreciated)

### Alternative
If you cannot use the above email, you may use the GitHub "Report a vulnerability" feature under the repository Security tab (private vulnerability reporting).

## Responsible Disclosure Guidelines

We ask researchers to:
- Give us a reasonable amount of time to investigate and fix the issue before public disclosure.
- Avoid testing against the live public testnet in ways that could disrupt validators or users.
- Not perform denial-of-service, spam, or social engineering attacks as part of research.

We commit to:
- Acknowledging receipt of reports within 72 hours (best effort).
- Keeping researchers informed of our progress.
- Crediting researchers in release notes / security advisories (unless they prefer to remain anonymous).

## Safe Harbor

We will not pursue legal action against researchers who:
- Follow this policy in good faith
- Make a good-faith effort to avoid privacy violations, destruction of data, or disruption of service
- Do not exploit the vulnerability beyond what is necessary to demonstrate the issue

## Bug Bounty

qXRP does **not** currently operate a formal bug bounty program.

We may offer discretionary rewards for high-quality reports on critical issues in the future. This policy will be updated if/when a bounty program is launched.

## Relationship with Upstream XRPL

Many vulnerabilities in the shared codebase should be reported to the upstream XRPL project (via Ripple's Bugcrowd program where eligible). We will coordinate with upstream maintainers on cross-cutting issues.

For qXRP-specific logic (especially Falcon cryptography, economic mechanisms, and new transaction types), please report directly to us first.

## Contact

- Security reports: security@beartec.uk
- General security discussions: Open a GitHub Discussion or contact the maintainers

---

*Last updated: 2026-05-30*