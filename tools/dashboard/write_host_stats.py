#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
"""Write host disk / memory / ledger-db size for the public dashboard.

Run on the host (cron every minute). Output is public-safe capacity data only
(no secrets, keys, or process dumps).

Usage:
  write_host_stats.py --out /var/lib/qxrp-full/dashboard-data/host_stats.json \\
                      --ledger /var/lib/qxrp-full
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import time
from pathlib import Path
from typing import Any, Dict, Optional


def dir_size(path: Path) -> int:
    total = 0
    try:
        for root, _dirs, files in os.walk(path):
            for name in files:
                try:
                    total += (Path(root) / name).stat().st_size
                except OSError:
                    pass
    except OSError:
        return 0
    return total


def disk_usage(path: str = "/") -> Optional[Dict[str, Any]]:
    try:
        u = shutil.disk_usage(path)
    except OSError:
        return None
    total, used, free = int(u.total), int(u.used), int(u.free)
    if total <= 0:
        return None
    return {
        "path": path,
        "total_bytes": total,
        "used_bytes": used,
        "free_bytes": free,
        "used_percent": round(100.0 * used / total, 1),
    }


def meminfo() -> Dict[str, Any]:
    try:
        raw = Path("/proc/meminfo").read_text()
    except OSError:
        return {}
    kv: Dict[str, int] = {}
    for line in raw.splitlines():
        parts = line.replace(":", " ").split()
        if len(parts) >= 2 and parts[1].isdigit():
            kv[parts[0]] = int(parts[1]) * 1024
    total = kv.get("MemTotal")
    avail = kv.get("MemAvailable")
    if not total or avail is None:
        return {}
    used = max(0, total - avail)
    return {
        "total_bytes": total,
        "available_bytes": avail,
        "used_bytes": used,
        "used_percent": round(100.0 * used / total, 1),
    }


def container_image_info(names: list[str]) -> Dict[str, Any]:
    """Best-effort docker inspect for running xrpld container image/digest."""
    import subprocess

    for name in names:
        if not name:
            continue
        try:
            out = subprocess.check_output(
                [
                    "docker",
                    "inspect",
                    name,
                    "--format",
                    "{{.Config.Image}}|{{.Image}}",
                ],
                stderr=subprocess.DEVNULL,
                text=True,
                timeout=8,
            ).strip()
        except Exception:
            continue
        if not out or "|" not in out:
            continue
        cfg_image, image_id = out.split("|", 1)
        digest = None
        revision = None
        labels: Dict[str, str] = {}
        try:
            raw = subprocess.check_output(
                ["docker", "image", "inspect", image_id, "--format", "{{json .}}"],
                stderr=subprocess.DEVNULL,
                text=True,
                timeout=8,
            )
            meta = json.loads(raw)
            for d in meta.get("RepoDigests") or []:
                if "@sha256:" in d:
                    digest = d.split("@", 1)[1]
                    break
            labels = meta.get("Config", {}).get("Labels") or {}
            revision = labels.get("org.opencontainers.image.revision")
        except Exception:
            pass
        return {
            "container": name,
            "image": cfg_image,
            "image_id": image_id[:19] if image_id else None,
            "digest": digest,
            "revision": revision,
            "labels": {
                k: labels[k]
                for k in (
                    "falcon.names",
                    "falcon.scoring",
                    "falcon.pop",
                    "org.opencontainers.image.revision",
                )
                if k in labels
            }
            or None,
        }
    return {}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="host_stats.json path")
    ap.add_argument(
        "--ledger",
        action="append",
        default=[],
        help="Ledger data root(s) containing nudb/ and/or db/ (repeatable)",
    )
    ap.add_argument("--disk-path", default="/", help="Filesystem path for df (default /)")
    ap.add_argument(
        "--container",
        action="append",
        default=[],
        help="Docker container name(s) to inspect for image pin (first match wins)",
    )
    args = ap.parse_args()

    ledger_parts: Dict[str, int] = {}
    ledger_total = 0
    for root in args.ledger:
        base = Path(root)
        for name in ("nudb", "db", "data"):
            p = base / name
            if p.is_dir():
                key = f"{base.name}/{name}" if len(args.ledger) > 1 else name
                sz = dir_size(p)
                ledger_parts[key] = sz
                ledger_total += sz
        # Also count large debug.log under the root (ops signal).
        dbg = base / "debug.log"
        if dbg.is_file():
            try:
                sz = dbg.stat().st_size
                key = f"{base.name}/debug.log" if len(args.ledger) > 1 else "debug.log"
                ledger_parts[key] = sz
                # Do not add debug.log into "ledger_bytes" (chain data only).
            except OSError:
                pass

    containers = args.container or [
        "qxrp-full",
        "qxrp-val2",
        "qxrp-validator",
        "falcon-validator",
    ]
    xrpld = container_image_info(containers)

    payload = {
        "updated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "disk": disk_usage(args.disk_path),
        "memory": meminfo() or None,
        "ledger_bytes": ledger_total if ledger_parts else None,
        "ledger": ledger_parts or None,
        "xrpld": xrpld or None,
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(".tmp")
    tmp.write_text(json.dumps(payload, indent=2))
    tmp.replace(out)
    print(json.dumps({"ok": True, "out": str(out), "ledger_bytes": payload["ledger_bytes"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
