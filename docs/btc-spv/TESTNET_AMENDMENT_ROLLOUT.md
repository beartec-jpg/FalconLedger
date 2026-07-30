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

## 5. Activation sequence — **amendment vote only** (no force)

### Never do this on 1001

```ini
# WRONG — force-enables rules, can fork the network
[features]
BitcoinSPVBridge
```

### Right way

1. **Deploy binary** with SPV code to **every** bonded validator (same generation).  
   Until then, `feature` RPC will not list `BitcoinSPVBridge` at all.
2. Confirm on public RPC: `supported=true`, `enabled=false` (usually `vetoed=true` by DefaultNo).
3. Each validator **votes yes** by listing the amendment hash under **`[amendments]`** (not `[features]`) and setting `vetoed=false` via admin RPC.
4. Keep **`[amendment_majority_time]`** (testnet often `15 minutes`).
5. Wait until enough validators vote and majority time elapses → `enabled=true`.
6. Only then run ops (`BTCBridgeActivate`, header submitters, etc.).

### Fleet helper

```bash
# Status + dry-run (safe)
bash scripts/enable-btc-spv-fleet.sh

# After ALL vals run new binary — starts amendment vote + waits for enable
bash scripts/enable-btc-spv-fleet.sh --execute --wait
```

### Single-validator commands (send to other ops)

After they have upgraded `xrpld` to a build that includes `BitcoinSPVBridge`:

```bash
HASH=76DAF975D0E23358239AED3C74A8600600A0FBEBB7E12B1FEA314C41469F3D33
# Adjust CFG + container name per host
CFG=/var/lib/qxrp-validator/config/xrpld.cfg
CONTAINER=qxrp-validator

# Ensure majority clock exists
grep -q '\[amendment_majority_time\]' "$CFG" || printf '\n[amendment_majority_time]\n15 minutes\n' >> "$CFG"
grep -q '\[amendments\]' "$CFG" || printf '\n[amendments]\n' >> "$CFG"
# Vote YES (amendment list) — never under [features]
grep -q "$HASH" "$CFG" || echo "$HASH BitcoinSPVBridge" >> "$CFG"

# Remove if someone force-listed it under [features] by mistake
# (edit cfg by hand: delete BitcoinSPVBridge lines under [features] only)

docker restart "$CONTAINER"
sleep 5
docker exec "$CONTAINER" curl -sf -X POST http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d '{"method":"feature","params":[{"feature":"BitcoinSPVBridge","vetoed":false}]}'

# Watch network (from anywhere)
curl -s -X POST http://46.224.0.140:6005 -H 'Content-Type: application/json' \
  -d '{"method":"feature","params":[{"feature":"BitcoinSPVBridge"}]}'
```

Amendment hash (this branch):  
`76DAF975D0E23358239AED3C74A8600600A0FBEBB7E12B1FEA314C41469F3D33` = `BitcoinSPVBridge`

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
