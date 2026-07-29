# Bitcoin SPV Bridge + BitVM-class peg (Falcon)

**Status (2026-07-29):** Isolated e2e **PASS** (mint + peg-out). Amendment `BitcoinSPVBridge` is **DefaultNo** — ships dark until explicitly voted. **Not activated on public testnet 1001.**

## What this is

| Direction | Mechanism | Intermediate custodian? |
|-----------|-----------|-------------------------|
| **BTC → FBTC** | On-ledger SPV: headers + Merkle proof + OP_RETURN `FALC‖AccountID` | **No** |
| **FBTC → BTC** | Burn → challenge window → finalize → BitVM-class vault claim (CSV + hashlock) | **No company multisig** (prototype vault, not full BitVM dispute) |

Falcon verifies Bitcoin data under consensus rules. This is the non-custodial path; EVM F-assets still use lock–mint relays.

## Documents

| Doc | Purpose |
|-----|---------|
| [SPV-BRIDGE-PROOF-REPORT.md](./SPV-BRIDGE-PROOF-REPORT.md) | End-to-end proof: what we ran, decentralization claims (honest) |
| [BITCOIN_SPV_LIGHT_CLIENT_DESIGN.md](./BITCOIN_SPV_LIGHT_CLIENT_DESIGN.md) | Full design (headers, deposits, constants, tx codes) |
| [BITVM_PEG.md](./BITVM_PEG.md) | Peg-out / vault model |
| [ISOLATED_TESTNET.md](./ISOLATED_TESTNET.md) | Network **1101** research net (force-enable via `[features]`) |
| [TESTNET_AMENDMENT_ROLLOUT.md](./TESTNET_AMENDMENT_ROLLOUT.md) | **How to get this onto public testnet 1001 without activating yet** |

## Amendment

```
XRPL_FEATURE(BitcoinSPVBridge, Supported::Yes, VoteBehavior::DefaultNo)
```

- **Hash (stable name):** query `feature` RPC after deploying this binary → name `BitcoinSPVBridge`
- **Tx types:** 98 Activate, 99 HeaderSubmit, 110 DepositClaim, 111 BridgeBurn, 112 WithdrawFinalize
- **Gated:** all return `temDISABLED` until the amendment is enabled on the ledger

## Isolated proof (regtest + 1101)

```bash
# Bitcoin Core regtest (Docker)
./scripts/btc-spv/start-bitcoin-regtest.sh

# Falcon standalone network_id 1101
./scripts/btc-spv/start-isolated-falcon.sh   # needs built .build/xrpld

export PATH="$PWD/data/btc-spv-1101/bin:$PATH"
# Suite (mint path + vault tools)
python3 scripts/btc-spv/run-all-spv-tests.py

# Full two-way peg (clean NuDB recommended)
scripts/btc-spv/.venv/bin/python scripts/btc-spv/e2e_full_peg.py
```

**Proven:** `BTCBridgeActivate` → `BTCHeaderSubmit` → `BTCDepositClaim` → `BTCBridgeBurn` → challenge ledgers → `BTCWithdrawFinalize` → Bitcoin vault claim.

## Testnet / mainnet policy

| Net | `network_id` | Force `[features] BitcoinSPVBridge`? | Status |
|-----|--------------|--------------------------------------|--------|
| Isolated research | **1101** | Yes (cfg only) | E2E green |
| Public Falcon testnet | **1001** | **Never force** — amendment vote only | **Not activated** — rollout doc ready |
| Mainnet | ceremony id | Never until audit + product gate | Blocked |

See [TESTNET_AMENDMENT_ROLLOUT.md](./TESTNET_AMENDMENT_ROLLOUT.md) for deploy-binary → majority-time → vote sequence **when operators choose to enable**.

## Scripts

| Path | Role |
|------|------|
| `scripts/btc-spv/e2e_full_peg.py` | Full mint + peg-out e2e |
| `scripts/btc-spv/run-all-spv-tests.py` | Broader harness |
| `scripts/btc-spv/bitvm/` | Vault build / fund / claim helpers |
| `scripts/enable-btc-spv-fleet.sh` | Fleet helper — **default is dry-run / prepare only** |

## Honest claims

**OK:** Non-custodial SPV mint and BitVM-class peg-out demonstrated on isolated stack.  
**Not OK yet:** Mainnet; full BitVM fraud-proof disputes; production security audit complete.
