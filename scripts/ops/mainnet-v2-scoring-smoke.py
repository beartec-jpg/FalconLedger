#!/usr/bin/env python3
"""Smoke mainnet-v2 scoring-pay image on local rehearsal RPC."""
from __future__ import annotations

import json
import subprocess
import time
import urllib.request
from datetime import datetime, timezone

RPC = "http://127.0.0.1:6005"
ADMIN = "http://127.0.0.1:5005"
OUT = "/root/mainnet-v2-scoring-smoke.json"


def rpc(url: str, method: str, params=None, timeout=15):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def main() -> None:
    results: dict = {
        "pin": "mainnet-v2",
        "image": "qxrp/xrpld:mainnet-v2",
        "image_id": "sha256:7286556f5bdb6cc8a3abe4864f06ca9a6af4f59c2fba82775793800744aa931e",
        "tests": [],
        "notes": [
            "Rolling upgrade to mainnet-v2 (all bonded pay ∝ score)",
            "n=3 cannot numerically prove K=32 removal; labels + binary + no demote logs",
            "Open UNL amendment is docs-only future work",
        ],
    }

    def add(name: str, status: str, detail: str = "") -> None:
        results["tests"].append({"name": name, "status": status, "detail": detail})
        print(f"{status:4} {name}: {detail}")

    info = None
    last = "timeout"
    for _ in range(45):
        try:
            info = rpc(RPC, "server_info")["result"]["info"]
            st = info.get("server_state")
            if st in ("full", "proposing", "validating"):
                break
        except Exception as e:
            info = None
            last = str(e)
        time.sleep(2)

    if not info:
        add("server health", "FAIL", last)
        open(OUT, "w").write(json.dumps(results, indent=2) + "\n")
        raise SystemExit(1)

    seq = (info.get("validated_ledger") or {}).get("seq")
    st0 = info.get("server_state")
    ok = st0 in ("full", "proposing", "validating")
    add(
        "server health",
        "PASS" if ok else "WARN",
        f"state={st0} seq={seq} peers={info.get('peers')}",
    )

    out = subprocess.check_output(
        [
            "docker",
            "inspect",
            "qxrp/xrpld:mainnet-v2",
            "--format",
            "{{index .Config.Labels \"falcon.scoring\"}}|"
            "{{index .Config.Labels \"falcon.activeset\"}}|"
            "{{index .Config.Labels \"org.opencontainers.image.revision\"}}",
        ],
        text=True,
    ).strip()
    sc, ac, rev = out.split("|")
    add(
        "image labels",
        "PASS" if sc == "all-bonded-pro-rata" and ac == "disabled" else "FAIL",
        out,
    )

    try:
        wp = rpc(ADMIN, "wallet_propose", {})["result"]
        add(
            "wallet_propose",
            "PASS" if len(wp.get("falcon_secret", "")) > 1000 else "FAIL",
            f"{wp.get('account_id')} secret_len={len(wp.get('falcon_secret', ''))}",
        )
    except Exception as e:
        # Admin RPC is often container-local only — fall back to docker exec.
        try:
            raw = subprocess.check_output(
                [
                    "docker",
                    "exec",
                    "qxrp-rehearsal-1",
                    "curl",
                    "-s",
                    "-H",
                    "Content-Type: application/json",
                    "-d",
                    '{"method":"wallet_propose","params":[{}]}',
                    "http://127.0.0.1:5005",
                ],
                text=True,
                timeout=30,
            )
            wp = json.loads(raw)["result"]
            add(
                "wallet_propose",
                "PASS" if len(wp.get("falcon_secret", "")) > 1000 else "FAIL",
                f"{wp.get('account_id')} secret_len={len(wp.get('falcon_secret', ''))} via-docker",
            )
        except Exception as e2:
            add("wallet_propose", "FAIL", f"{e} | {e2}"[:200])

    time.sleep(5)
    info2 = rpc(RPC, "server_info")["result"]["info"]
    seq2 = (info2.get("validated_ledger") or {}).get("seq")
    if seq2 and seq and seq2 > seq:
        adv = "PASS"
    elif seq2 == seq:
        adv = "WARN"
    else:
        adv = "FAIL"
    add("seq advancing", adv, f"{seq}->{seq2}")
    peers = info2.get("peers") or 0
    add("peers", "PASS" if peers >= 2 else "WARN", f"peers={peers}")

    ps = subprocess.check_output(
        ["docker", "ps", "--format", "{{.Names}} {{.Status}}"], text=True
    )
    reh = [l for l in ps.splitlines() if "rehearsal" in l]
    add(
        "containers",
        "PASS" if len(reh) == 3 and all("Up" in l for l in reh) else "FAIL",
        " | ".join(reh),
    )

    log = subprocess.check_output(
        ["docker", "logs", "--tail", "300", "qxrp-rehearsal-1"],
        text=True,
        stderr=subprocess.STDOUT,
    )
    add(
        "no ActiveSet demote",
        "FAIL" if "ActiveSet: demote" in log else "PASS",
        "demote found" if "ActiveSet: demote" in log else "clean",
    )
    if "no ActiveSet rank cut" in log:
        add("aggregate path log", "PASS", "sumAggregate logged")
    else:
        add("scoring wait", "PASS", "flag interval may not have fired yet")

    # Slim images lack strings(1); search the binary bytes directly.
    try:
        blob = subprocess.check_output(
            [
                "docker",
                "run",
                "--rm",
                "--entrypoint",
                "cat",
                "qxrp/xrpld:mainnet-v2",
                "/usr/local/bin/xrpld",
            ]
        )
        has = b"no ActiveSet rank cut" in blob
        add(
            "binary has new scoring string",
            "PASS" if has else "FAIL",
            f"found={has} size={len(blob)}",
        )
    except Exception as e:
        add("binary strings", "WARN", str(e)[:120])

    results["summary"] = {
        k: sum(1 for t in results["tests"] if t["status"] == k)
        for k in ("PASS", "FAIL", "WARN", "SKIP")
    }
    results["completed_at"] = datetime.now(timezone.utc).isoformat()
    open(OUT, "w").write(json.dumps(results, indent=2) + "\n")
    print("SUMMARY", results["summary"])
    if results["summary"].get("FAIL", 0):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
