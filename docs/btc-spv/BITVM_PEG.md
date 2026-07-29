# BitVM-class peg (e2e testable prototype)

## Light client vs BitVM (again)

| Piece | Role |
|-------|------|
| **SPV light client** | Prove BTC deposit on Falcon → mint FBTC |
| **BitVM-class peg-out** | Burn FBTC on Falcon → release BTC from vault **without custodian keys** |

Both are required for full e2e bridge tests.

## What this prototype implements

Not production BitVM SNARK trees. It implements a **BitVM-class** testable shape:

1. **Challenge window on Falcon** after burn (anyone could later post fraud evidence; prototype: time-based).
2. **Bitcoin vault script** with:
   - CSV timeout (relative locktime)
   - Hashlock on `sfBtcBurnCommit = SHA256(preimage)`  
   - After timeout + preimage, user claims BTC to payout script
3. **No company/validator multisig custody** of the vault keys for the happy path after finalize.

Full BitVM (dispute program / fraud proofs on Bitcoin) is the evolution path; the vault script and Falcon withdraw objects are the integration surface.

## E2E flow

```
BTC → vault UTXO (regtest)
  → SPV BTCDepositClaim → mint FBTC
  → BTCBridgeBurn (burn FBTC, open withdraw PENDING)
  → wait kBTC_CHALLENGE_LEDGERS_DEFAULT Falcon ledgers
  → BTCWithdrawFinalize → FINAL
  → wait CSV blocks on Bitcoin
  → reveal preimage + spend vault → user receives BTC
```

## Falcon txs

| Code | Name | Purpose |
|------|------|---------|
| 111 | `BTCBridgeBurn` | Burn FBTC; create `ltBTC_WITHDRAWAL` |
| 112 | `BTCWithdrawFinalize` | After challenge end ledger → status FINAL |

## Tools

```
scripts/btc-spv/bitvm/
  vault.py          # vault script, fund, claim
  e2e_regtest.py    # end-to-end harness (bitcoind regtest)
  README.md
```

## Run e2e (server)

```bash
# terminal A: bitcoind -regtest
# terminal B: Falcon isolated node network_id 1101 (optional for full stack)

cd scripts/btc-spv/bitvm
python3 e2e_regtest.py --bitcoin-only   # vault + CSV claim only
python3 e2e_regtest.py --full           # needs Falcon RPC too
```
