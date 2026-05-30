# qXRP Prototype Destructive Consensus Test Report

**Campaign Date:** 2026-05-30  
**Network:** qXRP Prototype Testnet (Network ID 999)  
**Test Type:** Full autonomous destructive testing  
**Duration:** ~2.5 hours of active fault injection  
**Operator:** Fully autonomous (minimal manual intervention after start)

---

## Executive Summary

A comprehensive destructive testing campaign was executed against the 4-validator qXRP prototype to stress consensus liveness, recovery, and Proof-of-Participation behavior under extreme conditions.

**Major Findings:**
- The network is **fragile at 2 validators** — ledger progress halts and proposers remain stuck at 2 for extended periods.
- Single validator loss under load frequently leads to degradation and stall.
- Full recovery to healthy consensus (proposers=3 + advancing ledger) generally required the return of the 4th validator.
- Long network partitions cause split/degraded state but the network converges cleanly once healed.
- Transaction submission via the signing proxy remained functional even during severely degraded consensus.

**Overall Assessment:** The current 4-validator configuration with quorum expectations of 3 shows clear liveness risks under fault conditions. This data is highly valuable for hardening the final release testnet.

---

## Environment

**Active Validators (all direct binary + systemd):**

| Node | Host | Service | Notes |
|------|------|---------|-------|
| Node 1 (LAC) | 46.224.0.140 | `qxrp.service` | Full history + Signing Proxy (:3001) |
| Node 2 (MILT) | 37.27.47.236 | `qxrp-node2.service` | Shared host with Node 3 |
| Node 3 (TATE) | 37.27.47.236 | `qxrp-node3.service` | Shared host with Node 2 |
| Node 4 (MATH) | 204.168.175.194 | `qxrp-node4.service` | — |

**Spare Machines:** 89.167.109.241 and 46.62.156.169 (unused during campaign)

**Key Configuration:**
- Amendment `ProofOfParticipation` enabled
- Validation quorum observed as 4 in some states (inconsistent with documented target of 3)
- Signing proxy used for all transaction submission during testing

---

## Test Phases Executed (Fully Autonomous)

### Phase 1: Baseline + Sustained Load
- Established normal operation under continuous load from genesis account via signing proxy.
- All 4 nodes stable: `full`, proposers=3, ledger advancing normally.

### Phase 2: Single Validator Dropout + Recovery
- Node 4 stopped under load.
- Network degraded: proposers dropped, eventual stall observed.
- Node 4 restarted → full recovery to proposers=3 within ~60 seconds.

**Finding:** Single node loss is sufficient to cause liveness problems under load.

### Phase 3: Double Validator Dropout + Staged Recovery
- Node 3 + Node 4 stopped simultaneously.
- With only 2 validators remaining:
  - Proposers stuck at 2
  - Ledger progress halted for extended period (Node 2 remained at seq ~168586 for a long window)
- Staged recovery:
  - Node 4 back → still only proposers=2, no progress
  - Node 3 back → immediate full recovery (all nodes `full`, proposers=3, ledger advanced to 168665)

**Critical Finding:** 2-validator mode provides extremely limited liveness. Recovery required the 4th validator in this test.

### Phase 4: Extended Network Partition
- Node 1 + Node 4 isolated from 37.27.47.236 host (Node 2 + Node 3) via iptables for >10 minutes.
- Load continued during partition.
- Network entered split/degraded state.
- After healing: clean convergence, all nodes returned to healthy state (proposers=3, same ledger).

**Finding:** Partitions are survivable but the network operates in a degraded split-brain-like state while partitioned.

### Phase 5: Fault + Transaction Submission Chaos (Option C Style)
- Node 4 dropped.
- Heavy batch of transactions submitted via signing proxy while consensus was degraded.
- Node 4 brought back.
- Full recovery achieved.

**Finding:** Signing proxy continued to function and accept transactions even when the network was in a severely degraded state (proposers=2).

---

## Key Findings & Recommendations

### Critical / High Severity

| # | Finding | Impact | Recommendation |
|---|---------|--------|----------------|
| 1 | 2-validator mode causes near-total liveness failure | High | Strongly consider lowering effective quorum or adding more validators before final testnet |
| 2 | Single validator loss under load frequently leads to stall | High | Improve validator scoring / NegativeUNL responsiveness or add more redundancy |
| 3 | Recovery often requires the 4th validator | Medium-High | Validate that the documented quorum of 3 is actually configured and enforced consistently |
| 4 | Long partitions leave the network in degraded state | Medium | Add better partition detection / healing telemetry |

### Medium / Operational

- Transaction submission via signing proxy is resilient even during consensus degradation (positive for availability).
- All nodes recovered cleanly once full validator set was restored in most cases.
- Need better visibility into actual proposers vs expected quorum during faults.

---

## Recommendations for Final Release Testnet

1. **Re-evaluate validator count and quorum**
   - Current behavior suggests 4 validators with effective quorum near 4 is too fragile.
   - Strongly consider 5+ validators or explicit quorum=3 enforcement with clear documentation.

2. **Improve fault detection and scoring**
   - Downtime and partition scenarios should trigger faster and more reliable scoring penalties / NegativeUNL updates.

3. **Add chaos testing to CI / release process**
   - The patterns used here (single/double dropout, partitions, tx submission during faults) should be repeated on the final testnet before mainnet.

4. **Strengthen monitoring**
   - Add alerts for "proposers < expected quorum" and stalled ledger sequences.
   - Instrument the signing proxy with health metrics during degraded consensus.

5. **Credential & Key Hygiene**
   - All root passwords and the signing proxy token used during this campaign must be rotated before any further testing.

---

## Artifacts

**Location:** `destructive_test_results/`

- **Snapshots:** 15+ JSON state captures (before, during, and after every major fault)
- **Logs:** 8+ sets of journalctl output from all nodes across phases
- **Load logs:** `/tmp/qxrp_*_load.log` on Node 1 (various phases)

Notable snapshots for review:
- Immediately after double dropout
- During long partition
- Post full recovery states

---

## Campaign Notes

- All testing was performed with **minimal manual intervention** after the initial "full auto" instruction.
- The campaign successfully stressed the exact areas requested (consensus breaking, resync, slashing-adjacent behavior via scoring, and validator lifecycle under stress).
- No permanent damage was done (throwaway prototype).
- The network was left in a healthy state (all 4 nodes `full`, proposers=3, ledger advancing) at the end of the campaign.

---

**Report Generated:** 2026-05-30 (autonomous)  
**Next Recommended Action:** Rotate all credentials used, review key snapshots, and apply findings to final release testnet configuration and monitoring.

