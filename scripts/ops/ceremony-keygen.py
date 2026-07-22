#!/usr/bin/env python3
# Copyright (c) 2026 Falcon Ledger / qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Mainnet ceremony key generation with password-encrypted JSON backup.

Generates (via freeze image / live node admin RPC):
  - GENESIS, AIRDROP, FAUCET, DEV Falcon wallets  (wallet_propose)
  - N validator identities                        (validation_create)

Writes under OUT_DIR (default /root/mainnet-ceremony-artifacts/ceremony-<ts>/):
  - falcon-ceremony-backup.enc.json   ← download this (secrets encrypted)
  - ADDRESSES.public.json             ← public only (safe-ish to keep)
  - unl-public.txt                    ← validator pubkeys for UNL
  - DOWNLOAD.txt                      ← how to scp + wipe

Nothing secret is written in plaintext. Decryption:
  python3 scripts/ops/ceremony-key-decrypt.py falcon-ceremony-backup.enc.json

Usage (on build/ceremony host with a running mainnet-v1 container admin RPC):

  # Interactive password (recommended):
  python3 scripts/ops/ceremony-keygen.py \\
    --container qxrp-rehearsal-1 \\
    --validators 5

  # Non-interactive (CI / automation only — prefer env, not argv):
  CEREMONY_PASSWORD='…' python3 scripts/ops/ceremony-keygen.py --validators 5

Requires: Python 3.10+, cryptography, curl inside container (or --rpc URL).
"""

from __future__ import annotations

import argparse
import base64
import getpass
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    from cryptography.hazmat.primitives.hashes import SHA256
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
except ImportError as e:  # pragma: no cover
    print("ERROR: need Python package 'cryptography' (pip install cryptography)", file=sys.stderr)
    raise SystemExit(2) from e

BACKUP_TYPE = "falcon-mainnet-ceremony-backup"
BACKUP_VERSION = 1
KDF_ITERATIONS = 210_000
MIN_PASSWORD_LEN = 12

WALLET_ROLES = ("GENESIS", "AIRDROP", "FAUCET", "DEV")


def die(msg: str, code: int = 1) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    raise SystemExit(code)


def validate_password(pw: str) -> str | None:
    if len(pw) < MIN_PASSWORD_LEN:
        return f"Password must be at least {MIN_PASSWORD_LEN} characters"
    classes = sum(
        [
            bool(re.search(r"[a-z]", pw)),
            bool(re.search(r"[A-Z]", pw)),
            bool(re.search(r"[0-9]", pw)),
            bool(re.search(r"[^A-Za-z0-9]", pw)),
        ]
    )
    if classes < 3:
        return "Password must use at least 3 of: lowercase, uppercase, digits, symbols"
    return None


def b64(data: bytes) -> str:
    return base64.b64encode(data).decode("ascii")


def derive_key(password: str, salt: bytes) -> bytes:
    kdf = PBKDF2HMAC(
        algorithm=SHA256(),
        length=32,
        salt=salt,
        iterations=KDF_ITERATIONS,
    )
    return kdf.derive(password.encode("utf-8"))


def encrypt_payload(password: str, payload: dict[str, Any]) -> dict[str, str]:
    salt = os.urandom(32)
    iv = os.urandom(12)
    key = derive_key(password, salt)
    ct = AESGCM(key).encrypt(iv, json.dumps(payload, separators=(",", ":")).encode("utf-8"), None)
    return {"salt": b64(salt), "iv": b64(iv), "data": b64(ct)}


def rpc_via_container(container: str, method: str, params: dict | None = None) -> dict[str, Any]:
    body = json.dumps({"method": method, "params": [params or {}]})
    cmd = [
        "docker",
        "exec",
        container,
        "curl",
        "-sS",
        "--max-time",
        "30",
        "-H",
        "Content-Type: application/json",
        "-d",
        body,
        "http://127.0.0.1:5005",
    ]
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as e:
        die(f"docker exec RPC failed: {e.output}")
    except FileNotFoundError:
        die("docker not found — run on a host with Docker and the freeze container")
    try:
        doc = json.loads(out)
    except json.JSONDecodeError:
        die(f"non-JSON RPC response: {out[:300]}")
    if "error" in doc or "error" in doc.get("result", {}):
        die(f"RPC error for {method}: {doc}")
    result = doc.get("result") or doc
    if result.get("status") == "error":
        die(f"RPC status error for {method}: {result}")
    return result


def rpc_via_url(url: str, method: str, params: dict | None = None) -> dict[str, Any]:
    import urllib.request

    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(
        url, data=body, headers={"Content-Type": "application/json"}, method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            doc = json.loads(resp.read().decode())
    except Exception as e:
        die(f"RPC to {url} failed: {e}")
    result = doc.get("result") or doc
    if result.get("status") == "error" or "error" in doc:
        die(f"RPC error for {method}: {doc}")
    return result


def make_rpc(args: argparse.Namespace):
    if args.rpc:
        return lambda method, params=None: rpc_via_url(args.rpc, method, params)
    if not args.container:
        die("provide --container (e.g. qxrp-rehearsal-1) or --rpc http://127.0.0.1:5005")
    return lambda method, params=None: rpc_via_container(args.container, method, params)


def propose_wallet(rpc, role: str) -> dict[str, Any]:
    r = rpc("wallet_propose", {})
    secret = r.get("falcon_secret")
    account = r.get("account_id")
    if not secret or not account:
        die(f"wallet_propose incomplete for {role}: keys={list(r.keys())}")
    if len(secret) < 1000:
        die(f"wallet_propose secret too short for {role}")
    return {
        "role": role,
        "account_id": account,
        "falcon_secret": secret,
        "key_type": r.get("key_type") or "falcon512",
        "public_key_hex": r.get("public_key_hex") or r.get("public_key") or "",
    }


def create_validator(rpc, index: int) -> dict[str, Any]:
    r = rpc("validation_create", {})
    secret = r.get("falcon_secret")
    pub = r.get("validation_public_key") or r.get("validation_public_key_hex")
    if not secret or not pub:
        die(f"validation_create incomplete for VAL{index}: keys={list(r.keys())}")
    return {
        "role": f"VAL{index}",
        "validation_public_key": pub,
        "validation_public_key_hex": r.get("validation_public_key_hex") or pub,
        "falcon_secret": secret,
        "key_type": r.get("key_type") or "falcon512",
        # consensus key may equal validation key for Falcon-native fleet
        "label": f"mainnet-validator-{index}",
    }


def read_image_digest(path: Path | None) -> str:
    if path and path.is_file():
        for line in path.read_text().splitlines():
            line = line.strip()
            if line.startswith("qxrp/xrpld@"):
                return line
    return "qxrp/xrpld:mainnet-v1"


def main() -> None:
    ap = argparse.ArgumentParser(description="Generate password-encrypted mainnet ceremony keys")
    ap.add_argument(
        "--container",
        default=os.environ.get("CEREMONY_CONTAINER", "qxrp-rehearsal-1"),
        help="Docker container with admin RPC on :5005 (default qxrp-rehearsal-1)",
    )
    ap.add_argument(
        "--rpc",
        default=os.environ.get("CEREMONY_RPC", ""),
        help="Admin JSON-RPC URL (overrides --container), e.g. http://127.0.0.1:5005",
    )
    ap.add_argument(
        "--validators",
        type=int,
        default=5,
        help="Number of validator identities to create (default 5)",
    )
    ap.add_argument(
        "--network-id",
        type=int,
        default=int(os.environ.get("CEREMONY_NETWORK_ID", "1026")),
        help="Mainnet network id recorded in backup (default 1026)",
    )
    ap.add_argument(
        "--out-dir",
        default="",
        help="Output directory (default /root/mainnet-ceremony-artifacts/ceremony-<utc-ts>)",
    )
    ap.add_argument(
        "--image-digest-file",
        default="",
        help="Optional IMAGE_DIGEST.txt to embed pin in backup metadata",
    )
    ap.add_argument(
        "--label",
        default="mainnet-T0",
        help="Human label stored in backup (not secret)",
    )
    args = ap.parse_args()

    if args.validators < 1 or args.validators > 32:
        die("--validators must be 1..32")

    password = os.environ.get("CEREMONY_PASSWORD")
    if password:
        print("Using CEREMONY_PASSWORD from environment", file=sys.stderr)
    else:
        password = getpass.getpass("Ceremony backup password: ")
        password2 = getpass.getpass("Confirm password: ")
        if password != password2:
            die("passwords do not match")
    err = validate_password(password)
    if err:
        die(err)

    rpc = make_rpc(args)
    # smoke one call
    try:
        rpc("server_info", {})
    except SystemExit:
        raise
    except Exception:
        pass  # server_info may be restricted; wallet_propose will fail hard if dead

    print(f"Generating {len(WALLET_ROLES)} wallets + {args.validators} validators…", file=sys.stderr)

    wallets_plain: list[dict[str, Any]] = []
    for role in WALLET_ROLES:
        w = propose_wallet(rpc, role)
        wallets_plain.append(w)
        print(f"  {role}: {w['account_id']}", file=sys.stderr)
        time.sleep(0.05)

    validators_plain: list[dict[str, Any]] = []
    for i in range(1, args.validators + 1):
        v = create_validator(rpc, i)
        validators_plain.append(v)
        print(f"  VAL{i}: pubkey…{v['validation_public_key'][:24]}…", file=sys.stderr)
        time.sleep(0.05)

    created = datetime.now(timezone.utc)
    digest_path = Path(args.image_digest_file) if args.image_digest_file else None
    if not digest_path:
        # best-effort from repo relative to script
        cand = Path(__file__).resolve().parents[1] / "mainnet-ceremony" / "IMAGE_DIGEST.txt"
        if cand.is_file():
            digest_path = cand
    image_pin = read_image_digest(digest_path)

    secret_payload = {
        "version": BACKUP_VERSION,
        "type": BACKUP_TYPE,
        "created_at": created.isoformat(),
        "label": args.label,
        "network_id": args.network_id,
        "image_pin": image_pin,
        "wallets": wallets_plain,
        "validators": validators_plain,
        "notes": [
            "REAL mainnet ceremony material — not for rehearsal reuse",
            "Decrypt only on a trusted machine at T0",
            "After download, prefer shredding server copy or leave only .enc.json",
        ],
    }

    enc_blob = encrypt_payload(password, secret_payload)
    # wipe password from process ASAP (best-effort)
    password = ""  # noqa: F841

    public = {
        "version": BACKUP_VERSION,
        "type": BACKUP_TYPE,
        "encrypted": True,
        "created_at": created.isoformat(),
        "label": args.label,
        "network_id": args.network_id,
        "image_pin": image_pin,
        "wallets": [
            {"role": w["role"], "account_id": w["account_id"], "key_type": w["key_type"]}
            for w in wallets_plain
        ],
        "validators": [
            {
                "role": v["role"],
                "validation_public_key": v["validation_public_key"],
                "label": v["label"],
            }
            for v in validators_plain
        ],
    }

    backup_file = {
        **public,
        "kdf": "PBKDF2-HMAC-SHA256",
        "kdf_iterations": KDF_ITERATIONS,
        "cipher": "AES-256-GCM",
        "payload": enc_blob,
    }

    ts = created.strftime("%Y%m%dT%H%M%SZ")
    if args.out_dir:
        out = Path(args.out_dir)
    else:
        base = Path("/root/mainnet-ceremony-artifacts")
        if not base.is_dir():
            base = Path.cwd() / "mainnet-ceremony-artifacts"
        out = base / f"ceremony-{ts}"
    out.mkdir(parents=True, mode=0o700, exist_ok=True)
    os.chmod(out, 0o700)

    enc_path = out / "falcon-ceremony-backup.enc.json"
    pub_path = out / "ADDRESSES.public.json"
    unl_path = out / "unl-public.txt"
    dl_path = out / "DOWNLOAD.txt"

    enc_path.write_text(json.dumps(backup_file, indent=2) + "\n")
    os.chmod(enc_path, 0o600)
    pub_path.write_text(json.dumps(public, indent=2) + "\n")
    os.chmod(pub_path, 0o644)

    unl_lines = [
        f"# Falcon mainnet UNL public keys — generated {created.isoformat()}",
        f"# network_id={args.network_id}  image={image_pin}",
        "# Paste into validators.txt / UNL publisher (public only)",
        "",
    ]
    for v in validators_plain:
        unl_lines.append(v["validation_public_key"])
    unl_path.write_text("\n".join(unl_lines) + "\n")

    host = os.uname().nodename
    dl_path.write_text(
        f"""Falcon mainnet ceremony backup — {created.isoformat()}

DOWNLOAD (from your laptop):
  scp root@{host}:{enc_path} ./
  scp root@{host}:{pub_path} ./

DECRYPT (when you need secrets at T0):
  python3 scripts/ops/ceremony-key-decrypt.py falcon-ceremony-backup.enc.json -o ceremony-plain.json
  # or print wallets only to stdout after password prompt

AFTER DOWNLOAD — pick one:
  A) Leave only encrypted file on server (OK if password is strong and host is trusted)
     rm -f is not needed for .enc.json
  B) Remove everything from server after you have local copies + offline backup:
     shred -u {enc_path} 2>/dev/null || rm -f {enc_path}
     rm -rf {out}

NEVER commit falcon-ceremony-backup.enc.json or decrypted JSON to git.
Public addresses file is non-secret but still operationally sensitive (targets).

Network ID: {args.network_id}
Image pin:  {image_pin}
Validators: {args.validators}
"""
    )

    # zeroize plain secrets from the structure (best-effort)
    for w in wallets_plain:
        w["falcon_secret"] = ""
    for v in validators_plain:
        v["falcon_secret"] = ""

    print(json.dumps({"ok": True, "out_dir": str(out), "encrypted": str(enc_path), "public": str(pub_path)}, indent=2))
    print(
        f"\nDownload:\n  scp root@<server>:{enc_path} ./\n",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
