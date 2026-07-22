# Falcon Ledger Roadmap

**Chain:** Falcon Ledger · **Token ticker:** qXRP · **Last updated:** July 2026

Falcon Ledger is a quantum-resistant fork of the XRP Ledger. It is named after the
**Falcon** post-quantum signature scheme it runs as standard, and after its heritage
as a fork of the XRP Ledger. The chain is **Falcon Ledger**; the token keeps the
ticker **qXRP**.

This roadmap summarizes what has been accomplished so far and the path to the end
goal: a mainnet launch where a **faucet**, a **wallet**, and **in-wallet swaps to
USDC and USDT** (via the built-in DEX/AMM) mean **no centralized exchange is
required** — validators can sell their rewards on-chain from day one.

For the full protocol rationale, see the [White Paper](docs/qxrp/whitepaper.md).
For ceremony / freeze status, see [docs/MAINNET_SECURITY_FREEZE.md](docs/MAINNET_SECURITY_FREEZE.md)
and [scripts/mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md](scripts/mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md).

---

## North Star

> A new validator can spin up a Falcon Ledger node, get funded by a faucet, earn
> qXRP rewards every epoch from a protocol-controlled treasury (no company, no
> grants), claim an optional human **account name**, and swap those rewards to
> USDC/USDT inside their wallet — without ever touching a centralized exchange.

---

## What's Accomplished (Done)

These were validated on testnet and private mainnet dress rehearsal, and form the
protocol foundation for the freeze pin (`qxrp/xrpld:mainnet-v1` @ `1789d2fb4`).

### Cryptography — Falcon as standard

- [x] Falcon-512 (NIST PQC, Level 1) integrated as the standard signature scheme for validators and transactions.
- [x] All validators register an 898-byte Falcon public key (`0xFB` prefix) on-chain.
- [x] Validators proposing/validating with Falcon-derived identities (`n9...` preserved for compatibility).
- [x] All wallets created with Falcon key pairs; all transactions signed and verified with Falcon.
- [x] Falcon-only P2P identity — classical `node_seed` refused at config load.

### Supply & treasury — no company control

- [x] Fixed 200B qXRP supply, hard-capped at genesis.
- [x] 2% genesis circulating (4B) / 98% protocol treasury (196B), treasury has no private key.
- [x] Treasury emits only via the protocol's `RewardEpoch` rules — no human/company withdrawal.

### Emission & fees — protocol-controlled rewards

- [x] **CID** (Continuous Inflationary Decline) epoch emission — smooth per-epoch decline; ~12% year-1 average toward ~1.5% long-term floor.
- [x] Bootstrap quiet period: first claimable emission at **epoch 8**.
- [x] PoPL participation split: validators + lending vault LPs + AMM LPs share each epoch pool.
- [x] Dynamic fee split: 40%–70% burned, remainder routed to active validators.
- [x] Drop-conservation invariant enforced at every transaction (including pure bond burns on slash).

### Proof of Participation — validators get paid

- [x] `ValidatorRegister`, `ValidatorBond`, `UnbondValidator`, `ReleaseBond` transactions.
- [x] **Fluid / smoothed scoring:** independent uptime, vote accuracy, relative latency (vs earliest correct signer), consistency (max absence streak); slash multiplier applied after.
- [x] **EMA composite** (35% new window / 65% history) so recovery is gradual, not a flat demerit snap.
- [x] **ActiveSet(K=32):** top-K by composite keep reward weight; others clear composite but retain diagnostics.
- [x] Re-scored every flag interval (256 ledgers).
- [x] `ClaimReward` / `ClaimLPReward` / `ClaimAmmLpReward` — pull-based, epoch pool hard-caps.
- [x] `ValidatorSlash` — double-sign (100% bond + forced unbond) enforced; pure burn path re-proven on rehearsal.

### Account Names — human addresses

- [x] Amendment `AccountNames` with `NameSet` / `NameUnbond` / `NameRelease`.
- [x] 100 qXRP bond, one name per account, 1-epoch cooldown, free on release.
- [x] Freeze-pin smoke: claim, lookup, duplicate reject, unbond, `tecTOO_SOON` on early release.
- [x] Design doc: [docs/qxrp/NAME_SERVICE.md](docs/qxrp/NAME_SERVICE.md).

### Liquidity & lending (on-ledger)

- [x] AMM create/deposit under live amendment set (rehearsal + testnet).
- [x] SingleAssetVault + LendingProtocol + LendingCollateral + LendingPermissionless exercised (Vault → Broker → LoanSet → LoanPay).
- [x] USDC bridge multi-sig lock contract (N-of-M) — Sepolia **2-of-3 e2e PASS**; ETH mainnet redeploy still open.

### Governance — on-chain, no company gatekeeper

- [x] `GovernanceProposal` / `GovernanceVote` with composite-score-weighted voting.
- [x] 67% supermajority enforced; a burn-BPS change executed on-chain end-to-end.
- [x] Hard bounds that governance cannot cross (supply cap, treasury lock, consensus rules).

### Network & reliability

- [x] Direct XRPL fork with replay protection (testnet **1001**; mainnet ceremony **1026**).
- [x] Sustained testnet load test: 850k+ payments over 71+ hours, ~3s convergence, 0 stalls.
- [x] First testnet third-party security audit completed and remediated.
- [x] Private dress rehearsal (fast-epoch + freeze pin): split, bond, scores, vault/lend, adversarial smoke, AccountNames, multi-sig bridge.
- [x] Protocol **security freeze declared** 2026-07-22 — see freeze docs.

---

## In Progress / Near Term

### Proof of Participation hardening

- [ ] Enable absence (25%) and invalid-vote (50%) slashing (defined but currently `temDISABLED`).
- [ ] Address bond-minimum grandfathering edge cases surfaced in testnet.
- [ ] Continued soak on freeze pin + optional load tests separate from freeze soak.

### Portal / product

- [ ] Portal claim/release/send-by-name UX for Account Names.
- [ ] Live APY surfaces from epoch emission data in lend overview.
- [ ] Deeper testnet liquidity and additional stablecoin pairs (USDT).

### Test & audit

- [ ] Expand reward-scoring, bonding, and slashing test/simulation coverage.
- [ ] Long-horizon emission and treasury-depletion simulations.
- [ ] External review of freeze scope (slash, claims, scoring, bridge custody).

---

## Mainnet Launch Goal: Faucet · Wallet · In-Wallet Swaps

The launch experience that removes the need for any centralized exchange:

### 1. Faucet

- [x] Testnet faucet service (rate-limited FALCON drip) live on portal.
- [ ] Mainnet faucet env, cold/hot custody, and abuse protection for public launch.

### 2. Wallet

- [x] Passkey Falcon wallet (send/receive, backup/restore) on testnet portal.
- [ ] Claim epoch rewards (`ClaimReward`) from the wallet UI on mainnet.
- [ ] Bonding / unbonding management and optional **name claim** from the wallet.
- [ ] Validator deploy one-liner against freeze image digest.

### 3. In-wallet swaps (no exchange required)

- [x] Testnet: instant AMM swap, DEX limit orders, FALCON/F-USDC pool, Sepolia bridge.
- [ ] Mainnet qXRP → **USDC** / **USDT** path with production bridge lock (multi-sig, cold owners).
- [ ] One-click "claim rewards → swap to stablecoin" flow for validators.

### Launch readiness (ceremony)

- [x] Network id **1026** chosen; ceremony pack + freeze commit recorded.
- [x] Private dress rehearsal smoke PASS; soak left running.
- [x] Image pushed to registry with published digest (`qxrp/xrpld@sha256:e5086df99920…`).
- [ ] Genesis + AIRDROP / FAUCET / DEV keys offline; UNL public keys finalized.
- [ ] Production security audit packages / external review signed off.
- [ ] Operator tooling and public validator onboarding documentation published.
- [ ] Wipe rehearsal throwaway secrets before real T0.

**Definition of done for launch:** at mainnet genesis, a validator can be funded by
the faucet, earn qXRP rewards under fluid ActiveSet scoring, optionally claim a
human name, and swap rewards to USDC/USDT in-wallet — with no centralized exchange
anywhere in the loop.

---

## Long Term

- [ ] Performance-weighted validator influence replacing static trust assumptions (beyond ActiveSet reward weight).
- [ ] Expanded on-chain governance over additional bounded protocol parameters.
- [ ] Name marketplace / transfer (non-goal for v1).
- [ ] Adversarial testing and continuous reward-model simulation.

---

## Why This Beats the XRP Ledger

| Benefit                   | XRP Ledger                    | Falcon Ledger                               |
| ------------------------- | ----------------------------- | ------------------------------------------- |
| Post-quantum signatures   | None                          | Falcon-512 standard, always on              |
| Validator pay             | None                          | Paid every epoch from protocol treasury     |
| Company control of supply | High (escrow unlocks)         | None — treasury has no private key          |
| Ecosystem grants          | Company/foundation discretion | Protocol emission, no grant gatekeeper      |
| Reward distribution       | N/A                           | Fluid EMA scoring + ActiveSet(K=32)         |
| Human addresses           | None                          | Optional Account Names (100 qXRP bond)      |
| Governance                | Company-gated amendments      | On-chain bonded supermajority               |
| Economic security         | None                          | Bonding + slashing with cryptographic proof |
| Selling rewards           | Requires an exchange          | In-wallet swaps to USDC/USDT, no exchange   |
