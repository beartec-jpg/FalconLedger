# Validator slash model (honest launch surface)

**Status:** Protocol freeze truth · **Pin:** mainnet-v2 / `b007db22d`  
**Audience:** reviewers, operators, community  
**Related:** [validator-lifecycle.md](validator-lifecycle.md) · whitepaper §6.4

---

## One-line summary

**At launch, only cryptographically proven double-signing burns bond.**  
Absence and invalid-vote offense codes exist in the protocol surface but return `temDISABLED` until detection is production-ready. Do not claim a full three-mode slash suite at T0.

---

## Offense matrix

| Code | Name | Bond effect (when enabled) | Launch status | Evidence |
|------|------|----------------------------|---------------|----------|
| 1 | **DOUBLE_SIGN** | **100%** burn + forced unbond | **Enabled** | Two Falcon-signed `STValidation` blobs: same consensus key, same ledger seq, different hashes |
| 2 | ABSENCE | 25% (`kSLASH_ABSENCE_BPS = 2500`) | **`temDISABLED`** | Sustained absence (design: 3+ epochs) — detection not ready |
| 3 | INVALID_VOTE | 50% (`kSLASH_INVALID_VOTE_BPS = 5000`) | **`temDISABLED`** | Proven invalid vote path — detection not ready |

Constants: `include/xrpl/protocol/QXRPConstants.h` (`kSLASH_*`).

---

## Economic properties (DOUBLE_SIGN)

1. **Pure burn** — slashed drops use `destroyXRP`; not paid to reporter, not credited to treasury.  
2. **No bounty** — removes profit motive for grief / fake reporting.  
3. **Forced unbond** — 100% slash moves the validator into UNBONDING immediately.  
4. **Crypto gate** — garbage evidence → `tecNO_PERMISSION` (or equivalent reject); cannot slash without valid dual validations.

---

## What is *not* economic security today

| Mechanism | Role at T0 |
|-----------|------------|
| Score penalties (uptime / consistency) | Soft: lower ClaimReward share; no bond burn |
| Unbonding lock (~30 d) | Exit delay; past proven offenses may still apply |
| Bootstrap UNL | Trust list for who closes ledgers — **not** a slash |
| Faucet rate limits | Sybil resistance for free drips — **not** consensus security |

Reviewers should weight **DOUBLE_SIGN + bond + score-weighted pay** as the live deterrent set, not the disabled codes.

---

## Why ABSENCE / INVALID_VOTE stay disabled

- **False positives** on absence (network partitions, client bugs) can punish honest validators.  
- **Invalid-vote** needs a robust, non-griefable adjudication path beyond score demerits.  
- Enabling incomplete detection would **lower** economic-security scores (dishonest claims + operator risk).

Enable path: redesign detection → unit + fuzz + adversarial soak → amendment or feature flip with supermajority ops sign-off. Tracked as post-T0 in `MAINNET_SCORE_UPLIFT_PLAN.md` Priority D.

---

## Operator checklist

- [x] Document launch surface honestly (this file)  
- [x] Whitepaper status note matches code  
- [ ] Rehearse DOUBLE_SIGN on private net before T0 (optional confidence)  
- [ ] Never advertise “full slashing” in public materials until codes 2–3 ship  

---

## Test expectations

| Case | Expected |
|------|----------|
| Valid dual-sign evidence | Slash applied; bond burned; unbond forced |
| Mismatched keys / seq / identical hashes | Reject, no slash |
| Offense code 2 or 3 | `temDISABLED` |
| Post-unbond new offenses | Policy: past may still prove; new after unbond start restricted (see lifecycle) |
