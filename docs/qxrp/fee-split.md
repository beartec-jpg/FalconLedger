# qXRP Dynamic Fee Split

## Overview

Every transaction fee in qXRP is split into two parts:

1. **Burn** — permanently destroyed (deflationary pressure).
2. **Reward pool** — routed to the treasury for future validator rewards.

The split is formula-driven and varies based on two on-chain signals, keeping the
burn fraction within the hard bounds `[kFEE_BURN_MIN_BPS, kFEE_BURN_MAX_BPS]`
= `[4 000, 7 000]` bps (40 %–70 %).

## Formula

```
treasuryPressure = (treasuryBalance * kBPS_DENOM) / kINITIAL_XRP.drops()
volumePressure   = feeVolumeEMA_bps   // stored in sfFeeVolumeEMA

rawBurnBps = kFEE_BURN_DEFAULT_BPS
           + (treasuryPressure * kFEE_TREASURY_SENSITIVITY_BPS / kBPS_DENOM)
           - (volumePressure   * kFEE_USAGE_SENSITIVITY_BPS    / kBPS_DENOM)

burnBps = clamp(rawBurnBps, kFEE_BURN_MIN_BPS, kFEE_BURN_MAX_BPS)
```

| Constant | Value | Meaning |
|----------|-------|---------|
| `kFEE_BURN_DEFAULT_BPS` | 5 500 | Default midpoint when no EMA data |
| `kFEE_BURN_MIN_BPS` | 4 000 | Hard floor (40 %) |
| `kFEE_BURN_MAX_BPS` | 7 000 | Hard ceiling (70 %) |
| `kFEE_TREASURY_SENSITIVITY_BPS` | 1 000 | Treasury-fill → burn pressure |
| `kFEE_USAGE_SENSITIVITY_BPS` | 500 | High volume → lower burn (more to treasury) |

All arithmetic is integer basis-point arithmetic; no floating-point is used.

## EMA Update

`sfFeeVolumeEMA` is updated at every ledger close using:

```
feeVolumeEMA = (feeVolumeEMA * (EMA_WINDOW - 1) + currentLedgerFees) / EMA_WINDOW
```

where `EMA_WINDOW` = 256 ledgers (~15 minutes).

## Governance Override

The active `burnBps` can be updated via a `GovernanceProposal` of type
`kPROPOSAL_TYPE_BURN_BPS`.  A successful governance vote (≥ `kGOVERNANCE_SUPERMAJORITY_BPS`
= 6 700 bps of aggregate composite score voting YES) overrides the formula result
for `sfCurrentBurnBps` until the next governance proposal supersedes it.

## Implementation Location

`src/libxrpl/tx/ApplyContext.cpp` — `computeBurnBps()` — called during the fee
debit step in `preflight2`.
