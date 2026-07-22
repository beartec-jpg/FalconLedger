# AccountNames smoke — post names pin (mainnet-v1)

**Image:** `qxrp/xrpld:mainnet-v1` @ `1789d2fb4` (`falcon.names=AccountNames`)  
**Prefer:** fast-epoch rehearsal for cooldown (256 ledgers) · long-epoch for launch soak only  

Config must include under `[features]`:

```
AccountNames
ProofOfParticipation
```

(plus existing AMM/lending features as needed)

---

## A. Redeploy on val5 (fast-epoch, recommended for names e2e)

```bash
# On val5 as root
IMAGE=qxrp/xrpld:mainnet-v1
docker inspect $IMAGE --format 'rev={{index .Config.Labels "org.opencontainers.image.revision"}} names={{index .Config.Labels "falcon.names"}}'

# Point compose / container env at IMAGE (same as prior rehearsal stack).
# Example if using prior compose on val5:
#   export QXRP_XRPLD_IMAGE=$IMAGE
#   # rebuild/restart validators with wipe if starting clean network_id
#   docker compose -f <rehearsal-compose> down
#   # optional wipe NuDB if new genesis
#   docker compose -f <rehearsal-compose> up -d

# Or rolling: stop each val, replace image tag, wipe data if hard-fork / new features at genesis,
# restart. AccountNames is an amendment — can enable without wipe if chain already running
# and validators vote it in (rehearsal: force in [features] from genesis is simpler).
```

**Genesis / private net:** put `AccountNames` in `[features]` so it is force-enabled (same pattern as `ProofOfParticipation`).

Health check:

```bash
curl -s http://127.0.0.1:6005 -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}' | jq '.result.info | {seq:.validated_ledger.seq, state:.server_state, peers:.peers}'
```

---

## B. Names e2e (script outline)

Use Falcon `wallet_propose` + fund from genesis/faucet. Bond = **100 FALCON** (`100000000` drops).

| Step | Tx / action | Expect |
|------|-------------|--------|
| 1 | Fund Alice ≥ 100 + fees + reserve | balance OK |
| 2 | `NameSet` Name=`alice.bob` | tesSUCCESS · `ltACCOUNT_NAME` status=0 |
| 3 | `ledger_entry` `account_name`: `"alice.bob"` | Account = Alice |
| 4 | Second `NameSet` same name from Bob | `tecDUPLICATE` |
| 5 | Second name for Alice | `tecDUPLICATE` |
| 6 | `NameUnbond` | status=1 releasing · UnbondingStartLedger set |
| 7 | Resolve / name-routed pay while releasing | **reject** (UI/RPC; on-ledger Payment still uses AccountID) |
| 8 | `NameRelease` before epoch elapsed | `tecTOO_SOON` |
| 9 | Wait `kNAME_UNBOND_LEDGERS` (fast-epoch: **256**) | — |
| 10 | `NameRelease` | bond returned · object deleted |
| 11 | Bob `NameSet` `alice.bob` | tesSUCCESS (name free) |

Tx types: `NameSet` (95), `NameUnbond` (96), `NameRelease` (97). Field `Name` = VL normalized lowercase.

Example RPC submit shape (after Falcon sign):

```json
{
  "TransactionType": "NameSet",
  "Account": "r…",
  "Name": "alice.bob",
  "Fee": "12",
  "Sequence": N,
  "NetworkID": 1099
}
```

```json
{
  "TransactionType": "NameUnbond",
  "Account": "r…",
  "Fee": "12",
  "Sequence": N,
  "NetworkID": 1099
}
```

```json
{
  "TransactionType": "NameRelease",
  "Account": "r…",
  "Name": "alice.bob",
  "Fee": "12",
  "Sequence": N,
  "NetworkID": 1099
}
```

Lookup:

```bash
curl -s http://127.0.0.1:6005 -H 'Content-Type: application/json' -d '{
  "method": "ledger_entry",
  "params": [{ "account_name": "alice.bob", "ledger_index": "validated" }]
}'
```

---

## C. Long-epoch soak (after names smoke)

```bash
# Same IMAGE, default epoch 172800 / first emission 8
# Do NOT wait full name unbond on long-epoch in one sitting —
# only NameSet + active lookup + optional NameUnbond start.
# Run multi-day: peers, proposers, ClaimReward sample, no crashes.
```

---

## D. Pin verification (ceremony)

```bash
# On build host (val5)
docker inspect qxrp/xrpld:mainnet-v1 --format '{{.Id}}'
# Compare scripts/mainnet-ceremony/IMAGE_DIGEST.txt and FREEZE_COMMIT.txt

# After registry push:
# docker push qxrp/xrpld:mainnet-v1
# docker inspect qxrp/xrpld:mainnet-v1 --format '{{index .RepoDigests 0}}'
```
