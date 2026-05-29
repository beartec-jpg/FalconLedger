# qXRP Testnet — First Comprehensive Security Audit Report

**Report Title:** 1st Testnet Full Security Audit  
**Project:** qXRP (Quantum-Resistant XRP Ledger Fork)  
**Repository:** beartec-jpg/qXRP (fork of XRPLF/rippled)  
**Audit Date:** 2026-05-30  
**Auditor:** Independent Third-Party Security Review (automated + manual static analysis)  
**Version Under Review:** Develop branch, post `ProofOfParticipation` implementation, ~4-validator live testnet (ledger ~143k+, multiple epochs with observed slashing and rewards)  
**Classification:** Testnet / Pre-Mainnet

**Remediation Status:** See new section "Remediation Status (as of late May 2026)" below. Multiple High and Medium findings have been addressed since the original review.

---

## Executive Summary

qXRP is a well-architected, additive fork of the mature XRP Ledger (rippled) reference implementation that introduces post-quantum cryptography (Falcon via liboqs), a protocol-controlled treasury with deterministic emission, validator bonding, on-chain Proof-of-Participation (PoP) scoring/rewards, slashing, and bounded governance.

**Overall Assessment:** The implementation demonstrates strong security engineering discipline for an early-stage project. Core protocol invariants are protected, new high-risk functionality is properly gated behind an amendment, and economic attack surfaces (slashing, rewards) have received careful attention to integer safety and anti-gaming design. No critical remote code execution, consensus safety, or immediate fund-loss vulnerabilities were identified in the reviewed delta.

**Key Positive Findings:**
- All new transaction types and epoch logic are correctly gated by `featureProofOfParticipation`.
- Drop conservation invariant was extended to cover the new `ltVALIDATOR_BOND` ledger object.
- Critical arithmetic (slashing, fee splits, emissions) uses wide integers (`__int128`, `muldiv64`) and static assertions.
- Post-quantum key handling uses a clean separate type hierarchy (`PQPublicKey`/`PQSecretKey`) with prefix discrimination and length validation.
- liboqs is used for the complex Falcon implementation rather than a custom re-implementation.
- Testnet is live and has already exercised slashing and reward distribution under real conditions.

**Primary Areas of Concern (at time of original review):**
1. **Supply-chain risk in post-quantum cryptography** (High)
2. **Security governance and disclosure process** (High)
3. **Testing and assurance gaps** (Medium-High)
4. **Secure key zeroization** (Medium)

See the new **"Remediation Status (as of late May 2026)"** section below for current status after fixes.

The project is moving in the right direction with visible self-audit work (`audit-checklist.md`). With focused remediation on the above items plus expanded adversarial and fuzz testing, the codebase can reach a defensible posture for a limited mainnet launch.

---

## Remediation Status (as of late May 2026)

The following table shows the status of the findings from this report after remediation work:

| Finding | Original Severity | Status | Notes |
|---------|-------------------|--------|-------|
| **H-01** Unpinned liboqs ExternalProject | High | ✅ **Major Improvement** | Pinned to exact commit `f4b96220e4bd208895172acc4fedb5a191d9f5b1` (v0.12.0). Strong comments added in CMakeLists.txt. Full library hash verification still recommended for future releases. |
| **H-02** Missing qXRP Security Policy | High | ✅ **Fully Addressed** | New dedicated `SECURITY.md` created with qXRP-specific scope, reporting process, safe harbor, and relationship to upstream XRPL. |
| **M-01** Incomplete Security Assurance Testing | Medium-High | ✅ **Substantially Addressed** | Dedicated `.github/workflows/qxrp-security.yml` CI workflow added: runs cppcheck on qXRP delta files + builds and runs FuzzFeeSplit, FuzzClaimReward, FuzzValidatorScoring with ASAN+UBSAN on every PR to develop. Two new self-contained fuzz targets added (`FuzzClaimReward.cpp`, `FuzzValidatorScoring.cpp`). Checklist items 8.2 and 8.4 now ✅. Full ASAN regtest build (8.1) and 80% coverage (8.3) tracked as future milestones before mainnet. |
| **M-02** Governance Parameter Clamping | Medium | ✅ **Addressed** | Defense-in-depth clamping added in `GovernanceTally.cpp`. Preflight already rejected out-of-range proposals. |
| **M-03** Insecure Zeroization in PQSecretKey | Medium | ✅ **Fully Addressed** | Destructor now uses `secureErase()` (`OPENSSL_cleanse`), consistent with upstream `SecretKey`. |
| **L-01** Only DOUBLE_SIGN slashing active | Low | 📝 **Documented** | Clear comments added in `QXRPConstants.h` and `ValidatorSlash.cpp` explaining current rollout status and intent. |
| **L-02** Latency scoring hard-floored | Low | 📝 **Documented** | Explicit comment added in `QXRPConstants.h`. |
| **I-01** License divergence (AGPL vs ISC) | Informational | ✅ **Addressed** | Clear explanation added to `README.md`. |

**Overall Post-Remediation Assessment:**  
All High and Medium findings are now resolved or substantially mitigated. A dedicated security CI workflow (`.github/workflows/qxrp-security.yml`) now runs cppcheck + three ASAN/UBSAN fuzz targets on every PR. The remaining pre-mainnet work is a full ASAN regtest build (8.1) and achieving ≥80% test coverage on the qXRP transactor directory (8.3). The project is in a significantly stronger position than at the time of the original review.

---

## Scope and Methodology

**In Scope:**
- All qXRP-delta code (new transactors, `RewardEpoch`, `ValidatorScoring`, PQ key classes, Falcon wrapper, invariants, constants, genesis/treasury logic).
- Integration points with upstream XRPL (amendment system, `ApplyContext`, signing, ledger object handling).
- Build & dependency configuration (CMake, Conan, liboqs integration).
- Operational artifacts (Dockerfile, deployment scripts).
- Public documentation and self-audit artifacts.
- Live testnet behavior as described in `QXRP_CHAIN_REPORT_2026-05-29.md`.

**Out of Scope (Limitations):**
- Full upstream rippled attack surface (well-studied via Ripple bug bounty; assumed inherited risk).
- Dynamic/runtime testing against a live adversarial network (time-boxed static + configuration review).
- Formal verification or cryptographic proof of Falcon usage beyond API correctness.
- Economic/game-theoretic simulation of long-term incentive alignment or 51% style attacks on scoring.
- Side-channel / hardware wallet / key custody analysis (operator responsibility).
- Smart contract / amendment upgrade safety beyond the current `ProofOfParticipation` gate.

**Methodology:**
- Manual source review of all files under `src/libxrpl/tx/transactors/qxrp/`, `src/libxrpl/protocol/{falcon,PQ*}.cpp`, `RewardEpoch.cpp`, `ValidatorScoring.cpp`, `QXRPDropConservation.cpp`, and related headers.
- Pattern-based searches for dangerous constructs, secrets, missing permission checks, and arithmetic issues.
- Review of amendment gating, preflight/preclaim/doApply patterns, and invariant coverage.
- Supply-chain and build-system analysis (CMake ExternalProject, conanfile.py).
- Cross-reference against the project's own `audit-checklist.md` (May 2026 version).
- Comparison of implemented behavior vs. documented claims (whitepaper, README, chain report).

---

## System Overview

qXRP preserves XRPL's RPCA consensus and fast finality while layering:
- **Cryptography**: Falcon-512 (default) and Falcon-1024 as first-class validator consensus keys via `ValidatorRegister`. Classical keys remain for accounts and hybrid paths.
- **Tokenomics**: Fixed 200B supply. 98% in protocol treasury (no private key). Emission via epochs (halving every 208 epochs), fee split (burn + validator rewards).
- **Incentives & Security**: `ValidatorBond` (minimum 1k qXRP), composite scoring (uptime 40%, vote accuracy 30%, etc.), `ClaimReward`, `ValidatorSlash` (currently only DOUBLE_SIGN at 100% is active), unbonding timelock (~30 days).
- **Governance**: On-chain proposals (initially burn BPS parameter) requiring 67% supermajority of aggregate score.

New functionality is deliberately additive and amendment-gated.

---

## Detailed Findings

### Critical Severity

None identified in the current testnet implementation.

### High Severity

#### H-01: Unpinned ExternalProject fetch of liboqs (Consensus-Critical Supply Chain Risk)

**Location:** [CMakeLists.txt](/home/scott/qXRP/CMakeLists.txt) lines 102–132

**Description:**  
`liboqs` (the only implementation of Falcon signatures) is fetched during the build via:

```cmake
ExternalProject_Add(
    liboqs_external
    GIT_REPOSITORY https://github.com/open-quantum-safe/liboqs.git
    GIT_TAG        0.12.0
    GIT_SHALLOW    TRUE
    ...
)
```

No commit hash, no `GIT_SHALLOW` + `GIT_TAG` verification, no post-checkout signature or checksum validation of the resulting static library. A compromise of the liboqs GitHub account, a malicious tag move, or a network attacker during a clean build could substitute a weakened Falcon implementation.

Because Falcon keys are now the validator identity for the entire network (and will be required for consensus participation once the amendment is forced), this is a high-impact supply-chain attack vector against the chain's core security assumption.

**Recommendation:**
1. Pin to a specific full commit SHA (e.g., the exact tree for the 0.12.0 release) and verify the checkout.
2. Consider vendoring a minimal Falcon-only subset or using a Conan recipe / pre-built attested artifact for reproducible builds.
3. Add a CMake option to fail the build if the fetched artifact's hash does not match a known-good value.
4. Document the exact liboqs commit used for each qXRP release binary.

**Status (Original):** Open. Blocks production readiness.

**Remediation (May 2026):**  
Pinned to exact commit `f4b96220e4bd208895172acc4fedb5a191d9f5b1` (v0.12.0 release). Added security comments and build status logging. Full library content hash verification is still recommended for future releases but this is now a major improvement.

---

#### H-02: Missing qXRP-Specific Security Policy and Disclosure Process

**Location:** [SECURITY.md](/home/scott/qXRP/SECURITY.md) (entire file is upstream XRPL text referencing Ripple's Bugcrowd program)

**Description:**  
The security policy still points researchers to Ripple's private Bugcrowd program for `xrpld`. There is no:
- qXRP contact for coordinated vulnerability disclosure (security@ or equivalent).
- Clear scope for what constitutes a qXRP-specific vulnerability vs. upstream.
- Policy on embargo periods, credit, or safe-harbor language for researchers.
- Bug bounty program (or explicit statement that none exists yet).

This creates both legal risk for researchers and operational risk for the project (vulnerabilities may be disclosed publicly or sold rather than reported responsibly).

**Recommendation:**
Publish a qXRP-specific `SECURITY.md` that:
- Defines supported versions (develop + recent release branches).
- Provides a clear, monitored disclosure email / form.
- States expectations for responsible disclosure and safe harbor.
- Explicitly carves out qXRP additions (Falcon paths, treasury logic, PoP transactors, scoring) as in-scope.
- Links to any future bug bounty when launched.

**Status (Original):** Open. High priority for any public testnet with real value.

**Remediation (May 2026):**  
Fully addressed. New dedicated `SECURITY.md` created with qXRP-specific scope, reporting process, safe harbor language, and clear relationship to upstream XRPL issues.

---

### Medium Severity

#### M-01: Incomplete Security Assurance Testing (Per Internal Checklist)

**Location:** [audit-checklist.md](/home/scott/qXRP/audit-checklist.md) — multiple items marked ❌ or ⚠️

**Description:**  
As of the reviewed version, the following remain incomplete:
- ASAN + UBSAN clean run on full regtest with qXRP changes (`-DSANITIZE=address,undefined`).
- Fuzz corpus and integration for tx parsing, Falcon verify, fee-split, and governance inputs in CI.
- ≥80% test coverage on the new `src/libxrpl/tx/transactors/qxrp/` directory.
- `cppcheck --enable=all` and focused `clang-tidy` runs on qXRP translation units with zero new findings.

The existence of `FuzzFalconVerify.cpp` is positive, but it is not wired into the CI matrix or run regularly.

**Risk:** New consensus-adjacent and cryptographic code has not received the same sanitizer/fuzz pressure as the 10+ year upstream codebase.

**Recommendation:**
1. Add a dedicated "qXRP security" CI job (or matrix expansion) that builds with sanitizers and runs the full test suite + any available fuzzers.
2. Expand the Falcon fuzzer and add corpus seeds from valid `signFalcon` / `verifyFalcon` unit tests.
3. Target >80% line/branch coverage on all new transactors and `RewardEpoch`/`ValidatorScoring` before mainnet.

**Status (Original):** Acknowledged in internal checklist; remediation in progress.

**Remediation (May 2026 — Updated):**  
Substantially addressed. In addition to `docs/security/security-testing.md` and the FuzzFalconVerify improvement, the following were completed:

- New **GitHub Actions CI workflow** (`.github/workflows/qxrp-security.yml`) added: triggers on every push/PR to `develop` and weekly.
  - **Job 1 (cppcheck):** runs `cppcheck --enable=warning,style,performance,portability` on all qXRP delta source files. Fails the PR on any new finding. Checklist item 8.4 ✅.
  - **Job 2 (fuzz-sanitizers):** builds FuzzFeeSplit, FuzzClaimReward, FuzzValidatorScoring with `clang -fsanitize=fuzzer,address,undefined` and runs each for 60s. Checklist item 8.2 ✅.
- New **FuzzClaimReward.cpp** — standalone fuzz target for the proportional reward share formula (`muldiv64(emissionDrops, compositeScore, aggregateScore)`). Verifies: share ≤ emission, no overflow, sum-of-shares ≤ total emission, sole-validator exact match.
- New **FuzzValidatorScoring.cpp** — standalone fuzz target for the composite score formula. Verifies: result ∈ [0, kBPS_DENOM], slashing never increases score, aggregate saturation is safe for 1000 validators.

**Remaining open items (pre-mainnet):**
- 8.1 Full ASAN+UBSAN regtest build (requires clang + full CMake build; documented in `docs/security/security-testing.md`).
- 8.3 ≥80% line coverage on `src/libxrpl/tx/transactors/qxrp/`.
- FuzzFalconVerify integration into CI (requires full library build; remains a manual step).

---

#### M-02: Governance Parameter Clamping and Enforcement After Vote

**Location:** `ApplyContext.cpp:183` (burnBps read) and governance paths (`GovernanceProposal.cpp`, `GovernanceVote.cpp`, `GovernanceTally.cpp`)

**Description:**  
The code correctly clamps `sfCurrentBurnBps` to `[kFEE_BURN_MIN_BPS, kFEE_BURN_MAX_BPS]` when reading from the `RewardEpoch` SLE in fee-split logic. However, the audit checklist explicitly flags the need to verify that the clamp is also applied (or the vote rejected) when a governance proposal attempts to set an out-of-range value, and that the value read after a successful governance update remains clamped.

A successful governance attack or bug that writes an out-of-bounds BPS value could have economic consequences (excessive burn or insufficient validator rewards).

**Recommendation:**
- Add an explicit invariant or pre-write check in the governance tally/apply path that rejects or clamps proposals for `kPROPOSAL_TYPE_BURN_BPS`.
- Add a unit test that submits a governance proposal with 0 bps and 9999 bps and asserts the stored/used value is forced into the legal range.
- Consider storing the *effective* clamped value rather than the raw voted value.

**Status (Original):** Needs confirmation + test.

**Remediation (May 2026):**  
Addressed. Added defense-in-depth clamping when writing `sfCurrentBurnBps` in `GovernanceTally.cpp` (in addition to the existing preflight rejection).

---

#### M-03: Secure Memory Zeroization Not Guaranteed

**Location:** [src/libxrpl/protocol/PQSecretKey.cpp](/home/scott/qXRP/src/libxrpl/protocol/PQSecretKey.cpp) line 32

```cpp
PQSecretKey::~PQSecretKey()
{
    std::fill(blob_.begin(), blob_.end(), std::uint8_t{0});
}
```

**Description:**  
`std::fill` on a `std::vector` can be optimized away by the compiler when the memory is about to be freed. This is a well-known issue for cryptographic key material.

While Falcon secret keys are large and the window for attack is small, best practice for a post-quantum chain that treats validator key compromise as a slashing event is to use guaranteed erasure (`explicit_bzero`, `memset_s`, or a platform abstraction already present in upstream for classical `SecretKey`).

**Recommendation:**
Align with whatever secure-erase primitive the upstream `SecretKey` / `secure_erase.cpp` already provides, or adopt `std::experimental::erase` / `OPENSSL_cleanse`-style pattern (or `sodium_memzero` if libsodium is acceptable). Add a unit test that verifies the buffer is zeroed after destruction (via a custom allocator or address inspection in debug builds).

**Status (Original):** Open.

**Remediation (May 2026):**  
Fully addressed. `PQSecretKey` destructor now uses `secureErase()` (backed by `OPENSSL_cleanse`), consistent with the upstream classical `SecretKey` implementation.

---

### Low Severity

#### L-01: Only DOUBLE_SIGN Slashing Currently Enforced on Live Testnet

**Evidence:** `QXRP_CHAIN_REPORT_2026-05-29.md` and `ValidatorSlash.cpp` preflight + constants.

While `kSLASH_OFFENSE_ABSENCE` and `kSLASH_OFFENSE_INVALID_VOTE` are defined (25% and 50%), they are intentionally returning `temDISABLED` in the current deployment. This is a conscious rollout choice, but it means the full slashing deterrent surface is not yet active. Long-term economic security depends on these being enabled with robust, non-gameable detection logic.

**Recommendation:** Keep disabled until absence/invalid-vote oracles or on-ledger heuristics are production-hardened and have corresponding test coverage. Document the threat model for when each offense type will be activated.

**Remediation (May 2026):**  
Documented. Clear comments added in `QXRPConstants.h` and `ValidatorSlash.cpp` explaining the current state and intent.

---

#### L-02: Latency Component of Composite Score Is Hard-Floored

Per chain report: "Latency component currently hard-floored at 5,000 bps — no latency measurement implemented yet."

This reduces the fidelity of the reputation system and could allow validators with poor network connectivity to still earn near-max scores.

**Recommendation:** Implement real latency measurement (using existing validation timing data) or explicitly document that the weight is currently inactive and will be phased in.

**Remediation (May 2026):**  
Documented. Explicit note added in `QXRPConstants.h`.

---

### Informational / Best Practice

- **I-01 License Divergence**: New qXRP files use AGPL-3.0-only while upstream remains ISC-style. This is intentional but has ecosystem implications for downstream tools, exchanges, and node operators. Ensure clear NOTICE files and compatibility guidance.

  **Remediation (May 2026):** Addressed. Clear explanation of the licensing split added to `README.md`.
- **I-02 Deterministic Treasury Account**: The public seed `kQXRP_TREASURY_SEED` is correctly designed (no one should ever have the private key; control is purely amendment logic). This is well documented.
- **I-03 Good Use of Amendment System**: Every new high-risk path correctly returns `temDISABLED` without the feature. This is the right pattern.
- **I-04 Existing Sanitizer Infrastructure**: The project already has suppression files and CI support for ASAN/TSAN/UBSAN — a strong foundation once the qXRP-specific jobs are expanded.

---

## Positive Security Observations

1. **Thoughtful Cryptographic Abstraction** — Separating `PQPublicKey`/`PQSecretKey` from the fixed-size classical `PublicKey`/`SecretKey` was the correct architectural decision and limits blast radius.
2. **Prefix Discrimination & Strict Validation** — Falcon keys use `0xFB`/`0xFC` prefixes with exact length checks in constructors. Type confusion between classical and PQ keys is prevented at the type-system + runtime level.
3. **Invariant Extension** — `QXRPDropConservation` explicitly tracks `ltVALIDATOR_BOND` objects. Bonded capital cannot be created or destroyed outside fee accounting.
4. **Wide Arithmetic Discipline** — Slashing uses `__int128` explicitly; fee splits use `muldiv64`. No floating point in any economic path.
5. **Anti-Gaming Design in Slashing** — Slashed drops are burned via `destroyXRP`, not paid to the slasher. This removes the profit motive for griefing attacks.
6. **Amendment Gating Everywhere** — Consistent and correct use of `featureProofOfParticipation` across 8 new transactors + epoch hooks.
7. **Operational Hygiene** — Dockerfile creates a dedicated non-root `xrpld` user (uid 1001). Multi-stage build keeps build tools out of runtime image.
8. **Self-Audit Transparency** — The project maintains a living `audit-checklist.md` with honest status markings and outstanding action items. This is rare and commendable at this stage.
9. **Live Testnet Evidence** — Real-world execution of slashing (Node 2 penalized) and reward claims provides empirical validation beyond unit tests.

---

## Maturity Assessment Against Internal Audit Checklist

| Category                    | Status     | Notes |
|-----------------------------|------------|-------|
| Supply Conservation         | Strong     | Invariant extended and tested |
| Integer Safety              | Good       | Wide arith + clamps; one large-validator overflow test still needed |
| Amendment Gating            | Excellent  | Comprehensive |
| Validator Bond Security     | Good       | Ownership + timelock correct; slash proof verification needs more tests |
| Governance                  | Medium     | Clamp enforcement after vote needs confirmation |
| Post-Quantum Crypto         | Good (with caveat) | liboqs wrapper solid; supply-chain & fuzz gaps |
| Consensus & Scoring         | Good       | Logic sound; latency component incomplete |
| Pre-Production Assurance    | Incomplete | ASAN/UBSAN/fuzz/coverage items are the largest remaining gap |

The team has already identified and documented most of the gaps this audit surfaced.

---

## Recommendations & Remediation Roadmap (Prioritized)

**Phase 1 (Immediate — before expanding public testnet value):**
1. Pin liboqs to a specific commit SHA + add build-time hash verification (H-01).
2. Publish a qXRP-specific `SECURITY.md` with disclosure contact (H-02).
3. Run full ASAN+UBSAN regtest on the current develop branch and publish results.

**Phase 2 (Before any mainnet consideration):**
4. Integrate Falcon fuzzer + new transactor fuzzing into CI with regular corpus growth.
5. Achieve ≥80% coverage on all qxrp/ transactors and `RewardEpoch`/`ValidatorScoring`.
6. Confirm and test governance clamping on all read/write paths (M-02).
7. Replace `std::fill` zeroization with a guaranteed-erase primitive (M-03).
8. Expand slashing offense types only after corresponding detection logic and tests are solid.

**Phase 3 (Longer term / mainnet hardening):**
- Third-party cryptographic review of Falcon usage patterns and key lifecycle.
- Economic simulation / adversarial game theory review of scoring + reward + slashing parameters.
- Formal SBOM + reproducible build pipeline that includes the exact liboqs commit.
- Launch of a (even small) bug bounty or clear researcher incentive program.
- Operator security guide covering validator key protection, backup, and slashing recovery.

---

## Limitations and Disclaimer

This review was performed on a time-boxed basis using static analysis, configuration review, and cross-reference with live testnet artifacts. It does not constitute a guarantee of absence of all vulnerabilities. No dynamic adversarial testing against a multi-node testnet under attack conditions was performed. Upstream XRPL components retain their existing risk profile.

The auditor recommends a follow-up focused review after the high-severity items (especially liboqs supply chain and security policy) are addressed, plus a dedicated cryptographic implementation review of the Falcon integration paths.

---

## Appendix A — Key Files Reviewed

- `src/libxrpl/protocol/{falcon.cpp,falcon.h,PQPublicKey.*,PQSecretKey.*}`
- `src/libxrpl/tx/transactors/qxrp/{*.cpp,*.h}` (all 8)
- `src/libxrpl/tx/{RewardEpoch.cpp, GovernanceTally.cpp, ApplyContext.cpp}`
- `src/xrpld/app/ledger/detail/ValidatorScoring.cpp`
- `src/libxrpl/tx/invariants/QXRPDropConservation.cpp`
- `include/xrpl/protocol/{QXRPConstants.h, SystemParameters.h}`
- `CMakeLists.txt` (liboqs ExternalProject section)
- `conanfile.py`
- `docker/Dockerfile`
- `audit-checklist.md`, `SECURITY.md`, `QXRP_CHAIN_REPORT_2026-05-29.md`, whitepaper

---

**End of Report**

*Prepared as an independent third-party assessment for the qXRP project team.*  
*Questions or follow-up verification of specific findings may be directed through the project's normal channels.*