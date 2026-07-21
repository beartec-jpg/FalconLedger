# Multi-node soak runbook (protocol item 2)

**Companion:** `docs/PROTOCOL_READINESS_7_5_PLAN.md`  
**Goal:** Prove the **freeze image** under continuous consensus — not a ceremony dry-run alone.

---

## 0. Preconditions

- [ ] Item 1 complete: `IMAGE_DIGEST.txt` populated (not example)  
- [ ] ≥3 hosts (or VMs) with Docker  
- [ ] Throwaway Falcon keys only (`rehearsal-` labels)  
- [ ] Private network / firewall — not advertised as mainnet  
- [ ] Same digest on every node  

```bash
export QXRP_XRPLD_IMAGE="$(grep -v '^#' scripts/mainnet-ceremony/IMAGE_DIGEST.txt | tail -1)"
echo "$QXRP_XRPLD_IMAGE"
```

Network id: use **rehearsal-only** id **or** 1026 on private IPs only (never mix with public testnet 1001).

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

If claims must be proven and epochs are 7 days, either soak ≥1 epoch **or** accept claims tested only on a separate short-epoch build (note: that binary ≠ mainnet freeze).

---

## 4. Green criteria

- [ ] No crash loops / OOM  
- [ ] No invariant fatal logs  
- [ ] Ledgers never stuck for extended periods  
- [ ] Restart recovery OK  
- [ ] Adversarial checklist (`ADVERSARIAL_PROTOCOL_CHECKLIST.md`) pass  
- [ ] Disk growth acceptable for node_size  

---

## 5. End of soak

1. Archive logs + health check output under `scripts/mainnet-ceremony/dry-runs/soak-YYYYMMDD/`  
2. **Wipe** chain data (`wipe-fleet-for-genesis` or compose down -v)  
3. **Destroy** rehearsal secrets — never reuse on real mainnet  
4. If failures: fix → new freeze tip → rebuild item 1 → re-soak  

---

## 6. What soak is not

- Not public T0  
- Not Neon/portal production  
- Not a substitute for external audit (item 11)  
