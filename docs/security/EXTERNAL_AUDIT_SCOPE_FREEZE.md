# External audit scope — FalconLedger freeze delta (item 11)

**Project:** Falcon Ledger / qXRP  
**Repository:** https://github.com/beartec-jpg/FalconLedger  
**Branch / tip:** `develop` @ freeze commit (see `scripts/mainnet-ceremony/FREEZE_COMMIT.txt`)  
**Classification:** Pre-mainnet protocol review  

---

## 1. Objective

Independent review of **qXRP-specific / freeze-delta** security before public mainnet, focusing on post-quantum integration and economic transactors introduced or hardened for launch.

---

## 2. In scope (minimum)

### 2.1 Cryptography & consensus

- Falcon key types (`PQPublicKey` / `PQSecretKey`), verify/sign paths  
- `STValidation` Falcon validity  
- `RCLCxPeerPos::checkSign` Falcon proposal verification  
- Ban of classical `node_seed` / Falcon-only node identity  
- liboqs pin / supply-chain notes (`CMakeLists.txt` commit pin)

### 2.2 Slashing (C-01)

- `ValidatorSlash` preflight/preclaim/doApply  
- DOUBLE_SIGN evidence: dual Falcon STValidation, same key/seq, different ledger hash  
- ABSENCE / INVALID_VOTE disabled (`temDISABLED`)  
- Bond burn via `destroyXRP` (no slasher payout)

### 2.3 Emissions & claims (C-02)

- `RewardEpoch` / CID / first emission epoch 8  
- PoPL split (validator / vault LP / AMM LP)  
- `ClaimReward`, `ClaimLPReward`, `ClaimAmmLpReward`  
- Hard-cap to `sfEpochPoolBalance`  
- Live aggregate floor for LP/AMM denominators  

### 2.4 Scoring

- `ValidatorScoring` composite score  
- Relative latency vs earliest signer  
- Aggregate score writeback  

### 2.5 Bridge (if mainnet USDC planned)

- `contracts/FalconCollateralLock.sol` multi-sig model  
- Relay trust assumptions (`bridge-*-relay.py`)  
- Mainnet `REQUIRED≥2` policy  

### 2.6 Invariants

- `QXRPDropConservation`  
- Amendment gating `featureProofOfParticipation`  

---

## 3. Out of scope (default)

- Full historical XRPLF/rippled audit  
- Formal proof of Falcon primitive (defer to NIST/liboqs)  
- Side-channel / HSM certification  
- Social engineering / physical security  
- Portal/frontend UX (except faucet secret handling if requested)  

---

## 4. Deliverables requested

1. Written report with severity ratings  
2. PoC or clear repro for any High+ finding  
3. Remediation guidance  
4. Retest of critical fixes  

---

## 5. Artifacts to provide auditor

- Freeze git commit + image digest  
- This scope doc  
- Prior reports under `docs/security/`  
- `docs/PROTOCOL_READINESS_7_5_PLAN.md`  
- Soak + adversarial results when available  

---

## 6. Timeline suggestion

| Phase | Duration |
|-------|----------|
| Kickoff + access | 2–3 days |
| Review + testing | 2–3 weeks |
| Report | 3–5 days |
| Fix retest | 1 week |

Adjust to vendor capacity. Prefer audit **in parallel** with soak (items 2–3), not after public T0.
