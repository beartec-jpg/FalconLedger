# Governance surface inventory (what can change, how)

**Status:** Launch-truth map · **Date:** 2026-07-23  
**Related:** [governance.md](governance.md) · [OPEN_UNL_AMENDMENT.md](OPEN_UNL_AMENDMENT.md) · [slash-model.md](slash-model.md)

Reviewers need a crisp answer to: *who can change what, with what weight, and what is frozen?*

---

## Layers

| Layer | Mechanism | Weight / gate | At T0 |
|-------|-----------|---------------|-------|
| **A. Protocol code** | git freeze image + digest | Operators rebuild + redeploy fleet | **Frozen** to pin |
| **B. On-chain params** | `GovernanceProposal` / `GovernanceVote` | Score-weighted supermajority (67%) | **Burn BPS only** |
| **C. Amendments** | Feature / amendment enable | Majority amendment process (rippled-style) | PoP + names + lending stack at genesis; OPEN_UNL **not** |
| **D. Bootstrap UNL** | Operator-published list | Ops charter | Active until open-UNL |
| **E. Portal / faucet** | Env + Neon | Ops secrets; `LIVE=false` until T0 | Off-chain |

---

## A — Protocol freeze (hard)

Consensus, scoring formula shape, claim hard-caps, DOUBLE_SIGN verification, Falcon-only identity: **change requires new freeze commit + image digest + re-smoke + re-soak**.

See `scripts/mainnet-ceremony/FREEZE_COMMIT.txt` and `IMAGE_DIGEST.txt`.

---

## B — On-chain governance (soft params)

Documented in [governance.md](governance.md):

| Proposal type | Field | Notes |
|---------------|-------|-------|
| Burn BPS (`kPROPOSAL_TYPE_BURN_BPS = 1`) | `sfCurrentBurnBps` on `ltGOVERNANCE_PARAMS` | Only type at launch |

- Voting window ≈ 1 epoch (`kGOVERNANCE_VOTING_LEDGERS`)  
- Pass if YES weight ≥ 6700 bps of aggregate composite score  
- Requires ProofOfParticipation amendment active  
- Score snapshot at vote time (no retroactive weight games)

**Not** governable on-chain at T0: emission CID constants, bond minimum, slash BPS, UNL membership, treasury keys (treasury is protocol-owned).

---

## C — Amendments (protocol features)

| Feature | Genesis | Notes |
|---------|---------|-------|
| ProofOfParticipation | Yes | Bond / claim / emission gates |
| AccountNames | Yes | Human names |
| AMM / lending stack | Yes (as configured) | Product paths |
| OPEN_UNL (planned) | **No** | Design only — [OPEN_UNL_AMENDMENT.md](OPEN_UNL_AMENDMENT.md) |
| ABSENCE / INVALID_VOTE slash enable | **No** | Codes present; `temDISABLED` |

---

## D — Bootstrap UNL

Ops-published trust list. See [UNL_CHARTER.md](UNL_CHARTER.md).  
Does not control pay. Changes are operational, not on-chain votes.

---

## E — Off-chain product surface

| Control | Default mainnet posture |
|---------|-------------------------|
| `NEXT_PUBLIC_MAINNET_LIVE` / portal live flags | **false** until T0 green |
| Faucet drip amount / daily cap / cooldown | Env + durable quota |
| Bridge relay / multi-sig owners | Ops custody; ETH mainnet redeploy separate |
| Airdrop freeze | Neon `airdrop_config.frozen_at` |

Compromising the portal does **not** rewrite ledger rules; worst case: faucet drain within caps, UI lies. Consensus security remains on validators + pin.

---

## Explicit non-goals of “governance” marketing

- No token-weighted DAO controlling treasury at T0  
- No off-chain council that can slash by decree  
- No silent emission schedule edits without new binary  

---

## Checklist for external review

- [x] Inventory written (this file)  
- [x] Burn-BPS path documented in governance.md  
- [x] Slash enablement honesty in slash-model.md  
- [x] UNL vs pay separation in UNL_CHARTER + OPEN_UNL  
- [ ] Optional: third-party review of ValidatorSlash + Claim paths (scope freeze doc)  
