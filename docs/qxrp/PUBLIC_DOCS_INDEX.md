# Falcon Ledger — Public documentation index

Documents in `docs/qxrp/` intended for **public** sharing (community, X, press, integrators).  
Internal launch ops live under `docs/MAINNET_*.md` and `scripts/mainnet-ceremony/` and should not be reposted wholesale.

| Document | One-line pitch for X / blog |
|----------|------------------------------|
| [whitepaper.md](whitepaper.md) | Full protocol story: Falcon-only, CID, PoP, names, liquidity |
| [TEST_AND_VERIFICATION.md](TEST_AND_VERIFICATION.md) | What we actually tested before freeze (AMM, bridge multi-sig, scoring, names, pin) |
| [MAINNET_READINESS_SUMMARY.md](MAINNET_READINESS_SUMMARY.md) | Short “where we are vs public T0” without ops secrets |
| [NAME_SERVICE.md](NAME_SERVICE.md) | Bonded human names → `r…` addresses |
| [validator-lifecycle.md](validator-lifecycle.md) | Bond, score, claim, slash |
| [epoch-emission.md](epoch-emission.md) | CID emission math |
| [supply-model.md](supply-model.md) | 200B cap, treasury, conservation |
| [fee-split.md](fee-split.md) | Burn band + validator share |
| [governance.md](governance.md) | On-chain parameter votes |

## Suggested announcement order

1. **TEST_AND_VERIFICATION** — credibility / progress  
2. **MAINNET_READINESS_SUMMARY** — “not live yet, here’s the gap to T0”  
3. **whitepaper v2.6** themes — fluid scoring + Account Names + CID  
4. **NAME_SERVICE** — product tease for wallet UX  

## Do not post publicly as-is

- `docs/MAINNET_REHEARSAL.md`, `REHEARSAL_RESULTS.md`, soak host notes  
- Ceremony encrypted backups, faucet secrets, UNL private material  
- Validator IPs, SSH, or private RPC endpoints  
