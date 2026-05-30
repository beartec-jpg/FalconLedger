#!/usr/bin/env python3
"""
qXRP Destructive Consensus Test Orchestrator
============================================
Accurate version for the current prototype environment (all systemd, no Docker on active nodes).

Real node mapping (discovered 2026-05-30):
- node1: 46.224.0.140   → qxrp.service          (has signing proxy)
- node2: 37.27.47.236   → qxrp-node2.service
- node3: 37.27.47.236   → qxrp-node3.service    (same host as node2)
- node4: 204.168.175.194 → qxrp-node4.service

Usage examples:
    python3 qxrp_destructive_consensus_test.py --stage baseline
    python3 qxrp_destructive_consensus_test.py --stage single_dropout --target node4
    python3 qxrp_destructive_consensus_test.py --full --dry-run
"""

import subprocess
import time
import json
import os
import sys
import argparse
from datetime import datetime
from pathlib import Path
import requests

# =============================================================================
# CONFIGURATION - ACCURATE FOR CURRENT ENVIRONMENT
# =============================================================================

SSH_USER = "root"
TEST_OUTPUT_DIR = Path("destructive_test_results")

# Real discovered services and connection info
NODES = {
    "node1": {
        "ip": "46.224.0.140",
        "hostname": "ubuntu-8gb-fsn1-1",
        "service": "qxrp.service",
        "signing_proxy_service": "qxrp-faucet-signer.service",
        "public_rpc": "http://46.224.0.140:6005",
        "admin_local": "http://127.0.0.1:5005",
        "signing_proxy": "http://46.224.0.140:3001/sign",
        # Use the token you provided earlier - ROTATE AFTER TESTING
        "signing_proxy_token": "b9d66ffeaf0be2c06422f21a6eff20795399ce56bce2ce0ad158d19202bdeb6f",
        "genesis_seed": "snoPBrXtMeMyMHUVTgbuqAfg1SUTb",
        "genesis_address": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
    },
    "node2": {
        "ip": "37.27.47.236",
        "hostname": "ubuntu-4gb-hel1-3",
        "service": "qxrp-node2.service",
        "public_rpc": "http://37.27.47.236:6006",
        "admin_local": "http://127.0.0.1:5006",
    },
    "node3": {
        "ip": "37.27.47.236",
        "hostname": "ubuntu-4gb-hel1-3",
        "service": "qxrp-node3.service",
        "public_rpc": "http://37.27.47.236:6007",
        "admin_local": "http://127.0.0.1:5007",
    },
    "node4": {
        "ip": "204.168.175.194",
        "hostname": "ubuntu-4gb-hel1-6",
        "service": "qxrp-node4.service",
        "public_rpc": "http://204.168.175.194:6005",
        "admin_local": "http://127.0.0.1:5005",
    },
}

# Spare machines available
SPARES = ["89.167.109.241", "46.62.156.169"]

# =============================================================================
# SSH & REMOTE CONTROL (Systemd based - accurate for current setup)
# =============================================================================

def log(msg, level="INFO"):
    ts = datetime.utcnow().isoformat(timespec="seconds") + "Z"
    print(f"[{ts}] [{level}] {msg}")

def ssh_run(node_id: str, command: str, timeout=90, capture=True):
    node = NODES[node_id]
    ssh_cmd = ["ssh", "-o", "StrictHostKeyChecking=no", "-o", "ConnectTimeout=12",
               f"{SSH_USER}@{node['ip']}", command]
    try:
        result = subprocess.run(ssh_cmd, capture_output=capture, text=True, timeout=timeout)
        return (result.stdout or "").strip()
    except Exception as e:
        log(f"SSH failed on {node_id}: {e}", "ERROR")
        return ""

def systemctl(node_id: str, action: str):
    """action = stop | start | restart | status"""
    service = NODES[node_id]["service"]
    out = ssh_run(node_id, f"systemctl {action} {service}")
    log(f"{action.upper()} {service} on {node_id} → {out[:100]}")
    return out

def stop_node(node_id: str):
    systemctl(node_id, "stop")
    time.sleep(5)

def start_node(node_id: str):
    systemctl(node_id, "start")
    time.sleep(8)

def restart_node(node_id: str):
    systemctl(node_id, "restart")
    time.sleep(10)

def capture_logs(node_id: str, label: str):
    node = NODES[node_id]
    out_dir = TEST_OUTPUT_DIR / "logs"
    out_dir.mkdir(parents=True, exist_ok=True)
    ts = datetime.utcnow().strftime("%Y%m%d_%H%M%S")
    filename = out_dir / f"{node_id}_{label}_{ts}.log"

    # Capture journalctl for the specific service
    cmd = f"journalctl -u {node['service']} --no-pager -n 800 --since '30 minutes ago'"
    logs = ssh_run(node_id, cmd, timeout=45)
    with open(filename, "w") as f:
        f.write(logs)
    log(f"Captured logs → {filename}")

def get_status(node_id: str):
    node = NODES[node_id]
    try:
        r = requests.post(node["public_rpc"], json={"method": "server_info", "params": [{}]}, timeout=8)
        info = r.json()["result"]["info"]
        return {
            "server_state": info.get("server_state"),
            "peers": info.get("peers"),
            "proposers": info.get("last_close", {}).get("proposers"),
            "quorum": info.get("validation_quorum"),
            "ledger_seq": info.get("validated_ledger", {}).get("seq"),
            "ledger_age": info.get("validated_ledger", {}).get("age"),
        }
    except Exception as e:
        return {"error": str(e)}

def print_network_status(label="CURRENT STATUS"):
    log(f"=== {label} ===")
    for nid in ["node1", "node2", "node3", "node4"]:
        s = get_status(nid)
        if "error" in s:
            log(f"  {nid}: ERROR - {s['error']}")
        else:
            log(f"  {nid}: state={s['server_state']}, peers={s['peers']}, "
                f"proposers={s['proposers']}, quorum={s['quorum']}, "
                f"seq={s['ledger_seq']}, age={s['ledger_age']}s")

# =============================================================================
# LOAD GENERATION (via signing proxy on node1)
# =============================================================================

def start_load(minutes=60, rate_per_sec=6):
    node = NODES["node1"]
    token = node["signing_proxy_token"]
    seed = node["genesis_seed"]
    addr = node["genesis_address"]

    # Background load using curl loop on node1
    cmd = f"""
    nohup bash -c '
    for i in $(seq 1 {minutes * 60 * rate_per_sec}); do
        curl -s -X POST {node["signing_proxy"]} \
             -H "Content-Type: application/json" \
             -H "Authorization: Bearer {token}" \
             -d "{{\\"tx_json\\":{{\\"TransactionType\\":\\"Payment\\",\\"Account\\":\\"{addr}\\",\\"Destination\\":\\"{addr}\\",\\"Amount\\":\\"1\\"}}, \\"secret\\":\\"{seed}\\"}}" >/dev/null 2>&1
        sleep 0.16
    done
    ' > /tmp/qxrp_destructive_load.log 2>&1 &
    echo $!
    """
    pid = ssh_run("node1", cmd.strip())
    log(f"Load generator started on node1 (background PID ~{pid}) for ~{minutes} min", "LOAD")
    return pid

def stop_load():
    ssh_run("node1", "pkill -f 'qxrp_destructive_load' || true; pkill -f 'curl.*sign' || true")
    log("Load generation stopped", "LOAD")

# =============================================================================
# STAGES
# =============================================================================

def stage_baseline():
    log("=== STAGE 0: BASELINE + SUSTAINED LOAD ===")
    print_network_status("BEFORE LOAD")
    start_load(minutes=45, rate_per_sec=5)
    time.sleep(8 * 60)
    print_network_status("UNDER LOAD")
    for nid in ["node1", "node2", "node3", "node4"]:
        capture_logs(nid, "baseline")
    stop_load()

def stage_single_dropout(target="node4"):
    log(f"=== STAGE 1: SINGLE DROPOUT + RESYNC (target={target}) ===")
    start_load(minutes=60)
    time.sleep(60)

    log(f"Stopping {target}...")
    stop_node(target)
    capture_logs(target, "just_before_stop")
    time.sleep(30)

    print_network_status("ONE NODE DOWN")
    time.sleep(12 * 60)  # 12 minutes gap

    print_network_status("BEFORE RECOVERY")
    start_node(target)
    time.sleep(3 * 60)
    print_network_status("RECOVERING")
    time.sleep(8 * 60)
    print_network_status("AFTER RECOVERY")

    capture_logs(target, "post_resync")
    for nid in ["node1", "node2", "node3"]:
        capture_logs(nid, "during_single_dropout")
    stop_load()

def stage_double_dropout():
    log("=== STAGE 3: DOUBLE VALIDATOR DROPOUT (ATTEMPT STALL) ===")
    start_load(minutes=90)
    time.sleep(60)

    log("Stopping node3 and node4 simultaneously...")
    stop_node("node3")
    stop_node("node4")
    capture_logs("node3", "double_drop")
    capture_logs("node4", "double_drop")

    time.sleep(15 * 60)
    print_network_status("TWO NODES DOWN")

    log("Bringing node3 back...")
    start_node("node3")
    time.sleep(5 * 60)
    print_network_status("ONE RECOVERED")

    log("Bringing node4 back...")
    start_node("node4")
    time.sleep(10 * 60)
    print_network_status("BOTH RECOVERED")

    capture_logs("node3", "double_recovery")
    capture_logs("node4", "double_recovery")
    stop_load()

# Add more stages as needed (partition, slashing, key rotation) in future iterations

def run_full_campaign():
    log("=== STARTING FULL DESTRUCTIVE CAMPAIGN ===")
    stage_baseline()
    time.sleep(120)
    stage_single_dropout("node4")
    time.sleep(180)
    stage_double_dropout()
    log("=== FULL CAMPAIGN COMPLETE (basic stages) ===")

# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--stage", choices=["baseline", "single_dropout", "double_dropout"])
    parser.add_argument("--target", default="node4", help="Target node for single dropout")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    TEST_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    if args.dry_run:
        log("DRY RUN - no actions will be taken")
        print_network_status("DRY RUN")
        return

    if args.stage == "baseline":
        stage_baseline()
    elif args.stage == "single_dropout":
        stage_single_dropout(args.target)
    elif args.stage == "double_dropout":
        stage_double_dropout()
    elif args.full:
        run_full_campaign()
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
