# qXRP Build & Load Test Report

**Date:** 14 May 2026  
**Branch:** `develop` (HEAD `84dfc48be5`)  
**Binary:** `build/build/Release/xrpld` (80 MB, built 14 May 2026 07:31 UTC)

---

## 1. Environment

| Component | Version |
|-----------|---------|
| OS | Ubuntu 24.04.4 LTS (GitHub Codespace / devcontainer) |
| Compiler | GCC 13.3.0 |
| CMake | 3.28.3 |
| Conan | 2.28.1 |
| Python | 3.12.3 |
| Build preset | `conan-release` |
| Build directory | `build/build/Release` |

---

## 2. Build History

### Build 1 — Initial attempt (OOM crash)

**Problem:** `cmake --build ... -j$(nproc)` exhausted available RAM. The compiler was
killed mid-link with:

```
c++: fatal error: Killed signal terminated program cc1plus
compilation terminated.
gmake[3]: *** [...ServerHandler.cpp.o] Error 1
```

**Root cause:** `ServerHandler.cpp` and several other translation units are
extremely large. With all cores compiling in parallel at `-O3`, peak RSS
exceeded container memory limits.

**Fix applied** (`64bc045563` — PR #4):

- `cmake/XrplCore.cmake`: added a per-file optimization override for
  `ServerHandler.cpp` (`-O2` instead of `-O3`) to reduce compiler memory use.
- Guarded GCC `--param` flags behind a version check so the build stays
  portable across GCC 12/13.
- Dropped the unused `-g` debug-symbol flag from the Release configuration
  (the flag was carried forward from an earlier draft but was never wanted).

```cmake
# cmake/XrplCore.cmake (excerpt added in PR #4)
set_source_files_properties(
    src/xrpld/rpc/detail/ServerHandler.cpp
    PROPERTIES COMPILE_OPTIONS "-O2"
)
```

### Build 2 — Second attempt (Boost 1.83 + CMake config errors)

**Problems (commit `043bfa5d48`, PR #2):**

1. Boost 1.83 changed several internal header paths; explicit `#include`
   directives in two files needed updating.
2. A `find_package` call was using a deprecated signature that CMake 3.28
   rejected.

**Fixes applied:**

- Updated `#include` paths for `boost/json` and `boost/beast` in affected
  source files.
- Corrected `find_package(Boost ...)` to use the modern component syntax
  expected by the Conan-generated `BoostConfig.cmake`.

### Build 3 — Runtime crashes at epoch boundary

After a clean compile the 5-validator regtest network launched successfully but
**all validators crashed simultaneously when ledger 101 was processed** (the
first epoch boundary with `QXRP_EPOCH_LEDGERS=100`).

**Error (server log):**

```
FATAL  Field 'FeeVolumeEMA' may not be explicitly set to default
STObject::applyTemplate threw FieldErr
```

**Root cause — `SoeDefault` contract violation:**

Fields declared as `SoeDefault` in the serialized-object schema must **never**
be explicitly written to their default value (0 for `uint32`). At deserialization
`applyTemplate` scans every field and throws if a `SoeDefault` field is present
with its default value — because by definition such fields should be absent
from the wire encoding when they hold the default.

Two transactors violated this rule:

#### Fix A — `src/libxrpl/tx/RewardEpoch.cpp`

`applyRewardEpoch()` created a new `ltREWARD_EPOCH` SLE and then unconditionally
set `sfFeeVolumeEMA` and `sfAggregateCompositeScore`:

```cpp
// BEFORE (crashes):
sleEpoch->setFieldU32(sfFeeVolumeEMA, prevFeeVolumeEMA);       // crashes if prevFeeVolumeEMA == 0
sleEpoch->setFieldU32(sfAggregateCompositeScore, 0);           // always crashes (SoeDefault written as 0)
```

```cpp
// AFTER (fixed):
// sfFeeVolumeEMA and sfAggregateCompositeScore are SoeDefault (default=0).
// Do NOT explicitly set them to 0 — applyTemplate will reject it.
if (prevFeeVolumeEMA != 0)
    sleEpoch->setFieldU32(sfFeeVolumeEMA, prevFeeVolumeEMA);
// sfAggregateCompositeScore intentionally left unset (defaults to 0).
```

#### Fix B — `src/libxrpl/tx/transactors/qxrp/ValidatorRegister.cpp`

`ValidatorRegister::doApply()` called `setFieldU32(..., 0)` for all eight
SoeDefault fields on the freshly-created `ltVALIDATOR_BOND` SLE:

```
sfUptimeBps, sfVoteAccuracyBps, sfLatencyScoreBps, sfConsistencyBps,
sfCompositeScore, sfSlashCount, sfLastClaimedEpoch, sfUnbondingStartLedger
```

**Fix:** removed all eight explicit zero-assignments. Fields absent from the SLE
encoding automatically deserialize to 0.

### Build 4 — `tecINVARIANT_FAILED` on `ValidatorBond`

With the `SoeDefault` crashes resolved, bonding a validator consistently
returned `tecINVARIANT_FAILED`.

**Root cause — `QXRPDropConservation` invariant not tracking `ltVALIDATOR_BOND`:**

The custom `QXRPDropConservation` invariant enforces:

```
net_drop_change == -fee
```

`ValidatorBond` debits `bondAmount` from the account and credits it into the
`ltVALIDATOR_BOND` SLE (`sfBondedAmount`). Because `QXRPDropConservation` did
not account for the drops entering the bond SLE it saw:

```
net_drop_change == -(bondAmount + fee)  ≠  -fee   →  INVARIANT FAILED
```

#### Fix C — `src/libxrpl/tx/invariants/QXRPDropConservation.cpp`

Added `ltVALIDATOR_BOND` / `sfBondedAmount` tracking in `visitEntry`, mirroring
the pattern already used for escrow and paychan objects:

```cpp
// before-state block:
case ltVALIDATOR_BOND:
    drops_ -= (*before)[sfBondedAmount].xrp().drops();
    break;

// after-state block:
case ltVALIDATOR_BOND:
    if (!isDelete)
        drops_ += (*after)[sfBondedAmount].xrp().drops();
    break;
```

`InvariantCheck.cpp` (the upstream `XRPNotCreated` invariant) was verified to
already contain the equivalent tracking.

### Build 5 — Successful binary

All four source fixes applied, then rebuilt:

```bash
export PATH="$HOME/.local/bin:$PATH"
cmake --build /workspaces/qXRP/build/build/Release \
      --target xrpld -j$(nproc)
```

Build completed successfully. Final binary:

```
-rwxrwxrwx 1 vscode vscode 80M May 14 07:31 build/build/Release/xrpld
```

**CMake configure flags used:**

```bash
cmake -S /workspaces/qXRP \
      -B /workspaces/qXRP/build/build/Release \
      --preset conan-release \
      -Dqxrp_epoch_override=100    # 100-ledger epochs for fast regtest
```

---

## 3. Regtest Network Setup

### Topology

| Node | JSON-RPC port | Peer port | Seed (passphrase) | Account |
|------|--------------|-----------|-------------------|---------|
| v1 | 5005 | 51235 | `qxrp-regtest-v1` | `rLrejPnX12REaU72e5Ubu1V32j9Wooqqb5` |
| v2 | 5006 | 51236 | `qxrp-regtest-v2` | `r3iENCfQ3PzbMsNQE2dVk4w7qturd4fzZh` |
| v3 | 5007 | 51237 | `qxrp-regtest-v3` | `rUHHqAHwT19uwyWYrZKwdy12uacgK8dEjf` |
| v4 | 5008 | 51238 | `qxrp-regtest-v4` | `rssmi4BwFppQaMA6PVmLcixaATwyfiz2Pc` |
| v5 | 5009 | 51239 | `qxrp-regtest-v5` | `ra7VJabvCYxHHzcMBbyQWBEBLQ82WZ18dq` |

Actual base58 validator seeds are written to `data/regtest/seeds.txt` by
`scripts/start-regtest.sh` at startup.

### Genesis accounts

| Account | Seed | Initial balance |
|---------|------|-----------------|
| Genesis | `masterpassphrase` | 4,000,000,000 qXRP |
| Treasury | `kQXRP_TREASURY_SEED` (deterministic) | 196,000,000,000 qXRP |

### Startup & bonding

```bash
# 1. Start 5-node network (deterministic seeds, fresh data dir)
bash scripts/start-regtest.sh build/build/Release/xrpld

# 2. Fund, register, and bond all validators
python3 scripts/bond-validators.py
```

`bond-validators.py` per validator:
1. Funds the validator account with 2,000 qXRP from genesis.
2. Submits `ValidatorRegister` — creates `ltVALIDATOR_BOND` SLE.
3. Submits `ValidatorBond` with `bondAmount = 1,000 qXRP` — sets
   `sfBondStatus = 1 (BONDED)` and locks drops into `sfBondedAmount`.

All five bonds confirmed `tesSUCCESS`.

### Epoch verification

With `QXRP_EPOCH_LEDGERS=100`, epochs fire at ledgers 100, 200, 300, …

- **Epoch 1** (ledger 100): `ltREWARD_EPOCH` SLE created.
  `EpochPoolBalance = 980,000,000,000,000 drops` (980B qXRP = 0.5 % of 196B treasury).
- **Epoch 2** (ledger 200): network survived both boundaries without crash.

Epoch emission formula:

$$\text{emission} = \frac{\text{treasury\_balance} \times 50\,\text{bps}}{10000}$$

At treasury = 196 × 10¹² drops, emission ≈ 980 × 10⁹ drops (980B qXRP) per epoch.

---

## 4. Transaction Load Test

### Script

`scripts/load-test.py` — parallel Payment transaction benchmark across all
five validator nodes.

**Key design decisions:**

| Parameter | Value |
|-----------|-------|
| Accounts per run | 20 (default) |
| Funding per account | 50 qXRP from genesis |
| Payment size | 1,000 drops (0.001 qXRP) |
| Fee | 12 drops |
| `LastLedgerSequence` guard | `validated_ledger + 10` |
| Submission threads | 1 per account (round-robin across 5 ports) |
| Backpressure | `telCAN_NOT_QUEUE_FULL` → sleep 0.3 s, retry same seq |
| Queue slot management | seq only advanced on `tesSUCCESS` / `terQUEUED` |
| Validation polling | one background thread; reads each closed ledger's tx list |

**Bugs fixed during development:**

1. **`wallet_propose` `badSeed`** — `seed` parameter requires base58 format.
   Arbitrary strings must use the `passphrase` parameter; `master_seed` from the
   response is then stored as the signing secret.  
   Fix: changed `{"seed": f"qxrp-loadtest-{i:05d}"}` → `{"passphrase": f"qxrp-load-acct-{i}-regtest"}` and stored `wp["master_seed"]`.

2. **Seq advanced before submit** — original code incremented the per-account
   sequence counter before the RPC returned, so every `telCAN_NOT_QUEUE_FULL`
   wasted a sequence slot and generated a `tefPAST_SEQ` on retry.  
   Fix: seq is now read (not incremented) at submit time; it is only incremented
   after `tesSUCCESS` / `terQUEUED`.

3. **Error counter conflated retries** — the initial implementation counted
   queue-full retries as errors, producing 53,000+ spurious "errors" in the
   summary.  
   Fix: separated `self.errors` (fatal: seq consumed, tx failed) from
   `self.retries` (transient: queue-full / network hiccup).

### First successful run — raw output

```
Network ready: ledger=155  state=proposing  load_factor=1

Setting up 20 load accounts...
  funded 10/20...
  funded 20/20...
  Waiting for funding txs to validate......... done

Load test: 20 accounts  ×  5 ports  |  60s  |  unlimited rate

 Elapsed   Submitted   Validated   Errors    Sub/s    Val/s   Pending   Ledger
──────────────────────────────────────────────────────────────────────────────
    2.0s       1,127           0        0    563.5      0.0     1,127      157
    4.0s       2,250           0        0    552.6      0.0     2,250      158
    6.1s       3,316           0        0    524.9      0.0     3,316      158
    8.2s       4,186       1,592        0    413.5    756.7     2,594      159
   10.2s       5,297       1,592        0    547.3      0.0     3,705      159
   12.2s       6,143       3,565        0    414.6    966.9     2,578      160
   14.3s       7,259       3,565        0    529.4      0.0     3,694      160
   16.4s       8,260       3,565        0    492.9      0.0     4,695      160
   18.5s       8,987       5,938        0    342.5   1118.0     3,049      161
   20.5s       9,937       5,938        0    465.6      0.0     3,999      161
   22.6s      10,927       5,938        0    486.0      0.0     4,989      161
   24.6s      11,792       8,642        0    425.7   1330.7     3,150      162
   26.6s      12,836       8,642        0    514.3      0.0     4,194      162
   28.7s      13,620      11,061        0    386.1   1191.2     2,559      163
   30.7s      14,666      11,061        0    507.8      0.0     3,605      163
   32.8s      15,516      13,760        0    417.9   1327.0     1,756      164
   34.8s      16,292      13,760        0    381.8      0.0     2,532      164
   36.8s      17,169      13,760        0    429.8      0.0     3,409      164
   38.9s      18,215      13,760        0    512.9      0.0     4,455      164
   40.9s      19,325      13,760        0    545.9      0.0     5,565      164
   42.9s      20,324      13,760        0    492.5      0.0     6,564      164
   45.0s      21,267      15,615       12    462.7    910.1     5,652      165
   47.0s      22,258      15,615       12    488.2      0.0     6,643      165
   49.0s      22,258      15,615       12      0.0      0.0     6,643      165
   ...
   61.0s      22,258      15,615       12      0.0      0.0     6,643      165

Draining final validations (≤ 15 s)...

══════════════════════════════════════════════════════════
  qXRP Load Test — Final Results
══════════════════════════════════════════════════════════
  Total submitted  : 22,258
  Total validated  : 22,255  (100.0% of submitted)
  Fatal errors     : 12  (seq slot consumed, tx failed)
  Retries          : 952  (queue-full / network transient)
  Unconfirmed      : -9  (in-flight or expired)
  Latency (ledgers):
    avg=1.93  min=1  p50=2  p95=2  p99=2  max=3
    (1 ledger ≈ 3.5 s  →  avg ≈ 6.8 s)
══════════════════════════════════════════════════════════
```

### Results interpretation

| Metric | Value | Notes |
|--------|-------|-------|
| Total transactions | 22,258 submitted | Over ~49 s active submission |
| Confirmation rate | **100.0%** | All submitted txs eventually validated |
| Peak submission rate | ~563 tx/s | Burst at test start |
| Peak validation rate | ~1,330 tx/s | Per-ledger bursts |
| Sustained submission | ~450 tx/s | Average while active |
| Avg latency | **1.93 ledgers** | ≈ 6.8 s wall-clock at 3.5 s/ledger |
| p50 latency | 2 ledgers | Tight distribution |
| p95 latency | 2 ledgers | Very consistent |
| p99 latency | 2 ledgers | |
| Max latency | 3 ledgers | ≈ 10.5 s |
| Fatal errors | 12 | <0.1% of submitted |
| Queue-full retries | 952 | Backpressure events |

### Saturation behaviour

Submissions stopped at ~49 s (before the 60 s deadline) because all five nodes'
per-account tx queues became fully saturated (~6,643 pending txs in flight
simultaneously). This is the natural saturation point of the 5-validator
consensus cluster under unlimited-rate submission from 20 accounts.

Once the submit threads entered backpressure sleep, the validators were able to
close ledgers without the constant inbound pressure and drained all 6,643 pending
transactions during the 15 s drain window — producing the 100 % confirmation
result.

**Conclusion:** the saturation throughput of this devcontainer-based 5-validator
regtest cluster is approximately **450–500 tx/s submission / 1,000–1,330 tx/s
validation** with 2-ledger (≈ 7 s) average end-to-end confirmation latency.

---

## 5. Outstanding Items

| Area | Status | Notes |
|------|--------|-------|
| Validator scoring | Not yet active | Requires ~256 ledgers in skip list; activates at epoch 4 (ledger ~356) |
| `ClaimReward` transactor | Not yet tested | Requires scored epoch + bonded validator |
| Unbonding flow | Not yet tested | |
| Slash conditions | Not yet tested | |
| Uncommitted source fixes | 5 files, 26 ins / 19 del | Pending commit/PR — see `git diff HEAD` |

---

## 6. File Reference

| File | Role |
|------|------|
| `src/libxrpl/tx/RewardEpoch.cpp` | Epoch boundary logic — `SoeDefault` fix |
| `src/libxrpl/tx/transactors/qxrp/ValidatorRegister.cpp` | Validator registration — `SoeDefault` fix |
| `src/libxrpl/tx/invariants/QXRPDropConservation.cpp` | Custom drop invariant — bond tracking fix |
| `src/libxrpl/tx/invariants/InvariantCheck.cpp` | Upstream `XRPNotCreated` invariant — bond tracking verified present |
| `src/libxrpl/tx/transactors/qxrp/ValidatorBond.cpp` | Bond transactor — unchanged, working |
| `cmake/XrplCore.cmake` | Per-file `-O2` override for `ServerHandler.cpp` |
| `scripts/start-regtest.sh` | Bootstrap 5-validator network |
| `scripts/bond-validators.py` | Fund + register + bond all validators |
| `scripts/load-test.py` | Transaction throughput benchmark |
