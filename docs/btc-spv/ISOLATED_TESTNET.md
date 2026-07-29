# Bitcoin SPV Isolated Testnet (network_id 1101)

Prototype / research network for the **Bitcoin SPV payment-attestation mint**.  
Does **not** touch live testnet 1001 or custodial ETH relays.

## Hard rules

- Branch: `feature/btc-spv-light-client`
- Enable feature only via **`[features]`** (not `[amendments]`):
  ```
  [features]
  BitcoinSPVBridge
  MPTokensV1
  ```
- `network_id = 1101` (all user txs need `NetworkID: 1101`)
- No pause guardians; covenant/BitVM redeem is later; MVP is mint-only attestation
- Never enable this feature in `cfg/falcon-validator.cfg` used by 1001

## Build

```bash
cd FalconLedger
cmake --build build -j$(nproc) --target xrpld
```

## Activate + smoke (after nodes up)

1. Run bitcoind regtest; generate blocks.
2. `BTCBridgeActivate` with regtest chain id `3`, anchor header, watch script hash, mint cap.
3. `BTCHeaderSubmit` with batches of 80-byte headers (fee auto-scales with count).
4. Pay BTC to watch script + OP_RETURN `FALC || AccountID20`.
5. `BTCDepositClaim` with raw tx + merkle proof (claimer == destination).

## Product label

FBTC is **SPV payment attestation** until BitVM peg-out is used.  
BitVM-class burn/finalize + vault tools: `docs/btc-spv/BITVM_PEG.md`, `scripts/btc-spv/bitvm/`.

## Full e2e (SPV + BitVM)

```
activate → headers → deposit claim (mint)
  → BTCBridgeBurn → wait challenge ledgers → BTCWithdrawFinalize
  → Bitcoin vault claim (CSV + preimage)
```

```bash
python3 scripts/btc-spv/bitvm/e2e_regtest.py --bitcoin-only
python3 scripts/btc-spv/bitvm/e2e_regtest.py --full --falcon-url http://127.0.0.1:5115
```
