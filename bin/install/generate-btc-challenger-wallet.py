#!/usr/bin/env python3
"""Generate a Bitcoin fee-only wallet for the BitVM challenger (NOT the reserve).

Uses openssl for secp256k1; stdlib for address encoding.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile


B58 = b"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"


def b58encode(data: bytes) -> str:
    n = int.from_bytes(data, "big")
    out = bytearray()
    while n > 0:
        n, r = divmod(n, 58)
        out.append(B58[r])
    pad = sum(1 for b in data if b == 0)
    # count leading zero bytes
    pad = 0
    for b in data:
        if b == 0:
            pad += 1
        else:
            break
    return (B58[0:1] * pad + bytes(out[::-1])).decode()


def b58check(payload: bytes) -> str:
    chk = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    return b58encode(payload + chk)


def hash160(data: bytes) -> bytes:
    return hashlib.new("ripemd160", hashlib.sha256(data).digest()).digest()


def openssl_secp256k1() -> tuple[bytes, bytes]:
    """Return (priv32, uncompressed_pub65)."""
    with tempfile.TemporaryDirectory() as td:
        pem = os.path.join(td, "k.pem")
        subprocess.check_call(
            ["openssl", "ecparam", "-name", "secp256k1", "-genkey", "-noout", "-out", pem],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        text = subprocess.check_output(
            ["openssl", "ec", "-in", pem, "-text", "-noout"],
            stderr=subprocess.DEVNULL,
        ).decode()
    priv_m = re.search(r"priv:\s*((?:[0-9a-fA-F]{2}:?\s*)+)", text)
    pub_m = re.search(r"pub:\s*((?:[0-9a-fA-F]{2}:?\s*)+)", text)
    if not priv_m or not pub_m:
        raise RuntimeError("openssl parse failed")
    priv_hex = re.sub(r"[^0-9a-fA-F]", "", priv_m.group(1))
    pub_hex = re.sub(r"[^0-9a-fA-F]", "", pub_m.group(1))
    priv = bytes.fromhex(priv_hex)
    if len(priv) < 32:
        priv = priv.rjust(32, b"\x00")
    elif len(priv) > 32:
        priv = priv[-32:]
    pub = bytes.fromhex(pub_hex)
    if len(pub) != 65 or pub[0] != 0x04:
        raise RuntimeError(f"unexpected pub len {len(pub)}")
    return priv, pub


def compress_pub(pub65: bytes) -> bytes:
    x, y = pub65[1:33], pub65[33:65]
    prefix = b"\x02" if (y[-1] % 2 == 0) else b"\x03"
    return prefix + x


def wif(priv32: bytes, *, testnet: bool, compressed: bool = True) -> str:
    prefix = b"\xef" if testnet else b"\x80"
    payload = prefix + priv32 + (b"\x01" if compressed else b"")
    return b58check(payload)


def p2pkh_address(pub_compressed: bytes, *, testnet: bool) -> str:
    ver = b"\x6f" if testnet else b"\x00"
    return b58check(ver + hash160(pub_compressed))


def main() -> int:
    out_path = sys.argv[1] if len(sys.argv) > 1 else ""
    priv, pub_u = openssl_secp256k1()
    pub = compress_pub(pub_u)
    data = {
        "v": 1,
        "role": "bitvm_challenger_fee_wallet",
        "note": (
            "FEE ONLY — pays Bitcoin challenge tx fees. "
            "Does NOT control the shared BTC reserve. Never use as custody key."
        ),
        "private_key_hex": priv.hex(),
        "wif_testnet": wif(priv, testnet=True),
        "wif_mainnet": wif(priv, testnet=False),
        "address_testnet": p2pkh_address(pub, testnet=True),
        "address_mainnet": p2pkh_address(pub, testnet=False),
        "network_default": "testnet",
        "suggested_float_testnet_btc": "0.001",
        "suggested_float_mainnet_btc": "0.001",
        "cost_note": (
            "Day-to-day cost ~0 if no attacks. Hold a small float for rare challenge fees "
            "(testnet: faucet dust; mainnet: ~0.001–0.005 BTC recommended buffer)."
        ),
    }
    text = json.dumps(data, indent=2)
    if out_path:
        with open(out_path, "w") as f:
            f.write(text + "\n")
        os.chmod(out_path, 0o600)
        print(out_path)
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
