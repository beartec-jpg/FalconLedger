# Multi-chain bridge ops (testnet)

Companion to wallet doc: `Falcon-faucet-wallet/docs/MULTI-CHAIN-IMPLEMENTATION.md`.

## Live services (coordinator)

| Unit | Role |
|------|------|
| `qxrp-bridge-relay` | Sepolia USDC deposit mint + withdraw release stack |
| `qxrp-feth-relay` | Sepolia WETH → Falcon `ETH` (FETH) mint |
| `qxrp-fbnb-relay` | BSC testnet WBNB → Falcon `BNB` (FBNB) mint |

## Contracts / issuers

| Route | Lock | Wrapped token | Falcon currency | Issuer key / account |
|-------|------|---------------|-----------------|----------------------|
| F-USDC | `0x2dae31Cbf2E3a418d617081985661fCD0117b75C` | USDC Sepolia | `QUC` | `qUSDC_issuer` / `rPh77fAAmvbVuMQQP9H9JKtyTFuhRjp3Fk` |
| FETH | `0x11808B5Cda14d4144dbD2279f92f447e0f8F8F1d` | WETH Sepolia | `ETH` | `FETH_issuer` / `rNn8xd3hbeTEAcEmRQae8xVaKshAj7HrED` |
| FBNB | `0x682D60Bbf8dE13065C71cbF35c1dAdAa23E79938` | WBNB BSC testnet | `BNB` | `FBNB_issuer` / `rf8NZLdcwxrXAnPppttTeenQcAJW75uj7E` |

Lock owner EOA (testnet): `0x64BA18002B6E72fE443f3F8a146cE529250Db107`

## Parameterized deposit relay

```bash
python3 scripts/bridge-deposit-relay.py --loop --interval 30 \
  --currency BNB --decimals 18 --issuer-key FBNB_issuer \
  --lock-contract 0x682D60Bbf8dE13065C71cbF35c1dAdAa23E79938 \
  --sepolia-rpc https://bsc-testnet-rpc.publicnode.com \
  --relay-state /var/lib/qxrp-bridge/fbnb_relay_state.json \
  --stables-state /var/lib/qxrp-stables/stables_state.json
```

(`--sepolia-rpc` is historical naming; pass any EVM JSON-RPC.)

## New issuer (bridge-only)

```bash
python3 scripts/issue-bridge-iou.py --symbol FBTC --currency BTC
```

## FBTC: need a new script?

| Path | Deploy lock? | New relay script? |
|------|--------------|-------------------|
| **WBTC on EVM** (recommended first) | Yes — `deploy-falcon-lock.js` with WBTC | **No** — same `bridge-deposit-relay.py` + new systemd unit |
| **Native BTC deposits** | No ERC-20 lock | **Yes** — new BTC UTXO watcher + mint (e.g. `bridge-btc-deposit-relay.py`) |

Native Multi-chain BTC send/receive does **not** need a new Falcon script.

## Files

- Contract: `contracts/FalconCollateralLock.sol`
- Deploy: `scripts/deploy-falcon-lock.js`
- Deposit relay: `scripts/bridge-deposit-relay.py`
- Withdraw (USDC): `scripts/bridge-withdraw-relay.py`
- Issue IOU: `scripts/issue-bridge-iou.py`
- Configs: `config/usdc-bridge.json`, `config/feth-bridge.json`, `config/fbnb-bridge.json`, `config/testnet-stables.json`
