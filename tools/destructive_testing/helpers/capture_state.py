#!/usr/bin/env python3
"""
Quick state snapshot tool for all 4 nodes.
Run this before/after every major action during destructive testing.
"""

import requests
import json
from datetime import datetime
from pathlib import Path

NODES = {
    "node1": "http://46.224.0.140:6005",
    "node2": "http://37.27.47.236:6006",
    "node3": "http://37.27.47.236:6007",
    "node4": "http://204.168.175.194:6005",
}

def get_info(url):
    try:
        r = requests.post(url, json={"method": "server_info", "params": [{}]}, timeout=6)
        return r.json()["result"]["info"]
    except Exception as e:
        return {"error": str(e)}

def main():
    out = {
        "timestamp": datetime.utcnow().isoformat(),
        "nodes": {}
    }
    for name, url in NODES.items():
        out["nodes"][name] = get_info(url)

    Path("destructive_test_results/snapshots").mkdir(parents=True, exist_ok=True)
    fname = f"destructive_test_results/snapshots/state_{datetime.utcnow().strftime('%Y%m%d_%H%M%S')}.json"
    with open(fname, "w") as f:
        json.dump(out, f, indent=2)
    print(f"State snapshot saved → {fname}")

if __name__ == "__main__":
    main()
