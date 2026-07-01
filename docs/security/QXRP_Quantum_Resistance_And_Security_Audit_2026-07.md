# FalconLedger (qXRP) — Security & Quantum-Resistance Audit Report

**Report Title:** Independent Security & Post-Quantum Resistance Assessment
**Project:** FalconLedger / qXRP — Quantum-Resistant XRP Ledger fork (of XRPLF/rippled)
**Repository:** `beartec-jpg/FalconLedger`
**Audit Date:** 2026-07-01
**Auditor:** Independent third-party review (static source analysis + dynamic runtime verification of the published Docker image)
**Classification:** Testnet / Pre-Mainnet
**Report Type:** Full technical audit — security posture and quantum-resistance

---

## 1. Executive Summary

FalconLedger (qXRP) is an additive, post-quantum-first fork of the mature XRP Ledger
(`rippled`) reference implementation. It replaces the classical validator identity and
transaction-signing model with the NIST-selected **Falcon** lattice signature scheme
(Falcon-512 default, Falcon-1024 optional) supplied by **liboqs (Open Quantum Safe)**,
and layers on a protocol-controlled treasury, validator bonding, on-chain
Proof-of-Participation (PoP) scoring/rewards, slashing, and bounded governance.

This assessment combines **static source review** of the quantum-cryptography and
economic-protocol delta with **dynamic runtime verification** of the Docker image
published to Docker Hub on the audit date. The dynamic testing confirms that the
published binary genuinely performs Falcon account creation and Falcon transaction
signing end-to-end — not a classical fallback.

**Overall assessment.** The cryptographic architecture is sound and the post-quantum
integration is real and working at runtime. Post-quantum keys are modelled with a
dedicated type hierarchy (`PQPublicKey` / `PQSecretKey`) with prefix discrimination and
strict length validation, liboqs is pinned to an exact commit, and secret material is
securely erased. The most significant open issue is a **consensus-path verification gap**
(H-01 below) in which validator *proposals* are Falcon-signed but the proposal
*verification* path is still secp256k1-only and will abort on a Falcon key — a
consensus-safety risk for a pure-Falcon validator set that must be closed before the
network runs entirely on Falcon consensus keys.

### Quantum-resistance verdict

| Dimension | Status |
|---|---|
| Transaction signing (account authority) | ✅ Falcon (post-quantum) — verified at runtime |
| Validator identity / validation messages (`STValidation`) | ✅ Falcon-aware branching present and correct |
| Consensus proposal verification (`RCLCxPeerPos::checkSign`) | ⚠️ **Not yet Falcon-aware** — secp256k1-only (H-01) |
| Key generation randomness | ✅ liboqs Falcon keypair (not seed-derived) |
| Signature primitive supply chain | ✅ liboqs pinned to exact commit |
| Key material lifecycle (zeroization) | ✅ `secureErase` / `OPENSSL_cleanse` |

---

## 2. Scope and Methodology

**In scope**
- Post-quantum key classes and Falcon wrapper: `PQPublicKey`, `PQSecretKey`, `falcon.cpp/.h`.
- Signing/verification dispatch: `PublicKey.cpp`, `STTx.cpp`, `STValidation.cpp`.
- Consensus signing/verification paths: `RCLConsensus.cpp`, `RCLCxPeerPos.cpp`.
- qXRP economic transactors (`src/libxrpl/tx/transactors/qxrp/`), reward epoch and
  governance tally logic, drop-conservation invariant.
- Build & supply chain: `CMakeLists.txt` liboqs `ExternalProject`, `conanfile.py`.
- Operational artifacts: `docker/Dockerfile` and compose files.
- **The published Docker image** (`qxrp/xrpld:falcon` / `:latest`) — pulled and executed.

**Out of scope**
- The full upstream `rippled` attack surface (inherited; covered by Ripple's long-running
  bug-bounty and review history).
- Formal cryptographic proof of the Falcon implementation itself (delegated to liboqs /
  PQClean upstream and NIST standardization).
- Long-horizon economic / game-theoretic simulation of incentive alignment.
- Side-channel, HSM, and key-custody analysis (operator responsibility).

**Methodology**
- Manual source review of all quantum-crypto and qXRP-delta translation units.
- Cross-reference of implemented behaviour against project docs and the internal
  `audit-checklist.md`.
- Supply-chain review of the liboqs fetch/pin and the multi-stage Docker build.
- **Dynamic verification:** pulled the published image, inspected the compiled binary and
  its linked symbols, then ran a standalone node and exercised Falcon key generation,
  funding, and Falcon-signed transaction submission over RPC.

---

## 3. System Overview

qXRP preserves XRPL's RPCA consensus and fast finality while layering:

- **Cryptography.** Falcon-512 (default) and Falcon-1024 as first-class validator and
  account keys via liboqs. Post-quantum keys are distinguished by an on-wire prefix byte
  (`0xFB` = Falcon-512, `0xFC` = Falcon-1024) and validated by exact length.
- **Tokenomics.** Fixed supply with a protocol-controlled treasury (no private key),
  epoch-based emission (halving schedule), and a burn + validator-reward fee split.
- **Incentives & security.** `ValidatorBond` (minimum bond), composite PoP scoring,
  `ClaimReward`, `ValidatorSlash` (currently DOUBLE_SIGN at 100% active), and an
  unbonding timelock.
- **Governance.** On-chain proposals requiring a supermajority of aggregate score.

All new functionality is additive and amendment-gated behind
`featureProofOfParticipation`.

---

## 4. Quantum-Resistance Assessment

### 4.1 Signature scheme selection

Falcon is one of the NIST-selected post-quantum signature standards (a lattice /
NTRU-based scheme). The project uses the **liboqs (Open Quantum Safe)** implementation
rather than a bespoke re-implementation, which is the correct risk decision: the complex
and side-channel-sensitive Falcon signing/sampling code is delegated to a widely-reviewed
upstream library.

- Falcon parameter sets enabled: `Falcon-512` and `Falcon-1024` (plus padded variants
  present in the linked library).
- Public-key sizes: `kFALCON512_PUBKEY_BYTES = 897`, `kFALCON1024_PUBKEY_BYTES = 1793`
  (`include/xrpl/protocol/PQPublicKey.h`).

### 4.2 Key model and type safety

Post-quantum keys **cannot** be represented by the fixed-size classical `PublicKey`
(33 bytes) / `SecretKey` (32 bytes) types. The project introduces `PQPublicKey` /
`PQSecretKey` and dispatches classical-vs-Falcon by the leading prefix byte:

- `signingPubKeyType(Slice)`, `verify(Slice, msg, sig)`, and `calcAccountID(Slice)`
  branch on key type, preventing type confusion at both the type-system and runtime level
  (`src/libxrpl/protocol/PublicKey.cpp`, `STTx.cpp`).
- Falcon keys are **randomly generated** by the liboqs keypair routine and are not
  seed-derivable; `wallet_propose` returns a `falcon_secret` (hex of the on-wire public
  blob plus the raw secret key) rather than a recoverable seed
  (`src/xrpld/rpc/handlers/admin/keygen/WalletPropose.cpp`, `falcon.cpp`).

### 4.3 Key-material lifecycle

`PQSecretKey`'s destructor performs a guaranteed erase of secret bytes via `secureErase`
(`OPENSSL_cleanse`), consistent with the upstream classical `SecretKey` and resistant to
dead-store elimination (`src/libxrpl/protocol/PQSecretKey.cpp:29-33`).

### 4.4 Coverage of quantum-vulnerable paths

The one place where post-quantum coverage is **incomplete** is consensus proposal
verification — see **H-01**. Until that is resolved, a validator set running purely on
Falcon keys cannot safely verify each other's consensus proposals. This is the single
most important item gating a fully post-quantum consensus deployment.

---

## 5. Detailed Findings

Severity scale: **Critical / High / Medium / Low / Informational.**

### 5.1 High

#### H-01 — Consensus proposal verification is still secp256k1-only (consensus-safety)

**Locations:**
- `src/xrpld/app/consensus/RCLCxPeerPos.cpp` (`checkSign` → `verifyDigest`)
- `src/libxrpl/protocol/PublicKey.cpp` (`verifyDigest` requires secp256k1, else `logicError`)
- `src/xrpld/app/consensus/RCLConsensus.cpp` (proposal signing via `signFalcon`)

**Description.**
Validator proposals are **signed** with Falcon when a `falconSecret` is configured:

```cpp
// RCLConsensus.cpp
if (!keys.falconSecret.empty()) {
    auto decoded = decodeFalconSecret(keys.falconSecret);
    if (decoded) {
        auto const& [pqPk, pqSk] = *decoded;
        sig = signFalcon(pqSk, proposal.signingHash());
    }
}
```

However, the **verification** side has not been made Falcon-aware:

```cpp
// RCLCxPeerPos.cpp
bool RCLCxPeerPos::checkSign() const {
    return verifyDigest(publicKey(), proposal_.signingHash(), signature(), false);
}

// PublicKey.cpp — verifyDigest()
if (publicKeyType(publicKey) != KeyType::Secp256k1)
    logicError("sign: secp256k1 required for digest signing");
```

On a peer receiving a **Falcon-signed** proposal, `verifyDigest` will hit
`logicError(...)` and abort rather than verifying the signature. `STValidation::isValid`
was correctly updated to branch on key type (Falcon vs secp256k1), but the equivalent
proposal path was not.

**Impact.** For a validator set running entirely on Falcon consensus keys, proposal
verification cannot succeed and can crash the process (`logicError`). This blocks
fully-post-quantum consensus and is a denial-of-service / liveness risk. It is the
primary gap between "Falcon-signed transactions" (which work — see §6) and "Falcon-signed
consensus".

**Recommendation.** Mirror the `STValidation::isValid` pattern in
`RCLCxPeerPos::checkSign`: detect the key type via `signingPubKeyType(publicKey.slice())`
and route Falcon keys through the Slice-based `verify(pub, hash, sig)` dispatcher, keeping
`verifyDigest` only for secp256k1. Add a consensus-level unit/integration test that signs
and verifies a proposal with a Falcon validator key.

**Status:** Open (recommend fixing before any pure-Falcon consensus testnet).

---

### 5.2 Medium

#### M-01 — Full ASAN/UBSAN regtest and coverage targets not yet executed

**Location:** `audit-checklist.md`, `docs/security/security-testing.md`,
`.github/workflows/qxrp-security.yml`.

**Description.** A dedicated `qXRP Security CI` workflow now runs `cppcheck` on the qXRP
delta plus ASAN/UBSAN fuzz targets (`FuzzFeeSplit`, `FuzzClaimReward`,
`FuzzValidatorScoring`) on every push/PR to `develop` and weekly. This is a substantial
improvement. Still outstanding before mainnet:
- A full ASAN+UBSAN regtest build across the qXRP-modified consensus paths.
- ≥80% line/branch coverage on `src/libxrpl/tx/transactors/qxrp/`.
- Integration of `FuzzFalconVerify` into CI (currently a manual step; needs the full
  library build).

**Recommendation.** Land the full-build sanitizer job and coverage gate; wire the Falcon
verify fuzzer into the scheduled run with a growing corpus.

**Status:** Partially addressed.

#### M-02 — liboqs pinned by commit but not yet content-hash verified

**Location:** `CMakeLists.txt` liboqs `ExternalProject` block.

**Description.** liboqs is pinned to an exact commit
(`f4b96220e4bd208895172acc4fedb5a191d9f5b1`, v0.12.0) with `GIT_SHALLOW` and
`UPDATE_DISCONNECTED` — a strong improvement over a floating tag. Because Falcon is the
root of the chain's quantum resistance, a compromise or tag-move of the upstream repo is
still a supply-chain concern. There is no post-checkout checksum/signature verification of
the produced static library.

**Recommendation.** Add build-time hash verification of the fetched tree or the resulting
`liboqs.a`, and/or consume liboqs via an attested Conan package for reproducible builds.
Record the exact commit in per-release notes and an SBOM.

**Status:** Major improvement; residual hardening recommended.

---

### 5.3 Low

#### L-01 — Only DOUBLE_SIGN slashing currently enforced

`ValidatorSlash` implements DOUBLE_SIGN at 100%; ABSENCE / INVALID_VOTE offenses are
defined but intentionally return `temDISABLED` in the current rollout. This is a conscious
staging choice but means the full slashing deterrent surface is not yet active. Enable the
remaining offenses only alongside robust, non-gameable detection and tests.

#### L-02 — Latency scoring component incomplete

The latency element of the composite score is currently floored/placeholder; documented in
constants. Complete before relying on latency for reward differentiation.

---

### 5.4 Informational / Positive Observations

- **I-01 — Thoughtful crypto abstraction.** Separate `PQPublicKey`/`PQSecretKey` types
  limit blast radius and prevent silent classical fallback.
- **I-02 — Prefix + length discrimination.** `0xFB`/`0xFC` prefixes with exact length
  checks in constructors defend against type confusion and malformed input.
- **I-03 — Wide-integer economic arithmetic.** Slashing and fee splits use `__int128` /
  `muldiv64`; no floating point in economic paths.
- **I-04 — Anti-griefing slashing.** Slashed drops are burned via `destroyXRP`, not paid
  to the slasher — removing the profit motive for malicious slashing.
- **I-05 — Consistent amendment gating.** `featureProofOfParticipation` gates all eight
  qXRP transactors plus the reward-epoch and governance hooks.
- **I-06 — Operational hygiene.** The multi-stage `docker/Dockerfile` runs as a non-root
  `xrpld` user (uid 1001) and keeps build tooling out of the runtime image.

---

## 6. Dynamic Runtime Verification (Published Docker Image)

To move beyond static review, the auditor pulled and executed the image published to
Docker Hub on the audit date.

### 6.1 Image identity

- **Tags:** `qxrp/xrpld:falcon` and `qxrp/xrpld:latest` — both resolve to the **same
  digest** `sha256:5cf92545c0f05af0b67c7cb5bdfddb70fb1f90fc2816b84102650baf0708af66`.
- **Built:** 2026-07-01 (`amd64/linux`, ~175 MB).
- **Runtime layout matches `docker/Dockerfile`:** non-root `xrpld` user, entrypoint
  `/usr/local/bin/xrpld --conf /cfg/xrpld.cfg`, exposes `5005 / 51235 / 8080`.

### 6.2 Binary provenance

```
xrpld version 3.2.0-b0
branch: scoring-fix-build
commit: 10d66cc2c7506048b530556ae674c34b769d011a
```

- liboqs Falcon-512/1024 symbols (`OQS_SIG_falcon_*`, `PQCLEAN_FALCON*`) are linked in.
- On boot the node logs: `Node identity: Falcon-512 post-quantum key loaded`.
- **Note:** the image was compiled from `scoring-fix-build @ 10d66cc2`, which is ahead of
  the branch reviewed statically here. The deployed binary and the source branch should be
  reconciled so the running image corresponds to reviewed, tagged source.

### 6.3 Live post-quantum behaviour (standalone node)

1. **Falcon account creation** — `wallet_propose {"key_type":"falcon512"}` returned an
   `account_id`, an **898-byte** `public_key_hex` (Falcon-512 public key, not a 33-byte
   classical key), and a `falcon_secret` carrying the explicit non-recoverable-secret
   warning.
2. **Falcon-signed transaction** — after funding the new account from genesis, an
   `AccountSet` submitted **from** that account using `falcon_secret` returned
   `tesSUCCESS` and landed in-ledger. The applied transaction carried an **898-byte
   `SigningPubKey` matching the account's Falcon key** and a **657-byte `TxnSignature`**
   (consistent with Falcon-512; ed25519 would be 64 bytes and secp256k1 ≈ 70 bytes).
3. **No silent classical fallback** — the server rejects a seed-based `secret` for Falcon
   key types ("Falcon key types require 'falcon_secret'") and rejects a malformed secret
   (`badSeed` / "Invalid falcon_secret").

**Conclusion of dynamic testing.** The published image genuinely creates Falcon accounts
and produces Falcon-signed transactions end-to-end. Transaction-level quantum resistance
is confirmed in practice. Consensus-level quantum resistance remains gated on **H-01**.

---

## 7. Maturity Assessment

| Category | Status | Notes |
|---|---|---|
| PQ key model & type safety | Strong | Dedicated PQ types, prefix + length checks |
| Transaction signing (Falcon) | Strong | Verified at runtime end-to-end |
| Validation message signing | Good | `STValidation` branches correctly on key type |
| Consensus proposal verification | **Incomplete** | secp256k1-only; must add Falcon branch (H-01) |
| Supply chain (liboqs) | Good | Pinned commit; add content-hash verification |
| Key zeroization | Strong | `secureErase` / `OPENSSL_cleanse` |
| Economic integer safety | Good | `__int128` / `muldiv64`, no floats |
| Amendment gating | Excellent | Comprehensive and consistent |
| Pre-production assurance | Improving | Security CI live; full ASAN + coverage pending |

---

## 8. Prioritized Remediation Roadmap

**Phase 1 — before any pure-Falcon consensus testnet**
1. Fix **H-01**: make `RCLCxPeerPos::checkSign` Falcon-aware (mirror
   `STValidation::isValid`), with a consensus-level Falcon proposal sign/verify test.
2. Reconcile the deployed image commit (`scoring-fix-build @ 10d66cc2`) with a reviewed,
   tagged source branch; document the exact liboqs commit per release.

**Phase 2 — before mainnet consideration**
3. Land the full ASAN+UBSAN regtest build and an ≥80% coverage gate on `qxrp/`
   transactors (M-01).
4. Add build-time content-hash verification for liboqs and/or an attested package (M-02).
5. Integrate `FuzzFalconVerify` into CI with a growing corpus.

**Phase 3 — mainnet hardening**
6. Third-party cryptographic review of the Falcon usage patterns and validator key
   lifecycle.
7. Enable additional slashing offenses alongside robust detection (L-01) and complete
   latency scoring (L-02).
8. Publish a reproducible-build pipeline + SBOM and an operator key-protection guide.

---

## 9. Limitations and Disclaimer

This review was time-boxed and combines static source analysis with dynamic verification
of the published Docker image on the audit date. It does not constitute a guarantee of the
absence of all vulnerabilities. No adversarial multi-node consensus testing under attack
conditions was performed, and the Falcon primitive itself is trusted to the liboqs / NIST
standardization process. Upstream XRPL components retain their existing risk profile.

The auditor recommends a focused follow-up review after **H-01** is resolved, together with
a dedicated cryptographic implementation review of the Falcon consensus and signing paths.

---

## Appendix A — Key Files Reviewed

- `src/libxrpl/protocol/{falcon.cpp, falcon.h, PQPublicKey.*, PQSecretKey.*, PublicKey.cpp, STTx.cpp, STValidation.cpp}`
- `src/xrpld/app/consensus/{RCLConsensus.cpp, RCLCxPeerPos.cpp}`
- `src/xrpld/rpc/handlers/admin/keygen/WalletPropose.cpp`
- `src/libxrpl/tx/transactors/qxrp/` (all eight transactors)
- `src/libxrpl/tx/{RewardEpoch.cpp, GovernanceTally.cpp}`
- `include/xrpl/protocol/{PQPublicKey.h, QXRPConstants.h}`
- `CMakeLists.txt` (liboqs `ExternalProject`), `conanfile.py`
- `docker/Dockerfile`, `.github/workflows/qxrp-security.yml`
- Published image `qxrp/xrpld@sha256:5cf92545…0708af66`

---

**End of Report**

*Prepared as an independent third-party assessment for the FalconLedger (qXRP) project team.*
