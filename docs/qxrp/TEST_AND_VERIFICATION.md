# Falcon Ledger — Test & Verification Summary

**Status:** Public engineering summary · **Date:** July 2026  
**Audience:** Community, integrators, and press — no operator secrets or host inventory.

This note summarizes **what has been proven** on Falcon Ledger testnet and in private mainnet dress-rehearsal / freeze-pin work. It is not a mainnet launch announcement. Public mainnet remains off until the go-live ceremony.

For protocol design, see the [White Paper](whitepaper.md). For account names, see [NAME_SERVICE.md](NAME_SERVICE.md). For a short launch-readiness snapshot, see [MAINNET_READINESS_SUMMARY.md](MAINNET_READINESS_SUMMARY.md).

---

## At a glance

| Area | Result | Notes |
|------|--------|--------|
| Falcon-512 accounts & consensus | **Verified** | Falcon-native signing; classical P2P `node_seed` disabled |
| CID emission + PoPL LP split | **Verified** | Continuous decline schedule; first claimable unlock at epoch 8 |
| Fluid scoring + pay ∝ score (all bonded) | **In tree** | EMA composite; no ActiveSet rank-cut; joiners scored when vals seen |
| AMM + vault + lending path | **Verified** | Create pool → vault → broker → borrow → repay |
| USDC bridge multi-sig (test EVM) | **Verified** | N-of-M lock; single owner cannot release; threshold does |
| Account Names | **Verified** | Claim, lookup, duplicate reject, unbond, early-release gate |
| Protocol freeze image | **Pinned** | Public Hub tag + content digest (see below) |
| Public mainnet go-live | **Not yet** | Soak, ceremony ops, and production RPC still ahead of T0 |

---

## 1. Cryptography & network identity

- Wallets and transactions use **Falcon-512** end to end.
- Validator consensus identities are Falcon; trusted lists use Falcon hex material.
- Peer identity is Falcon-only; classical `node_seed` is refused at config load.

Sustained testnet load has previously been reported at **850k+ payments** over multi-day windows with stable consensus (see historical testnet reports in this repository’s security docs).

---

## 2. Economics — emission & fees

- **Fixed supply:** 200B qXRP hard cap; 98% protocol treasury (no private key).
- **CID emission:** Continuous Inflationary Decline (smooth per-epoch rate on remaining treasury), not multi-year discrete halvings.
- **Bootstrap:** Epochs before **epoch 8** schedule zero claimable emission; first unlock at epoch 8.
- **PoPL split:** Epoch pools shared among validators and participating vault / AMM liquidity providers.
- **Fees:** Dynamic burn band (40%–70%); remainder to active validators.
- **Claims:** Pull-based (`ClaimReward` / LP claim paths) with hard caps to the epoch pool.

---

## 3. Fluid scoring & ActiveSet

Validator reward weight is computed fully on-ledger from a rolling window (flag interval / 256 ledgers):

| Signal | Role |
|--------|------|
| Uptime | Presence in the window |
| Vote accuracy | Correct canonical-hash votes / votes cast (independent of uptime) |
| Latency | Continuous score vs earliest correct signer (relative, not a flat floor) |
| Consistency | Penalizes long absence streaks more than scatter |
| Slash multiplier | Applied after the weighted blend |

Then:

1. **EMA smooth** (~35% new window / ~65% history) so recovery after a dip is gradual.  
2. **Pay ∝ score** — all bonded keep composite; share = pot × score / sum(scores); min floor at claim (500 bps). No top‑K wipe.

Double-sign slashing (100% bond + forced unbond) is enforced with a pure bond-burn path. Absence and invalid-vote slash codes remain defined but disabled until detection is production-ready.

---

## 4. AMM, vaults, and lending

Under the live amendment set (AMM, MPTokensV1, SingleAssetVault, LendingProtocol, and related lending flags), private and testnet runs exercised:

1. Issuer **DefaultRipple** + IOU setup  
2. **AMMCreate** / **AMMDeposit**  
3. **VaultCreate** / **VaultDeposit**  
4. **LoanBrokerSet** → **LoanSet** (permissionless path with AMM-priced collateral)  
5. **LoanPay**

Permissionless borrow requires a native↔asset AMM so health-factor checks can price collateral; missing pool surfaces as a pricing failure rather than a silent under-collateralized loan.

Portal lending E2E and third-party-style product reports for the testnet stack are published separately via the Falcon portal document set (wallet, bridge, and lending implementation reports).

---

## 5. Bridge — multi-sig custody model

The production-shaped custody model is an **N-of-M** EVM collateral lock (not a single hot EOA).

**Proven on public test EVM (Sepolia):**

| Check | Result |
|-------|--------|
| Deploy 2-of-3 lock | Pass |
| Deposit stablecoin → mint Falcon IOU | Pass |
| Single owner confirm | **Blocked** (insufficient confirmations) |
| Second owner confirm (threshold) | **Release succeeds** |

Legacy single-signer lock paths may still exist for older testnet flows; **mainnet is specified for multi-sig redeploy** with new cold owners and mainnet Circle USDC — not reuse of test keys.

---

## 6. Account Names

Optional human handles on top of Falcon `r…` addresses (settlement remains the AccountID):

| Rule | Value |
|------|--------|
| Bond | 100 qXRP |
| Limit | One name per account |
| Transactions | `NameSet` · `NameUnbond` · `NameRelease` |
| Cooldown | One epoch after unbond before bond return and name free |
| While releasing | Name resolution rejects; raw `r…` payments still work |

**Freeze-pin smoke:** claim → ledger lookup → duplicate reject → second-name reject → unbond → early `NameRelease` correctly too-soon on long-epoch pin.

Full product rules: [NAME_SERVICE.md](NAME_SERVICE.md).

---

## 7. Protocol freeze pin (public identifiers only)

A **protocol freeze image** was cut for mainnet preparation:

| Field | Value |
|-------|--------|
| Git commit (short) | `1789d2fb4` |
| Image tag | `qxrp/xrpld:mainnet-v1` |
| Content digest | `sha256:e5086df99920ca62a6c7c09e65d49decd43ae3a67cf6167aa46419e006fcb31c` |
| Labels (intent) | AccountNames + freeze commit metadata |
| Hub | Published under `qxrp/xrpld` |

```bash
docker pull qxrp/xrpld:mainnet-v1
# or pin by digest:
docker pull qxrp/xrpld@sha256:e5086df99920ca62a6c7c09e65d49decd43ae3a67cf6167aa46419e006fcb31c
```

Private-network smoke on this pin covered health, product amendments at genesis, and Account Names paths above. A multi-day **soak** (idle stability) is the remaining protocol-aging step before treating the pin as fully aged for public launch ops.

**Mainnet network id (ceremony):** `1026` (distinct from Falcon testnet `1001`).

---

## 8. What this is not

- Not a guarantee of public mainnet launch date  
- Not a full formal audit report (external review is recommended on freeze scope)  
- Not a substitute for running your own node against the published image  
- Does not include validator host lists, faucet secrets, or genesis private keys  

---

## Suggested one-liners (community / X)

> Falcon Ledger freeze pin `mainnet-v1` (`1789d2fb4`) is on Docker Hub — Falcon-only crypto, CID emission, fluid ActiveSet scoring, Account Names, and Sepolia multi-sig bridge e2e verified. Public mainnet still offline until go-live.

> We don’t pay validators with a flat demerit score. Composite is EMA-smoothed; every bonded validator shares epoch rewards in proportion to score.

> Human names on Falcon: bond 100 qXRP, one name per account, one-epoch release cooldown — payments still settle to `r…`.

---

## Related public docs in this folder

| Doc | Topic |
|-----|--------|
| [whitepaper.md](whitepaper.md) | Full protocol narrative (v2.6) |
| [MAINNET_READINESS_SUMMARY.md](MAINNET_READINESS_SUMMARY.md) | Short “where we are vs T0” |
| [NAME_SERVICE.md](NAME_SERVICE.md) | Account Names product rules |
| [validator-lifecycle.md](validator-lifecycle.md) | Bonding, scoring, slash |
| [epoch-emission.md](epoch-emission.md) | CID emission detail |
| [supply-model.md](supply-model.md) | Supply / treasury |
| [fee-split.md](fee-split.md) | Fee burn / validator share |
| [governance.md](governance.md) | On-chain governance |

Portal-hosted security and product PDFs (wallet, bridge, lending E2E) live with the Falcon web app and complement this engineering summary.
