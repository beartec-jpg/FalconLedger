# qXRP Destructive Consensus Testing Toolkit

**Accurate for the current prototype environment (May 2026)**

## Current Real Environment

- All active nodes run **direct binary** + **systemd** (no Docker on the live validators).
- 4 validators with proper services:
  - `node1` (46.224.0.140) → `qxrp.service` + signing proxy
  - `node2` + `node3` on shared host 37.27.47.236 → `qxrp-node2.service` / `qxrp-node3.service`
  - `node4` (204.168.175.194) → `qxrp-node4.service`
- Two spare machines available (89.167.109.241 and 46.62.156.169).

## Directory Contents

- `DESTRUCTIVE_TEST_PLAN.md` — Full staged progressive destruction campaign (recommended reading first)
- `qxrp_destructive_consensus_test.py` — Main Python orchestrator (updated with correct systemd commands)
- `helpers/`
  - `capture_state.py` — Quick JSON snapshot of all 4 nodes (run before/after every major action)
  - `capture_all_logs.sh` — Pulls recent journalctl from all nodes
- `report_template.md` (to be added)

## Quick Start

1. **Make the scripts executable**
   ```bash
   chmod +x helpers/capture_all_logs.sh
   chmod +x qxrp_destructive_consensus_test.py
   ```

2. **Test connectivity**
   ```bash
   python3 qxrp_destructive_consensus_test.py --dry-run
   ```

3. **Run individual stages** (recommended)
   ```bash
   # Baseline
   python3 qxrp_destructive_consensus_test.py --stage baseline

   # Single node dropout (you choose the target)
   python3 qxrp_destructive_consensus_test.py --stage single_dropout --target node4

   # Double dropout (most aggressive so far)
   python3 qxrp_destructive_consensus_test.py --stage double_dropout
   ```

4. **Manual helpers (very useful during testing)**
   ```bash
   # Snapshot current state of all nodes
   python3 helpers/capture_state.py

   # Pull fresh logs from everything
   ./helpers/capture_all_logs.sh
   ```

## Important Security Notes

- The signing proxy token and all root passwords used during this campaign **must be rotated immediately** after testing finishes.
- This is a throwaway prototype. Break it aggressively, but document everything.

## Recommended Workflow

1. Read `DESTRUCTIVE_TEST_PLAN.md`
2. Run Stage 0 (baseline) and get comfortable with the tools
3. Run Stage 1 (single dropout) on one node
4. Review snapshots + logs
5. Proceed to more aggressive stages only when ready

## Giving Grok Direct Execution Access

You can run the scripts yourself and paste output, or provide temporary SSH access (you already shared passwords once). I can then execute stages live and give real-time analysis + the final report.

All artifacts will be collected under `destructive_test_results/`.

---

**This toolkit + plan is now ready for execution.** Let me know when you want to begin (e.g., "start with baseline" or "run stage 1 on node4 now").
