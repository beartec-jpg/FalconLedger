# qXRP Security Testing Guide

This document outlines the recommended practices for security testing qXRP-specific code (Falcon cryptography, Proof-of-Participation transactors, rewards, slashing, governance, etc.).

## 1. Sanitizer Builds (High Priority)

When developing or reviewing qXRP changes, always build with sanitizers:

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSANITIZE=address,undefined \
  -Dxrpld=ON \
  -Dtests=ON
```

Then run the test suite:

```bash
ctest --output-on-failure -j$(nproc)
```

**Recommended sanitizers for qXRP work:**
- `address,undefined` (most important)
- `thread` (when working on concurrent scoring/reward logic)
- `memory` (with Clang, for use-after-free detection)

See also the sanitizers/ directory for existing suppression files.

## 2. Fuzzing

### Falcon Verification Fuzzer

The primary fuzzer for post-quantum cryptography is located at:

```
src/test/fuzz/FuzzFalconVerify.cpp
```

**Build (requires liboqs):**

```bash
clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
  -I include \
  src/test/fuzz/FuzzFalconVerify.cpp \
  -o fuzz_falcon_verify \
  -Lbuild/lib -lxrpl -loqs
```

**Run:**

```bash
mkdir -p corpus/falcon
./fuzz_falcon_verify corpus/falcon/ -max_len=8192 -timeout=30
```

**CI (H-02):** A **standalone** Falcon shape fuzzer (`FuzzFalconStandalone.cpp`) runs
on every PR via `.github/workflows/qxrp-security.yml` (no liboqs link required).
The full `FuzzFalconVerify` target still needs a library build with liboqs —
run it manually on freeze tags:

```bash
# After a full build with liboqs:
clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
  -I include FuzzFalconVerify.cpp -o fuzz_falcon_verify \
  -L build/lib -lxrpl -loqs   # adjust link line for your build
./fuzz_falcon_verify corpus/falcon/ -max_len=8192 -max_total_time=300
```

### CI fuzz targets (qxrp-security.yml)

| Target | Linkage | Invariants |
|--------|---------|------------|
| FuzzFeeSplit | standalone | burn BPS clamps |
| FuzzValidatorScoring | standalone | composite score bounds + slash mult |
| FuzzClaimReward | standalone | shares ≤ emission |
| FuzzEpochPoolCap | standalone | pay ≤ remaining pool (C-02) |
| FuzzFalconStandalone | standalone | Falcon key prefix + length shapes |

### Future Fuzz Targets (High Value)

- Governance proposal / vote parsing and tally logic
- Full-lib FuzzFalconVerify in CI (needs cached liboqs build)

## 3. Test Coverage Targets

**Goal:** ≥ 80% line + branch coverage on all code under:

- `src/libxrpl/tx/transactors/qxrp/`
- `src/libxrpl/tx/RewardEpoch.cpp`
- `src/libxrpl/tx/GovernanceTally.cpp`
- `src/xrpld/app/ledger/detail/ValidatorScoring.cpp`
- `src/libxrpl/protocol/{falcon,PQ*}.cpp`

Run coverage with:

```bash
cmake .. -Dcoverage=ON
make
ctest
# Then use gcov/lcov or the project's coverage tooling
```

## 4. Static Analysis

Run the following on qXRP translation units:

```bash
# clang-tidy (focus on new code)
clang-tidy -p build/compile_commands.json \
  src/libxrpl/tx/transactors/qxrp/*.cpp \
  src/libxrpl/protocol/falcon.cpp \
  src/libxrpl/protocol/PQ*.cpp

# cppcheck
cppcheck --enable=all --inconclusive \
  --suppress=missingIncludeSystem \
  src/libxrpl/tx/transactors/qxrp/ \
  src/libxrpl/tx/RewardEpoch.cpp
```

## 5. Continuous Integration

**Live:** `.github/workflows/qxrp-security.yml` runs cppcheck + ASAN/UBSAN fuzz
targets on every PR/push that touches the qXRP delta (and weekly on schedule).

**Still recommended locally before freeze:**

1. Full `-DSANITIZE=address,undefined` build + `ctest`
2. Full-lib FuzzFalconVerify for ≥5 minutes
3. clang-tidy on qxrp/ translation units
4. Coverage report for `transactors/qxrp/` (target ≥80%)

See also `docs/MAINNET_SECURITY_FREEZE.md`.

## 6. Threat Model Focus Areas

When testing, pay special attention to:

- Any path that can influence validator scores or rewards
- Falcon key handling and signature verification under malformed input
- Governance proposal value handling (especially burn BPS)
- Treasury balance and emission calculations
- Slashing proof validation (when implemented)

## 7. Current Status (as of 2026-05-30)

- liboqs is pinned to a specific commit
- Secure zeroization is in place for PQSecretKey
- Basic fuzz target exists for Falcon
- Internal audit checklist is being actively updated

See the main `audit-checklist.md` in the repo root for the latest status of individual checks.

---

**Last updated:** 2026-05-30 (following initial security audit)