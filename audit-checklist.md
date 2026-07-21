# qXRP Security Audit Checklist

> Status key: ✅ Done · ⚠️ Needs work · ❌ Not started · N/A Not applicable

---

## 1. Supply Conservation

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 1.1 | `kINITIAL_XRP` = 200 B qXRP — enforced by `static_assert` at compile time | ✅ | `QXRPConstants.h` |
| 1.2 | `kQXRP_GENESIS_ALLOCATION + kQXRP_TREASURY_ALLOCATION == kINITIAL_XRP` — enforced by `static_assert` | ✅ | `QXRPConstants.h` |
| 1.3 | `QXRPDropConservation` invariant fires on every transaction | ✅ | `src/libxrpl/tx/invariants/QXRPDropConservation.cpp` |
| 1.4 | `XRPNotCreated` invariant still active (upstream) | ✅ | Inherited |
| 1.5 | Treasury emission path (`RewardEpoch`) is the only source of new circulating drops | ⚠️ | Needs verification on new clean testnet (see NEW_TESTNET_BOOTSTRAP.md) |

## 2. Integer Arithmetic & Overflow

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 2.1 | No floating-point in fee-split, emission, or scoring paths | ✅ | Verified in `ApplyContext.cpp`, `RewardEpoch.cpp` |
| 2.2 | Emission bps shift never produces zero before hitting `kQXRP_MIN_EMISSION_BPS` floor | ✅ | `max()` in `RewardEpoch.cpp` |
| 2.3 | Fee-split `rawBurnBps` clamped to `[kFEE_BURN_MIN_BPS, kFEE_BURN_MAX_BPS]` before use | ✅ | `ApplyContext.cpp` |
| 2.4 | Composite score weight sum enforced by `static_assert` | ✅ | `QXRPConstants.h` |
| 2.5 | No integer overflow in `validatorShare = epochEmit * score / aggregateScore` | ⚠️ | Add defensive wide-arith comments + test on new net (see ClaimReward.cpp note) |
| 2.6 | Bond slash computation `slashBps * bondAmount / kBPS_DENOM` — verify no truncation issues | ⚠️ | Uses __int128 in ValidatorSlash.cpp — needs explicit test coverage |

## 3. Amendment Gating

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 3.1 | All 4 qXRP tx types return `temDISABLED` without `ProofOfParticipation` | ✅ | Stubs verified |
| 3.2 | `RewardEpoch` pseudo-tx cannot be submitted by external accounts | ⚠️ | Verify on new net (internal pseudo-tx only) |
| 3.3 | Governance transactions gated on amendment | ✅ | `GovernanceProposal.cpp`, `GovernanceVote.cpp` |

## 4. Validator Bond Security

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 4.1 | Only bond owner can call `ValidatorUnbond` | ✅ | `sfAccount` check |
| 4.2 | `kUNBONDING_LOCK_LEDGERS` (30 days) enforced before fund release | ✅ | `ValidatorUnbond.cpp` |
| 4.3 | Slash proof is validated before deducting bond | ✅ | Falcon STValidation double-sign crypto verify in preclaim/doApply; ABSENCE/INVALID_VOTE return `temDISABLED` |
| 4.4 | Double-sign slash forces UNBONDING — validator cannot re-bond without `ReleaseBond` | ✅ | `ValidatorSlash.cpp` |
| 4.5 | Slashed drops are burned (not paid to slasher) | ✅ | `destroyXRP` in `ValidatorSlash.cpp` (anti-griefing) |
| 4.6 | Minimum bond `kQXRP_MIN_BOND_DROPS` checked in `ValidatorBond` | ✅ | `ValidatorBond.cpp` |

## 5. Governance Security

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 5.1 | Duplicate vote per `(ProposalID, AccountID)` rejected | ✅ | `GovernanceVote.cpp` |
| 5.2 | Proposal expiry enforced — no votes after `sfExpiry` | ✅ | `GovernanceVote.cpp` |
| 5.3 | Supermajority threshold is 67 % of aggregate score | ✅ | `kGOVERNANCE_SUPERMAJORITY_BPS` |
| 5.4 | `sfCurrentBurnBps` bounded to `[4000, 7000]` even after governance update | ✅ | Defense-in-depth clamp added in GovernanceTally.cpp (preflight already rejects bad proposals) |

## 6. Post-Quantum Crypto (Falcon)

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 6.1 | Falcon library is optional (`-Dliboqs=OFF` → stubs, amendment inactive) | ✅ | `CMakeLists.txt` |
| 6.2 | `verifyFalcon` returns false (not crash) on malformed input | ⚠️ | Fuzz test needed |
| 6.3 | `PQPublicKey`/`PQSecretKey` variable-length buffers bounds-checked | ✅ | Strict length + prefix checks in constructors |
| 6.4 | Falcon keys not mixed with secp256k1 key slots | ✅ | Separate PQPublicKey/PQSecretKey types + secureErase in destructor |
| 6.5 | Secure zeroization of Falcon secret keys | ✅ | Now uses secureErase (OPENSSL_cleanse) instead of std::fill |

## 7. Consensus & Scoring

| # | Check | Status | Notes |
|---|-------|--------|-------|
| 7.1 | Composite score written only during ledger close (not by external tx) | ✅ | `NegativeUNLVote.cpp` |
| 7.2 | `sfAggregateCompositeScore` is sum of individual scores (no double-count) | ⚠️ | Verify accumulation logic |
| 7.3 | Score = 0 validator cannot earn rewards | ✅ | `kMIN_COMPOSITE_SCORE_BPS` check in `ClaimReward` |

## 8. Code Quality Gates (pre-audit)

| # | Check | Status |
|---|-------|--------|
| 8.1 | ASAN+UBSAN clean (`cmake -DSANITIZE=address,undefined`) | ❌ | See docs/security/security-testing.md |
| 8.2 | Fuzz corpus: tx parsing, Falcon verify, fee-split inputs | ✅ | FuzzFalconVerify, FuzzFeeSplit, FuzzClaimReward, FuzzValidatorScoring exist. CI integrates 3 standalone targets on every PR via `.github/workflows/qxrp-security.yml`. FuzzFalconVerify requires a full build (run manually — see docs/security/security-testing.md). |
| 8.3 | Test coverage ≥ 80 % on `src/libxrpl/tx/transactors/qxrp/` | ❌ | Target defined in docs/security/security-testing.md |
| 8.4 | `cppcheck --enable=all` zero findings on qXRP files | ✅ | Runs on every PR via `.github/workflows/qxrp-security.yml` (job: cppcheck). Covers all qXRP delta source files. |
| 8.5 | Static analysis: `clang-tidy` on qXRP translation units | ❌ | See docs/security/security-testing.md |

---

## Outstanding Action Items (post 2026-05-30 security fixes)

1. **Verify `RewardEpoch` privilege check** — ensure no external account can submit it. (Still open)
2. **Overflow audit** for `validatorShare` with large validator counts (> 1 000). (Still open)
3. ~~**Fuzz `verifyFalcon`** with libFuzzer corpus + integrate into CI.~~ ✅ FuzzClaimReward + FuzzValidatorScoring added; FuzzFeeSplit, FuzzClaimReward, FuzzValidatorScoring all integrated into qxrp-security.yml CI. FuzzFalconVerify still requires a full build; documented in security-testing.md.
4. ~~**Governance clamp** — confirm `sfCurrentBurnBps` is clamped when read by `ApplyContext`.~~ ✅ Defense-in-depth added in GovernanceTally.cpp
5. **ASAN/UBSAN run** on qXRP changes + full regtest. (Still open — high priority). See docs/security/security-testing.md
6. **liboqs supply chain** — pinned to exact commit f4b96220e4bd208895172acc4fedb5a191d9f5b1 in CMakeLists.txt. ✅
7. **Secure zeroization** of PQSecretKey. ✅ Now uses secureErase (OPENSSL_cleanse).
8. **Security Testing Guide** created at docs/security/security-testing.md. ✅ (M-01 progress)
9. **Slashing rollout** — only DOUBLE_SIGN active. Documented as known limitation for beta. ✅
10. **Falcon fuzzing** — basic target exists and improved; needs regular CI runs + corpus growth. ⚠️
