# Open / rotating UNL — future protocol amendment

**Status:** Planned post-genesis amendment · **not** at mainnet T0  
**Pay model (genesis):** already separate — all bonded validators paid ∝ composite score  

## Why two phases

| Phase | Trust (UNL) | Pay (emissions) |
|-------|-------------|-----------------|
| **Genesis / bootstrap** | Operator-published fixed UNL (safety net) | **All bonded** with score ≥ floor share pot by score |
| **Open trust (amendment)** | Protocol-governed dynamic UNL | Unchanged (still ∝ score) |

Pay is **not** gated on UNL membership after the scoring-pay image.  
UNL only decides **who closes ledgers** until the amendment enables open seats.

## Genesis (T0) — what operators do

1. Publish small bootstrap UNL (launch peers).  
2. Run image with **all-bonded pro-rata scoring** (`mainnet-v2`+).  
3. Anyone may bond + run a node → scored when validations are observed → **ClaimReward** share.  
4. Bootstrap UNL remains until amendment enable criteria are met.

## Amendment goals (decentralized UNL)

When the network is large enough and healthy:

### Eligibility
- Bonded (`BondStatus = bonded`)
- Composite score ≥ floor (same or higher than claim floor)
- Consensus key present on bond

### UNL size
```
unl_size = max(MIN_UNL, ceil(0.33 × eligible_count))
# e.g. MIN_UNL = 5 or 10 for safety
```

### Seat rotation (~10% per epoch)
```
rotate_n = max(1, round(0.10 × unl_size))
# each epoch (or every N epochs): drop rotate_n worst/longest-stale seats;
# add rotate_n best eligible not currently seated
```

### Ties (many perfect scores)
- Order by score desc, then **oldest bond / registration** first  
- Sliding window of `unl_size` over that ordered list  
- Rotate so oversubscribed perfect scorers take turns  

### Safety
- **Delayed apply** (compute at epoch N, activate N+1)  
- **Hysteresis** (enter threshold > leave threshold)  
- Optional: require min observed peer connectivity  
- Enable only by amendment majority + optional size/time gates  

### Enable criteria (policy examples)
- ≥ N eligible bonded for M consecutive epochs, and/or  
- Governance supermajority of bonded composite, and/or  
- Time lock after genesis  

Until **enabled**, behavior stays: **bootstrap UNL only**.

## What this does not change
- CID emission schedule  
- LP / AMM PoPL baskets  
- ClaimReward formula shape (`pot × score / aggregate`)  
- Bond / slash rules  

## Implementation status
- [x] Design written (this doc)  
- [x] Pay: all bonded ∝ score (protocol code + `mainnet-v2` image)  
- [ ] Amendment code + tests  
- [ ] Enable after live network sized  

## Relation to old ActiveSet K=32
ActiveSet rank-cut for **pay** is **removed**. Do not confuse with UNL size.  
Future open UNL may use ~33% of *eligible* validators for **trust seats**, which is unrelated to wiping composites for pay.
