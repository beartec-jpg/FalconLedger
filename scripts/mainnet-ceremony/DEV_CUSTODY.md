# DEV wallet custody plan (0.5% / 1B FALCON)

**Purpose:** Hold the development / ops bucket after genesis split.  
**Risk if hot single key:** full 1B can be drained by one compromise.  
**Rule:** never keep the full 1B on a portal-hot or faucet-hot key.

---

## Recommended model (mainnet)

| Layer | What | Where |
|-------|------|--------|
| **Cold master** | Falcon secret for DEV account | Offline encrypted USB / hardware vault; 2-of-3 people |
| **Signer list** | XRPL-style multi-sig on DEV account after fund | On-chain `SignerListSet` once chain is live |
| **Ops working capital** | Small hot wallet (e.g. ≤ 5–20M) | Funded from DEV only after multi-sig approve |
| **Grants / infra** | Batch payments from working capital | Scripted; dual review |

### Threshold (suggested)

- **2-of-3** Falcon signers for any payment ≥ 1M FALCON from DEV  
- **2-of-3** for SignerList changes  
- Each key holder on a different device / geography  

### Key holders (fill names — do not commit real keys)

| Slot | Role | Person | Device | Backup |
|------|------|--------|--------|--------|
| A | Protocol lead | ________ | offline laptop | sealed envelope |
| B | Ops lead | ________ | offline laptop | sealed envelope |
| C | Independent (legal/finance) | ________ | offline laptop | sealed envelope |

---

## Ceremony sequence

1. **Offline:** generate DEV Falcon wallet (`wallet_propose` key_type falcon512) on air-gapped machine with a Falcon-capable binary.  
2. Record **address only** in `wallets/ADDRESSES.txt`.  
3. Store secret as `wallets/DEV.secret` **outside git** (chmod 600).  
4. After T0 split funds DEV with 1B:  
   - Submit `SignerListSet` (2-of-3) from DEV.  
   - Optionally rotate: create new multi-sig-controlled account and migrate.  
5. Create **OPS_HOT** wallet; multi-sig transfer ≤ working capital.  
6. Portal / CI never receive DEV master secret.

---

## What is forbidden

- DEV secret in Vercel / GitHub Actions / Discord  
- Single-EOA control of full 1B after day 0  
- Reusing testnet genesis or faucet secrets on mainnet  

---

## Recovery

- 2-of-3 can always reconstitute control if one key is lost.  
- If two keys lost: treat as catastrophic — document in incident plan; community disclosure required.  
- Annual key ceremony: verify all three can still sign a dry-run offline hash.

---

*Template only — fill people and execute SignerListSet after mainnet is live.*
