# Falcon Ledger — Mainnet Readiness Summary (Public)

**Date:** July 2026 · **Not a launch announcement**

A short, shareable snapshot of progress toward public mainnet. Operator runbooks, host inventories, and secrets are intentionally omitted.

---

## Protocol (ready for freeze-aged ops)

| Item | State |
|------|--------|
| Falcon-512 for accounts, consensus, P2P | Done |
| Fixed 200B supply + protocol treasury | Done |
| CID emission (first unlock epoch 8) + PoPL LP split | Done |
| Fluid scoring; pay ∝ score all bonded (no K=32 cut) | Done — `mainnet-v2` fresh-genesis smoke **PASS** (12/12) |
| Open/rotating UNL | Future amendment — bootstrap UNL until then ([OPEN_UNL_AMENDMENT.md](OPEN_UNL_AMENDMENT.md)) |
| Claim paths with epoch pool hard-caps | Done |
| Double-sign slash (pure bond burn) | Done |
| Account Names amendment | Done (smoke on freeze pin + mainnet-v2) |
| AMM + lending stack (vault / broker / loan) | Exercised on test & private nets |
| Bridge multi-sig model (N-of-M) | Code + Sepolia e2e proven |
| Freeze / T0 image on Docker Hub | **`mainnet-v2`** @ `1af01dbfb` (all bonded pay ∝ score) |
| Prior names-only freeze | `mainnet-v1` @ `1789d2fb4` — historical only |

**Image pin (public freeze / T0):**  
`qxrp/xrpld@sha256:7286556f5bdb6cc8a3abe4864f06ca9a6af4f59c2fba82775793800744aa931e`  
Tags: `mainnet-v2` · `mainnet-v2-scoring-pay` · `mainnet-v2-1af01dbfb`

**Mainnet network id (planned):** `1026`  
**Testnet network id (live):** `1001`

---

## Product / portal (testnet live; mainnet gated)

| Item | State |
|------|--------|
| Passkey Falcon wallet, faucet, swap, pool | Live on testnet portal |
| Lending UI | Live on testnet |
| Bridge (test EVM) | Live on testnet; mainnet lock redeploy still open |
| White paper (site) | v2.6 — scoring, names, CID |
| Mainnet portal flag | **`LIVE=false`** until T0 |
| Neon durable schema (faucet / airdrop / board) | Provisioned for mainnet prep |

---

## Still ahead of public T0

1. Multi-day freeze-pin **soak** (stability aging)  
2. Launch-day ceremony (genesis split, UNL bring-up, public RPC/DNS)  
3. ETH **mainnet** multi-sig lock + cold owners (new keys; not testnet)  
4. Fund mainnet faucet from ceremony FAUCET bucket; set portal secrets only then  
5. Optional external review of freeze scope (slash, claims, scoring, bridge custody)  
6. Flip `NEXT_PUBLIC_MAINNET_LIVE` only after consensus + split are green  

---

## What “ready” means here

**Protocol ready for a freezed image** means: features intended for launch are implemented, smoke-tested on that pin, and published as a reproducible container digest.

**Public mainnet live** means: real network id 1026 peers, funded genesis allocation split, public RPC, and portal mainnet mode enabled — **none of which are claimed in this document**.

---

## Deeper reading

- [TEST_AND_VERIFICATION.md](TEST_AND_VERIFICATION.md) — what was tested  
- [whitepaper.md](whitepaper.md) — full design  
- [NAME_SERVICE.md](NAME_SERVICE.md) — human addresses  
