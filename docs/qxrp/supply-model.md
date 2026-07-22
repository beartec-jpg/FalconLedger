# qXRP Supply Model

## Total Supply

qXRP has a fixed maximum supply of **200,000,000,000 qXRP** (200 billion).

Each qXRP is subdivided into **1,000,000 drops** (same granularity as upstream XRP).
Total supply in drops: `200,000,000,000 × 1,000,000 = 2×10^17 drops`.

This value is encoded as `kINITIAL_XRP` in `include/xrpl/protocol/SystemParameters.h`.

## Genesis Allocation

At ledger 1 two accounts are created:

| Account | qXRP | Fraction | Constant |
|---------|------|----------|----------|
| Genesis circulating | 4,000,000,000 | 2 % | `kQXRP_GENESIS_ALLOCATION` |
| Treasury (on-chain) | 196,000,000,000 | 98 % | `kQXRP_TREASURY_ALLOCATION` |

The compile-time `static_assert` in `QXRPConstants.h` ensures these two values sum
exactly to `kINITIAL_XRP`.

## Treasury Account

The treasury is a deterministic account derived from the well-known seed
`kQXRP_TREASURY_SEED`. The seed is public by design — no private key controls
the account. Funds may only leave the treasury via the `RewardEpoch` ledger-close
pseudo-transaction, which is governed by the `ProofOfParticipation` amendment.

## Emission Schedule

Emission is pulled from the treasury at the end of each reward epoch via
`RewardEpoch`. The emission amount is computed as:

```
epochEmissionDrops = treasuryBalance × emissionBps / BPS_DENOM
```

where `emissionBps` comes from **CID** (`cidEmissionBps(epochIndex)`): a smooth
per-epoch decline targeting ~12% of remaining treasury in year 1, ~4.5% by year
5, and a ~1.5%/year long-term floor (~3 bps per epoch). Epochs before
`kQXRP_FIRST_EMISSION_EPOCH` (8) schedule zero claimable emission. See
[epoch-emission.md](epoch-emission.md).

## Drop Conservation Invariant

After every transaction the `QXRPDropConservation` invariant verifies that the
total XRP held across all `ltACCOUNT_ROOT`, `ltPAYCHAN`, and `ltESCROW` objects
never **increases**.  New drops can only enter circulation from the treasury
account via `RewardEpoch`.  Burns (fee destruction) permanently reduce the total.
