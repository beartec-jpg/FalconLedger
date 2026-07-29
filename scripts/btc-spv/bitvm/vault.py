#!/usr/bin/env python3
"""BitVM-class vault helpers for Bitcoin regtest (CSV + hashlock).

This is a testable prototype of the peg-out path:
  - Funds locked in P2WSH
  - After relative locktime (CSV), spend with SHA256(preimage)==commit + user sig
  - No custodial multisig

Requires bitcoin-cli on PATH (regtest).
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import struct
import sys
from dataclasses import dataclass
from typing import Optional


def sha256(b: bytes) -> bytes:
    return hashlib.sha256(b).digest()


def bitcoin_cli(*args: str, wallet: Optional[str] = None) -> str:
    cmd = ["bitcoin-cli", "-regtest"]
    if wallet:
        cmd += [f"-rpcwallet={wallet}"]
    cmd += list(args)
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"bitcoin-cli failed: {' '.join(cmd)}\n{r.stderr}")
    return r.stdout.strip()


def ensure_wallet(name: str = "bitvm") -> None:
    """Load or create a named wallet. Idempotent if already loaded."""
    try:
        wlist = json.loads(bitcoin_cli("listwallets") or "[]")
        if name in wlist:
            return
    except RuntimeError:
        pass
    try:
        bitcoin_cli("loadwallet", name)
        return
    except RuntimeError as e:
        err = str(e)
        # -35 already loaded / already exists in memory
        if "already loaded" in err.lower():
            return
    try:
        bitcoin_cli("createwallet", name)
        return
    except RuntimeError as e:
        err = str(e)
        if "already exists" in err.lower() or "Database already exists" in err:
            # On disk but not loaded (or race) — load again
            try:
                bitcoin_cli("loadwallet", name)
                return
            except RuntimeError as e2:
                if "already loaded" in str(e2).lower():
                    return
                raise
        raise


@dataclass
class VaultParams:
    csv_blocks: int
    burn_commit: bytes  # 32-byte SHA256
    user_pubkey: bytes  # 33-byte compressed
    preimage: bytes


def build_vault_script(p: VaultParams) -> bytes:
    """Minimal BitVM-class redeem script (CSV + hashlock + CHECKSIG).

    OP_IF
      OP_SHA256 <fraud> OP_EQUALVERIFY OP_TRUE   # stub challenge path
    OP_ELSE
      <csv> OP_CHECKSEQUENCEVERIFY OP_DROP
      OP_SHA256 <commit> OP_EQUALVERIFY
      <pubkey> OP_CHECKSIG
    OP_ENDIF
    """
    if len(p.burn_commit) != 32:
        raise ValueError("burn_commit must be 32 bytes")
    if len(p.user_pubkey) not in (33, 65):
        raise ValueError("user_pubkey unexpected length")

    # For prototype, fraud path uses a fixed never-used hash (zeros) so IF branch
    # is effectively dead unless someone knows SHA256(x)==0^32 (they don't).
    fraud = bytes(32)

    script = bytearray()
    # OP_IF
    script.append(0x63)
    # OP_SHA256
    script.append(0xA8)
    script.append(0x20)
    script.extend(fraud)
    # OP_EQUALVERIFY
    script.append(0x88)
    # OP_TRUE (1)
    script.append(0x51)
    # OP_ELSE
    script.append(0x67)
    # CSV relative locktime (MINIMALDATA: 1..16 must be OP_1..OP_16, not PUSHDATA)
    n = p.csv_blocks
    if 1 <= n <= 16:
        script.append(0x50 + n)  # OP_1 .. OP_16
    elif n == 0:
        script.append(0x00)  # OP_0
    elif n < 0x100:
        script.append(0x01)
        script.append(n & 0xFF)
    else:
        script.append(0x02)
        script.extend(struct.pack("<H", n))
    # OP_CHECKSEQUENCEVERIFY (0xb2)
    script.append(0xB2)
    # OP_DROP
    script.append(0x75)
    # OP_SHA256
    script.append(0xA8)
    script.append(0x20)
    script.extend(p.burn_commit)
    # OP_EQUALVERIFY
    script.append(0x88)
    # push pubkey
    script.append(len(p.user_pubkey))
    script.extend(p.user_pubkey)
    # OP_CHECKSIG
    script.append(0xAC)
    # OP_ENDIF
    script.append(0x68)
    return bytes(script)


def script_to_p2wsh_address(script: bytes) -> str:
    """Derive regtest P2WSH address from redeem script (Bitcoin Core 24+ / 28)."""
    # Core 28 rejects wsh(raw(...)) nested; decodescript exposes segwit address.
    try:
        info = json.loads(bitcoin_cli("decodescript", script.hex()))
        seg = info.get("segwit") or {}
        addr = seg.get("address")
        if addr:
            return addr
    except RuntimeError:
        pass
    # Fallback: descriptor path on older cores that still accept wsh(raw())
    desc = f"wsh(raw({script.hex()}))"
    try:
        info = json.loads(bitcoin_cli("getdescriptorinfo", desc))
        desc_c = info["descriptor"]
        addrs = json.loads(bitcoin_cli("deriveaddresses", desc_c))
        return addrs[0]
    except Exception as e:
        raise RuntimeError(
            f"Could not derive P2WSH address: {e}\nscript_hex={script.hex()}"
        ) from e


def mine(blocks: int, to_address: Optional[str] = None) -> None:
    if to_address is None:
        to_address = bitcoin_cli("getnewaddress", wallet="bitvm")
    bitcoin_cli("generatetoaddress", str(blocks), to_address, wallet="bitvm")


def fund_address(addr: str, btc: float = 1.0) -> str:
    txid = bitcoin_cli("sendtoaddress", addr, str(btc), wallet="bitvm")
    mine(1)
    return txid


def make_preimage(label: str = "falcon-burn") -> bytes:
    return sha256(label.encode() + os.urandom(16))


def commit_of(preimage: bytes) -> bytes:
    return sha256(preimage)


def demo_local_vault(csv_blocks: int = 6) -> dict:
    """Create keys, vault, fund, mine CSV, document claim path (signing via core)."""
    ensure_wallet("bitvm")
    # Mine some coinbase maturity
    addr = bitcoin_cli("getnewaddress", wallet="bitvm")
    mine(101, addr)

    # New address for user claim key
    user_addr = bitcoin_cli("getnewaddress", "", "bech32", wallet="bitvm")
    # getaddressinfo for pubkey
    info = json.loads(bitcoin_cli("getaddressinfo", user_addr, wallet="bitvm"))
    pubkey = bytes.fromhex(info["pubkey"])

    preimage = make_preimage()
    commit = commit_of(preimage)
    params = VaultParams(
        csv_blocks=csv_blocks, burn_commit=commit, user_pubkey=pubkey, preimage=preimage
    )
    script = build_vault_script(params)
    vault_addr = script_to_p2wsh_address(script)

    txid = fund_address(vault_addr, 1.0)
    # Mine CSV blocks
    mine(csv_blocks + 1)

    return {
        "vault_address": vault_addr,
        "funding_txid": txid,
        "script_hex": script.hex(),
        "burn_commit": commit.hex(),
        "preimage": preimage.hex(),
        "user_address": user_addr,
        "user_pubkey": pubkey.hex(),
        "csv_blocks": csv_blocks,
        "note": "Claim: spend vault after CSV with preimage + user signature (see e2e_regtest.py)",
    }


if __name__ == "__main__":
    out = demo_local_vault(int(os.environ.get("CSV_BLOCKS", "6")))
    print(json.dumps(out, indent=2))
