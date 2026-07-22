# Rehearsal results — fast-epoch full run (2026-07-21)

**Logged at:** 2026-07-21T21:27Z (snapshot) · continued testing after this doc  
**Host:** `5.78.142.246` (val5), isolated `network_id=1099`  
**RPC:** `http://5.78.142.246:6005` (HTTP JSON-RPC; peer 51235; WS-ish 5005 host map)  

## Images

| Role | Tag | Local id / note |
|------|-----|-----------------|
| **Rehearsal (running)** | `qxrp/xrpld:mainnet-rehearsal-fast-epoch` | `a29be2921e72` (= `…-e12fd7417`) — RewardEpoch LPAllocationBps fix + scoring |
| Source commit (fast-epoch build) | `/root/fast-epoch-SOURCE_COMMIT.txt` | `91f7668d46adb3af426393ac7a543bcef80b9fb1` |
| **Launch pin (names — NEW)** | `qxrp/xrpld:mainnet-v1` | `e5086df99920` · tip **`1789d2fb4`** · labels `falcon.names=AccountNames` · built 2026-07-22T08:39Z on val5 |
| Prior launch pin (pre-names) | `…-2eef57ea1` / `…-e12fd7417` | scoring-final only — **superseded** if names required at T0 |
| Labels (intent) | epoch=172800 default | AccountNames + fluid-activeset-k |

## Live chain snapshot (21:27Z)

| Field | Value |
|-------|--------|
| server_state | `full` |
| seq | **1540** (complete_ledgers ~1028–1540; earlier windows included 516+) |
| peers / proposers | 2 / **2** |
| validation_quorum | 3 |
| build_version | `3.2.0-b0` |

### Amendments (enabled at snapshot)

| Amendment | Enabled |
|-----------|---------|
| AMM | **true** |
| SingleAssetVault | **true** |
| LendingProtocol | **true** |
| LendingPermissionless | **true** |
| LendingCollateral | **true** |
| **MPTokensV1** | **true** (was false during product run ~21:18; vault had been skipped) |
| fixUniversalNumber | **true** |

## Phase results (consolidated)

| Phase | Status | Evidence |
|-------|--------|----------|
| health | **PASS** | 3 containers healthy; full; peers=2; proposers=2; seq advancing past 1500 |
| payments / faucet | **PASS** | Fund steps in full-results; prior smoke pays |
| genesis split | **PASS** | `split_air/fau/dev` all `tesSUCCESS` (hashes below) |
| bond | **PASS** | All 3 `ValidatorRegister` + `ValidatorBond` `tesSUCCESS`; live BondStatus=1 |
| scoring | **PASS** | First scores at ledger **≥512** (not 256): all CompositeScore **9500**; live ~9499–9500 |
| emissions / ClaimReward | **PASS** (session) | Claim path exercised post-epoch with ConsensusKey; ~163M drops observed in operator session (not re-hashed in JSON artifacts) |
| knockdown / recovery | **PASS** (session) | Stop val → score dip **9500→8331** → restart recover **~8500** (session notes; soak JSON captures pre-knock 9500 baseline only) |
| DefaultRipple (IOU) | **PASS** | Product step `issuer_DefaultRipple` `tesSUCCESS` |
| AMM create + deposit | **PASS** | `AMMCreate` + `AMMDeposit` `tesSUCCESS`; `has_amm=true` |
| vault / lend | **PASS** | Full path: AMM (native/IOU) → VaultCreate → VaultDeposit → LoanBrokerSet → **LoanSet** → **LoanPay** (see vault section) |
| bridge (legacy 1-of-1) | **PASS** (prior) | Sepolia lock `0x2dae…b75C` · 38 mints / 9 releases on host `46.224.0.140` |
| bridge (multi-sig 2-of-3) | **PASS** (2026-07-22) | Mainnet-parity Sepolia deploy + deposit→mint + dual-confirm release (see Bridge section) |
| teardown | **PENDING** | Stack still up; wipe before real T0 |

## Key tx hashes (from val5 JSON)

### Split + bond (`rehearsal-full-results.json`)

| Step | Result | Hash (prefix) |
|------|--------|----------------|
| split_air | tesSUCCESS | `F4BA58A0BEFD37E383…` |
| split_fau | tesSUCCESS | `BF6B08133B40373910…` |
| split_dev | tesSUCCESS | `15F98819414AE54817…` |
| fund_rP2pWy / r9PM7Z / r9XAn4 | tesSUCCESS | `3E7C2351…` / `64E49B43…` / `CD933DA6…` |
| reg+bond VAL1 rP2pWy | tesSUCCESS | reg `C907C753…` bond `8635EA2B…` |
| reg+bond VAL2 r9PM7Z | tesSUCCESS | reg `E685DDBD…` bond `8E5C8956…` |
| reg+bond VAL3 r9XAn4 | tesSUCCESS | reg `A1B911A7…` bond `77F261D8…` |

### Product AMM path (`rehearsal-product-results.json`, ~21:18Z)

| Step | Result | Hash (prefix) |
|------|--------|----------------|
| fund_iss / lp / tr | tesSUCCESS | `71B664DF…` / `6CD23549…` / `86D2D95C…` |
| issuer_DefaultRipple | tesSUCCESS | `7C4E6D24…` |
| trust + mint lp/tr | tesSUCCESS | … |
| **AMMCreate** | tesSUCCESS | `BF567CFFBA94F5BF805E1E8ADFB24457CAD5BEC19A7E3F5FAEEAC7E89D93AE70` |
| **AMMDeposit** | tesSUCCESS | `193C49E995DA72C4C79BD6AC7AC237BAD4F0396EE25C48B414932FAE5B17D57E` |
| vault | **skipped** | note: `MPTokensV1 not enabled in time` |

### Scores (`rehearsal-soak2-results.json`)

| Tag | Seq | CompositeScore (all 3 vals) |
|-----|-----|----------------------------|
| start → ≥257 | 34–257 | null (no write yet; skip-list needs ≥256 ancestors) |
| **≥512** | **514** | **9500** each (Uptime/Vote/Latency/Consistency 10000) |
| done | 521 | 9500 |

### Live bonds (21:27Z RPC)

| Account | BondStatus | CompositeScore | LatencyScoreBps |
|---------|------------|----------------|-----------------|
| rP2pWy…N4DhmS | 1 | 9499 | 9998 |
| r9PM7Z…ZTnPKu | 1 | 9500 | 10000 |
| r9XAn4…xti19X | 1 | 9499 | 9998 |

### Vault + lend (`rehearsal-vault-lend-results.json`, ~01:55Z 2026-07-22)

After **MPTokensV1** enabled (was blocking vault earlier):

| Step | Result | Hash (prefix) |
|------|--------|----------------|
| fund / DefaultRipple / trust / mint | tesSUCCESS | … |
| **AMMCreate** (native + USD) | tesSUCCESS | `98A5796FA991EBBF…` |
| **VaultCreate** | tesSUCCESS | `5B894B8620B89491…` |
| **VaultDeposit** | tesSUCCESS | `73BD88F1FBE70B5A…` |
| **LoanBrokerSet** | tesSUCCESS | `339FD6EF7DF95AF2…` |
| **LoanSet** | tesSUCCESS | `6A997A010778A062…` |
| **LoanPay** | tesSUCCESS | `233761404EAA9D97…` |

- **VaultID:** `BEA3523BCD2E7F857FC0FFA81B09333E1AC39EDBE3DA6D527EECA625D3E0D3F9`
- **LoanBrokerID:** `B83F05579691264C00B75864FE429E0ADA5CD5A89E494A380A99A84ACFF6CC81`
- **LoanID:** `BFA4EE0477E5ECF86C957E896605435AE204A6EF1FF2BC3D8F4874E4A1FB051F`
- **AMM price:** 1.0 USD/FALCON · principal 100 · collateral 165 FALCON (150% HF + buffer)

#### LoanSet `tecNO_LINE` root cause (fixed in harness)

Permissionless borrow calls `Lending::checkPermissionlessCollateral`, which needs an **AMM** of native ↔ vault asset to price FALCON collateral. If the pool is missing, the helper returns **`tecNO_LINE`** with log *“AMM price unavailable for collateral check”* (not a missing trust line).

Fix: create AMM for this issuer before LoanSet; size collateral so health factor ≥ **15000 bps** (`kPermissionlessMinCollateralBps`).

### Bridge — multi-sig mainnet-parity (2026-07-22)

**Host:** `46.224.0.140` (bridge/full-history; not val5)  
**Chain:** Sepolia `11155111` · Falcon testnet `network_id=1001`  
**Goal:** Prove lock + custody **as mainnet will run** (`REQUIRED≥2`), not the legacy single-EOA path.  
**Full write-up:** `docs/MAINNET_BRIDGE.md` · config `config/usdc-bridge.json`  
**Host artifacts:** `/var/lib/qxrp-bridge/mainnet-parity/` (`deploy.public.json`, `test_deposit.json`, `test_multisig_withdraw.json`, `owners.addresses.json`)

#### Locks

| Role | Address | Custody |
|------|---------|---------|
| **Legacy (still live for older portal path)** | `0x2dae31Cbf2E3a418d617081985661fCD0117b75C` | 1-of-1 EOA on host — **testnet only** |
| **Mainnet-parity multi-sig** | `0x8A300bC6726C633ae350F58380194Ce3008CE295` | **2-of-3** owners |

#### Multi-sig deploy

| Field | Value |
|-------|--------|
| Deploy tx | `0x5b4c4373f8c6c31711f219a2ce84304e7e849175550ddca23311463964264de6` |
| Deploy block | `11325466` |
| Deployed at | `2026-07-22T08:19:26Z` |
| USDC (Sepolia) | `0x1c7D4B196Cb0C7B01d743Fbc6116a902379C7238` |
| `required` (on-chain) | **2** |
| `ownerCount` | **3** |
| Deployer | `0x04E65Bc6e63df2813737caBbaD91C7b16fa7c325` |
| Owner 1 | `0x7b25BC68eE9CC145dA7AfcA1D852bA616195aa15` |
| Owner 2 | `0x92474d6320204D098759dB3998e42ec4904a5B55` |
| Owner 3 | `0x5B707F798aA8834Fe810C5D32FE82856Dd4c8439` |
| Contract | `contracts/FalconCollateralLock.sol` (multi-sig; `confirmWithdraw` / `confirmRelease` public) |

#### E2E steps

| Step | Status | Evidence |
|------|--------|----------|
| Deploy 2-of-3 lock | **PASS** | deploy tx above; `isOwner` true for all three |
| Deposit 5 USDC | **PASS** | tx `0xd55ffe77828f347ac9418aaae2815a47e1572f33c4cc7f2f98e4188d2535d752` · depositId `0x2e762c7a87d902ef…` · block `11325474` |
| Mint 5 QUC on Falcon | **PASS** | dest `rMpmiVGjTVqHKC97FoD7gNBpicH97HSxGZ` · Falcon tx `170298BDFCE33622CDD6B16C8CE8F854B349AA4C1A18B909C2E28935C94861C7` · issuer `rPh77fAAmvbVuMQQP9H9JKtyTFuhRjp3Fk` |
| Owner1 alone `confirmWithdraw` | **PASS (blocked)** | tx `0x0a189253d7327c7e56ecccfd804fe2b77e3f4d30874a69ad376c6fba430bef13` · confirmations=1 · `processed=false` · no USDC movement |
| Owner2 `confirmWithdraw` (threshold) | **PASS** | tx `0x759b7ce175a29ec550062da269bd15e57e2d97ac8882004d794f266d8bd4a8bb` · confirmations=2 · **+3 USDC** to recipient · lock 5→2 USDC |

#### Withdraw op (multi-sig)

| Field | Value |
|-------|--------|
| withdrawalId | `0x895634c580637339857769a01f2bd0297a84bc50d3fd1a722cc9d26c0a92f539` |
| opHash | `0x60a592d8321594780c38154a0e9e7f5fcf0fa368e3380770421187d455ea086a` |
| Amount released | **3 USDC** |
| Recipient | `0x64BA18002B6E72fE443f3F8a146cE529250Db107` (deposit user) |

#### What this proves for mainnet

- N-of-M constructor + on-chain `required=2`  
- Deposit event → deposit-relay mint still works on a **new** lock  
- **Single owner cannot release** collateral  
- Threshold confirm moves USDC exactly  

#### Explicitly still open for real ETH mainnet

- Deploy **new** lock on chain id **1** with Circle USDC `0xA0b8…eB48`  
- **New** cold multi-sig owners (do **not** reuse Sepolia test keys / host `owners.json`)  
- Owner keys **not** on the relay host  
- Falcon mainnet QUC issuer + RPC  
- Optional: full QUC burn + memo → withdraw-relay automation under multi-sig (confirm API already proven)

#### Legacy 1-of-1 baseline (unchanged)

| Metric | Value |
|--------|--------|
| Service | `qxrp-bridge-relay.service` active on `46.224.0.140` |
| Mints / releases | 38 / 9 (through 2026-07-21) |
| Custody | single EOA + key on host — **not** mainnet-shaped |

## Bugs fixed this rehearsal

1. **`LPAllocationBps` FieldErr** at epoch boundary when value is 0  
   - Crash at ledger 256 on early fast-epoch image  
   - Fix: only set field when non-zero (`RewardEpoch.cpp`)  
   - **Verified:** stack past seq 1500+ with epoch=256; no abort  

2. **STValidation journal** — use `toStyledString` (compile/log path)  

3. **AMMCreate `terNO_RIPPLE`** — issuer must `AccountSet` SetFlag **DefaultRipple (8)** before pool create  

4. **VaultCreate `temDISABLED`** — needs **MPTokensV1** (not only SingleAssetVault)  

## Artifact index (durable)

On val5 `/root/`:

| File | Purpose |
|------|---------|
| `rehearsal-consolidated-2026-07-21.json` | **Snapshot** summary + server + bonds + embedded step JSONs |
| `rehearsal-product-results.json` | AMM product steps (overwritable by re-runs) |
| `rehearsal-vault-lend-results.json` | VaultCreate / Deposit / Broker / LoanSet attempt |
| `rehearsal-full-results.json` | Split/bond + early amendment wait |
| `rehearsal-soak2-results.json` | Score timeline to 512 |
| `rehearsal-*-e2e.log` / soak logs | Text trails |

**Repo copy:** `scripts/mainnet-ceremony/dry-runs/artifacts/` (same filenames)  
**This report:** `scripts/mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md`

> Note: individual `*-results.json` files are **overwritten** by later harness runs. Prefer the **consolidated** snapshot + this markdown for the permanent record.

## Accounts (rehearsal throwaway)

| Role | Address |
|------|---------|
| GENESIS | rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh |
| AIRDROP | rHUqG3ASJkPpw4Tv7smSBmhhdsHsUWDSht |
| FAUCET | rGoSzgiL47qQaNoaGxC4udCB4xujYoEBA7 |
| DEV | rGHMqJMTQ33pQ833WD9zGYnHMHJqQCpBKh |
| VAL1–3 | rP2pWy… / r9PM7Z… / r9XAn4… |

## Sign-off

- [x] Fast-epoch past first epochs without FieldErr crash  
- [x] Bond + fluid scores at 512+  
- [x] DefaultRipple + AMM create/deposit  
- [x] Product amendments (incl. MPTokensV1) enabled on chain  
- [x] VaultCreate → VaultDeposit → LoanBrokerSet  
- [x] LoanSet / LoanPay full path (permissionless + AMM HF)  
- [x] Bridge multi-sig 2-of-3 Sepolia e2e (deposit→mint, 1-owner block, 2-owner release)  
- [ ] Ready for real T0 — **NO** until mainnet-v1 long-epoch pin signed off + wipe throwaway secrets  
- [ ] Chain wiped; throwaway secrets destroyed  
- [ ] ETH mainnet multi-sig lock deploy (new owners; Circle USDC) — after T0 bridge go-live plan



## Adversarial / attack-surface smoke (2026-07-22 ~03:36Z)

Source checklist: `docs/ops/ADVERSARIAL_PROTOCOL_CHECKLIST.md`  
Artifact: `dry-runs/artifacts/rehearsal-adversarial-results.json` (+ val5 `/root/rehearsal-adversarial-results.json`)

| ID | Test | Status | Result |
|----|------|--------|--------|
| D2 | wallet_propose Falcon | **PASS** | falcon_secret len=4358 |
| D3 | Falcon Payment | **PASS** | tesSUCCESS |
| N1 | Wrong NetworkID (1026 on 1099) | **PASS** | `telWRONG_NETWORK` |
| N2 | Missing NetworkID | **PASS** | sign auto-injects NetworkID=1099 |
| A1 | Garbage slash evidence | **PASS** | `temMALFORMED` |
| A2 | Slash ABSENCE (offense 2) | **PASS** | `temDISABLED` |
| A3 | Slash INVALID_VOTE (offense 3) | **PASS** | `temDISABLED` |
| A4 | Valid Falcon double-sign slash | **PASS** | tesSUCCESS hash `652830D3…`; BondStatus 1→2 UNBONDING; bond burned 1000→0; image e2615b362 |
| A5 | Replay same slash | **DEFERRED** | depends on A4 |
| A6 | Slash wrong target | **DEFERRED** | depends on A4 |
| B5 | ClaimReward no bond | **PASS** | requires ConsensusKey / not tesSUCCESS |
| B4 | Duplicate ClaimReward | **PARTIAL** | need val secret; single claim done in session |
| B1–B2 | Epoch pool cap / exhaust | **CI** | FuzzClaimReward + FuzzEpochPoolCap |
| C1–C2 | Validator knockdown / rejoin | **SESSION** | prior: continue + score recovery |
| R1 | Malformed RPC account | **PASS** | `actMalformed` |
| R2 | Public RPC `sign` | **PASS** | `notSupported` |
| L1 | Chain live after negatives | **PASS** | full, proposers=2 |

**Summary:** 11 PASS · 0 FAIL · 3 DEFERRED (double-sign harness) · remainder CI/SESSION/PARTIAL.

Also available offline (not re-run this pass): ASAN freeze build, full destructive suite `tools/destructive_testing/`, CI fuzz in `qxrp-security.yml`.



### A4 double-sign slash (2026-07-22)

**Harness:** `tools/make_double_sign_evidence.cpp` (local link against `.build` + liboqs) produces two Falcon `STValidation` blobs (same key, same `LedgerSequence`, different `LedgerHash`).

| Check | Result |
|-------|--------|
| Evidence crypto (preclaim) | **PASS** — engine reached doApply (`tecINVARIANT_FAILED`, not `tecNO_PERMISSION`) |
| On-ledger slash apply | **FAIL** — `tecINVARIANT_FAILED` hash `8DED82D2A491EF8E…` |
| Bond after | still BondStatus=1, BondedAmount=1000000000 (tx not applied) |

**Root cause found (real bug):**

1. `ValidatorSlash::doApply` called `ctx_.destroyXRP(slashed)` which is the **fee-split** path (~65% burn + treasury credit), not a pure bond burn.
2. Even with pure burn, `XRPNotCreated` / `QXRPDropConservation` require `net_drop_change == fee` only, so intentional bond burns always fail once `ltVALIDATOR_BOND` is tracked.

**Fix applied in tree (needs image rebuild to re-prove on-chain):**

- `ValidatorSlash.cpp`: `view().rawDestroyXRP(slashed)` instead of fee-split `destroyXRP`
- `InvariantCheck.cpp` + `QXRPDropConservation.cpp`: for `ttVALIDATOR_SLASH`, allow `-drops_ >= fee` (fee + bond burn)

**A5/A6:** still blocked until A4 applies cleanly after rebuild.

## Next

1. Optional: re-verify ClaimReward + knockdown with hashes appended here  
2. Sign off mainnet-v1 long-epoch pin (AccountNames tip building; pin digest when green)  
3. Wipe rehearsal stack before real T0  
4. Real ETH bridge: new 2-of-N lock + cold owners (reuse ceremony multi-sig runbook, not Sepolia keys)

