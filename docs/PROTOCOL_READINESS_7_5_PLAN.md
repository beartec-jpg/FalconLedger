# Protocol readiness 7.5 → 8.5+ plan (items 1–12)

**Date:** 2026-07-21  
**Freeze tip (at plan authoring):** `e3de0db11` on `develop` (refresh with `git rev-parse HEAD`)  
**Scope:** Protocol confidence only — not public T0 marketing. Dress rehearsal is assumed separately.

---

## Summary

| # | Item | Type | Owner | Status | Target |
|---|------|------|-------|--------|--------|
| 1 | Build freeze image + pin digest | Ops/tooling | Build host | **Script ready** — run on Docker host | Before soak |
| 2 | Multi-node soak (same image) | Ops | Fleet | **Runbook ready** | 1–7 days private |
| 3 | Adversarial / destructive pass | Ops/scripts | Fleet | **Checklist + hooks ready** | During soak |
| 4 | Full ASAN/UBSAN freeze build | Dev/CI | Build host | **Commands documented** | Parallel to soak |
| 5 | ABSENCE / INVALID_VOTE slash | Protocol | Eng | **Keep disabled** until detection | Post-mainnet OK |
| 6 | Absolute latency (RTT) | Protocol | Eng | **Defer** — relative scoring OK for v1 | Post-mainnet |
| 7 | Richer vote accuracy | Protocol | Eng | **Defer** — correct-hash count OK for v1 | Post-mainnet |
| 8 | Per-wallet claim snapshots | Protocol | Eng | **Optional harden** — pool cap already in | Pre- or post-T0 |
| 9 | Bridge end-to-end multi-party | Protocol/ops | Eng+ops | **Contract multi-sig done**; mint policy open | Before mainnet USDC |
| 10 | Lending surface assurance | Test/audit | Eng | **E2E scripts exist**; soak + audit | During soak |
| 11 | External audit of freeze delta | Assurance | Vendor | **Scope brief ready** | Before public T0 ideal |
| 12 | ≥80% coverage on `qxrp/` | CI/dev | Eng | **Stretch** — fuzz/unit exist | Ongoing |

**Implement now (in repo):** tooling + runbooks + audit scope for 1–4, 9, 11.  
**Do not implement now:** 5–7 product expansions that change economics mid-freeze.  
**Decide later:** 8 (extra claim freeze), 10 soak depth, 12 coverage gate.

---

## Phase A — Prove the freeze binary (items 1–4)

### 1. Build freeze image + pin digest

**Goal:** One immutable reference every node runs.

```bash
# On a Docker build host (not required in this workspace)
cd /path/to/FalconLedger
git fetch && git checkout e3de0db11   # or later freeze tag
bash scripts/ops/build-and-pin-image.sh mainnet-v1
# Writes scripts/mainnet-ceremony/IMAGE_DIGEST.txt
```

**Done when:**
- [ ] `docker run --rm $DIGEST xrpld --version` shows expected commit  
- [ ] Digest copied into ceremony pack and shared with all operators  
- [ ] No node uses floating `:latest`  

**Script:** `scripts/ops/build-and-pin-image.sh`

---

### 2. Multi-node soak

**Goal:** Same digest × ≥3 validators × private network × days of real closes.

**Runbook:** `docs/ops/SOAK_RUNBOOK.md`  
**Health check:** `scripts/ops/soak-health-check.sh`

**Minimum green criteria:**
- Ledgers advance continuously for soak window  
- No crash loops / OOM  
- No invariant fatals (`QXRPDropConservation`, etc.)  
- Node restart mid-soak resyncs  
- After soak: **wipe** — never reuse rehearsal secrets on real mainnet  

**Duration:** Prefer ≥1 epoch if testing claims; else multi-day consensus soak on mainnet-identical binary.

---

### 3. Adversarial / destructive pass (on soak fleet)

**Goal:** Confirm C-01/C-02 hold on the **binary**, not only in source review.

**Checklist:** `docs/ops/ADVERSARIAL_PROTOCOL_CHECKLIST.md`  
**Existing tooling:** `tools/destructive_testing/`

Must pass:
- [ ] Valid Falcon double-sign → slash succeeds, bond UNBONDING, burn occurs  
- [ ] Garbage slash evidence → rejected (`tecNO_PERMISSION` / not success)  
- [ ] ABSENCE / INVALID_VOTE → `temDISABLED`  
- [ ] Claim spam cannot pull more than epoch pool commitment  
- [ ] Peer disconnect / one validator down → network stays live  

---

### 4. Full ASAN/UBSAN freeze build

**Goal:** Catch memory/UB on freeze tag (complement CI fuzz).

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DSANITIZE=address,undefined -Dtests=ON -Dxrpld=ON
cmake --build build-asan -j"$(nproc)"
ctest --test-dir build-asan --output-on-failure -j"$(nproc)"
```

Also keep CI: `.github/workflows/qxrp-security.yml` (fuzz + cppcheck).  
Full-lib `FuzzFalconVerify` with liboqs: see `docs/security/security-testing.md`.

**Done when:** freeze tag ASAN suite green (or known suppressions documented).

---

## Phase B — Protocol design decisions (items 5–10)

### 5. ABSENCE / INVALID_VOTE slash

| Decision | **Keep `temDISABLED` for mainnet v1** |
|----------|--------------------------------------|
| Why | Enabling without robust detection re-opens grief risk |
| Later | Design detection + proofs + tests; amendment or soft enable |
| Work now | None in code — document only (this plan) |

### 6. Absolute latency (RTT)

| Decision | **Ship relative latency (earliest signer); defer RTT** |
|----------|------------------------------------------------------|
| Why | Absolute RTT needs peer instrumentation / clock policy |
| Later | Optional post-mainnet scoring enhancement |
| Work now | None required for freeze |

### 7. Richer vote accuracy

| Decision | **Ship correct-hash participation as accuracy proxy** |
|----------|------------------------------------------------------|
| Why | Wrong-vote index needs more validation history plumbing |
| Later | Post-mainnet scoring v2 |
| Work now | None required for freeze |

### 8. Per-wallet claim snapshots (optional harden)

| Decision | **Optional** — pool hard-cap already prevents treasury over-drain |
|----------|-------------------------------------------------------------------|
| Risk left | Intra-basket fairness if balances change after epoch open |
| If doing pre-T0 | Snapshot user share/score at epoch boundary into state or freeze fields |
| Work now | Spec only unless product demands pre-T0 fairness freeze |

**Spec sketch (if implemented later):**
1. At `applyRewardEpoch`, write frozen entitlement fields (or Merkle/list).  
2. Claims use frozen numerator/denominator only.  
3. Tests: mint after epoch → claim unchanged.

### 9. Bridge end-to-end multi-party

| Done | `FalconCollateralLock` N-of-M multi-sig; deploy script `REQUIRED≥2` |
|------|---------------------------------------------------------------------|
| Open | Redeploy mainnet lock with multi-sig owners; multi-party mint on Falcon side; HSM/relay policy |
| Work now | Ops checklist in plan + existing contract/scripts |
| Blocker for | Mainnet **USDC bridge only** — not pure Falcon consensus |

**Mainnet rule:** either `REQUIRED≥2` + documented key holders, or **bridge OFF** at T0.

### 10. Lending + claim surface

| Done | Lending txs, E2E scripts under `scripts/lend-e2e-*.py`, HF monitor |
|------|-------------------------------------------------------------------|
| Work | Run E2E against soak fleet; log failures; include in external audit scope |
| Runbook | `docs/ops/SOAK_RUNBOOK.md` § lending |

---

## Phase C — Assurance (items 11–12)

### 11. External audit (freeze delta)

**Scope brief:** `docs/security/EXTERNAL_AUDIT_SCOPE_FREEZE.md`

In scope (minimum):
1. `ValidatorSlash` evidence verification  
2. ClaimReward / ClaimLPReward / ClaimAmmLpReward + RewardEpoch  
3. ValidatorScoring (latency + aggregate)  
4. Falcon proposal verify + node identity bans  
5. Bridge multi-sig + relay trust model  

Out of scope: full upstream rippled history (unless vendor expands).

### 12. Coverage ≥80% on `qxrp/`

| Status | Stretch goal |
|--------|----------------|
| Now | Unit tests + CI fuzz invariants cover critical paths |
| Later | `cmake -Dcoverage=ON` + gate in CI when pipeline capacity allows |
| Not a T0 hard gate if 1–4 + 11 are green |

---

## Execution order (recommended)

```text
Week 0 (tools — DONE in repo)
  └─ build-and-pin script, soak runbook, adversarial checklist, audit scope

Week 1 (prove binary)
  ├─ (1) Build + pin image on Docker host
  ├─ (2) Stand up 3-node private net, start soak
  ├─ (3) Adversarial checklist mid-soak
  └─ (4) ASAN build on freeze tag (parallel)

Week 2 (close loop)
  ├─ Fix any soak/ASAN findings
  ├─ (9) Bridge policy decision: multi-sig deploy OR off at T0
  ├─ (10) Lending E2E on soak if shipping lend day-0
  └─ (11) Kick external audit with scope brief + freeze digest

Week 3+
  ├─ (5–8, 12) only if product requires pre-T0
  └─ Dress rehearsal + public T0 (ops track — separate)
```

---

## Definition of “protocol 8.5+”

- [ ] Freeze image digest known and matched on all soak nodes  
- [ ] Soak green (no crash / invariant fail; restart OK)  
- [ ] Adversarial checklist green (slash + claims)  
- [ ] ASAN freeze run green or documented exceptions  
- [ ] Bridge policy locked (multi-sig live or bridge off)  
- [ ] External audit of delta in progress or complete  

**9+** additionally: external audit clean + longer soak + no deferred high-risk product claims marketed as final.

---

## Repo artifacts for this plan

| Artifact | Path |
|----------|------|
| This plan | `docs/PROTOCOL_READINESS_7_5_PLAN.md` |
| Build + pin | `scripts/ops/build-and-pin-image.sh` |
| Soak runbook | `docs/ops/SOAK_RUNBOOK.md` |
| Soak health | `scripts/ops/soak-health-check.sh` |
| Adversarial checklist | `docs/ops/ADVERSARIAL_PROTOCOL_CHECKLIST.md` |
| External audit scope | `docs/security/EXTERNAL_AUDIT_SCOPE_FREEZE.md` |
| Ceremony pack | `scripts/mainnet-ceremony/` |
| Security freeze | `docs/MAINNET_SECURITY_FREEZE.md` |

---

*Update FREEZE_COMMIT and IMAGE_DIGEST as builds land. Do not mark items 1–4 complete without binary proof.*
