# Dry-run log

## Offline plan validation

Run from repo root (no RPC required):

```bash
python3 scripts/mainnet-genesis-split.py --offline-plan \
  --genesis rGENESIS --airdrop rAIRDROP --faucet rFAUCET --dev rDEV
```

Save output:

```bash
python3 scripts/mainnet-genesis-split.py --offline-plan \
  --genesis r… --airdrop r… --faucet r… --dev r… \
  | tee scripts/mainnet-ceremony/dry-runs/genesis-split-offline.out.txt
```

## Live dry-run (after private rehearsal chain or mainnet RPC up)

```bash
set -a && source /path/to/offline/mainnet-genesis-split.env && set +a
python3 scripts/mainnet-genesis-split.py --dry-run \
  | tee scripts/mainnet-ceremony/dry-runs/genesis-split-live.out.txt
```

## Results

| Date | Mode | Result | Operator |
|------|------|--------|----------|
| | offline-plan | | |
| | live dry-run | | |
| | execute | | **T0 only** |

## Ceremony public pack filled (2026-07-22)

| Item | Status |
|------|--------|
| `wallets/ADDRESSES.txt` | GENESIS / AIRDROP / FAUCET / DEV r-addresses from ceremony keygen |
| `wallets/ADDRESSES.public.json` | Full public JSON (validators FB09… pubs) |
| `validators/unl-public.txt` | VAL1–VAL5 validation public keys |
| `NETWORK_ID.txt` | **1026** |
| `IMAGE_DIGEST.txt` | `qxrp/xrpld@sha256:e5086df99920…` (Hub) |
| `portal.env.mainnet` | **LIVE=false**; FAUCET account set; secrets empty (local/gitignored) |
| Encrypted secrets | Offline on operator machine: `falcon-ceremony-backup.enc.json` |

Neon airdrop schema: apply when `DATABASE_URL` is available:
`psql "$DATABASE_URL" -f docs/sql/airdrop-schema.sql`

## Bridge multi-sig (Sepolia mainnet-parity)

| Date | Result | Lock | Notes |
|------|--------|------|--------|
| 2026-07-22 | **PASS** | `0x8A300bC6726C633ae350F58380194Ce3008CE295` | 2-of-3 deploy; deposit 5 USDC; mint 5 QUC; 1-owner block; 2-owner release 3 USDC. Details: `REHEARSAL_RESULTS.md` § Bridge. Host: `46.224.0.140:/var/lib/qxrp-bridge/mainnet-parity/` |
