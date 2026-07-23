# Mainnet security freeze notes

**Status:** **PROTOCOL FREEZE — mainnet-v2 scorefix 2026-07-22**  
**Freeze commit:** `b007db22d` (all bonded pay ∝ score + PoP Supported::Yes + scoring Blob fix)  
**Image:** `qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5`  
  tags: `mainnet-v2` · `mainnet-v2-scoring-pay` · `mainnet-v2-scorefix` (Hub push 2026-07-22)  
**Smoke:** bond 11/11 · PoP enabled · claim e2e on fast-epoch · soak on scorefix  
**Emission:** long-epoch first unlock at **epoch 8**; scores accrue from genesis for bonded vals  
**Last updated:** 2026-07-22  
**Companion docs:** `MAINNET_GO_LIVE_CHECKLIST.md`, `MAINNET_REHEARSAL.md`, `ops/MAINNET_OPS_RUNBOOK.md`, `scripts/mainnet-ceremony/dry-runs/SOAK_CHECK.md`

This document records the **security freeze** assumptions and remaining ops gates before a public mainnet bootstrap. It addresses audit items H-02 (assurance) and H-03 (ops/ceremony).

---

## 1. Protocol hard-gates (must be in the launch image)

| Gate | Status | Commit / note |
|------|--------|----------------|
| Falcon consensus proposal verify | ✅ | `RCLCxPeerPos::checkSign` Falcon path |
| Classical `node_seed` banned | ✅ | Falcon-only P2P identity |
| ValidatorSlash crypto evidence (DOUBLE_SIGN) | ✅ | Garbage evidence → `tecNO_PERMISSION`; ABSENCE/INVALID_VOTE → `temDISABLED` |
| Claim paths hard-cap to `sfEpochPoolBalance` | ✅ | ClaimReward / ClaimLPReward / ClaimAmmLpReward |
| LP claim denom uses live aggregate floor | ✅ | Prevents post-epoch mint overpay |
| Relative latency scoring (not flat 5000) | ✅ | `ValidatorScoring` earliest-signer baseline + continuous 10 ms steps |
| Fluid composite (EMA; pay ∝ score all bonded) | ✅ | independent signals; 35% new / 65% history EMA; **no ActiveSet K=32 rank-cut** (`mainnet-v2`) |
| ProofOfParticipation Supported::Yes | ✅ | bond/claim/emission gates; genesis `[amendments]` + `[features]` |
| ValidatorScoring ConsensusKey Blob lifetime | ✅ | `getFieldVL` Blob kept for `makeSlice` (scores write) |
| AccountNames (NameSet / Unbond / Release) | ✅ | 100 FALCON bond; 1/account; 1-epoch cooldown |
| Bridge contract multi-sig (N-of-M) | ✅ code · Sepolia e2e | **Redeploy** ETH mainnet lock with `REQUIRED≥2` |

If any of the above is missing from `xrpld --version` git commit, **do not launch**.

---

## 2. Image pin (H-03)

```bash
# Build from freeze commit (scoring pay)
docker build -t qxrp/xrpld:mainnet-v2 -f docker/Dockerfile \
  --label falcon.scoring=all-bonded-pro-rata \
  --label falcon.activeset=disabled \
  --label falcon.names=AccountNames \
  --label org.opencontainers.image.revision=1af01dbfb .
docker push qxrp/xrpld:mainnet-v2
DIGEST=$(docker inspect qxrp/xrpld:mainnet-v2 --format '{{index .RepoDigests 0}}')
echo "$DIGEST" > scripts/mainnet-ceremony/IMAGE_DIGEST.txt
```

**All** validators, full-history nodes, and install one-liners must use:

```bash
export QXRP_XRPLD_IMAGE='qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5'
```

Installer / compose defaults use `qxrp/xrpld:lending-v5` for testnet continuity; **mainnet must override with the digest**.  
Historical `mainnet-v1` remains on Hub but is **not** the T0 pin (ActiveSet pay).

Never roll a mixed fleet (two digests in the UNL set).
---

## 3. Ceremony / ops gates

Scaffold in repo: `scripts/mainnet-ceremony/` + `bash scripts/ops/prepare-mainnet-ceremony.sh`

| Gate | Status | Where |
|------|--------|--------|
| Mainnet **network id** chosen | ✅ **1026** locked in pack | `NETWORK_ID.txt` |
| Freeze commit recorded | ✅ tip at pack authoring | `FREEZE_COMMIT.txt` (refresh before image) |
| DEV multi-sig custody plan | ✅ template | `DEV_CUSTODY.md` |
| Roles matrix | ✅ template | `roles/ROLES.md` |
| Offline genesis-split plan | ✅ scripted | `mainnet-genesis-split.py --offline-plan` |
| Airdrop/faucet SQL schema | ✅ | `docs/sql/airdrop-schema.sql` |
| Mainnet validator cfg template | ✅ | `cfg/mainnet/` |
| DNS / RPC publish list | ✅ template | `DNS_RPC.md` |
| Genesis + AIRDROP / FAUCET / DEV **keys** | ❌ offline human | Falcon `wallet_propose` on freeze image |
| UNL public keys finalized | ❌ offline human | `validators/unl-public.txt` |
| Image built + **digest** on Hub | ✅ `qxrp/xrpld@sha256:9362005f1360…` | `IMAGE_DIGEST.txt` |
| Live `--dry-run` vs real balances | ❌ needs chain | after rehearsal / T0 |
| Private dress rehearsal + wipe | ⚠️ smoke **PASS** · claim e2e **PASS** · soak **PASS_EARLY** (~10 h+) · **wipe still due** before T0 | `MAINNET_REHEARSAL.md` · `REHEARSAL_RESULTS.md` · `dry-runs/SOAK_CHECK.md` |
| Neon provisioned + schema applied | ❌ ops cloud | runbook §1 |
| Portal env `LIVE=false` deployed | ❌ Vercel/host | `portal.env.mainnet` |
| DNS pointed at live RPC | ❌ T0 only | `DNS_RPC.md` |

---

## 4. Security assurance (H-02)

### Automated (CI)

Workflow: `.github/workflows/qxrp-security.yml`

| Check | Frequency |
|-------|-----------|
| cppcheck on qXRP delta | every PR / push to develop |
| FuzzFeeSplit (ASAN+UBSAN, 60s) | every PR / push |
| FuzzValidatorScoring (ASAN+UBSAN, 60s) | every PR / push |
| FuzzClaimReward (ASAN+UBSAN, 60s) | every PR / push |
| FuzzEpochPoolCap (ASAN+UBSAN, 60s) | every PR / push |
| FuzzFalconStandalone shape fuzzer (60s) | every PR / push |
| Weekly full schedule | Monday 02:00 UTC |

### Before freeze tag

Operators should also:

```bash
# Full sanitizer build (when build host available)
cmake -B build-asan -DSANITIZE=address,undefined -Dtests=ON …
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

# Optional: full-lib FuzzFalconVerify with liboqs (see docs/security/security-testing.md)
```

Coverage target ≥80% on `src/libxrpl/tx/transactors/qxrp/` remains a **stretch goal**; critical paths above are covered by unit + fuzz invariants.

### External review

Recommend a third-party review focused on:

1. `ValidatorSlash` evidence verification  
2. Claim / RewardEpoch pool accounting  
3. ValidatorScoring latency + score writeback  
4. Bridge multi-sig + relay key custody  

---

## 5. Explicit non-goals at freeze

- Paying mainnet airdrop for **testnet** activity  
- Floating image tags (`:latest`) on validators  
- Single-EOA ownership of the mainnet USDC lock contract  
- Enabling ABSENCE / INVALID_VOTE slash without detection redesign  

---

## 6. Known residual risks (accept or fix later)

| Risk | Mitigation |
|------|------------|
| Latency score is *relative* (earliest signer baseline), not absolute RTT | Accept for launch; absolute RTT needs peer instrumentation |
| Vote accuracy currently tracks uptime (trusted correct-hash count) | Accept; wrong-hash path needs separate index |
| Full ASAN regtest of entire rippled suite | Run on freeze tag when CI capacity allows |
| Bridge mint still uses Falcon issuer key in relay | Ops: HSM / multi-party mint policy for mainnet |

---

## 7. Path from protocol 7.5 → 8.5+

See **`docs/PROTOCOL_READINESS_7_5_PLAN.md`** (items 1–12): image pin, soak,
adversarial checklist, ASAN, design deferrals, external audit scope.

Score-uplift track (economic security + readiness honesty):  
**`docs/MAINNET_SCORE_UPLIFT_PLAN.md`** · eco docs under `docs/qxrp/`  
(`slash-model.md`, `UNL_CHARTER.md`, `GOVERNANCE_SURFACE.md`, emission tables).

*Update this file when the freeze commit SHA and image digest are final.*