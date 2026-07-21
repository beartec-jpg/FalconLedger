# Rehearsal results — fast-epoch full run (2026-07-21)

- **When:** 2026-07-21T18:46Z – 18:59Z UTC (partial; halted at first epoch boundary)
- **Host:** `5.78.142.246` (val5), isolated `network_id=1099`
- **Image:** `qxrp/xrpld:mainnet-rehearsal-fast-epoch` (`de87256a5111`)
- **Labels:** epoch=256, first_emission=2, scoring=fluid-activeset-k
- **Launch pin (untouched):** `qxrp/xrpld:mainnet-v1` (`1ea8dc0baac8`)

## Phase results

| Phase | Status | Detail |
|-------|--------|--------|
| health | **PASS** | 3/3 vals, peers=2, proposers=2, `full`/`proposing` |
| payments | **PASS** | Genesis→DEV 1000 FALCON; faucet→vals; extra traffic pays |
| split | **PASS** | AIRDROP 2B, FAUCET 1B, DEV ~1B (script dry-run blocked after 1000 pre-pay; manual execute OK) |
| faucet drip | **PASS** | 100 FALCON to VAL1 `76ED2145…` |
| bond | **PASS** | All 3 ValidatorRegister + ValidatorBond (status=1, 1000 bonded) |
| lend | **SKIP** | Not run this pass (env/time); scripts exist |
| amm | **SKIP** | Not run this pass |
| bridge | **SKIP** | Sepolia not wired this pass |
| scoring | **PARTIAL** | Bonded before flag interval; crash at seq **256** before score write observed |
| emissions | **BLOCKED** | Epoch boundary crash (see bug) |
| claims | **BLOCKED** | Needs emission epoch |
| teardown | **PENDING** | Stack still up post-crash restart; wipe before T0 |

## Balances after split (pre-crash)

| Account | Role | Balance (approx) |
|---------|------|------------------|
| rHb9… | GENESIS | ~100 FALCON residual |
| rHUq… | AIRDROP | 2,000,000,000 |
| rGoS… | FAUCET | ~1B less drip/fund |
| rGHM… | DEV | ~999,999,900 |
| rP2p… / r9PM… / r9XA… | VAL1–3 | Funded + bonded |

## Critical bug found (must fix before re-soak / mainnet)

At **ledger 256** (first epoch boundary with `qxrp_epoch_override=256`):

```
terminate called after throwing an instance of 'xrpl::STObject::FieldErr'
  what():  Field 'LPAllocationBps' may not be explicitly set to default.
```

- **Where:** `applyRewardEpoch` in `RewardEpoch.cpp` always `setFieldU32(sfLPAllocationBps, lpAllocBps)` even when `lpAllocBps == 0`.
- **Impact:** All 3 validators aborted; docker restart; chain rewound/stalled from operator view.
- **Fix applied in tree:** only set `sfLPAllocationBps` when non-zero (same pattern as AMM alloc). Needs rebuild of fast-epoch **and** mainnet-v1 if that path can hit zero LP alloc on mainnet epoch 1–7 quiet period.

**Also noted:** genesis-split script rejects when genesis &lt; 4B after a smoke payment — use dry-run first or leave full 4B for execute.

## Sign-off

- [ ] Ready for real T0 — **NO** until RewardEpoch FieldErr fix is in both images and re-soak past epoch 2 + ClaimReward
- [x] Blocked — epoch boundary crash
- [ ] Chain wiped; throwaway secrets destroyed

## Next

1. Rebuild `mainnet-rehearsal-fast-epoch` + `mainnet-v1` with RewardEpoch fix  
2. Wipe rehearsal data, re-run from genesis  
3. Confirm scores after 256, emission pool after 512, ClaimReward  
4. Then lend/AMM/bridge phases  
