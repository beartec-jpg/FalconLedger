# Faucet anti-Sybil posture (portal)

**Status:** Implemented in `qXRP-faucet-wallet` · **Not** consensus security  
**Code:** `src/lib/faucet-sybil.ts`, `src/lib/faucet-quota.ts`, `src/app/api/faucet/route.ts`

Free drips are an **airdrop / UX** surface. Hardening reduces farming; it does not secure ledger consensus (bonds + slash + UNL do).

---

## Controls

| Control | Default | Env knobs |
|---------|---------|-----------|
| Per-address daily cap | 5 / UTC day | `FAUCET_CLAIMS_PER_DAY`, `MAINNET_FAUCET_CLAIMS_PER_DAY` |
| Per-IP daily cap | same keys with `ip:` prefix | same |
| Cooldown | 1 hour | `FAUCET_COOLDOWN_SECONDS`, `MAINNET_FAUCET_COOLDOWN_SECONDS` |
| Mainnet fail-closed | no Redis/DB → 503 | Override only with `FAUCET_ALLOW_MEMORY_MAINNET=true` (dev) |
| Bot UA block | curl/wget/scripting UAs | `FAUCET_ALLOW_CLI=true` for ops |
| Turnstile captcha | off until secret set | `TURNSTILE_SECRET_KEY` / `FAUCET_TURNSTILE_SECRET`; testnet also if `FAUCET_CAPTCHA_TESTNET=true` |
| Global daily budget | mainnet 2000 / testnet 50000 | `FAUCET_GLOBAL_CLAIMS_PER_DAY_MAINNET`, `FAUCET_GLOBAL_CLAIMS_PER_DAY` |
| Subnet daily budget | mainnet /24 → 20 | `FAUCET_SUBNET_CLAIMS_PER_DAY` |
| Durable claim log | Neon when configured | `DATABASE_URL` · schema `docs/sql/airdrop-schema.sql` |
| Unlimited testnet | **off** | `TESTNET_FAUCET_UNLIMITED=true` (never mainnet) |
| Origin allowlist | portal origins | existing `isOriginAllowed` |

Quota is consumed only after **ledger-validated** success. Global/subnet counters increment after per-IP/account peek passes (before sign) to shed Sybil load without minting.

---

## Mainnet go-live checklist (faucet)

- [ ] `LIVE` / network config live only after ceremony fund  
- [ ] Redis (Upstash/Vercel KV) **or** Neon configured  
- [ ] Turnstile site + secret on mainnet  
- [ ] Conservative drip (`MAINNET_FAUCET_DRIP_QXRP`, e.g. 10)  
- [ ] Global budget sized to FAUCET bucket / expected users  
- [ ] `TESTNET_FAUCET_UNLIMITED` **not** set on production  

---

## What this is not

- Not proof-of-humanity KYC  
- Not a substitute for airdrop scoring windows  
- Not DOUBLE_SIGN / bond economics  
