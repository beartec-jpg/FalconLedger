#!/usr/bin/env python3
# Copyright (c) 2026 Falcon Ledger / qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Decrypt falcon-ceremony-backup.enc.json produced by ceremony-keygen.py.

  python3 scripts/ops/ceremony-key-decrypt.py path/to/falcon-ceremony-backup.enc.json
  python3 scripts/ops/ceremony-key-decrypt.py backup.enc.json -o plain.json
  python3 scripts/ops/ceremony-key-decrypt.py backup.enc.json --addresses-only

Password via prompt or CEREMONY_PASSWORD env.
"""

from __future__ import annotations

import argparse
import base64
import getpass
import json
import os
import sys
from pathlib import Path
from typing import Any

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    from cryptography.hazmat.primitives.hashes import SHA256
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
except ImportError as e:  # pragma: no cover
    print("ERROR: need Python package 'cryptography'", file=sys.stderr)
    raise SystemExit(2) from e

BACKUP_TYPE = "falcon-mainnet-ceremony-backup"


def die(msg: str, code: int = 1) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    raise SystemExit(code)


def b64d(s: str) -> bytes:
    return base64.b64decode(s.encode("ascii"))


def derive_key(password: str, salt: bytes, iterations: int) -> bytes:
    kdf = PBKDF2HMAC(algorithm=SHA256(), length=32, salt=salt, iterations=iterations)
    return kdf.derive(password.encode("utf-8"))


def decrypt_file(doc: dict[str, Any], password: str) -> dict[str, Any]:
    if doc.get("type") != BACKUP_TYPE:
        die(f"unexpected type {doc.get('type')!r}")
    if not doc.get("encrypted"):
        die("file is not marked encrypted")
    payload = doc.get("payload") or {}
    for k in ("salt", "iv", "data"):
        if k not in payload:
            die(f"missing payload.{k}")
    iterations = int(doc.get("kdf_iterations") or 210_000)
    key = derive_key(password, b64d(payload["salt"]), iterations)
    try:
        raw = AESGCM(key).decrypt(b64d(payload["iv"]), b64d(payload["data"]), None)
    except Exception as e:
        die(f"decrypt failed (wrong password or corrupt file): {e}")
    return json.loads(raw.decode("utf-8"))


def main() -> None:
    ap = argparse.ArgumentParser(description="Decrypt Falcon ceremony backup JSON")
    ap.add_argument("backup", type=Path, help="falcon-ceremony-backup.enc.json")
    ap.add_argument("-o", "--output", type=Path, help="Write full plaintext JSON here (mode 0600)")
    ap.add_argument(
        "--addresses-only",
        action="store_true",
        help="Print public addresses / validator pubs only (no secrets)",
    )
    ap.add_argument(
        "--stdout-secrets",
        action="store_true",
        help="Print full plaintext JSON to stdout (dangerous — prefer -o)",
    )
    args = ap.parse_args()

    if not args.backup.is_file():
        die(f"not found: {args.backup}")

    doc = json.loads(args.backup.read_text())
    if args.addresses_only:
        safe = {
            "created_at": doc.get("created_at"),
            "network_id": doc.get("network_id"),
            "image_pin": doc.get("image_pin"),
            "wallets": doc.get("wallets"),
            "validators": doc.get("validators"),
        }
        print(json.dumps(safe, indent=2))
        return

    password = os.environ.get("CEREMONY_PASSWORD") or getpass.getpass("Ceremony backup password: ")
    plain = decrypt_file(doc, password)

    if args.output:
        args.output.write_text(json.dumps(plain, indent=2) + "\n")
        os.chmod(args.output, 0o600)
        print(f"Wrote {args.output} (mode 0600)", file=sys.stderr)
        print(
            "Shred when finished: shred -u "
            + str(args.output)
            + " 2>/dev/null || rm -f "
            + str(args.output),
            file=sys.stderr,
        )
        return

    if args.stdout_secrets:
        print(json.dumps(plain, indent=2))
        return

    # Default: summary without dumping secrets
    print(
        json.dumps(
            {
                "ok": True,
                "created_at": plain.get("created_at"),
                "network_id": plain.get("network_id"),
                "image_pin": plain.get("image_pin"),
                "wallets": [
                    {"role": w["role"], "account_id": w["account_id"]}
                    for w in plain.get("wallets", [])
                ],
                "validators": [
                    {
                        "role": v["role"],
                        "validation_public_key": (v.get("validation_public_key") or "")[:32]
                        + "…",
                    }
                    for v in plain.get("validators", [])
                ],
                "hint": "Re-run with -o ceremony-plain.json to write full secrets (0600)",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
