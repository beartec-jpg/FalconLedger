# T0 runbook — launch day only

Do **not** run this until Phase 0 of `CHECKLIST.md` is complete.  
Have the ceremony pack open (addresses + secrets + image digest).

---

## 1. Image & network id

```bash
# Confirm digest matches IMAGE_DIGEST.txt
cat IMAGE_DIGEST.txt
docker pull "$(cat IMAGE_DIGEST.txt)"   # or your registry ref
```

- [ ] Network id matches `NETWORK_ID.txt`
- [ ] All operator nodes use the **same** digest

---

## 2. Start your validators

On each genesis operator machine (order: seed peers first if you use static peers):

```bash
export QXRP_XRPLD_IMAGE="$(cat /path/to/IMAGE_DIGEST.txt)"
# use your install one-liner / compose with mainnet config from pack
```

- [ ] N validators up
- [ ] Consensus healthy (`server_info` → not stuck disconnected)
- [ ] Ledger index advancing

---

## 3. Publish connect details

- [ ] Public RPC URL(s)
- [ ] Network id
- [ ] Image digest
- [ ] Wallet page one-liner (same image)
- [ ] UNL / validator onboarding doc

---

## 4. Genesis split (4B → 2B / 1B / 1B)

```bash
export PUBLIC_RPC='https://…'
export ADMIN_RPC='…'          # signing path
export GENESIS_SECRET='…'     # from pack
export GENESIS_ADDRESS='…'
export AIRDROP_ADDRESS='…'
export FAUCET_ADDRESS='…'
export DEV_ADDRESS='…'

python3 scripts/mainnet-genesis-split.py --dry-run
# verify balances & addresses
python3 scripts/mainnet-genesis-split.py --execute
```

- [ ] AIRDROP ≈ 2B
- [ ] FAUCET ≈ 1B
- [ ] DEV ≈ 1B

---

## 5. Portal + faucet

- [ ] Set `NEXT_PUBLIC_MAINNET_LIVE=true` + production env from pack
- [ ] Redeploy portal
- [ ] Fund **hot** faucet from FAUCET bucket (bounded amount, not full 1B)
- [ ] Smoke: one faucet claim = **100** FALCON, cooldown works
- [ ] Confirm DB log row `network=mainnet`

---

## 6. Airdrop clock + cron

- [ ] Set `airdrop_config.genesis_at` = now (UTC)
- [ ] Start snapshot cron: `NETWORK=mainnet` only
- [ ] Confirm `POST /api/airdrop/snapshot?network=testnet` still **400**

---

## 7. Lending (same day or +1)

- [ ] Bootstrap vault + broker on mainnet
- [ ] `python3 scripts/set-broker-pool-rate.py --rate 5000`  # 5%
- [ ] HF monitor on mainnet RPC

---

## 8. Announce

- [ ] Public post: RPC, install, faucet rules, airdrop = mainnet 60 days, batch pay 2B later
- [ ] Explicit: testnet does not count

---

## Stop if

- Validators disagree on genesis / UNL  
- Split pays wrong addresses  
- Faucet still drips 2000 or logs `testnet`  
- Image digest differs across operators  

Fix before marketing “mainnet is live.”
