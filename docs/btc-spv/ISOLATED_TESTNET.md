# Bitcoin SPV Isolated Testnet (network_id 1101)

Prototype / research network for the **Bitcoin SPV payment-attestation mint** and BitVM-class peg-out.  
Does **not** touch live testnet **1001** or mainnet.

## Hard rules

- Branch: `feature/btc-spv-light-client` (or merged equivalent)
- Enable feature only via **`[features]`** (not `[amendments]`) on this isolated cfg:
  ```
  [features]
  BitcoinSPVBridge
  MPTokensV1
  ProofOfParticipation
  ```
- `network_id = 1101` (user txs need `NetworkID: 1101`)
- Sample cfg: `cfg/btc-spv-isolated.cfg`
- Never copy `[features] BitcoinSPVBridge` into 1001 validator configs

## Build

```bash
cd qXRP
cmake --build .build -j1 --target xrpld
```

## Stack

```bash
./scripts/btc-spv/start-bitcoin-regtest.sh   # Docker bitcoind regtest
./scripts/btc-spv/start-isolated-falcon.sh   # standalone xrpld :5115
```

## E2E

```bash
export PATH="$PWD/data/btc-spv-1101/bin:$PATH"
# Full two-way peg (clean NuDB recommended)
scripts/btc-spv/.venv/bin/python scripts/btc-spv/e2e_full_peg.py
# Broader suite
python3 scripts/btc-spv/run-all-spv-tests.py
```

Flow:

```
activate → headers → deposit claim (mint FBTC)
  → BTCBridgeBurn → wait challenge ledgers → BTCWithdrawFinalize
  → Bitcoin vault claim (CSV + preimage)
```

## Product label

FBTC here is **SPV payment attestation** + BitVM-class redeem prototype.  
Public testnet enablement: [TESTNET_AMENDMENT_ROLLOUT.md](./TESTNET_AMENDMENT_ROLLOUT.md).
