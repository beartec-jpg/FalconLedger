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
