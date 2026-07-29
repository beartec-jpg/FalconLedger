# BitVM-class vault tools (regtest)

## Prerequisites

- `bitcoind` / `bitcoin-cli` regtest
- Python 3.10+
- Optional: Falcon RPC for full e2e

## Quick vault test (Bitcoin only)

```bash
# Start regtest
bitcoind -regtest -daemon -fallbackfee=0.0001

python3 e2e_regtest.py --bitcoin-only
```

This:

1. Creates a vault address (CSV + hashlock)
2. Funds it
3. Mines CSV blocks
4. Claims with preimage → payout

## Full stack (Falcon + Bitcoin)

1. Run Falcon with `BitcoinSPVBridge` + network 1101  
2. Activate bridge + SPV mint path  
3. `python3 e2e_regtest.py --full --falcon-url http://127.0.0.1:5115`

## Script model

See `vault.py` — P2WSH:

```
IF
  # reserved challenge branch (fraud preimage) — prototype stub
  ...
ELSE
  <CSV> CHECKSEQUENCEVERIFY DROP
  SHA256 <burnCommit> EQUALVERIFY
  <userPubkey> CHECKSIG
ENDIF
```
