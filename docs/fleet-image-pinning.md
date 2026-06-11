# Validator Fleet Image Pinning & Falcon Smoke Tests

**Purpose:** Prevent consensus failures where one node accepts Falcon-signed
transactions but others reject them with `Invalid signature`. This happened on
the testnet when node1 ran `qxrp/xrpld:falcon` while remote validators still
had an older `qxrp/xrpld:latest` image.

## Rule: never float `:latest` across a validator fleet

| Do | Don't |
|----|-------|
| Pin `qxrp/xrpld:falcon` or an explicit digest | Use `qxrp/xrpld:latest` on production validators |
| Upgrade every validator to the **same digest** | Upgrade one node and leave the rest behind |
| Run the Falcon smoke test after every image change | Assume matching `build_version` strings mean matching binaries |

Override only when you know what you are doing:

```bash
export QXRP_XRPLD_IMAGE='qxrp/xrpld@sha256:<digest>'
```

## Why this matters

Falcon Payment transactions can show `tesSUCCESS` on the **full-history node's
open ledger** while **never validating**, if validators disagree on signature
verification. Users see faucet success with zero balance; bonds and wallet
sends silently fail.

Symptoms:

- `submit` → `tesSUCCESS` on the public RPC entry node
- `tx` lookup → `validated: false` forever (or `txnNotFound`)
- Remote validator `submit` of the same blob → `invalidTransaction` /
  `Invalid signature`
- Faucet / sender sequence stuck (validated ledger never advanced)

## Falcon smoke test (manual)

Run after deploying or upgrading **any** validator image.

### 1. Local image check (on one host)

```bash
docker pull qxrp/xrpld:falcon
docker run --rm qxrp/xrpld:falcon --version   # optional

# Ephemeral sign check via admin RPC inside a throwaway container — see
# bin/install/install-qxrp-validator.sh (smoke_test_local_image).
```

### 2. Fleet signature check (required)

Fetch a recent validated Falcon Payment from the faucet account, then
re-submit the same `tx_blob` to **each** validator public RPC (`:6005`).

A healthy validator returns an XRPL engine code such as `tefPAST_SEQ` or
`tefALREADY` (signature verified, tx already seen). A **bad** binary returns
`invalidTransaction` / `Invalid signature`.

```bash
FAUCET=rwzhiWW4GYK2sQVR5Lw4iDpYLANB5krJXY
RPC=http://46.224.0.140:6005

HASH=$(curl -sf -X POST "$RPC" -H 'Content-Type: application/json' \
  -d "{\"method\":\"account_tx\",\"params\":[{\"account\":\"$FAUCET\",\"limit\":1}]}" \
  | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['transactions'][0]['tx']['hash'])")

BLOB=$(curl -sf -X POST "$RPC" -H 'Content-Type: application/json' \
  -d "{\"method\":\"tx\",\"params\":[{\"transaction\":\"$HASH\",\"binary\":true}]}" \
  | python3 -c "import sys,json; print(json.load(sys.stdin)['result']['tx'])")

for HOST in 46.224.0.140 89.167.109.241 204.168.175.194 167.233.55.43; do
  echo "=== $HOST ==="
  curl -sf -X POST "http://$HOST:6005" -H 'Content-Type: application/json' \
    -d "{\"method\":\"submit\",\"params\":[{\"tx_blob\":\"$BLOB\"}]}" \
    | python3 -c "import sys,json; r=json.load(sys.stdin)['result']; print(r.get('engine_result'), r.get('error'), r.get('error_message','')[:60])"
done
```

**Pass:** every host returns an engine result (`tefPAST_SEQ`, `tefALREADY`, …)
and **not** `invalidTransaction`.

**Fail:** any host returns `Invalid signature` → stop the launch, align images,
re-run smoke test.

## One-command installer integration

`bin/install/install-qxrp-validator.sh` runs both smoke tests automatically
**before** generating keys, starting the validator, or bonding:

1. Pull pinned image (`qxrp/xrpld:falcon`)
2. **Local smoke** — Falcon `wallet_propose` + `simulate` in ephemeral container
3. **Fleet smoke** — re-submit latest faucet Falcon tx to all bootstrap peers
4. Full install — keys, config, `docker compose up`, fund poll, bond, cron

Skip only for debugging:

```bash
curl -fsSL .../install-qxrp-validator.sh | bash -s -- \
  --payout rYourWallet --skip-smoke-test
```

## Mainnet launch checklist (image / consensus)

- [ ] All validators pinned to the same image digest
- [ ] Falcon fleet smoke test passes on every validator host
- [ ] Faucet uses dedicated Falcon account (bounded balance), not genesis
- [ ] Faucet waits for **validated** ledger confirmation before reporting success
- [ ] `server_info` / monitoring alerts if validator `build_version` or digest diverges

## Distributing images when registry pull fails

If `docker pull qxrp/xrpld:falcon` fails on a host (private registry), export
from a known-good node and load everywhere:

```bash
# On source node
docker save qxrp/xrpld:falcon | gzip -1 > qxrp-xrpld-falcon.tar.gz

# On each target
gunzip -c qxrp-xrpld-falcon.tar.gz | docker load
docker tag 08bf27da4841 qxrp/xrpld:falcon   # use actual loaded image ID
```

Then re-run the fleet smoke test before rejoining consensus.

---

**Last updated:** 2026-06-11