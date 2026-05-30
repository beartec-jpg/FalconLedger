# qXRP Destructive Consensus Test Report

**Campaign Period:** ___________________________  
**Test Lead:** ___________________________  
**Prototype Network ID:** 999  
**Goal:** Break consensus, test slashing/resync/recovery, validate PoP behavior under extreme conditions

---

## Executive Summary

(Write 1-2 paragraphs after the campaign: overall health of the network, biggest surprises, key bugs found, and recommendations for the final release testnet.)

---

## Environment

- Node 1 (46.224.0.140): qxrp.service + signing proxy
- Node 2 (37.27.47.236): qxrp-node2.service
- Node 3 (37.27.47.236): qxrp-node3.service
- Node 4 (204.168.175.194): qxrp-node4.service

**Spare machines used:** (list any)

---

## Stages Executed

| Stage | Date/Time | Target Nodes | Duration | Outcome | Key Observations |
|-------|-----------|--------------|----------|---------|------------------|
| 0. Baseline + Load | | All | | | |
| 1. Single Dropout + Resync | | | | | |
| 2. Single + Slashing | | | | | |
| 3. Double Dropout | | | | | |
| 4. Partition | | | | | |
| 5. Key Rotation Chaos | | | | | |
| 6. Extended Recovery | | | | | |

---

## Major Findings & Bugs

### Critical
- 

### High
- 

### Medium / Interesting Behaviors
- 

---

## Recommendations for Final Release Testnet

1. 
2. 
3. 

---

## Artifacts Location

- Snapshots: `destructive_test_results/snapshots/`
- Logs: `destructive_test_results/logs/`
- Load logs: on Node 1 at `/tmp/qxrp_destructive_load.log`

---

**Sign-off:**  
Test completed on: _______________  
All passwords/keys rotated: [ ] Yes
