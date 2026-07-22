# Mainnet ETH → Falcon USDC bridge

Status: **Sepolia multi-sig (mainnet-parity) PASS** · Ethereum mainnet not deployed  
Audit + e2e: 2026-07-22

## Live testnet (reference)

| Item | Value |
|------|--------|
| Host | `46.224.0.140` (`qxrp-bridge-relay.service` **active**) |
| EVM chain | Sepolia (`11155111`) |
| Lock | `0x2dae31Cbf2E3a418d617081985661fCD0117b75C` |
| USDC (test) | `0x1c7D4B196Cb0C7B01d743Fbc6116a902379C7238` |
| Falcon | network_id **1001**, QUC issuer `rfftKWuA7Dk7PF1YrH8NA7262oY3tejhqt` |
| Proven | **38** mints · **9** releases (last mint 2026-07-21, 20 USDC) |
| Custody | **single EOA** owner on host (`sepolia-owner.json` includes private key) — testnet only |

Contract: `contracts/FalconCollateralLock.sol` (N-of-M multi-sig ready).  
Deploy helper: `scripts/deploy-falcon-lock.js` (`OWNERS=…`, `REQUIRED=…`).

## Mainnet gaps (must close before T0 bridge)

1. **No Ethereum mainnet lock** — `config/usdc-bridge.json` → `ethereum_mainnet.lock_contract = null`.
2. **Mainnet USDC** is Circle `0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48` (decimals 6) — do not reuse Sepolia token.
3. **Custody** — deploy with `OWNERS` multi-sig and **`REQUIRED >= 2`**. Never store a mainnet owner private key on the relay host.
4. **Relays are Sepolia-shaped** — `bridge-deposit-relay.py` / `bridge-withdraw-relay.py` use `SEPOLIA_*` env and memo type `sepolia-withdraw`. Need chain-agnostic `ETH_RPC_URL` + `LOCK_CONTRACT` + memo type (or chain id).
5. **Falcon side** — new mainnet `network_id`, new QUC issuer from `deploy-mainnet-stables.sh`, new RPC endpoints (not 1001 / testnet issuer).
6. **Mint policy** — testnet single-issuer mint is not mainnet-grade; plan cold/HSM or multi-party attestation.
7. **Operational** — relay occasionally errors when `qxrp-full` container is down; mainnet needs health checks + alerting.

## Deploy sequence (mainnet)

```bash
# 1) Deploy lock (multi-sig)
cd scripts
RPC_URL=https://ethereum-rpc.publicnode.com \
USDC_TOKEN=0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48 \
OWNERS=0xOwner1,0xOwner2,0xOwner3 \
REQUIRED=2 \
PRIVATE_KEY=0x…deployer… \
  node deploy-falcon-lock.js

# 2) Record address in config/usdc-bridge.json → ethereum_mainnet.lock_contract

# 3) Falcon mainnet QUC issuer + trust lines (ceremony stables)

# 4) Generalize relays; run deposit/withdraw with mainnet env; dry-run small amount
```

## Checklist

See `config/usdc-bridge.json` → `mainnet_checklist`.

## Security notes

- Testnet owner key on `46.224.0.140` must **not** be reused for mainnet.
- Withdraw path requires multi-sig confirmations on-chain; relay should only *propose*, not sole-sign release.
- Prefer hardware wallets / multisig wallets (Safe) as `OWNERS`.
