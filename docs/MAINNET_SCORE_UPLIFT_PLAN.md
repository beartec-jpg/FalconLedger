# Mainnet score uplift plan (economic security · readiness)

**Status:** Active tracking · **Date:** 2026-07-23  
**Pin:** `b007db22d` · `qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5`  
**Target (honest):** Economic security **6.5 → 7.5–8.0** · Mainnet readiness **5.5 → 7.0–7.5**  
**Companion:** `MAINNET_SECURITY_FREEZE.md`, `PROTOCOL_READINESS_7_5_PLAN.md`, `qxrp/MAINNET_READINESS_SUMMARY.md`

This plan lists **in-repo and ops actions that raise independent technical review scores without over-claiming**.  
It does **not** authorize: real ETH mainnet lock deploy, `MAINNET_LIVE=true`, enabling ABSENCE/INVALID_VOTE slash, or wiping T0 secrets prematurely.

---

## Score model (what reviewers reward)

| Axis | What moves the needle | What does **not** |
|------|----------------------|-------------------|
| **Economic security** | Honest slash surface, emission tables, faucet Sybil isolation, UNL charter, governance surface clarity | Fake-enabling incomplete slash; marketing supply claims |
| **Mainnet readiness** | Freeze pin + digest, multi-day soak artifacts, claim e2e, ceremony dry-run, LIVE=false gates | Declaring “live” while soak/ceremony/ETH lock incomplete |

---

## Priority A — already landed (credit these)

| Gate | Evidence | Score impact |
|------|----------|--------------|
| All bonded pay ∝ score (no ActiveSet K=32) | `1af01dbfb` + smoke | Eco + readiness |
| PoP Supported::Yes at genesis | `bb82cba1b` | Readiness (bond/claim work) |
| Scoring Blob lifetime fix | `b007db22d` + ClaimReward e2e | Readiness (scores write) |
| Hub pin mainnet-v2 scorefix | `IMAGE_DIGEST.txt` `sha256:9362005f…` | Readiness |
| ClaimReward fast-epoch e2e | `dry-runs/mainnet-v2-claim-e2e-results.json` | Readiness |
| DOUBLE_SIGN slash only honesty | code + whitepaper status notes | Eco (honesty) |

---

## Priority B — in progress (this uplift track)

| # | Gate | Owner artifact | Status |
|---|------|----------------|--------|
| B1 | Multi-day soak on freeze pin (≥48 h, better 7 d) | `ops/SOAK_RUNBOOK.md` · `dry-runs/SOAK_CHECK.md` · soak snapshots | 🔄 soak running (~10 h+ as of 2026-07-23) |
| B2 | Slash model honesty doc | `qxrp/slash-model.md` | ✅ this PR track |
| B3 | Year-1 emission tables (CID) | `qxrp/epoch-emission.md` + tables | ✅ this PR track |
| B4 | Bootstrap UNL charter | `qxrp/UNL_CHARTER.md` | ✅ this PR track |
| B5 | Governance surface inventory | `qxrp/GOVERNANCE_SURFACE.md` | ✅ this PR track |
| B6 | Faucet anti-Sybil hardening | portal `faucet-quota` + route | ✅ this PR track |
| B7 | Freeze digest consistency in freeze docs | `MAINNET_SECURITY_FREEZE.md` | ✅ this PR track |
| B8 | Public readiness / index sync | `MAINNET_READINESS_SUMMARY` · `PUBLIC_DOCS_INDEX` | ✅ this PR track |

---

## Priority C — ops / human (required for 7.5 readiness)

| # | Gate | Notes | Status |
|---|------|-------|--------|
| C1 | Soak ≥48 h continuous, no crash | Archive snapshots; leave fleet up | 🔄 |
| C2 | Ceremony cold keys offline | `wallet_propose` on freeze image | ❌ human |
| C3 | Final UNL public keys | `validators/unl-public.txt` | ❌ human |
| C4 | Wipe rehearsal secrets before T0 | Never reuse soak keys | ❌ after soak |
| C5 | ETH mainnet multi-sig lock redeploy | N-of-M, cold owners; not Sepolia | ❌ human / separate |
| C6 | Neon schema + portal `LIVE=false` | `portal.env.mainnet` | ❌ ops |
| C7 | Fund mainnet FAUCET from ceremony | Only after split | ❌ T0 |
| C8 | External audit (optional) | Scope freeze doc exists | optional |

---

## Priority D — post-T0 / protocol follow-ons (do not block freeze)

| # | Item | Note |
|---|------|------|
| D1 | OPEN_UNL amendment code | Design only until network sized |
| D2 | ABSENCE / INVALID_VOTE slash enable | Needs detection redesign |
| D3 | Absolute RTT latency scoring | Relative baseline accepted at launch |
| D4 | Vote accuracy independent index | Currently tracks trusted correct-hash path |

---

## Explicit non-claims (keep scores honest)

- **Not public mainnet** until ceremony + public RPC + `LIVE=true` after green checks.  
- **Not full slash suite** — only DOUBLE_SIGN is live; others `temDISABLED`.  
- **Not open UNL** — bootstrap operator UNL until amendment.  
- **Not audited third-party** until engagement complete.  
- **Faucet ≠ economic security of consensus** — it reduces Sybil air-drop gaming; validators still need bonds.

---

## How to re-score after this track

| Axis | After Priority B + ≥48 h soak | After C1–C7 |
|------|------------------------------|-------------|
| Economic security | ~7.5 (honest docs + faucet isolation + slash clarity) | ~8.0 if ETH multi-sig + funded isolated faucet |
| Mainnet readiness | ~7.0 (pin + claim e2e + soak aging) | ~7.5–8.0 after ceremony dry-run wipe + LIVE gates |

Update this table when soak hits 48 h / 7 d and when ceremony gates flip.
