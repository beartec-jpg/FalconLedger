# Testnet: enable AccountNames **live** (amendment vote — no re-genesis)

**Goal:** Roll custom addresses (`NameSet` / `NameUnbond` / `NameRelease`) onto **network id 1001** without wiping NuDB or forcing a hard cutover that splits consensus.  
**Feature:** `AccountNames` · hash `AFBD9AF10542C5438ED4D41FF7672E90A87B231ED9316E4FAD27A2CEF8DBD953`  
**Status on testnet today:** binary **does not know** the feature (`feature` → `badFeature` / unknown) · image `qxrp/xrpld:lending-v7`  
**Target binary:** any pin that includes AccountNames, e.g.  
`qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5`  
(`mainnet-v2` / `b007db22d`, labels `falcon.names=AccountNames`)

Testnet already uses a short majority window:

```ini
[amendment_majority_time]
15 minutes
```

So after **≥80% of UNL** votes **yes**, enablement is ~15 minutes — not two weeks.

---

## Why past “force updates” broke consensus

| What people did | What actually happens |
|-----------------|------------------------|
| Wipe data + new genesis | New chain history — looks like a break |
| Put a **new** hash only in `[amendments]` on some nodes | Those nodes expect the amendment **already enabled**; ledger hash diverges |
| Mixed binaries with **different ledger-writing logic** (e.g. scoring) for many flag ledgers | Divergent score writes → fork |
| Upgrade one UNL node and leave others old | Unknown amendment cannot activate safely; scoring/code skew can still fork |

**Correct path:** same new binary on **all** UNL validators → **vote** via `[features]` / admin `feature` → wait majority time → amendment becomes enabled on-ledger → txs work. **No wipe.**

---

## Pre-flight (2026-07-27)

| Check | Value |
|-------|--------|
| Public RPC | `http://46.224.0.140:6005` |
| Network | **1001** · proposers **5** · UNL size **5** |
| Need yes votes | **ceil(0.8 × 5) = 4** validators |
| Current image | `qxrp/xrpld:lending-v7` (no AccountNames) |
| AccountNames in tree | `Supported::Yes`, `VoteBehavior::DefaultNo` |
| Majority time (testnet cfg) | **15 minutes** |

Peers / operator hosts seen in `ips_fixed` (operators must upgrade their own box):

| IP | Role (typical) | SSH from this workstation |
|----|----------------|---------------------------|
| `46.224.0.140` | full + val2 | yes |
| `192.241.247.158` | validator peer | no (key) |
| `89.167.109.241` | validator peer | no |
| `204.168.175.194` | validator peer | no |
| `167.233.55.43` | validator peer | no |

You need **each other validator operator** to run Phase 1–2 on their host. One machine alone cannot enable the amendment.

---

## Phase 0 — Tell every validator operator (copy/paste)

> We are enabling **AccountNames** on Falcon testnet **1001** via normal amendment vote (no re-genesis, no NuDB wipe).  
> 1) Pull image `qxrp/xrpld:mainnet-v2` (digest `sha256:9362005f1360…`).  
> 2) Rolling restart **your** node only (keep `/data`).  
> 3) Add `AccountNames` under `[features]` (vote yes). **Do not** add it under `[amendments]` until after it shows `enabled: true` on the public RPC.  
> 4) Confirm `feature AccountNames` → `supported: true`.  
> 5) After ≥4/5 UNL vote yes for ~15 minutes, the amendment enables network-wide.  
> Full runbook: `docs/ops/TESTNET_ACCOUNTNAMES_AMENDMENT.md`

---

## Phase 1 — Rolling binary upgrade (**keep chain live**)

Do **one validator at a time**. Wait until that node is `full`/`proposing` and peers reconnect before the next.

### 1a. Pull pin

```bash
docker pull qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5
docker tag qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5 qxrp/xrpld:mainnet-v2
# optional readable alias for compose:
docker tag qxrp/xrpld:mainnet-v2 qxrp/xrpld:testnet-names
```

### 1b. Point compose at the new image (example)

```yaml
# docker-compose.yml — only change the image line
services:
  xrpld:
    image: qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5
```

### 1c. Restart **without** wiping data

```bash
cd /var/lib/qxrp-val2   # or your node dir
docker compose up -d
# DO NOT: docker compose down -v
# DO NOT: rm -rf data/nudb
```

### 1d. Health

```bash
curl -s http://127.0.0.1:6005 -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}' | python3 -c "
import sys,json
i=json.load(sys.stdin)['result']['info']
print(i.get('server_state'), (i.get('validated_ledger') or {}).get('seq'), i.get('peers'), i.get('network_id'))
"
# Must still be network_id 1001 and seq advancing with the public network.
```

### 1e. Confirm the binary knows AccountNames

```bash
# admin RPC (localhost admin port — often 5005 inside container)
docker exec qxrp-val2 curl -s http://127.0.0.1:5005 \
  -H 'Content-Type: application/json' \
  -d '{"method":"feature","params":[{"feature":"AccountNames"}]}'
# Expect: supported=true, enabled=false (until vote completes)
# If error badFeature → still on old image
```

**Scoring note:** `mainnet-v2` also includes scoring-pay fixes. Upgrade **all UNL members** within a short window (ideally same hour) so flag-ledger score writes do not diverge for long. Full nodes (non-UNL) should upgrade too, but they do not vote.

---

## Phase 2 — Vote **yes** for AccountNames (no force)

### Preferred: config `[features]` (survives restart)

Edit `xrpld.cfg` — add the line under the existing feature votes:

```ini
[features]
LendingPermissionless
LendingCollateral
SingleAssetVault
LendingProtocol
ProofOfParticipation
AMM
AccountNames
# ... keep your other votes ...
```

Restart the container once so the vote is loaded:

```bash
docker compose up -d
```

### Alternative: admin RPC (if your build persists votes in wallet DB)

```bash
# Vote YES (vetoed: false)
curl -s http://127.0.0.1:5005 -H 'Content-Type: application/json' -d '{
  "method": "feature",
  "params": [{ "feature": "AccountNames", "vetoed": false }]
}'
```

To **block** (do not do this if you want enablement):

```bash
curl -s http://127.0.0.1:5005 -H 'Content-Type: application/json' -d '{
  "method": "feature",
  "params": [{ "feature": "AccountNames", "vetoed": true }]
}'
```

### ⚠️ Do **not** yet add AccountNames under `[amendments]`

On this fleet, `[amendments]` lists hashes that are **already enabled** on ledger. Adding AccountNames there **before** network enablement makes that node assume a different amendment set → **consensus break**.

Only after public RPC shows `enabled: true` may you optionally record the hash in `[amendments]` for documentation parity (not required for correctness).

---

## Phase 3 — Watch majority → enablement

From any upgraded node (or public RPC after support is universal):

```bash
RPC=http://46.224.0.140:6005

# 1) Feature known + majority timer
curl -s -X POST "$RPC" -H 'Content-Type: application/json' \
  -d '{"method":"feature","params":[{"feature":"AccountNames"}]}' | python3 -m json.tool

# 2) When enabled=true, NameSet is legal
```

Timeline on testnet cfg:

1. ≥4 of 5 UNL validators support + vote yes  
2. Amendment enters **majority**  
3. Holds for **`amendment_majority_time` = 15 minutes**  
4. Pseudo-tx enables `AccountNames` on the Amendments ledger object  
5. `NameSet` / `NameUnbond` / `NameRelease` return normal engine codes (not `temDISABLED`)

If majority never appears: someone is still on `lending-v7`, or voting **veto**, or not in UNL.

---

## Phase 4 — Smoke (after `enabled: true`)

Bond = **100 FALCON**. NetworkID **1001** on every tx.

| Step | Expect |
|------|--------|
| Fund account ≥ 100 + reserve + fee | OK |
| `NameSet` `Name=alice.bob` | `tesSUCCESS` |
| Second claim same name | `tecDUPLICATE` |
| `NameUnbond` | releasing |
| `NameRelease` early | `tecTOO_SOON` |
| After 1 epoch ledgers | `NameRelease` frees name |

See `docs/qxrp/NAME_SERVICE.md` and `scripts/mainnet-ceremony/dry-runs/NAMES_SMOKE_PLAN.md`.

---

## Operator checklist (per validator)

- [ ] Pulled digest `9362005f1360…`  
- [ ] Restarted **without** volume wipe  
- [ ] `network_id` still **1001**, seq tracking public chain  
- [ ] `feature AccountNames` → `supported: true`  
- [ ] `AccountNames` in `[features]` (vote yes)  
- [ ] **Not** in `[amendments]` until enabled network-wide  
- [ ] After enable: optional NameSet smoke from a test wallet  

---

## Coordinator checklist (you)

- [ ] Message all 5 UNL operators with Phase 0 blurb  
- [ ] Upgrade host(s) you control first; confirm no fork (seq matches peers)  
- [ ] Confirm ≥4 validators report `supported: true` + yes vote  
- [ ] Wait ≥15 minutes after majority  
- [ ] Public RPC: `enabled: true`  
- [ ] Portal/docs mention names live on testnet  
- [ ] Do **not** flip mainnet until ceremony path  

---

## Rollback

- If only **you** upgraded and something looks wrong: roll image tag back to `lending-v7` on **your** node only (still no wipe) **before** the amendment enables.  
- If amendment **already enabled** network-wide, rolling back binary loses support → that node cannot follow ledgers with Name* txs. Stay on the names-capable pin.

---

## FAQ

**Q: Can I enable by only editing `[amendments]`?**  
A: No on a live multi-node net. That is force-expect, not a vote.

**Q: Do full-history / non-UNL nodes need to upgrade?**  
A: Yes, to apply Name* transactions after enablement. They do not need to vote.

**Q: Will this re-genesis?**  
A: No, if you never wipe NuDB and never force unknown amendments.

**Q: Why was AccountNames unknown before?**  
A: `lending-v7` predates the names protocol commit; only newer images register the feature.
