# Rehearsal results

> Fill by running `python3 scripts/ops/rehearsal-e2e.py` or paste manually after a full soak.
> **Never** put real mainnet secrets here.

- **When:** _(pending)_
- **Image:** `qxrp/xrpld:mainnet-v1` @ local `sha256:1ea8dc0baac85b9acc7be657a67879c1247f514e1b77ea1264cca208a17a1e38`
- **OCI labels:** `falcon.scoring=fluid-activeset-k`, revision `2eef57ea19df46434a4735ff8d749976e695821e`
- **BuildInfo --version git (embedded):** may show host `.git` tip at build; trust **image labels** + scoring rebuild of `ValidatorScoring.cpp`
- **PUBLIC_RPC:** _(rehearsal only)_
- **Network id:** `1099` (rehearsal) / do not use public mainnet id until T0

| Phase | Status | Detail |
|-------|--------|--------|
| health | _ | |
| payments | _ | |
| split | _ | |
| faucet | _ | |
| bond | _ | |
| lend | _ | |
| amm | _ | |
| bridge (Sepolia) | _ | deposit tx / withdraw tx |
| scoring (EMA / ActiveSet) | _ | bond fields after ≥256 ledgers |
| emissions | _ | use fast-epoch image for claimable pool |
| claims | _ | ClaimReward / LP claims |
| teardown | _ | wiped? |

## Tx hashes (public)

| Action | Hash / ref |
|--------|------------|
| Genesis split payments | |
| Faucet drip | |
| AMM supply / swap | |
| Lend supply / borrow / repay | |
| Bridge deposit | |
| Bridge withdraw | |
| ClaimReward | |

## Sign-off

- [ ] Ready for real T0 (`mainnet-v1` digest only; long epoch; first emission epoch 8)
- [ ] Blocked — fixes required
- [ ] Chain wiped; throwaway secrets destroyed; real ceremony keys unused

## Notes

_(bugs found, score observations, emission numbers)_
