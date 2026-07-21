# qXRP Validator Lifecycle

## States

```
(unregistered)
      │  ValidatorRegister
      ▼
  REGISTERED  (kBOND_STATUS_REGISTERED = 0)
      │  ValidatorBond (≥ kQXRP_MIN_BOND_DROPS)
      ▼
   BONDED      (kBOND_STATUS_BONDED = 1)
      │  ValidatorUnbond  ─────────────────────────────────────────┐
      ▼                                                             │
  UNBONDING   (kBOND_STATUS_UNBONDING = 2)                         │
      │  (kUNBONDING_LOCK_LEDGERS elapsed)                         │
      ▼                                                             │
(funds released)                             forced unbond on slash ┘
```

## Transactions

### ValidatorRegister
- Registers a validator public key on-chain.
- Creates an `ltVALIDATOR_BOND` ledger object.
- Requires the `ProofOfParticipation` amendment to be active.

### ValidatorBond
- Locks at least `kQXRP_MIN_BOND_DROPS` (1 000 qXRP) into the bond object.
- Moves status from REGISTERED → BONDED.
- A validator must be BONDED to earn a composite score and receive rewards.

### ValidatorUnbond
- Initiates the unbonding lock period (`kUNBONDING_LOCK_LEDGERS` = 262,800 ledgers ≈ 30 days).
- Moves status from BONDED → UNBONDING.
- During the lock period the bond amount is held; the validator cannot be slashed for new offenses after unbonding begins (past offenses may still be proven).

### ClaimReward
- Transfers accumulated `sfRewardAccumulator` drops from the bond object to the validator's account.
- Fails with `tecINSUFFICIENT_QUALITY` if `sfCompositeScore < kMIN_COMPOSITE_SCORE_BPS`.

## Slashing

### Offense Codes

| Code | Name | Slash % | Constant |
|------|------|---------|----------|
| 1 | Double-sign | 100 % | `kSLASH_DOUBLE_SIGN_BPS` |
| 2 | Sustained absence (3+ epochs) | 25 % | `kSLASH_ABSENCE_BPS` |
| 3 | Proven invalid vote | 50 % | `kSLASH_INVALID_VOTE_BPS` |

### ValidatorSlash
- Submits a `ValidatorSlash` transaction with **cryptographic proof** of the offense.
- **DOUBLE_SIGN only** (offense code 1): two Falcon-signed `STValidation` blobs,
  same consensus key, same ledger sequence, different ledger hashes.
  ABSENCE / INVALID_VOTE return `temDISABLED` until detection is production-ready.
- Deducts `slashBps * bondAmount / kBPS_DENOM` from the bond.
- A double-sign slash (100 %) forces the validator into UNBONDING immediately.
- **Slashed drops are burned** via `destroyXRP` (not paid to the slasher, not
  credited to the treasury) — removes the profit motive for grief slashing.

## Composite Score

The composite score determines reward share and ClaimReward eligibility.

```
rawScore = (uptime      * 40
         +  voteAccuracy * 30
         +  latencyScore * 15
         +  consistency  * 10) / 100

compositeScore = rawScore * slashMultiplier / 10000
```

Additive weights: 40 / 30 / 15 / 10 (slash multiplier is applied separately).
`kSCORE_WEIGHT_SLASH_MULT` (5) is bookkeeping for the multiplier path, not an
additive term. Enforced by `static_assert` that all five constants sum to 100.

**Latency (on-ledger):** for each ledger in the scoring window, score each
validator relative to the earliest trusted sign time of that ledger
(0 s lag → 10_000 bps; −100 bps per second of lag, floor 0). Epoch average
is written to `sfLatencyScoreBps`.

Scores are written each epoch by `ValidatorScoring` into `sfCompositeScore`
on the `ltVALIDATOR_BOND` object, and summed into `sfAggregateCompositeScore`
on the `ltREWARD_EPOCH` object.
