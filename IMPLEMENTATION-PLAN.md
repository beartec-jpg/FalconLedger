# qXRP Implementation Plan — Regtest to Local Testnet

Copyright (c) 2026 qXRP Team. All rights reserved.

This document is the authoritative task list for taking qXRP from a documentation-only state to a running local testnet. It is organized as sequential phases. Each phase ends with a concrete build-and-test milestone that must pass before the next phase begins.

The plan is grounded in the actual source layout of the XRPLF/rippled `develop` branch. Every file path listed here is real and has been verified in the repository.

---

## Architecture Constraints Established By Upstream Code

Before planning changes, note the following hard constraints from the existing code:

- `PublicKey` is a fixed 33-byte struct (`include/xrpl/protocol/PublicKey.h`). Falcon-512 public keys are 897 bytes; Falcon-1024 are 1793 bytes. This means `PublicKey` must become variable-length or a separate type hierarchy must be introduced. This is the most invasive single change in the entire project.
- `SecretKey` is a fixed 32-byte struct (`include/xrpl/protocol/SecretKey.h`). Falcon secret keys are much larger. Same solution needed.
- `kINITIAL_XRP` is a `constexpr` in `include/xrpl/protocol/SystemParameters.h` set to `100'000'000'000 * kDROPS_PER_XRP`. Changing this value touches every invariant check and every test that asserts the total supply.
- Fee destruction goes through `rawDestroyXRP()` / `destroyXRP()` in `include/xrpl/ledger/RawView.h`, `OpenView.h`, and `ApplyContext.h`. The split logic belongs at this boundary.
- The amendment system is defined by `include/xrpl/protocol/detail/features.macro`. New features are registered there.
- The genesis ledger is created in `src/xrpld/app/main/Application.cpp::startGenesisLedger()`.

---

## Phase 0 — Fork Hygiene and Build Verification

Goal: confirm the base builds cleanly on the dev machine and set up the qXRP-specific context.

### 0.1 Create qXRP development branch

```
git checkout develop
git checkout -b qxrp/dev
```

### 0.2 Verify baseline build

Follow `BUILD.md`. Confirm `xrpld` compiles to completion and the upstream unit test suite passes with `ctest` (or the native test runner).

This baseline pass score is the regression floor. Any phase that breaks tests already present in the upstream suite must be treated as a regression, not a feature.

### 0.3 Tag the baseline

```
git tag qxrp-baseline-0
```

### 0.4 Milestone 0

`make && ctest` passes with no new failures. The tag exists.

---

## Phase 1 — Protocol Constants and Genesis Treasury

Goal: change the total supply to 200 billion, define the qXRP token drops constant, and create a deterministic treasury account in the genesis ledger.

### Files to change

| File | Change |
| --- | --- |
| `include/xrpl/protocol/SystemParameters.h` | Change `kINITIAL_XRP` to `200'000'000'000 * kDROPS_PER_XRP`. Add `constexpr XRPAmount kQXRP_GENESIS_ALLOCATION` for 2% (4 billion tokens). Add `constexpr XRPAmount kQXRP_TREASURY_ALLOCATION` for 98% (196 billion tokens). |
| `include/xrpl/protocol/SystemParameters.h` | Update both `static_assert` lines for the new drop count. |
| `src/xrpld/app/main/Application.cpp` | In `startGenesisLedger()` (line 1660), after the standard genesis sequence completes, add code to create a treasury account ledger object holding `kQXRP_TREASURY_ALLOCATION`. The treasury account address should be derived deterministically from a well-known seed reserved for this purpose. |
| `src/test/app/Regression_test.cpp` | Update the expected total-drops assertion at line 93. |

### New file

`include/xrpl/protocol/QXRPConstants.h` — defines treasury account seed, treasury account ID constant, and the halving schedule constants (ledgers per period, initial emission rate). This file should carry the qXRP copyright header.

### Treasury account design

The treasury account is a normal XRPL account with:
- Balance set to `kQXRP_TREASURY_ALLOCATION` at genesis.
- `asfRequireAuth` and `asfDisallowXRP` flags cleared.
- No regular key or signers at genesis. Withdrawals are only possible through the `ProofOfParticipation` amendment logic once that is enabled.

### Milestone 1

`ctest` passes. `account_info` on the genesis treasury account returns the correct balance. The genesis ledger total drops equal `kINITIAL_XRP` (200B * drops_per_xrp).

---

## Phase 2 — Falcon Signature Support

Goal: add Falcon-512 as a first-class key type. This is the most structurally disruptive phase and must be completed before any transaction-signing tests for qXRP are written.

### 2.1 Dependency decision

Falcon is not in the existing Boost or OpenSSL dependency set. The cleanest option is the reference implementation from the NIST PQC submission or a library such as `liboqs` (Open Quantum Safe). Decide and document this in `conanfile.py` or `cmake/deps/`. Add the library as a Conan package or CMake FetchContent dependency.

### 2.2 Variable-length key abstraction

Current `PublicKey` is unsuitable for Falcon. Options:

- Option A (recommended): keep `PublicKey` as the 33-byte classical key and add a new `PQPublicKey` class with a `std::vector<uint8_t>` blob plus a `KeyType` discriminator. Introduce a `CryptoKey` variant type (`std::variant<PublicKey, PQPublicKey>`) at transaction and validator levels.
- Option B: extend `PublicKey` to a heap-allocated blob with a size field and a type tag. More invasive because every place that takes a `PublicKey const&` by value is affected.

Option A is preferred because it minimizes the blast radius.

### Files to add

| File | Purpose |
| --- | --- |
| `include/xrpl/protocol/PQPublicKey.h` | Falcon public key class. Variable-length blob + type tag. Serialization compatible with XRPL's existing `Slice`-based pattern. |
| `include/xrpl/protocol/PQSecretKey.h` | Falcon secret key. Zeroes memory on destruction (same guarantee as `SecretKey`). |
| `src/libxrpl/protocol/PQPublicKey.cpp` | Implementation. |
| `src/libxrpl/protocol/PQSecretKey.cpp` | Implementation. |
| `include/xrpl/crypto/falcon.h` | Thin C++ wrapper around the chosen Falcon C library. `generateFalconKeyPair()`, `signFalcon()`, `verifyFalcon()`. |
| `src/libxrpl/crypto/falcon.cpp` | Implementation. |

### Files to change

| File | Change |
| --- | --- |
| `include/xrpl/protocol/KeyType.h` | Add `Falcon512 = 2` and `Falcon1024 = 3` to the `KeyType` enum. Extend `keyTypeFromString()` and `to_string()`. |
| `src/libxrpl/protocol/PublicKey.cpp` | The `publicKeyType()` detection function currently looks at the first byte. Add detection for Falcon keys (choose a prefix byte not used by secp256k1 or ed25519, e.g. `0xFB` for Falcon). |
| `src/libxrpl/protocol/Sign.cpp` and `include/xrpl/protocol/Sign.h` | Extend `sign()` and `verify()` to dispatch to Falcon paths when the key type is `Falcon512` or `Falcon1024`. |
| `src/libxrpl/protocol/STTx.cpp` | Ensure `checkSign()` calls the extended verification path. |
| `src/libxrpl/protocol/STValidation.cpp` | Same for validation messages. |

### Serialization note

The XRPL wire format for `STTx` stores the signing public key as an `STBlob` of variable length. Falcon public keys will simply be longer blobs. No wire format change is needed, but the `PublicKey` constructor that takes a `Slice` needs to accept larger inputs when the prefix byte indicates Falcon.

### Milestone 2

A unit test (`src/test/protocol/Falcon_test.cpp`) keypair-generates, signs a message, and verifies it correctly. A negative test confirms that a forged signature fails. Both tests pass.

---

## Phase 3 — ProofOfParticipation Amendment Scaffold

Goal: register the new amendment and add the minimal ledger objects and transaction types it needs, gate them behind the amendment flag so the existing suite still passes without it.

### 3.1 Register the amendment

Add to `include/xrpl/protocol/detail/features.macro` (at the top of the active list):

```
XRPL_FEATURE(ProofOfParticipation, Supported::No, VoteBehavior::DefaultNo)
```

This generates `featureProofOfParticipation` automatically.

### 3.2 New transaction types

Add to `include/xrpl/protocol/detail/transactions.macro`:

- `ValidatorRegister` — one-time registration that links a validator's consensus key to a reward-eligible identity and marks it ready for bonding. Separating registration from bonding keeps the bond transaction focused on economic commitment and makes it possible for a validator to be known on-ledger before it has locked capital.
- `ValidatorBond` — a registered validator locks a bond amount into a bonding ledger object.
- `ValidatorUnbond` — initiates an unbonding sequence with a time lock.
- `ClaimReward` — a qualifying, bonded validator claims accumulated reward drops from the treasury for a closed epoch.

Four transaction types instead of three. Registration (`ValidatorRegister`) is intentionally separated from capital commitment (`ValidatorBond`) so operators can test ledger connectivity and scoring before locking funds.

### 3.3 New ledger object types

Add to `include/xrpl/protocol/LedgerFormats.h` and its corresponding `.cpp`:

- `ltVALIDATOR_BOND` — stores bond amount, validator public key, bond status, slashing history, and accumulated reward balance.
- `ltREWARD_EPOCH` — one per reward epoch, stores the emission rate for that epoch, the epoch start ledger, and the aggregate qualifying-validator score used to divide rewards.

### 3.4 New SFields

Add to `include/xrpl/protocol/SField.h` (and `SField.cpp`):

- `sfValidatorBondAmount` (STAmount, XRP)
- `sfBondStatus` (STUInt8: bonded / unbonding / slashed)
- `sfReputationScore` (STUInt64)
- `sfEpochStartLedger` (STUInt32)
- `sfEmissionRate` (STUInt64, drops per qualifying validator per epoch)

### 3.5 Transactor stubs

Add skeleton transactor files (compile but return `temDISABLED` unless the amendment is active):

- `src/libxrpl/tx/transactors/ValidatorRegister.cpp`
- `src/libxrpl/tx/transactors/ValidatorBond.cpp`
- `src/libxrpl/tx/transactors/ValidatorUnbond.cpp`
- `src/libxrpl/tx/transactors/ClaimReward.cpp`

### Milestone 3

The build passes. All four new transaction types are correctly rejected with `temDISABLED` when submitted to a ledger that does not have `ProofOfParticipation` enabled. The amendment can be enabled in a test environment via the existing amendment-voting test harness.

---

## Phase 4 — Dynamic Fee Split

Goal: instead of burning 100% of the transaction fee, split it algorithmically between burning (deflationary pressure) and the validator reward pool. The split ratio is computed fresh at every ledger close from two on-chain signals — treasury health and recent network usage — so it self-adjusts without human intervention or off-chain data.

### 4.1 Split formula

The burn fraction in basis points (out of 10,000) is computed as:

```
burnBps = clamp(
    kFEE_BURN_BASE_BPS
        + treasuryPressure(treasuryBalance, initialTreasuryBalance)
        - usagePressure(feeVolumeEMA, kFEE_VOLUME_TARGET),
    kFEE_BURN_MIN_BPS,
    kFEE_BURN_MAX_BPS
)
rewardBps = 10000 - burnBps
```

**Boundary constants** (defined in `QXRPConstants.h`):

```cpp
// Burn fraction is always between 40% and 70%.
constexpr std::uint32_t kFEE_BURN_MIN_BPS  = 4000;   // floor: 40% burned
constexpr std::uint32_t kFEE_BURN_MAX_BPS  = 7000;   // ceiling: 70% burned
constexpr std::uint32_t kFEE_BURN_BASE_BPS = 5500;   // neutral set-point: 55%
```

#### Treasury pressure term

When the treasury is depleted, the network is in late-stage emission. Burning less at that point preserves validator revenue. When the treasury is full, burning more is safe and deflationary.

```
treasury_ratio = treasuryBalance / initialTreasuryBalance  // range [0.0, 1.0]
treasury_pressure_bps = round((treasury_ratio - 0.5) * kFEE_TREASURY_SENSITIVITY_BPS)
```

`kFEE_TREASURY_SENSITIVITY_BPS = 2000` means the treasury term can push the burn fraction up by 10 percentage points (treasury full) or down by 10 percentage points (treasury nearly empty).

#### Usage pressure term

When the network is heavily used, validators are already earning well from raw fee volume. We can burn more aggressively. When usage is low, the network needs to keep validators earning from each transaction.

```
usage_ratio = feeVolumeEMA / kFEE_VOLUME_TARGET     // range typically [0.0, 2.0]
usage_pressure_bps = round((usage_ratio - 1.0) * kFEE_USAGE_SENSITIVITY_BPS)
```

`kFEE_USAGE_SENSITIVITY_BPS = 1000` means the usage term can shift the burn fraction by up to 10 percentage points in either direction. The fee volume EMA is a 256-ledger exponential moving average of total drops burned per ledger.

#### Combined behavior

| Condition | Burn fraction | Reward fraction |
| --- | ---: | ---: |
| Treasury full, usage high | up to 70% | down to 30% |
| Treasury full, usage normal | ~65% | ~35% |
| Treasury at 50%, usage normal | ~55% (base) | ~45% |
| Treasury low, usage normal | ~45% | ~55% |
| Treasury low, usage low | down to 40% | up to 60% |

The formula is intentionally simple enough to be audited and replicated by any node. No floating-point is used; all arithmetic is integer basis-point arithmetic.

### 4.2 Where the change lives

The current destruction path is:

```
ApplyContext::destroyXRP()          (include/xrpl/tx/ApplyContext.h:100)
  -> view_->rawDestroyXRP(fee)      (include/xrpl/ledger/RawView.h)
     implemented in:
       OpenView::rawDestroyXRP      (src/libxrpl/ledger/OpenView.cpp:249)
       detail::ApplyStateTable::destroyXRP
       detail::RawStateTable::destroyXRP
```

The split intercepts at `ApplyContext::destroyXRP`. When `ProofOfParticipation` is enabled, the computed `burnBps` for the closing ledger is read from the current `ltREWARD_EPOCH` object (it is stored there at ledger-close time so all transactions in the same ledger use the same ratio):

1. Call `rawDestroyXRP(burnPortion)` for the burned fraction.
2. Credit the remainder to the `ltREWARD_EPOCH` pool object.

### 4.3 Where the ratio is computed and stored

The ratio must be deterministic across all nodes, so it is computed once per ledger close — not per transaction — and stored in `ltREWARD_EPOCH.sfCurrentBurnBps`.

Computation happens in the ledger-close path, before transactions are applied to the new ledger. The inputs (treasury balance, fee volume EMA) are both available from closed-ledger state, so the value is reproducible by any node.

**New SFields** (add to `SField.h` / `SField.cpp`):

- `sfCurrentBurnBps` (STUInt32) — burn fraction in basis points for the current ledger, stored on `ltREWARD_EPOCH`.
- `sfFeeVolumeEMA` (STUInt64) — 256-ledger EMA of drops burned per ledger, stored on `ltREWARD_EPOCH`.

### 4.4 Governance integration

The four sensitivity constants (`kFEE_BURN_MIN_BPS`, `kFEE_BURN_MAX_BPS`, `kFEE_TREASURY_SENSITIVITY_BPS`, `kFEE_USAGE_SENSITIVITY_BPS`) are governable parameters added to `ltGOVERNANCE_PARAMS` in Phase 8. At runtime the code reads from that object when present, falling back to the compile-time constants when absent. This means the formula adapts to governance votes without a protocol upgrade.

### Files to change

| File | Change |
| --- | --- |
| `include/xrpl/protocol/QXRPConstants.h` | Add all boundary and sensitivity constants. |
| `include/xrpl/protocol/SField.h` + `src/libxrpl/protocol/SField.cpp` | Add `sfCurrentBurnBps`, `sfFeeVolumeEMA`. |
| `include/xrpl/tx/ApplyContext.h` | Read `sfCurrentBurnBps` from `ltREWARD_EPOCH` and apply the split inside `destroyXRP()`. Gate on amendment. |
| `src/xrpld/app/ledger/` (ledger-close path) | Compute `burnBps` and `sfFeeVolumeEMA` at close time and write them to `ltREWARD_EPOCH` before the next ledger opens. |
| `src/xrpld/app/main/Application.cpp` | In `startGenesisLedger()`, create `ltREWARD_EPOCH` with `sfCurrentBurnBps = kFEE_BURN_BASE_BPS` and `sfFeeVolumeEMA = 0`. |
| `src/libxrpl/ledger/OpenView.cpp` | No change needed; split is upstream. |

### Milestone 4

Unit tests verify:
- At genesis-level treasury balance and normal usage, `burnBps` equals `kFEE_BURN_BASE_BPS` (55%).
- Artificially depleting the treasury object to 10% shifts the burn fraction toward the 40% floor.
- Artificially setting high fee-volume EMA shifts the burn fraction toward the 70% ceiling.
- All three cases: total drops are conserved (no drops created or lost by the split).
- The ratio is identical across two independently-computed node states given the same closed-ledger inputs.

---

## Phase 5 — Reward System (Emission + ClaimReward + Reputation Scoring)

Goal: implement the full reward pipeline in one phase — emission schedule, composite reputation scoring written at ledger close, and a `ClaimReward` transactor that distributes proportionally to score. Combining these here ensures the reward distribution always has real scoring data from the first commit; there is no intermediate state where `ClaimReward` works but reputation is a stub.

### 5.1 Emission schedule

Implement in `QXRPConstants.h` and a new `src/libxrpl/protocol/RewardSchedule.cpp`:

```
Halving period:      31'500'000 ledgers (approximately 4 years at 1 ledger per 3-4 seconds)
Initial epoch rate:  (treasury_balance_at_epoch_start * 2) / halving_period
                     (this gives a geometric decay that approximates Bitcoin halving)
```

The epoch rate is stored in `ltREWARD_EPOCH` at the start of each epoch. The scheduler runs deterministically from ledger state alone.

### 5.2 Epoch transition

When a new ledger's sequence number crosses a halving boundary:
1. Close the current `ltREWARD_EPOCH` object.
2. Create a new one with the updated rate.
3. This logic belongs in a pseudo-transaction or in the ledger-closing path in `src/xrpld/app/consensus/` or `src/xrpld/app/ledger/`.

### 5.3 Reputation scoring (written at ledger close)

The composite score is computed from five signals and written to `ltVALIDATOR_BOND.sfCompositeScore` at every ledger close, before any `ClaimReward` transactions are processed:

```
composite = (uptimeBps       * 40
           + voteAccuracyBps * 30
           + latencyScoreBps * 15
           + consistencyBps  * 10
           + slashMultiplier *  5) / 100
```

| Signal | Update timing | Weight |
| --- | --- | --- |
| Uptime percentage | Each ledger close: check if validator submitted a validation | 40% |
| Vote accuracy | Each ledger close: match validator vote against accepted ledger | 30% |
| Proposal latency | When a validator is the proposer: compare sequence latency | 15% |
| Participation consistency | Rolling window: penalize multi-ledger absences | 10% |
| Slashing history | `slashMultiplier` starts at 10000, decremented on each slash event | 5% |

All components are 0–10000 basis points. The slash multiplier means a validator with a history of misbehavior earns structurally less than a clean peer even after the immediate penalty is settled.

**Files to change for scoring:**

| File | Change |
| --- | --- |
| `src/xrpld/app/misc/NegativeUNLVote.cpp` | After consensus closes a ledger, iterate trust-listed validators, update uptime and vote-accuracy fields, recompute `sfCompositeScore`. |
| `src/xrpld/app/consensus/` (likely `RCLConsensus.cpp`) | On proposal receipt, record latency into the bond object for the proposing validator. |
| `src/libxrpl/tx/transactors/ValidatorRegister.cpp` | On registration, initialize all scoring fields to neutral starting values. |

`ClaimReward` distributes rewards proportional to a **composite weighted reputation score** stored in `ltVALIDATOR_BOND.sfCompositeScore`, not raw uptime alone. The composite is written back to the bond object at every ledger close by the Phase 7 scoring logic, so `ClaimReward` reads a single deterministic field that already encodes all five participation signals.

**Composite score formula** (integer arithmetic, units 1/10000):

```
composite = (uptimeBps       * 40
           + voteAccuracyBps * 30
           + latencyScoreBps * 15
           + consistencyBps  * 10
           + slashMultiplier *  5) / 100
```

Each component is a value from 0 to 10000 (basis points). `slashMultiplier` starts at 10000 and is decremented by slash events, so a validator with a slashing history structurally earns less than a clean peer even after the individual penalty has been served.

`ClaimReward` steps:

### 5.4 ClaimReward transactor

Tests verify:
- Five bond objects with distinct `sfCompositeScore` values produce reward shares strictly proportional to score.
- A validator with `sfCompositeScore == 0` is rejected with `tecINSUFFICIENT_QUALITY`.
- A validator with `bondStatus != bonded` is rejected with `tecNO_PERMISSION`.
- A registered but not yet bonded validator (`ValidatorRegister` only, no `ValidatorBond`) is rejected with `tecNO_PERMISSION`.
- Shares across all five validators sum to exactly `sfEpochPoolBalance` (remainder stays in pool, zero drops lost).
- Treasury balance decreases by the exact sum of distributed shares.
- A second `ClaimReward` in the same epoch returns `tecDUPLICATE`.
- After 100 ledger closes on a 5-node CSF simulation, each node's `sfCompositeScore` reflects its true participation rate (uptime, vote accuracy, latency).

---

## Phase 6 — Bonding, Unbonding, and Slashing

Goal: make running a qXRP validator accessible to non-technical operators. From a fresh Hetzner VPS to a running, reward-eligible, monitored validator should require one command and under five minutes.

### 6.1 Docker support

Add to repository root:

| File | Purpose |
| --- | --- |
| `docker/Dockerfile` | Multi-stage build: compile `xrpld` in a builder stage, copy the binary and default config into a minimal Debian slim runtime image. |
| `docker/docker-compose.yml` | Three-validator compose file matching `cfg/qxrp-regtest/`. Validators share an internal Docker network; ports are mapped to distinct host ports. |
| `docker/docker-compose.single.yml` | Single-node compose for development. Mounts config and data from a working-directory volume. |
| `.dockerignore` | Excludes build artifacts, test data, and any credential or key files from the build context. |

The image exposes ports 2459 (peer), 51235 (RPC), and 8080 (dashboard). The entrypoint accepts the config file path as its sole argument so the same image works for validator and full-history modes.

### 6.2 One-command installer

Create `bin/install/install-qxrp-validator.sh`. Intended usage:

```bash
curl -fsSL https://install.qxrp.example/validator | bash
```

The script:
1. Checks minimum hardware requirements (4 GB RAM, 80 GB disk) and exits with a clear error if unmet.
2. Installs Docker via the official Docker install script if not already present.
3. Creates `$HOME/.qxrp/config` and `$HOME/.qxrp/data` directories.
4. Downloads the default validator config template if no config file exists yet.
5. Pulls the published Docker image and starts the container with `--restart unless-stopped`.
6. Prints the local dashboard URL and validator public key on completion.

The script must be idempotent: running it twice on an already-configured node is a no-op.

### 6.3 Hetzner deployment

Create `bin/install/hetzner/`:

| File | Purpose |
| --- | --- |
| `user-data.yaml` | Cloud-init script that runs the installer automatically on first boot. Paste into the Hetzner Cloud console user-data field at VPS creation. |
| `create-snapshot.sh` | Uses the `hcloud` CLI to snapshot a running, configured node. The snapshot enables one-click multi-region deployment without re-syncing from scratch. |
| `recommended-specs.md` | Instance recommendations: CX22 (2 vCPU, 4 GB RAM) minimum; CX32 (4 vCPU, 8 GB RAM) recommended for a full validator with history. |

### 6.4 Validator web dashboard

Create `tools/dashboard/` — a lightweight, stateless HTTP server that reads validator state via the local xrpld RPC and presents it in a browser.

Implementation choice (to be finalized): single Go binary or Python + FastAPI. Decision and rationale recorded in `tools/dashboard/README.md`.

| Screen | Data shown |
| --- | --- |
| Overview | Sync status, current ledger, peer count, connection health |
| Reputation | Composite score, uptime %, vote accuracy, latency score, consistency, slash history |
| Rewards | Bond amount, bond status, estimated epoch share, last claimed epoch, cumulative earned |
| History | Per-epoch composite score chart (last 10 epochs) |

The dashboard:
- Polls the local RPC every 10 seconds; no persistent database required.
- Serves on `http://localhost:8080` by default; port is configurable via environment variable.
- Exposes `/metrics` in Prometheus exposition format for Grafana integration.
- Has no write access to the node and requires no authentication when bound to localhost.

### Milestone 6

- `docker build` completes and produces a working `xrpld` image.
- `docker compose up` (using `docker/docker-compose.yml`) brings three validators to consensus within 60 seconds.
- The one-command installer runs to completion on a clean Ubuntu 24.04 machine and produces a running node.
- The dashboard shows the correct composite score and a non-zero estimated reward after one full epoch on the local testnet.

---

## Phase 7 — Validator Reputation Scoring

Goal: compute reputation scores on-chain from observable ledger events so `ClaimReward` has real input data.

### Scoring signals

Implement as fields updated in `ltVALIDATOR_BOND` during ledger close:

| Signal | Update timing | Weight |
| --- | --- | --- |
| Uptime percentage | Each ledger close: check if validator submitted a validation | 40% |
| Vote accuracy | Each ledger close: match validator vote against accepted ledger | 30% |
| Proposal latency | When a validator is the proposer: compare sequence latency | 15% |
| Participation consistency | Rolling window: penalize multi-ledger absences | 10% |
| Slashing history | Penalty multiplier: decremented on each slash event | 5% |

### Files to change

| File | Change |
| --- | --- |
| `src/xrpld/app/misc/NegativeUNLVote.cpp` | After consensus closes a ledger, iterate trust-listed validators and update `ltVALIDATOR_BOND` fields for uptime and vote accuracy. |
| `src/xrpld/app/consensus/` (likely `RCLConsensus.cpp`) | On proposal receipt, record latency into the bond object for the proposing validator. |
| `src/libxrpl/tx/transactors/ValidatorBond.cpp` | On bond creation, initialize all scoring fields to neutral values. |

### Milestone 7

A unit test using the existing consensus simulation framework (`src/test/csf/`) runs a 5-node network for 100 ledgers, then checks that each node's reputation score reflects its actual participation rate.

---

## Phase 8 — Bonding, Unbonding, and Slashing

Goal: make `ValidatorRegister`, `ValidatorBond`, and `ValidatorUnbond` fully functional, and implement automatic slashing for proven misbehavior. Slashing triggers automatic forced unbonding so a slashed validator does not remain in an inconsistent half-bonded state.

### 6.1 ValidatorRegister transactor

1. Verify the caller does not already have a registered `ltVALIDATOR_BOND` object.
2. Verify the signing key is a valid qXRP key type (secp256k1, ed25519, or Falcon).
3. Create `ltVALIDATOR_BOND` with `bondStatus = registered` and all scoring fields at neutral starting values.
4. The validator is visible on-ledger and accrues scoring data, but is not yet reward-eligible.

### 6.2 ValidatorBond transactor

1. Verify the caller has an existing `ltVALIDATOR_BOND` in `registered` status.
2. Lock the specified bond amount (deduct from caller's balance, store in bond object).
3. Set `bondStatus = bonded`. The validator is now reward-eligible.
4. Add the validator to the per-epoch eligible set.

### 6.3 ValidatorUnbond transactor

1. Verify `bondStatus == bonded`.
2. Set `bondStatus = unbonding` and record `sfUnbondingStartLedger`.
3. The bond amount is released back to the validator's account automatically at ledger close once `currentLedger - sfUnbondingStartLedger >= kUNBONDING_LOCK_LEDGERS` (default 262,800 ≈ 30 days). Implement as a ledger-close hook that iterates unbonding objects and releases those past the threshold.
4. While unbonding, scoring continues but the validator is not reward-eligible.

### 6.4 Automatic forced unbonding after slashing

When a slash event is detected during ledger close, the protocol performs these steps atomically before the ledger is committed:

1. Apply the bond slash penalty (amount destroyed to the burn address).
2. If `bondStatus == bonded`, immediately set `bondStatus = unbonding` and record `sfUnbondingStartLedger = currentLedger`.
3. The reduced remaining bond amount (if any) becomes available to the validator after the standard lock period.
4. The slashed validator is removed from the epoch eligible set for the current epoch and all future epochs until re-bonding.
5. Re-bonding after a slash requires a new `ValidatorBond` transaction; the `ltVALIDATOR_BOND` registration object is preserved (the slash history it carries acts as a permanent on-ledger record).

Forced unbonding removes the edge case where a slashed validator still holds `bondStatus = bonded` and could theoretically attempt `ClaimReward`. The status check in `ClaimReward` already rejects non-bonded callers, but automatic unbonding makes the ledger state unambiguous.

### 6.5 Slashing rules

Implement as invariant-style checks during ledger close:

| Offense | Evidence source | Penalty | Forced unbond? |
| --- | --- | --- | --- |
| Double-sign (equivocation) | Two diverging validations from same key | Slash 100% of bond | Yes, immediately |
| Sustained absence | Uptime below 20% for 3 consecutive epochs | Slash 25% of bond; reduce `slashMultiplier` | Yes, immediately |
| Invalid vote (proven) | Validation for a ledger demonstrably wrong | Slash 50% of bond | Yes, immediately |

Slashed amounts are destroyed (sent to the burn address), not redistributed, to remove any incentive to trigger a competitor's slashing.

### Milestone 6

Unit tests for:
- `ValidatorRegister` creates bond object; double-register returns `tecDUPLICATE`.
- `ValidatorBond` transitions status to `bonded`; bonding without prior registration returns `tecNO_ENTRY`.
- Successful unbond: lock period enforced; bond released after threshold.
- Double-sign slash: bond zeroed, status set to `unbonding`, validator removed from epoch eligible set.
- Sustained-absence slash: partial penalty applied, `slashMultiplier` decremented, forced unbonding triggered.
- Slashed validator's `ClaimReward` returns `tecNO_PERMISSION`.
- After forced unbond and re-bond, `ClaimReward` succeeds for a future epoch.

---

## Phase 9 — On-Chain Governance (Minimal)

Goal: bonded validators can propose and vote on bounded parameter changes without a software upgrade.

### Governable parameters (initial set)

- `kFEE_BURN_MIN_BPS` / `kFEE_BURN_MAX_BPS` (burn fraction bounds; allowed adjustment range: 3000–8000)
- `kFEE_TREASURY_SENSITIVITY_BPS` (allowed range: 500–3000)
- `kFEE_USAGE_SENSITIVITY_BPS` (allowed range: 500–2000)
- Minimum bond amount (allowed range: 50–5000 XRP equivalent)
- Unbonding lock period (allowed range: 131,400–525,600 ledgers)

### 7.1 New transaction types

- `GovernanceProposal` — a bonded validator proposes a parameter change with a new value and a voting deadline.
- `GovernanceVote` — a bonded validator votes yes or no.

### 7.2 Execution

When a proposal reaches 67% yes votes from bonded validators (weighted by `sfCompositeScore`) before the deadline:
1. Write the new parameter value to a `ltGOVERNANCE_PARAMS` ledger object.
2. The relevant code paths read from this object at runtime, falling back to the compile-time constant if the object is absent.

### Milestone 7

A test creates a proposal, casts enough weighted votes, and confirms the parameter object is updated. A second test confirms the dynamic fee split formula reads the updated sensitivity constant. A third test confirms a proposal with insufficient vote weight expires without effect.

---

## Phase 8 — 5-Validator Local Testnet

Goal: run a self-contained multi-validator local network using five validators (up from the original three) to better exercise quorum edge cases and reward distribution proportionality. All qXRP features must work end-to-end.

### 8.1 Network topology

Minimum viable local testnet:
- 5 validator nodes sharing a dUNL (each lists the other four as trusted).
- 1 non-validating full-history node for query convenience.
- All nodes on `localhost` with distinct peer/RPC ports.
- 5 validators is the minimum that allows Byzantine fault tolerance testing: the network tolerates 1 faulty node while still reaching the 80% quorum threshold (4 of 5).

### 8.2 Configuration

Create `cfg/qxrp-regtest/` with six config files (five validators + one history node) following the pattern in `cfg/xrpld-example.cfg`.

Key differences from the example config:
- `[network_id]` set to a qXRP-specific value (e.g. `999`).
- `[validators]` section lists the local validator public keys directly.
- `[amendments]` section explicitly enables `ProofOfParticipation` at genesis.
- `[ledger_history]` set to full for the history node.

### 8.3 Genesis pre-enablement of amendments

For testing, amendments can be force-enabled at genesis by adding them to the `[amendments]` section of the config. The validator nodes must unanimously agree to have them enabled at startup, which works in a controlled local environment.

Alternatively, use the existing `feature` RPC command to vote for the amendment on all five validators and advance ledgers past the 80% majority window.

### 8.4 Startup script

Create `bin/regtest/start-local-testnet.sh`:

```bash
#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team. AGPL-3.0.
set -euo pipefail

BASEDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$BASEDIR/build/xrpld"
CFG="$BASEDIR/cfg/qxrp-regtest"

for i in 1 2 3 4 5; do
    mkdir -p "$TMPDIR/qxrp-node$i"
    "$BIN" --conf "$CFG/validator$i.cfg" --fg &
done

wait
```

### 8.5 End-to-end test checklist

Perform these steps manually or automate them as a shell test:

1. Start the five-validator network. Confirm all five reach consensus.
2. Confirm the history node syncs and advances with the network.
3. Submit `ValidatorRegister` from each validator. Confirm `ltVALIDATOR_BOND` objects appear in `registered` status.
4. Submit `ValidatorBond` from each validator. Confirm status transitions to `bonded`.
5. Submit a payment signed with a Falcon key. Confirm it is accepted and the fee split is reflected in the epoch pool.
6. Advance past one full epoch boundary. Confirm a new `ltREWARD_EPOCH` is created with the updated emission rate.
7. Check `sfCompositeScore` on each bond object. Confirm scores reflect actual participation.
8. Submit `ClaimReward` from each validator. Confirm treasury balance decreases and balances increase proportionally to score.
9. Simulate a double-sign from one validator. Confirm slash + forced unbond is applied.
10. Propose a governance parameter change. Vote from four of five validators. Confirm the parameter object updates.
11. Bring one validator offline for 20 ledgers. Confirm its score degrades. Bring it back; confirm recovery.

### Milestone 8

All eleven end-to-end checklist steps pass. The local testnet runs stably for at least 1,000 ledger closes without errors.

---

## Summary: Phase Order and Key Files

```
Phase 0   Baseline build
Phase 1   SystemParameters.h, Application.cpp (genesis), QXRPConstants.h
Phase 2   KeyType.h, PQPublicKey.h/cpp, PQSecretKey.h/cpp, falcon.h/cpp,
          Sign.h/cpp, STTx.cpp, STValidation.cpp
Phase 3   features.macro, transactions.macro, LedgerFormats.h,
          SField.h/cpp, ValidatorRegister.cpp, ValidatorBond.cpp,
          ValidatorUnbond.cpp, ClaimReward.cpp (stubs)
Phase 4   QXRPConstants.h, SField.h/cpp (sfCurrentBurnBps, sfFeeVolumeEMA),
          ApplyContext.h, ledger-close path (burnBps compute)
Phase 5   RewardSchedule.cpp (new), ClaimReward.cpp (complete),
          NegativeUNLVote.cpp + RCLConsensus.cpp (scoring write-back),
          sfCompositeScore + sfAggregateCompositeScore
Phase 6   ValidatorRegister.cpp, ValidatorBond.cpp, ValidatorUnbond.cpp
          (complete), slashing invariants, forced-unbond on slash
Phase 7   GovernanceProposal.cpp, GovernanceVote.cpp, ltGOVERNANCE_PARAMS
Phase 8   cfg/qxrp-regtest/ (5 validators), bin/regtest/start-local-testnet.sh
Phase 9   docker/Dockerfile, docker/docker-compose.yml, bin/install/,
          tools/dashboard/
Phase 10  docs/qxrp/, audit-checklist.md, invariant additions,
          fuzz targets, coverage enforcement
```

---

## Milestone Rollup

| Phase | What ships | Gate |
| --- | --- | --- |
| 0 | Confirmed baseline build | ctest passes |
| 1 | 200B supply, genesis treasury | Treasury balance correct in genesis |
| 2 | Falcon keygen + sign + verify | Falcon unit test passes |
| 3 | Amendment scaffold, 4 new tx type stubs | All 4 types rejected with temDISABLED without amendment |
| 4 | Dynamic fee split (40–70% burn, formula-driven) | 5 invariant tests: treasury/usage sensitivity + conservation |
| 5 | Reward system: emission + reputation scoring + ClaimReward | 8-case test suite; CSF simulation composite-score accuracy |
| 6 | Bonding + slashing + forced auto-unbond | 7 unit tests including re-bond after slash |
| 7 | On-chain governance | 3 tests: proposal passes, proposal expires, fee split reads updated param |
| 8 | 5-validator local testnet | All 11 E2E checklist steps pass; 1000-ledger stability run |
| 9 | Docker + Hetzner installer + web dashboard | docker-compose testnet reaches consensus; installer clean on Ubuntu 24.04 |
| 10 | Polish + audit prep | 5 protocol docs, drop-conservation invariant, 80% coverage, 0 ASAN findings |

---

## What is Explicitly Out of Scope For This Plan

- Mainnet launch or live network deployment.
- External exchange integration.
- Wallet or client tooling.
- Formal security audits.
- Production hardening of the Falcon library dependency.
- Dynamic UNL replacement (reducing static dUNL reliance is a post-testnet goal).
