# Bootstrap UNL charter (genesis → open amendment)

**Status:** Policy charter for T0 · **Not** an open/rotating UNL  
**Related:** [OPEN_UNL_AMENDMENT.md](OPEN_UNL_AMENDMENT.md) · [governance.md](governance.md) · `MAINNET_SCORE_UPLIFT_PLAN.md`

---

## Purpose

Until the open-UNL amendment is enabled, Falcon Ledger uses a **bootstrap Unique Node List** published by launch operators. This charter states what that list is, what it is not, and how it evolves.

---

## Separation of concerns

| Concern | Mechanism at T0 |
|---------|-----------------|
| **Who closes ledgers** | Bootstrap UNL (this charter) |
| **Who gets paid** | All **bonded** validators with score ≥ claim floor, ∝ composite score |
| **Who can bond** | Anyone meeting bond minimum (PoP amendment) |
| **Slash of double-sign** | On-chain evidence (any reporter path per protocol) |

**Pay is not UNL membership.** Operators must not imply “only UNL earns.”

---

## Bootstrap rules

1. **Size** — Prefer a small, highly available set (e.g. 5–15) that can form quorum under launch latency.  
2. **Identity** — Falcon consensus public keys only; published in `unl-public.txt` / validator list artifacts.  
3. **Geographic / host diversity** — Avoid single-host or single-provider monopoly of the bootstrap set when feasible.  
4. **No silent swap** — Changes to the published bootstrap list are versioned (git / signed release note) and coordinated so fleets do not split.  
5. **Rehearsal keys ≠ mainnet** — Soak and dress-rehearsal UNL material is destroyed before T0; never reuse.

---

## Operator duties

| Duty | Description |
|------|-------------|
| Availability | Keep bootstrap nodes patched, monitored, and on the freeze digest |
| Disclosure | Publish public keys and (as appropriate) operator identity / contact |
| Non-censorship of pay | Do not gate ClaimReward or bonding on off-chain favor |
| Handoff | Drive toward open-UNL enable criteria (see OPEN_UNL_AMENDMENT) |

---

## What bootstrap UNL does **not** do

- Does not mint or reallocate treasury  
- Does not replace on-chain governance for burn BPS  
- Does not authorize ABSENCE slash by “missing from UNL”  
- Does not freeze the set of who may bond and score  

---

## Path to open / rotating UNL

See [OPEN_UNL_AMENDMENT.md](OPEN_UNL_AMENDMENT.md):

- Eligibility: bonded + score floor + consensus key  
- Size: ~max(MIN, 33% of eligible)  
- Rotation: ~10% seats per epoch with hysteresis and delayed apply  
- Enable only by amendment after network is sized and healthy  

Until enabled, **behavior stays bootstrap UNL only**.

---

## Reviewer note

Bootstrap trust is a **known centralization residual** at genesis. Economic security scores should treat it as temporary ops trust, partially offset by:

- open bonding + score-proportional pay  
- DOUBLE_SIGN slash  
- fixed CID emission + claim caps  
- explicit charter + future amendment design  

Not claiming “fully decentralized UNL at T0.”
