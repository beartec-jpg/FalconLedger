# Freeze soak check (private net 1099)

**Pin:** `qxrp/xrpld:mainnet-v2` @ `b007db22d`  
**Digest:** `qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5`  
**Host:** val5 — containers `qxrp-rehearsal-1/2/3`  
**Runbook:** `docs/ops/SOAK_RUNBOOK.md`  
**Uplift plan:** `docs/MAINNET_SCORE_UPLIFT_PLAN.md` B1  

> Historical note: an earlier soak window used `mainnet-v1` @ `1789d2fb4`.  
> **Credit multi-day soak only against the current T0 pin (mainnet-v2 scorefix).**

---

## What “soak” means here

Leave the chain **idle but online** on the **freeze digest**. Goal: still healthy later — not to stress-test throughput.

| Minimum for readiness credit | Better |
|------------------------------|--------|
| **≥48 h** continuous, no crash | **7+ days** or ≥1 long reward epoch |

---

## Check results

### 2026-07-27T09:11Z — **PASS_48H** (mainnet-v2 scorefix) ✅

| Field | Value |
|-------|--------|
| containers | all 3 **Up (healthy)** · 4 days · `mainnet-v2` |
| revision / digest | `b007db22d` · `sha256:9362005f1360…` |
| server_state | `full` |
| seq | **129110** (complete window advancing) |
| peers / proposers | **2** / 2 |
| uptime | **~107.7 h (~4.5 days)** |
| network_id | **1099** (private — not public 1026) |
| multi_day_48h | **true** |
| multi_day_7d | false (need ~168 h; optional) |

Artifact: `dry-runs/soak-2026-07-27T0911Z.json`

**Verdict:** multi-day soak credit **earned** (≥48 h continuous on freeze pin). Leave running for optional 7 d.

### 2026-07-23T08:15Z — **PASS_EARLY** (mainnet-v2 scorefix)

| Field | Value |
|-------|--------|
| containers | all 3 **Up (healthy)** · `mainnet-v2` |
| revision / digest | `b007db22d` · `sha256:9362005f1360…` |
| server_state | `full` (proposers also `proposing`) |
| seq | **12842** (complete 12292–12842; advancing) |
| peers / proposers | **2** / 2 |
| uptime | **~10.7 h** since scorefix redeploy |
| network_id | **1099** (private — not public 1026) |
| peer_disconnects | 0 |
| io_latency_ms | 1 |

Artifact: `dry-runs/soak-2026-07-23T0815Z.json`

**Verdict (historical):** early healthy; later promoted to PASS_48H.
### Prior window (mainnet-v1 — historical only)

### 2026-07-22T10:09Z — PASS_EARLY on `mainnet-v1` @ `1789d2fb4`

Seq ~742 · peers 2 · image `e5086df99920`. Superseded by scorefix redeploy; **do not** count toward mainnet-v2 soak hours.

---

## Operator re-check (copy/paste)

```bash
ssh val5   # or your soak host
docker ps | grep rehearsal
export PUBLIC_RPC='http://127.0.0.1:6005'
bash /path/to/qXRP/scripts/ops/soak-health-check.sh

# richer snapshot:
curl -s http://127.0.0.1:6005 -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}' | python3 -c "
import sys,json,datetime
i=json.load(sys.stdin)['result']['info']
vl=i.get('validated_ledger') or {}
print('ts', datetime.datetime.utcnow().isoformat()+'Z')
print('state', i.get('server_state'))
print('seq', vl.get('seq'))
print('complete', i.get('complete_ledgers'))
print('peers', i.get('peers'))
print('proposers', (i.get('last_close') or {}).get('proposers'))
print('uptime_h', round((i.get('uptime') or 0)/3600, 2))
print('disconnects', i.get('peer_disconnects'))
"
docker inspect qxrp-rehearsal-1 --format \
  '{{.Config.Image}} rev={{index .Config.Labels "org.opencontainers.image.revision"}}'
```

Archive each check under:

```text
scripts/mainnet-ceremony/dry-runs/soak-YYYY-MM-DDThhmmZ.json
```

**Good:** `full`/`proposing`, seq **higher** than last check, peers **2**, containers **Up (healthy)** for days, same digest.  
**Bad:** restart loops, seq stuck, peers 0, OOM kills, mixed digests.

Optional: one manual Payment — proves txs still apply. Not required for soak credit.

---

## Image distribution

| Path | Status |
|------|--------|
| Docker Hub | **Pushed** — `mainnet-v2` · `mainnet-v2-scoring-pay` · `mainnet-v2-scorefix` |
| RepoDigest | `qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5` |
| FREEZE_COMMIT | `b007db22d` |

```bash
export QXRP_XRPLD_IMAGE='qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5'
```

---

## End of soak (before real T0)

1. Archive final snapshot + logs  
2. **Wipe** chain data  
3. **Destroy** rehearsal secrets — never reuse on mainnet 1026  
4. If failures: fix → new freeze tip → rebuild → re-soak  

---

## Tx simulator?

**Not required** for freeze soak. Soak = “does it stay up?” · Load gen = separate capacity test.
