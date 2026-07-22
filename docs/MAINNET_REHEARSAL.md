# Mainnet dress rehearsal — readiness + run plan

**Goal:** Prove genesis → split (2B/1B/1B) → faucet fund → faucet drip → (optional) lend 5%, on a **private throwaway** network. Then **wipe**. Real T0 uses **new keys**.

**Orchestrator (preferred for 8+):** `python3 scripts/ops/rehearsal-e2e.py --phases all`  
writes `scripts/mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md`.  
**Emissions soak:** use `bash scripts/ops/build-fast-epoch-rehearsal.sh` image only — never for real T0.  
**Launch pin:** `qxrp/xrpld:mainnet-v1` + `scripts/mainnet-ceremony/IMAGE_DIGEST.txt`.

**Not the goal:** Public one-liner, real airdrop clock, or keeping this chain.

---

## A. Prerequisites (must be true before you start rehearsal)

### 1. Image

| Item | Target | Status cue |
|------|--------|------------|
| Binary includes launch features | CID emission (first unlock epoch 8), PoPL LP split, ClaimAmmLpReward, fluid ActiveSet scoring, AccountNames, multi-sig bridge code | freeze tip `1789d2fb4` or later |
| Image available on rehearsal hosts | `qxrp/xrpld:mainnet-v1` (or digest) | `docker inspect` revision + `falcon.names=AccountNames` |
| Same image on every rehearsal node | no mixed tags | all operators pull same tag/digest |

**Freeze pin (2026-07-22):** `qxrp/xrpld:mainnet-v1` @ `1789d2fb4` · local id `e5086df99920…` — smoke **14/14 PASS** on private net 1099; see `scripts/mainnet-ceremony/IMAGE_DIGEST.txt` and `dry-runs/REHEARSAL_RESULTS.md`.

### 2. Machines

| Item | Minimum for rehearsal |
|------|------------------------|
| Validator count | **2–3** nodes (or 1 standalone if you only need split+faucet, not multi-val consensus) |
| Isolated | Not public DNS; not advertised as mainnet |
| Wipe path known | `wipe-fleet-for-genesis` or `docker compose down -v` + delete data dirs |

### 3. Throwaway keys (label everything `rehearsal-`)

Generate **fresh** keys — never reuse for real mainnet:

| Wallet | Role |
|--------|------|
| GENESIS | 4B circulating holder after genesis |
| AIRDROP | receives 2B after split |
| FAUCET cold | receives 1B after split |
| FAUCET hot | optional smaller funded account portal signs with |
| DEV | receives 1B after split |
| Validator keys | 2–3 falcon secrets + consensus keys |

Record **addresses only** in a local `rehearsal-addresses.txt`. Secrets offline.

### 4. Scripts & config (from repo)

| Piece | Path / env |
|-------|------------|
| Genesis split | `scripts/mainnet-genesis-split.py` |
| Split env example | `scripts/mainnet-genesis-split.env.example` |
| Wipe | `bin/install/wipe-fleet-for-genesis.sh` (or manual volume wipe) |
| Pool rate (optional) | `scripts/set-broker-pool-rate.py` |
| Ceremony templates | `scripts/mainnet-ceremony/` |

### 5. Portal / faucet (staging only)

Pick one:

**Option A — API-only smoke (simpler)**  
- Hit faucet route against rehearsal RPC with env pointing at throwaway faucet  
- No need for public Vercel mainnet live  

**Option B — staging portal**  
- Separate Vercel project or preview env  
- `MAINNET_RPC_URL` = rehearsal RPC (or temporary “mainnet” slot)  
- `MAINNET_FAUCET_*` = throwaway hot faucet  
- `MAINNET_DRIP_AMOUNT_QXRP=100`  
- `DATABASE_URL` = Neon (or skip durable log for pure balance test)  
- `NEXT_PUBLIC_MAINNET_LIVE=true` **only on staging**, not production domain  

### 6. Network parameters for rehearsal

| Param | Suggestion |
|-------|------------|
| Network id | Use a **rehearsal-only** id (not final public mainnet id if already chosen) **or** same id but never public |
| Peers | Private IPs only |
| Name | `Falcon Rehearsal` in configs so nobody confuses with real mainnet |

---

## B. Rehearsal day — ordered steps

### Phase 1 — Bring up private chain

1. Wipe any old data on rehearsal hosts  
2. Install/start 2–3 validators with **rehearsal** keys + **pinned image**  
3. Confirm:
   - `server_info` healthy  
   - ledgers advancing  
   - genesis circulating balance ≈ **4B** on GENESIS account  
   - treasury ≈ **196B** (not spendable by ops key)

### Phase 2 — Genesis split

```bash
export PUBLIC_RPC='http://<rehearsal-rpc>:6005'
export ADMIN_RPC='http://127.0.0.1:5005'   # or docker exec path
export GENESIS_SECRET='…'                  # throwaway
export GENESIS_ADDRESS='…'
export AIRDROP_ADDRESS='…'
export FAUCET_ADDRESS='…'
export DEV_ADDRESS='…'

python3 scripts/mainnet-genesis-split.py --dry-run
python3 scripts/mainnet-genesis-split.py --execute
```

**Pass criteria:**

| Account | Expected |
|---------|----------|
| AIRDROP | ~2,000,000,000 FALCON |
| FAUCET | ~1,000,000,000 FALCON |
| DEV | ~1,000,000,000 FALCON |
| GENESIS | ~0 (or residual fees) |

### Phase 3 — Faucet

1. Fund hot faucet from FAUCET (e.g. 50k–500k for smoke, not full 1B)  
2. Configure portal/API with drip **100**  
3. Claim once → wallet receives **100**  
4. Claim again immediately → **1h cooldown** error  
5. (Optional) Confirm DB row `network=…` if Neon wired  

### Phase 4 — Optional lend smoke

1. Bootstrap vault + broker on rehearsal  
2. `set-broker-pool-rate.py --rate 5000`  
3. One borrow/repay or fail-closed on wrong InterestRate  

### Phase 5 — Tear down

1. Stop all containers  
2. **Delete data volumes** (critical)  
3. Archive rehearsal notes (what worked / broken)  
4. **Destroy or quarantine throwaway secrets**  
5. Real mainnet keys remain unused in ceremony pack  

---

## C. Pass / fail gate (rehearsal “ready for real T0”)

| Gate | Required |
|------|----------|
| Split script worked end-to-end | Yes |
| DEV + FAUCET + AIRDROP balances correct | Yes |
| Faucet drip 100 + cooldown | Yes |
| Same image digest used | Yes |
| Chain wiped after | Yes |
| Real ceremony secrets never used on rehearsal | Yes |
| Multi-node consensus stable | Recommended |
| Full airdrop freeze + batch pay | Optional for first rehearsal |
| Public install one-liner | Not part of rehearsal |

---

## D. Minimal vs full rehearsal

| Level | What you run | When enough |
|-------|--------------|-------------|
| **Minimal** | 1 node standalone + split + faucet pay | Prove scripts + balances only |
| **Standard** | 2–3 vals + split + faucet + wipe | **Recommended before real T0** |
| **Full** | + portal staging + lend + HF monitor + snapshot API + **bridge multi-sig e2e** | If you want zero surprises |

Bridge multi-sig (Sepolia 2-of-3) **PASS** 2026-07-22 — lock `0x8A300…CE295`; see `scripts/mainnet-ceremony/dry-runs/REHEARSAL_RESULTS.md` and `docs/MAINNET_BRIDGE.md`.

---

## E. Checklist — “ready to start dress rehearsal”

Copy and tick:

### Ready when

- [x] Image `mainnet-v1` freeze pin on rehearsal hosts; commit hash checked (`1789d2fb4`)  
- [ ] 2–3 hosts (or 1 for minimal) free and wipeable  
- [ ] Throwaway GENESIS / AIRDROP / FAUCET / DEV keys generated  
- [ ] Throwaway validator keys generated  
- [ ] Split script env file filled for rehearsal  
- [ ] Faucet smoke path chosen (API or staging portal) with drip=100  
- [ ] Written wipe procedure tested once on a dummy compose  
- [ ] Someone free for 1–2 hours to run phases 1–5 without interruption  

### Explicitly not required for first rehearsal

- [ ] Public DNS / Hub push (can `docker save | load` image)  
- [ ] Real mainnet network id  
- [ ] Production Vercel mainnet live  
- [ ] Eth mainnet contract (can mock or skip)  
- [ ] Airdrop 60-day scoring  

---

## F. After rehearsal

1. Fix any script/docs bugs found  
2. Optionally second short rehearsal  
3. Real T0 = ceremony pack **real** keys + published connect details (see `MAINNET_GO_LIVE_CHECKLIST.md`)
