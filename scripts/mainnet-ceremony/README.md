# Mainnet ceremony pack

**What it is:** A single offline folder that holds everything you need for a calm T0 turn-on — so launch day is “open the pack and run the steps,” not inventing process under pressure.

**What it is not:** A running network. No validators, no ledgers, no public RPC until you choose T0.

**Model:** Fill the pack this week → on launch day start validators + publish connect details.  
See also:

- `docs/MAINNET_COLD_START.md` — why cold prep
- `docs/MAINNET_GO_LIVE_CHECKLIST.md` — full tick list
- `docs/MAINNET_LAUNCH_SPEC.md` — economics (2B/1B/1B, faucet 100, airdrop mainnet-only)
- `docs/ops/MAINNET_OPS_RUNBOOK.md` — Neon, cron, fleet

---

## Folder layout

Copy this tree somewhere **offline** (encrypted USB / air-gapped machine).  
**Never commit real secrets to git.**

```text
mainnet-ceremony/                    # this directory (template in repo)
│
├── README.md                        # this file
├── T0-RUNBOOK.md                    # ordered steps on launch day
├── CHECKLIST.md                     # copy of MAINNET_GO_LIVE_CHECKLIST.md
│
├── IMAGE_DIGEST.txt                 # qxrp/xrpld@sha256:… (from docker push)
├── NETWORK_ID.txt                   # chosen mainnet network id
│
├── portal.env.mainnet               # real env (local only — not in git)
├── portal.env.mainnet.example       # safe template (in git)
│
├── wallets/
│   ├── ADDRESSES.txt                # public r-addresses only
│   ├── GENESIS.secret               # OFFLINE ONLY
│   ├── AIRDROP.secret
│   ├── FAUCET.secret
│   └── DEV.secret                   # + multi-sig notes
│
├── validators/
│   ├── unl-public.txt               # public keys for installers / UNL
│   ├── peers.txt                    # seed IPs/hostnames (fill at T0 if needed)
│   └── secrets/                     # per-node falcon secrets — OFFLINE ONLY
│
└── dry-runs/
    ├── genesis-split.out.txt        # output of --dry-run
    └── notes.md
```

Repo ships only **templates** (examples). You create the real files locally.

---

## How to use it

### This week (cold)

1. Build/push image → write `IMAGE_DIGEST.txt`
2. Choose network id → `NETWORK_ID.txt`
3. Generate wallets + validator keys → `wallets/` + `validators/secrets/`
4. Fill `portal.env.mainnet` from the example (still `MAINNET_LIVE=false`)
5. Apply Neon schema; deploy portal with live=false
6. Dry-run genesis split → save output under `dry-runs/`
7. Walk `CHECKLIST.md` Phase 0

### Launch day (T0)

1. Open `T0-RUNBOOK.md` and execute top to bottom
2. Start validators with pinned image + secrets from pack
3. Publish digest, network id, RPC, one-liner
4. Split 2B/1B/1B, flip portal live, set airdrop `genesis_at`, start cron

---

## What’s already in the git template

| File | Purpose |
|------|---------|
| `IMAGE_DIGEST.txt.example` | Where to paste Hub digest after push |
| `portal.env.mainnet.example` | Vercel/env vars for mainnet faucet + airdrop |
| `T0-RUNBOOK.md` | Short ordered launch-day script |
| `README.md` | This overview |

Copy checklist from repo:

```bash
cp docs/MAINNET_GO_LIVE_CHECKLIST.md scripts/mainnet-ceremony/CHECKLIST.md
```
