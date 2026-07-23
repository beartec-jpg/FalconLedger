# Multi-node soak runbook (protocol item 2)

**Companion:** `docs/PROTOCOL_READINESS_7_5_PLAN.md` · `docs/MAINNET_SCORE_UPLIFT_PLAN.md` (B1)  
**Live checklist:** `scripts/mainnet-ceremony/dry-runs/SOAK_CHECK.md`  
**Goal:** Prove the **freeze image** under continuous consensus — not a ceremony dry-run alone.

**Current T0 pin (2026-07-22+):** `b007db22d` ·  
`qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5`  
(`mainnet-v2` / scorefix). Do not credit soak hours from older `mainnet-v1` windows.

---

## 0. Preconditions

- [x] Item 1 complete: `IMAGE_DIGEST.txt` populated (not example)  
- [ ] ≥3 hosts (or VMs) with Docker — **or** 3 containers on one host for private soak  
- [ ] Throwaway Falcon keys only (`rehearsal-` labels)  
- [ ] Private network / firewall — not advertised as mainnet  
- [ ] Same digest on every node  

```bash
export QXRP_XRPLD_IMAGE="$(grep -v '^#' scripts/mainnet-ceremony/IMAGE_DIGEST.txt | grep -v '^$' | tail -1)"
echo "$QXRP_XRPLD_IMAGE"
# expect: qxrp/xrpld@sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5
```

Network id: use **rehearsal-only** id (e.g. **1099**) **or** 1026 on private IPs only (never mix with public testnet 1001).
---

## 1. Bring-up

1. Install validators with pinned image (compose / `install-qxrp-validator.sh` with `QXRP_XRPLD_IMAGE`).  
2. Shared UNL (`validators.txt` Falcon hex keys).  
3. Start all nodes together.  
4. Confirm:
   - `server_info` healthy, ledgers advancing  
   - `xrpld --version` / commit matches freeze  
   - peers connected  

Health loop:

```bash
export PUBLIC_RPC='http://127.0.0.1:6005'   # or public WS/RPC of a soak node
bash scripts/ops/soak-health-check.sh
# cron every 5 min:
# */5 * * * * PUBLIC_RPC=... bash /path/to/soak-health-check.sh >> /var/log/soak-health.log 2>&1
```

---

## 2. Workload during soak

Keep the chain busy enough to exercise paths:

| Workload | How |
|----------|-----|
| Payments | Random small payments between funded accounts |
| Validator bonds | Register + bond; leave online for scoring |
| DEX/AMM (optional) | Create pool, swap lightly |
| Lending (if T0 includes lend) | `scripts/lend-e2e-*.py` against soak RPC |
| Restarts | Restart one validator mid-soak; confirm resync |

---

## 3. Duration

| Minimum | Better |
|---------|--------|
| 48–72 hours continuous | 7+ days or ≥1 full reward epoch |

If claims must be proven and epochs are 7 days, either soak ≥1 epoch **or** accept claims tested only on a separate short-epoch build (note: that binary ≠ mainnet freeze). Fast-epoch claim e2e is already recorded separately (`mainnet-v2-claim-e2e-results.json`).

### Snapshot cadence

| When | Action |
|------|--------|
| Start of soak | Write `dry-runs/soak-<ISO>Z.json` with seq, peers, uptime, digest |
| Every 12–24 h | Re-run health check; append or new JSON; update `SOAK_CHECK.md` |
| At 48 h / 7 d | Mark criteria in JSON (`multi_day_48h` / `multi_day_7d`) |
| End | Final archive + wipe checklist |

Minimal JSON shape:

```json
{
  "captured_at": "2026-07-23T08:15:36Z",
  "check": "PASS_EARLY|PASS_48H|PASS_7D|FAIL",
  "image": { "digest": "qxrp/xrpld@sha256:…", "revision": "b007db22d" },
  "server_info": {
    "server_state": "full",
    "validated_seq": 0,
    "peers": 2,
    "uptime_h": 0
  },
  "criteria": {
    "no_crash_loops": true,
    "peers_ge_2": true,
    "seq_advancing": true,
    "multi_day_48h": false,
    "multi_day_7d": false
  }
}
```

---

## 4. Green criteria

- [ ] No crash loops / OOM  
- [ ] No invariant fatal logs  
- [ ] Ledgers never stuck for extended periods  
- [ ] Restart recovery OK  
- [ ] Adversarial checklist (`ADVERSARIAL_PROTOCOL_CHECKLIST.md`) pass  
- [ ] Disk growth acceptable for node_size  
- [ ] **Same image digest** on all soak nodes for the whole window  
- [ ] ≥48 h continuous uptime on freeze pin (readiness credit)  

---

## 5. End of soak

1. Archive logs + health check output under `scripts/mainnet-ceremony/dry-runs/soak-YYYY-MM-DDThhmmZ.json`  
2. **Wipe** chain data (`wipe-fleet-for-genesis` or compose down -v)  
3. **Destroy** rehearsal secrets — never reuse on real mainnet  
4. If failures: fix → new freeze tip → rebuild item 1 → re-soak  

---

## 6. What soak is not

- Not public T0  
- Not Neon/portal production  
- Not a substitute for external audit (item 11)  
- Not claim-path proof on long epochs (use fast-epoch e2e artifact)  
- Not ETH mainnet bridge deploy  
