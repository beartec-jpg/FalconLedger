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

**TODO (M-01):** Integrate this fuzzer into CI with a time budget on every PR targeting `develop`.

### Future Fuzz Targets (High Value)

We should add dedicated fuzzers for:

- Governance proposal / vote parsing and tally logic
- Fee split and burn BPS calculations under adversarial inputs
- Validator scoring inputs (uptime, vote accuracy, slash multipliers)
- ClaimReward share calculations with extreme aggregate scores

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

## 5. Continuous Integration Recommendations

We should add a dedicated "qXRP Security" CI job that:

1. Builds with `-DSANITIZE=address,undefined`
2. Runs the full test suite
3. Runs the Falcon fuzzer for a fixed time budget (e.g. 5–10 minutes)
4. Runs clang-tidy + cppcheck on qXRP files only
5. (Future) Generates coverage reports for the qxrp/ directories

Until this job exists, developers are expected to run sanitizer builds locally before submitting PRs that touch consensus-critical paths.

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