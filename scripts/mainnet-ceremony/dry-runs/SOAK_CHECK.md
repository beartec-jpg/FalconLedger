# Freeze soak check (private net 1099)

**Pin:** `qxrp/xrpld:mainnet-v1` @ `1789d2fb4`  
**Host:** val5 `5.78.142.246` — containers `qxrp-rehearsal-1/2/3`  
**Frozen:** 2026-07-22 · leave running a few days  

## What “soak” means here

Leave the chain **idle but online**. Goal: still healthy later — not to stress-test throughput.

## Check in a couple of days

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

**Good:** `full` (or proposing), seq **higher** than when you froze, peers **2**, containers **Up (healthy)** for days.  
**Bad:** restart loops, seq stuck, peers 0, OOM kills.

Optional: one manual Payment from genesis to a test wallet — proves txs still apply. Not required.

## Tx simulator?

**Not normal / not required** for a freeze soak.

| | |
|--|--|
| **Skip heavy load-gen** | Noise, fills disk/logs, can hide real stability signals |
| **Optional light activity** | A few manual txs over days is fine |
| **When a simulator *is* useful** | Separate load/perf test *after* freeze, or before T0 if you care about capacity |

Soak = “does it stay up?” · Simulator = “how hard can we push?” — different jobs.
