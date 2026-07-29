# Testnet 1001 — BitcoinSPVBridge amendment (ready, not activated)

**Goal of this doc:** Get the SPV/BitVM bridge **code and ops path** to the point where the fleet only needs to **deploy the binary and (later) vote the amendment**.  
**Do not activate on 1001 until product/security sign-off.** This document stops short of enabling.

---

## 1. What “ready for network push” means

| Checkpoint | Status |
|------------|--------|
| Amendment registered `BitcoinSPVBridge` DefaultNo | Yes (`features.macro`) |
| All SPV txs `temDISABLED` when amendment off | Yes (preflight gates) |
| Isolated e2e mint + peg-out | **PASS** (`e2e_full_peg.py`) |
| Force-enable only on 1101 sample cfg | Yes (`cfg/btc-spv-isolated.cfg`) |
| **Not** in `cfg/falcon-validator.cfg` / 1001 sample `[features]` | Yes — keep it that way |
| Fleet enable script exists, **default dry-run** | `scripts/enable-btc-spv-fleet.sh` |
| Binary built from this branch | Build on deploy hosts / CI |

When operators are ready to **enable** on 1001, they follow §5. Until then: **ship binary only** (amendment supported, vetoed by default, not enabled).

---

## 2. Amendment identity

```text
Name:    BitcoinSPVBridge
Macro:   featureBitcoinSPVBridge
Support: Supported::Yes
Vote:    VoteBehavior::DefaultNo   ← dark until majority up-votes
```

**Hash:** After deploying this `xrpld`, query any node:

```bash
curl -s -X POST "$RPC" -H 'Content-Type: application/json' \
  -d '{"method":"feature","params":[{"feature":"BitcoinSPVBridge"}]}' \
  | python3 -m json.tool
```

Expect: `"supported": true`, `"enabled": false`, `"vetoed": true` (or false after un-veto, still not enabled until majority time).

Do **not** hard-code the hash into production docs until the deployed binary’s `feature` RPC is the source of truth (hash is content-defined from the feature name registration).

---

## 3. Network rules (hard)

1. **Never** add `BitcoinSPVBridge` to `[features]` on network_id **1001** or mainnet.  
   `[features]` **force-enables** at genesis/rules for standalone/research only (1101).
2. Public nets use **amendment majority** only (`[amendments]` / validator votes).
3. Isolated research remains **1101** with `[features] BitcoinSPVBridge` if needed for soak.

---

## 4. Pre-push checklist (this PR / branch)

- [x] Code + isolated e2e  
- [x] Design + proof docs under `docs/btc-spv/`  
- [x] Enable script dry-run / prepare mode  
- [ ] Build Release `xrpld` for fleet architecture  
- [ ] Deploy binary to validators (amendment still **off**)  
- [ ] Confirm `feature BitcoinSPVBridge` appears on public RPC  
- [ ] **Stop here** until explicit activation decision  

---

## 5. Later: activation sequence (do not run until approved)

When product/security says go:

1. Ensure **all** bonded validators run a binary that supports `BitcoinSPVBridge`.  
2. Set `amendment_majority_time` if needed (fleet scripts often use 15 minutes on testnet).  
3. Un-veto + vote on each validator (pattern in `scripts/enable-lending-fleet.sh`).  
4. Wait until `enabled=true` on public RPC.  
5. **Then** operational steps: one-time `BTCBridgeActivate` with chosen Bitcoin network (testnet3/signet/regtest for drills — **not mainnet BTC** without a separate gate), header submitters, wallet UX.

Activation helper (when approved):

```bash
# Dry-run / prepare only (default) — does not patch or vote
bash scripts/enable-btc-spv-fleet.sh

# ONLY after written approval:
# bash scripts/enable-btc-spv-fleet.sh --execute --wait
```

---

## 6. Dependencies on chain

SPV mint uses **MPT** issuance for FBTC. Testnet should already have (or will need) related amendments in the same binary generation:

- Prefer: `MPTokensV1` already enabled on 1001 before SPV activation.  
- SPV code also lists MPT create paths; if MPT is missing, activate/mint will fail even with SPV on.

Check:

```bash
curl -s -X POST "$PUBLIC_RPC" -H 'Content-Type: application/json' \
  -d '{"method":"feature","params":[{}]}' \
  | python3 -c "import sys,json;f=json.load(sys.stdin)['result']['features'];
print([(v['name'],v.get('enabled')) for v in f.values() if v.get('name') in
('BitcoinSPVBridge','MPTokensV1','ProofOfParticipation')])"
```

---

## 7. Rollback

- If binary is deployed but amendment **not** enabled: no protocol behaviour change (all SPV txs `temDISABLED`).  
- If amendment was enabled by mistake: stop header/deposit ops; there is **no automatic unmint** — isolation and mint caps exist for that reason. Prefer never enable without caps + ops plan.

---

## 8. Related

- Proof report: [SPV-BRIDGE-PROOF-REPORT.md](./SPV-BRIDGE-PROOF-REPORT.md)  
- Design: [BITCOIN_SPV_LIGHT_CLIENT_DESIGN.md](./BITCOIN_SPV_LIGHT_CLIENT_DESIGN.md)  
- Isolated net: [ISOLATED_TESTNET.md](./ISOLATED_TESTNET.md)
