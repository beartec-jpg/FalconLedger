#!/bin/bash
# Capture logs from all active qXRP nodes (systemd based)

set -euo pipefail
TS=$(date -u +%Y%m%d_%H%M%S)
OUTDIR="destructive_test_results/logs/$TS"
mkdir -p "$OUTDIR"

echo "Capturing logs at $TS ..."

# Node 1
ssh root@46.224.0.140 "journalctl -u qxrp.service --no-pager -n 1200 --since '45 minutes ago'" > "$OUTDIR/node1.log" 2>&1 || echo "Node1 failed"

# Node 2
ssh root@37.27.47.236 "journalctl -u qxrp-node2.service --no-pager -n 1200 --since '45 minutes ago'" > "$OUTDIR/node2.log" 2>&1 || echo "Node2 failed"

# Node 3
ssh root@37.27.47.236 "journalctl -u qxrp-node3.service --no-pager -n 1200 --since '45 minutes ago'" > "$OUTDIR/node3.log" 2>&1 || echo "Node3 failed"

# Node 4
ssh root@204.168.175.194 "journalctl -u qxrp-node4.service --no-pager -n 1200 --since '45 minutes ago'" > "$OUTDIR/node4.log" 2>&1 || echo "Node4 failed"

echo "Logs saved to $OUTDIR"
ls -lh "$OUTDIR"
