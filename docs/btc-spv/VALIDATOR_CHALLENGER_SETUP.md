# Validator + BitVM challenger (one-liner)

## One command

```bash
# Defaults to qxrp/xrpld:btc-spv-v6 (live Falcon testnet 1001 SPV bridge fleet)
curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \
  --payout rYourWalletAddress \
  --node-name my-validator
```

## What gets created automatically

| Artifact | Purpose |
|----------|---------|
| Falcon validator keys + r-address | Consensus + bond |
| BTC fee wallet (`btc-challenger-wallet.json`) | Challenge **fees only** |
| `xrpld` container | Falcon node |
| `bitvm-challenger` container | Watches Falcon/bridge; ready to dispute |
| `FUNDING.txt` | The only two steps you must do |

## Your only manual steps

1. **Send ≥ 1,100 FALCON** to the printed **Falcon r-address** (bond).  
2. **Send ~0.001 BTC** (testnet default) to the printed **BTC fee address** (challenger fees).

That’s it. Node + challenger already start with the one-liner.

## Costs

| Item | Amount |
|------|--------|
| Falcon bond | 1,000 FALCON (+ ~100 reserve/fees) |
| BTC challenger float | ~0.001 tBTC testnet; ~0.001–0.005 BTC mainnet buffer |
| Idle | ≈ 0 BTC spend if no disputes |

## Security

- Challenger address is **not** the shared BTC reserve.  
- Do **not** put reserve/custody keys on validators.  
- Fee wallet private key lives in `~/.qxrp/<node>/config/btc-challenger-wallet.json` (mode 600).
