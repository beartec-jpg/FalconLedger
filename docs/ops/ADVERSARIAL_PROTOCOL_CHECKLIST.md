# Adversarial protocol checklist (item 3)

Run **on the soak fleet** against the freeze image. Record results in ceremony `dry-runs/`.

Related: `tools/destructive_testing/`, security fixes C-01 / C-02.

---

## A. ValidatorSlash (C-01)

| # | Test | Expected | Pass? |
|---|------|----------|-------|
| A1 | Submit slash with two **random** evidence blobs | Reject (`tecNO_PERMISSION` / not tesSUCCESS) | |
| A2 | Submit slash ABSENCE (offense 2) | `temDISABLED` | |
| A3 | Submit slash INVALID_VOTE (offense 3) | `temDISABLED` | |
| A4 | Produce two Falcon `STValidation`s: same key, same ledger seq, **different** ledger hash; slash target = that validator | `tesSUCCESS`; bond → UNBONDING; bonded amount burned/reduced per 100% double-sign | |
| A5 | Replay same slash | `tecDUPLICATE` or no second full slash | |
| A6 | Slash with valid pair but **wrong** target bond id | Reject | |

**Notes:** A4 may need a controlled double-sign harness or offline-crafted validations using the freeze binary’s Falcon APIs.

---

## B. Claims / epoch pool (C-02)

| # | Test | Expected | Pass? |
|---|------|----------|-------|
| B1 | After epoch with non-zero pool, sum of successful claims ≤ `sfEmissionRate` / pool commitment | Cap holds | |
| B2 | Claim when `sfEpochPoolBalance` exhausted | `tecUNFUNDED` or zero pay / no over-drain | |
| B3 | Mint extra vault/AMM LP after epoch open, then claim | Pay does not exceed basket math with live-denom floor; treasury not over-drained | |
| B4 | Duplicate claim same epoch | `tecDUPLICATE` | |
| B5 | ClaimReward by non-owner of bond | `tecNO_PERMISSION` | |

If epochs are long: document “claims deferred to post-epoch soak” or use short-epoch **lab** build (label results carefully).

---

## C. Consensus / liveness

| # | Test | Expected | Pass? |
|---|------|----------|-------|
| C1 | Stop 1 of 3 validators 10+ minutes | Remaining set continues if quorum allows | |
| C2 | Restart stopped node | Resyncs, rejoins | |
| C3 | Network partition brief | Recovery without permanent split brain | |
| C4 | Destructive suite (optional) | `tools/destructive_testing/qxrp_destructive_consensus_test.py` | |

---

## D. Falcon identity

| # | Test | Expected | Pass? |
|---|------|----------|-------|
| D1 | Config with classical `[node_seed]` | Node refuses to start | |
| D2 | `wallet_propose` falcon512 | Returns falcon_secret + long pubkey | |
| D3 | Payment with Falcon signature | `tesSUCCESS` | |

---

## Sign-off

| Field | Value |
|-------|--------|
| Image digest | |
| Git commit | |
| Date | |
| Operator | |
| Result | PASS / FAIL |
| Notes | |
