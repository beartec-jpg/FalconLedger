# qXRP Prototype — Full Destructive Consensus Test Plan

**Status:** Production-ready plan for throwaway prototype testnet  
**Date:** 2026-05-30  
**Objective:** Deliberately break consensus liveness, induce slashing/scoring edge cases, test recovery/resync, and validate validator lifecycle under extreme conditions before wiping the network for the final release testnet.

---

## 1. Environment Overview (Current Discovered State)

**Active Validators (4 nodes):**

| Node | Hostname              | IP              | Systemd Service          | Config Path                          | Running User | Special Notes                     |
|------|-----------------------|-----------------|--------------------------|--------------------------------------|--------------|-----------------------------------|
| 1    | ubuntu-8gb-fsn1-1     | 46.224.0.140    | `qxrp.service`           | `/etc/qxrp/xrpld.cfg`                | qxrp         | Full history + Signing Proxy :3001 |
| 2    | ubuntu-4gb-hel1-3     | 37.27.47.236    | `qxrp-node2.service`     | `/var/lib/qxrp/node2/xrpld.cfg`      | qxrp         | Shared host with Node 3           |
| 3    | ubuntu-4gb-hel1-3     | 37.27.47.236    | `qxrp-node3.service`     | `/var/lib/qxrp/node3/xrpld.cfg`      | qxrp         | Shared host with Node 2           |
| 4    | ubuntu-4gb-hel1-6     | 204.168.175.194 | `qxrp-node4.service`     | `/etc/qxrp/xrpld.cfg`                | root         | —                                 |

**Spare Machines** (ready for extra validators or load generators):
- 89.167.109.241
- 46.62.156.169

**Control Method:** All nodes use clean `systemctl` units. No Docker on active nodes.

**Public RPC Endpoints:**
- Node 1: http://46.224.0.140:6005
- Node 2: http://37.27.47.236:6006
- Node 3: http://37.27.47.236:6007
- Node 4: http://204.168.175.194:6005

**Signing Proxy (Node 1 only):** http://46.224.0.140:3001/sign

---

## 2. Test Philosophy & Safety Rules

- This is a **throwaway prototype**. We are allowed (and encouraged) to break things.
- We will **gradually** increase destruction.
- Every major action must be logged with timestamps.
- We will capture state snapshots before and after every major fault.
- We will allow **recovery windows** between aggressive stages.
- All root passwords/keys used during testing **must be rotated** immediately after the campaign.

---

## 3. Staged Test Campaign (Progressive Difficulty)

### Stage 0: Baseline & Sustained Load (Mandatory Starting Point)

**Goal:** Establish normal behavior under load and capture clean reference data.

**Actions:**
- Start continuous load generation via signing proxy (Node 1)
- Run for 30–60 minutes
- Capture full state snapshot + logs from all 4 nodes every 15 minutes

**Key Metrics to Record:**
- Ledger close times
- Proposers count vs validation_quorum
- Peer counts
- Composite scores (if queryable)
- Any slashing events

**Success Criteria:** Network stays healthy with 4/4 validators proposing.

---

### Stage 1: Single Validator Dropout + Resync (Core Liveness Test)

**Goal:** Test what happens when one validator disappears under load, then returns after a significant gap.

**Procedure:**
1. Start load.
2. Stop one validator (recommend starting with Node 4).
3. Wait 15–40 minutes (force ledger gap).
4. Capture state from remaining 3 nodes.
5. Restart the downed validator.
6. Monitor resync time, scoring impact, and any slashing.
7. Allow full recovery (minimum 30–60 min).

**Repeat** for Node 2 and Node 3 (shared host test is valuable).

**What We Want to Break/Observe:**
- How long resync takes after 10k–30k+ ledger gap
- Whether scoring drops appropriately for downtime
- Whether the network stays live with 3 validators

---

### Stage 2: Single Dropout + Manual Slashing While Down

**Goal:** Combine absence with active slashing and observe recovery of a penalized validator.

**Procedure:**
1. Stop a validator.
2. While it is down, submit a `ValidatorSlash` transaction (DOUBLE_SIGN or high absence).
3. Wait for slash to be applied on-chain.
4. Bring the validator back online.
5. Observe whether it can participate again, how scoring recovers over epochs, and any special behavior in `ClaimReward`.

**High value for final testnet** — this is new qXRP logic.

---

### Stage 3: Double Validator Loss (Attempt to Stall Consensus)

**Goal:** Push the network to (or past) its liveness boundary.

**Procedure:**
1. Start heavy load.
2. Stop two validators simultaneously (good combinations to test: Node 2+3 on same host, or Node 1+4, or Node 3+4).
3. Monitor whether the remaining two can still close ledgers.
4. Leave them down for 20–60+ minutes.
5. Bring one back → observe partial recovery.
6. Bring the second back → full recovery test.

**Critical observations:**
- Does the network stall cleanly?
- How long does it take to regain liveness?
- Any corruption or stuck `RewardEpoch` state?
- Behavior of the two surviving validators under extreme load.

---

### Stage 4: Network Partition & Flaky Links

**Goal:** Simulate real-world network faults (most realistic failure mode).

**Techniques (in order of difficulty):**
- Block peer ports between specific nodes using `iptables`
- Add high latency + packet loss with `tc`
- Full split (e.g. Node 1+2 vs Node 3+4)
- Partial / flapping connectivity

**Duration:** 30–90 minutes per partition, then heal and observe convergence.

**High value** for understanding how NegativeUNL + scoring interact with partitions.

---

### Stage 5: Validator Lifecycle Chaos (Option C — Key Rotation Under Stress)

**Goal:** Test `ValidatorRegister`, key rotation, bonding/unbonding while the network is already damaged.

**Ideas:**
- While one or two validators are down, perform `ValidatorRegister` with a **new Falcon key** on a downed node and bring it back.
- Unbond a validator that was heavily slashed during chaos.
- Register a 5th temporary validator from one of the spare machines during a double-dropout, then remove it.
- Attempt rapid key rotation on a validator that is currently proposing.

This is the most advanced and highest-risk stage.

---

### Stage 6: Extended Recovery + Final Data Collection

**Goal:** Let the network run for several hours after the worst damage and observe long-term healing (scoring recovery, reward claims, epoch behavior).

**Actions:**
- Run normal load for 4+ hours after last major fault.
- Perform multiple `ClaimReward` transactions.
- Capture final on-chain state (all `ValidatorBond` objects, latest `RewardEpoch`).
- Pull final logs from all nodes.

---

## 4. Monitoring & Data Collection Requirements

**Mandatory at every major event (before/after every stop/start):**
- `server_info` from all 4 public RPCs
- `validators` (if accessible) or pubkey data
- Recent ledger sequence + age
- Journalctl logs (last 1000 lines) from the affected service(s)
- `systemctl status <service>`

**Recommended continuous monitoring:**
- Existing Prometheus + Grafana stack (if running)
- Simple polling script that records proposers, ledger seq, and load_factor every 30 seconds

**Artifacts to preserve:**
- All journalctl output
- State snapshots (JSON)
- Load generator logs
- On-chain transaction hashes for important events (slashes, registers, claims)

---

## 5. Execution Recommendations

**Preferred Approach:**
- Run **one major stage per session** (spread across hours or days).
- Use the Python orchestrator for automation where possible.
- Manually trigger the most dangerous actions (double drop, partitions, key rotations) with human oversight.
- Always have at least one node that remains stable as an observation point.

**Rollback / Emergency Recovery:**
- All nodes have `Restart=on-failure` in systemd.
- Worst case: `systemctl restart <service>` on affected nodes.
- If a node falls extremely far behind, it may need a long time to resync (plan for this).

---

## 6. Post-Test Actions (Mandatory)

1. **Immediately rotate** all passwords and keys used during testing (including signing proxy token).
2. Generate the final `DESTRUCTIVE_TEST_REPORT.md` using collected artifacts.
3. Document any bugs or unexpected behaviors found (especially in slashing, scoring, resync, or Falcon key handling).
4. Decide what must be fixed or hardened before spinning up the real release testnet.
5. Wipe the entire network when ready.

---

## 7. Risk Matrix

| Risk | Likelihood in Prototype | Impact | Mitigation |
|------|-------------------------|--------|----------|
| Complete consensus stall | High in Stage 3+ | Medium (throwaway net) | Allow long recovery windows |
| One node falls 50k+ ledgers behind | High | Low | Plan long resync time |
| Accidental data corruption | Low | Medium | Use spares if needed |
| Signing proxy abuse during test | Medium | Low | Rotate token immediately after |

---

**This plan is designed to give you maximum signal before you destroy the prototype.**

Run it in whatever order and depth makes sense for your timeline. The Python automation + helper scripts in this directory are built to support exactly this staged approach.

Next step: Use the updated `qxrp_destructive_consensus_test.py` and the helper scripts to execute the campaign.