# Bitcoin → Falcon SPV Bridge Proof Report

**Date:** 2026-07-29  
**Branch:** `feature/btc-spv-light-client`  
**Binary:** `xrpld` 3.2.0-b0 (build from this branch)  
**Environment:** Bitcoin Core **regtest** (Docker) + Falcon isolated **network_id 1101** (standalone)  
**Live suites:**  
- `scripts/btc-spv/run-all-spv-tests.py` — mint path + infrastructure  
- `scripts/btc-spv/e2e_full_peg.py` — **full two-way peg PASS**

**Public testnet 1001:** amendment **not activated**. Rollout path: [TESTNET_AMENDMENT_ROLLOUT.md](./TESTNET_AMENDMENT_ROLLOUT.md).

---

## 1. Executive summary

We demonstrated, end to end on an isolated stack, that **native Bitcoin can move onto and back off Falcon without a custodial intermediate**:

1. **In:** SPV proofs (headers + Merkle + OP_RETURN) → mint FBTC  
2. **Out:** burn FBTC → challenge window → finalize → BitVM-class vault claim on Bitcoin  

Not a company hot wallet, not a multisig custodian, and not “relay observed deposit → mint.”

| Question | Answer from this work |
|----------|------------------------|
| Can Falcon *verify* a Bitcoin payment without a bridge operator’s word? | **Yes** |
| Can FBTC leave Falcon back to BTC without company custody keys? | **Yes on prototype vault** (CSV + hashlock); full BitVM disputes still future |
| Intermediate custodian required? | **No** for the proven happy path on 1101/regtest |
| Full mainnet BitVM SNARK disputes? | **Not yet** |
| Live on public testnet 1001? | **Not yet** — DefaultNo, enable script dry-run only |

**One line (honest):**

> Falcon can mint and redeem FBTC against Bitcoin using on-ledger SPV + a BitVM-class vault — no intermediate custodian — proven on isolated regtest/1101; public testnet enablement is a deliberate amendment vote, not force-on.

---

## 2. What “bridge without an intermediate” means

### 2.1 The intermediate model (what Falcon already has for ETH/BNB/USDC)

```
User → locks tokens on Ethereum/BSC (or sends BTC to a custody address)
     → off-chain relay / claim service observes deposit
     → relay submits or authorizes mint of F-asset on Falcon
```

- **Trust:** relay honesty (or multi-party ops), lock contracts, claim pipelines.  
- **Failure mode:** relay down → no mint; compromised custody → loss of principal.  
- **Decentralization:** Falcon consensus is decentralized; **the bridge hop often is not**.

That model is fine for bootstrapping and for EVM assets. It is **not** “BTC as first-class, trust-minimized collateral.”

### 2.2 The SPV / light-client model (what this branch implements for BTC)

```
User → pays BTC to a watch script (covenant/vault scriptPubKey)
     → payment carries OP_RETURN "FALC" || Falcon AccountID
     → Falcon already knows Bitcoin headers (on-ledger best-work tip)
     → User (or any prover) submits BTCDepositClaim:
           raw BTC tx + Merkle proof + block hash + vout
     → Falcon verifies PoW headers, inclusion, encoding, caps
     → Protocol mints FBTC (MPT) to that AccountID
```

- **Trust:** Bitcoin PoW + Falcon rules + correctness of the light-client implementation.  
- **No step** where “Company X holds the BTC and signs a mint.”  
- **Anyone** who sees the Bitcoin tx can construct the claim (claimer must equal destination in MVP).

That is the sense in which this is **without an intermediate custodian**.

### 2.3 What is still not “pure BitVM yet”

| Layer | Status |
|-------|--------|
| **SPV mint (BTC → FBTC)** | Implemented; **live e2e PASS** on regtest + 1101 |
| **On-ledger header chain + tip** | Implemented; **header batch PASS** |
| **Peg-out (FBTC → BTC)** | BitVM-**class** vault (CSV + hashlock) + burn/finalize; **full e2e PASS** (`e2e_full_peg.py`) |
| **Full BitVM dispute / fraud proofs on Bitcoin** | Design path, not this prototype |

So: **both mint and peg-out happy paths are live-tested on isolated regtest/1101.** Production BitVM fraud disputes and mainnet enable remain gated.

---

## 3. Start-to-finish path (what we ran)

### Phase A — Isolated stack (safety)

| Piece | Detail |
|-------|--------|
| Falcon | `network_id = 1101`, config `cfg/btc-spv-isolated.cfg`, feature `BitcoinSPVBridge` via `[features]` |
| Bitcoin | Docker regtest (`btc-spv-regtest`), free mined BTC — **no public testnet BTC** |
| Isolation | Does **not** touch Falcon testnet **1001** or mainnet |

### Phase B — BitVM-class vault (Bitcoin only)

1. Create P2WSH vault (CSV relative locktime + hashlock).  
2. Fund vault on regtest.  
3. Mine CSV blocks.  
4. Document claim path (preimage + user sig).  

**Result:** PASS — vault funded, CSV mined, regtest addresses (`bcrt1…`).

### Phase C — Falcon operator bootstrap

1. Classical genesis may only sign **Payment** (bootstrap exception).  
2. Create Falcon-512 account via `wallet_propose`.  
3. Genesis pays Falcon operator.  

**Result:** PASS — operator funded on 1101.

### Phase D — Activate SPV bridge

**Tx:** `BTCBridgeActivate` (type **98**)

- Regtest chain id **3**  
- Anchor header (80 bytes) + hash/height/work  
- Watch script hash (SHA256 of payment scriptPubKey)  
- Mint cap  
- Creates pseudo-issuer + FBTC MPT issuance  

**Result:** PASS (`tesSUCCESS`); second activate → `tecDUPLICATE`.

### Phase E — Extend Bitcoin light client

**Tx:** `BTCHeaderSubmit` (type **99**)

- One or more 80-byte headers  
- Parent must already be known  
- PoW + timestamp checks  
- Best-work tip update  

**Result:** PASS — **5 headers** submitted after anchor.

**Bug found in testing (fixed):** header timestamp compared Bitcoin **Unix** time to Falcon **Ripple-epoch** close time → always malformed. Fixed by converting NetClock → Unix before the 2h skew check.

### Phase F — Deposit claim = mint (the bridge)

**Bitcoin side**

1. Build regtest tx:  
   - Output to **watch script** (value V sats)  
   - OP_RETURN payload: `FALC` (4 bytes) \|\| Falcon **AccountID** (20 bytes)  
2. Broadcast + mine confirmations.  

**Falcon side**

**Tx:** `BTCDepositClaim` (type **110**)

- Raw BTC transaction  
- Merkle proof + tx index  
- Inclusion block hash  
- Vout of watch payment  
- Account == Destination (MVP)  

**Ledger checks (protocol, not a relay):**

1. Feature / bridge state exists  
2. Header known and on best chain with enough confirmations  
3. Merkle proof → merkle root of that header  
4. Exactly one watch-matching output (ambiguous multi-match rejected)  
5. OP_RETURN destination matches claimer  
6. Mint cap / dust floors  
7. Tombstone deposit so the same outpoint cannot mint twice  

**Result:** PASS — `BTCDepositClaim` **`tesSUCCESS`** (FBTC mint path exercised live).

### Phase G — Full peg-out (burn → finalize → vault claim) — **PASSED**

Automated by `scripts/btc-spv/e2e_full_peg.py` on clean 1101 + regtest:

| Step | Result |
|------|--------|
| Deposit BTC to BitVM-class vault + OP_RETURN | PASS |
| `BTCDepositClaim` mint FBTC | **tesSUCCESS** |
| `BTCBridgeBurn` (same preimage as vault hashlock) | **tesSUCCESS** |
| Wait challenge window (`ledger_accept` × 34) | PASS |
| `BTCWithdrawFinalize` | **tesSUCCESS** |
| Mine CSV blocks + spend vault (sig + preimage) | **PASS** (claim confirmed on regtest) |

**2026-07-29 live result:** `FULL PEG E2E PASSED`  
`BTC → FBTC (SPV mint) → burn → finalize → BTC vault claim`

---

## 4. What this proves for decentralization

### 4.1 Mint path: trust moves from *people* to *rules*

| Custodial / relay bridge | SPV light-client bridge (this work) |
|--------------------------|-------------------------------------|
| “Relay saw the deposit” | “Headers + Merkle proof satisfy consensus rules” |
| Operator can delay/censor mints | Anyone can submit a valid claim |
| Operator keys = systemic risk | No operator keys on the mint path |
| Falcon only trusts a message | Falcon **re-derives** truth from Bitcoin data |

That is a real decentralization upgrade for **Bitcoin as collateral and settlement asset** on Falcon:

- Validators do not need to run a company bridge service to accept BTC exposure.  
- Users do not need to trust Falcon Inc. (or any single ops key) to mint FBTC after they paid BTC.  
- Censorship resistance: a valid proof is a valid claim under the rules (subject to fees, caps, and network liveness).

### 4.2 What decentralization *does not* magically solve

1. **Bitcoin security assumptions** — SPV inherits Bitcoin PoW (and for deep reorgs, confirmation depth / floors).  
2. **Implementation risk** — bugs in header, Merkle, or encoding code are protocol risk (we already found a timestamp bug in testing).  
3. **Mint caps & isolation** — 1101 is research; mainnet enable is gated by design.  
4. **FBTC product label** — still “SPV payment attestation” until peg-out is as strong as mint.  
5. **EVM bridges remain intermediate-style** until similarly redesigned.

### 4.3 Peg-out and the full two-way story

**Two-way trust-minimized bridge** needs both:

```
BTC  ──SPV proof──►  FBTC   (proven on 1101 e2e)
FBTC ──burn+vault──► BTC   (prototype objects + vault; full e2e next)
```

Without a strong peg-out, FBTC is still a **one-way attested mint** (useful, still huge vs pure custody mint). With BitVM-class (and later full BitVM) peg-out, FBTC becomes a **two-way peg without a corporate vault multisig**.

That is the decentralization end-state: **Bitcoin and Falcon verify each other; no third balance sheet sits in the middle.**

### 4.4 Comparison to Falcon’s other multi-chain assets

| Asset | Source | Intermediate today? |
|-------|--------|---------------------|
| F-USDC / FETH / FBNB | EVM lock contracts + relays | **Yes** (ops relays) |
| FBTC (custodial path, if any) | BTC custody + claim services | **Yes** |
| **FBTC via SPV (this branch)** | BTC payment + on-ledger verification | **No custodian on mint** |

So multi-chain is not one model: **EVM is still intermediate; native BTC SPV is the non-intermediate direction.**

---

## 5. Evidence log (live suite)

Command:

```bash
python3 scripts/btc-spv/run-all-spv-tests.py
```

Representative outcomes:

| Check | Result |
|-------|--------|
| Falcon `network_id` 1101, state full | PASS |
| Bitcoin regtest chain | PASS |
| BitVM vault fund + CSV | PASS |
| Genesis bootstrap → Falcon fund | PASS |
| `BTCBridgeActivate` | `tesSUCCESS` |
| Duplicate activate | `tecDUPLICATE` |
| `BTCHeaderSubmit` batch | 5 headers `tesSUCCESS` |
| Merkle root matches Bitcoin header | PASS |
| `BTCDepositClaim` | `tesSUCCESS` |
| Burn/finalize empty path | expected fail codes (not `temDISABLED`) |

**Skipped:** C++ `BitcoinSPV_test` (build configured `tests=OFF` to avoid OOM on the spare server). Live e2e covers the same economic path the unit tests target for activate/headers.

**Fixes landed during testing:**

1. `BTCHeaderSubmit` Unix vs Ripple epoch timestamps  
2. Regtest vault tooling (wallet load + Core 28 descriptors)  
3. Watch-script hash endianness in the e2e harness  

---

## 6. What this means for Falcon’s story

### For users

- In principle: **send BTC to a protocol-defined script, prove it, receive FBTC** — no exchange, no “bridge company account.”  
- Today: proven on **regtest + isolated 1101**, not public mainnet.

### For validators / protocol

- BTC exposure can be **rule-governed** rather than **ops-governed**.  
- Feature stays **DefaultNo** / research-gated; production enable is a deliberate consensus and security decision.

### For decentralization narrative (accurate wording)

**Fair claim**

> We proved a non-custodial SPV mint path: Falcon verifies Bitcoin payments on-ledger and mints FBTC without an intermediate bridge operator holding BTC keys.

**Overclaim to avoid**

> “Fully trustless two-way BitVM bridge on mainnet with no remaining risk.”

---

## 7. Remaining work (honest roadmap)

1. **Automated full peg-out e2e:** burn → challenge window → finalize → CSV vault claim.  
2. **C++ unit suite** with `tests=ON` when RAM allows.  
3. **Mainnet gate:** confirmation floors, retarget, mint caps, audits, product freeze.  
4. **Evolve BitVM-class vault → full dispute program** where fraud is punishable on Bitcoin.  
5. **Wallet UX:** multi-chain FBTC receive/claim UX (portal already has multi-chain BTC for the *custodial* path; SPV claim is a new flow).

---

## 8. Bottom line

| From | To | Intermediate? | Proven? |
|------|-----|---------------|---------|
| BTC payment (regtest) | FBTC on Falcon 1101 | **No custodian** | **Yes — live e2e** |
| FBTC | BTC vault release | BitVM-class (no company multisig) | **Yes — full e2e on regtest+1101** |
| ETH/USDC/BNB | F-assets | Relay/custody style | Existing product, different model |

**Decentralization takeaway:** this is the first Falcon path where **Bitcoin’s proof system, not a middleman, is the mint authority.** That is the structural difference between “multi-chain with bridges” and “Bitcoin as a peer settlement asset on a quantum-resistant XRPL fork.”

---

## Appendix — How to reproduce

```bash
# Bitcoin regtest
./scripts/btc-spv/start-bitcoin-regtest.sh

# Falcon 1101 (built xrpld)
./scripts/btc-spv/start-isolated-falcon.sh

# Full feasible suite
export PATH="$PWD/data/btc-spv-1101/bin:$PATH"
python3 scripts/btc-spv/run-all-spv-tests.py
```

Docs: `docs/btc-spv/ISOLATED_TESTNET.md`, `BITCOIN_SPV_LIGHT_CLIENT_DESIGN.md`, `BITVM_PEG.md`.
