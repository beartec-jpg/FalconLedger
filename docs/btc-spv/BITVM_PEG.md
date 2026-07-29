# BitVM-class peg (e2e testable prototype)

## Light client vs BitVM

| Piece | Role | E2E status (1101 + regtest) |
|-------|------|------------------------------|
| **SPV light client** | Prove BTC deposit on Falcon → mint FBTC | **PASS** |
| **BitVM-class peg-out** | Burn FBTC → release BTC from vault without custodian keys | **PASS** (`e2e_full_peg.py`) |

Full BitVM (on-Bitcoin dispute programs / SNARK fraud proofs) is the evolution path; this prototype is the integration surface.

## Prototype shape

1. **Challenge window on Falcon** after burn (`kBTC_CHALLENGE_LEDGERS_DEFAULT` = 32).  
2. **Bitcoin vault script** (P2WSH): CSV relative locktime + SHA256(preimage) hashlock + user CHECKSIG.  
3. **No company/validator multisig** on the happy-path vault claim after finalize.

## E2E flow (proven)

```
BTC → vault UTXO (regtest) + OP_RETURN FALC‖AccountID
  → SPV BTCDepositClaim → mint FBTC
  → BTCBridgeBurn (same preimage as vault commit)
  → wait challenge ledgers
  → BTCWithdrawFinalize → FINAL
  → wait CSV blocks on Bitcoin
  → claim vault with preimage + user sig → user BTC
```

## Falcon txs

| Code | Name | Purpose |
|------|------|---------|
| 111 | `BTCBridgeBurn` | Burn FBTC; create `ltBTC_WITHDRAWAL` PENDING |
| 112 | `BTCWithdrawFinalize` | After challenge end ledger → FINAL |

## Tools

```
scripts/btc-spv/bitvm/          # vault script, fund helpers
scripts/btc-spv/e2e_full_peg.py # full two-way peg
```

```bash
scripts/btc-spv/.venv/bin/python scripts/btc-spv/e2e_full_peg.py
```
