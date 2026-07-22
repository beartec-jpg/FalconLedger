# Freeze soak check (private net 1099)

**Pin:** `qxrp/xrpld:mainnet-v1` @ `1789d2fb4`  
**Host:** val5 `5.78.142.246` — containers `qxrp-rehearsal-1/2/3`  
**Frozen:** 2026-07-22 · leave running a few days  

## What “soak” means here

Leave the chain **idle but online**. Goal: still healthy later — not to stress-test throughput.

## Check results

### 2026-07-22T10:09Z (first re-check after pin) — **PASS_EARLY**

| Field | Value |
|-------|--------|
| containers | all 3 **Up (healthy)** · `mainnet-v1` |
| server_state | `full` |
| seq | **742** (complete 5–742; advancing) |
| peers / proposers | **2** / 2 |
| uptime | **~0.6 h** (pin redeploy window — not multi-day yet) |
| image | `e5086df99920` · rev `1789d2fb4` · AccountNames |

Artifact: `dry-runs/soak-check-2026-07-22.json`  
On host: `/root/mainnet-ceremony-artifacts/soak-check-2026-07-22.json`

**Verdict:** healthy now; re-check again after **≥24–48 h** for multi-day soak credit.

### Next re-check (operator)

```bash
ssh val5
docker ps | grep rehearsal
curl -s http://127.0.0.1:6005 -H 'Content-Type: application/json' \
  -d '{"method":"server_info","params":[{}]}' | python3 -c "
import sys,json
i=json.load(sys.stdin)['result']['info']
print('state', i.get('server_state'))
print('seq', (i.get('validated_ledger') or {}).get('seq'))
print('peers', i.get('peers'))
print('uptime_hours', round((i.get('uptime') or 0)/3600, 1))
"
```

**Good:** `full` (or proposing), seq **higher** than last check, peers **2**, containers **Up (healthy)** for days.  
**Bad:** restart loops, seq stuck, peers 0, OOM kills.

Optional: one manual Payment from genesis to a test wallet — proves txs still apply. Not required.

## Image distribution

| Path | Status |
|------|--------|
| Docker Hub | **Pushed** 2026-07-22 — `mainnet-v1` + `mainnet-v1-1789d2fb4` |
| RepoDigest | `qxrp/xrpld@sha256:e5086df99920ca62a6c7c09e65d49decd43ae3a67cf6167aa46419e006fcb31c` |
| Offline tarball on val5 | also available — `/root/mainnet-ceremony-artifacts/qxrp-xrpld-mainnet-v1-1789d2fb4.tar.gz` |

Pull pin: `docker pull qxrp/xrpld@sha256:e5086df99920ca62a6c7c09e65d49decd43ae3a67cf6167aa46419e006fcb31c`

## Tx simulator?

**Not normal / not required** for a freeze soak.

| | |
|--|--|
| **Skip heavy load-gen** | Noise, fills disk/logs, can hide real stability signals |
| **Optional light activity** | A few manual txs over days is fine |
| **When a simulator *is* useful** | Separate load/perf test *after* freeze, or before T0 if you care about capacity |

Soak = “does it stay up?” · Simulator = “how hard can we push?” — different jobs.
