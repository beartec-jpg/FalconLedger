# Open / rotating UNL — future amendment (not at genesis)

**Status:** Design commitment · **not implemented at genesis**  
**Depends on:** bonded-wide scoring + pay ∝ score (done in protocol; new image required)

## Genesis / bootstrap

- **UNL:** published by launch operators (safety net).
- **Pay:** every **bonded** validator with composite ≥ floor shares the validator
  emission pot **pro-rata by score** (no top-32 wipe).
- Joiners can **earn** without being on the bootstrap UNL once their
  validations are observed (`RELAY_UNTRUSTED_VALIDATIONS` default on).

## Later amendment (when network is sufficiently sized)

Target behaviour (subject to final spec):

| Rule | Intent |
|------|--------|
| UNL size ≈ **top 33%** of eligible bonded (min floor) | Trust scales with network |
| Eligible = bonded + min score | Performance required |
| Rotate ~**10% of seats** per epoch (or every N epochs) | Anti-entrenchment |
| Ties: oldest bond first, then rotate | Fair among equals |
| Delayed apply + hysteresis | Consensus stability |

Until the amendment is **enabled**, operators retain bootstrap UNL control.

## Enable conditions (policy examples)

- ≥ N bonded with score ≥ floor for M epochs, and/or  
- Bonded-score governance supermajority to enable, and/or  
- Time lock after genesis  

## Relationship to scoring

Scoring already measures bonded keys (UNL or not). The amendment only changes
**who is trusted for RPCA**, not the pay formula.
