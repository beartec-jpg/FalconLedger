#!/usr/bin/env python3
"""Minimal Bitcoin tx helpers for protocol reserve P2WSH (regtest/testnet).

No external deps. Build + serialize witness txs for keyless COMPLETE spends.
"""

from __future__ import annotations

import hashlib
import struct
from dataclasses import dataclass
from typing import List, Optional, Sequence, Tuple


def sha256(b: bytes) -> bytes:
    return hashlib.sha256(b).digest()


def hash256(b: bytes) -> bytes:
    return sha256(sha256(b))


def encode_varint(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", n)
    if n <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", n)
    return b"\xff" + struct.pack("<Q", n)


def encode_push(data: bytes) -> bytes:
    n = len(data)
    if n < 0x4C:
        return bytes([n]) + data
    if n <= 0xFF:
        return b"\x4c" + bytes([n]) + data
    if n <= 0xFFFF:
        return b"\x4d" + struct.pack("<H", n) + data
    return b"\x4e" + struct.pack("<I", n) + data


@dataclass
class TxOut:
    value: int
    script_pubkey: bytes

    def serialize(self) -> bytes:
        return struct.pack("<Q", self.value) + encode_varint(len(self.script_pubkey)) + self.script_pubkey


@dataclass
class TxIn:
    txid: bytes  # internal byte order (display reversed for hex id)
    vout: int
    script_sig: bytes = b""
    sequence: int = 0xFFFFFFFF
    witness: Optional[List[bytes]] = None

    def serialize_unsigned(self) -> bytes:
        return (
            self.txid
            + struct.pack("<I", self.vout)
            + encode_varint(len(self.script_sig))
            + self.script_sig
            + struct.pack("<I", self.sequence)
        )


def txid_from_hex(txid_hex: str) -> bytes:
    return bytes.fromhex(txid_hex)[::-1]


def txid_to_hex(txid: bytes) -> str:
    return txid[::-1].hex()


@dataclass
class Transaction:
    version: int
    inputs: List[TxIn]
    outputs: List[TxOut]
    locktime: int = 0

    def serialize(self, with_witness: bool = True) -> bytes:
        has_wit = with_witness and any(i.witness is not None for i in self.inputs)
        out = bytearray()
        out += struct.pack("<I", self.version)
        if has_wit:
            out += b"\x00\x01"  # marker + flag
        out += encode_varint(len(self.inputs))
        for i in self.inputs:
            out += i.serialize_unsigned()
        out += encode_varint(len(self.outputs))
        for o in self.outputs:
            out += o.serialize()
        if has_wit:
            for i in self.inputs:
                w = i.witness if i.witness is not None else []
                out += encode_varint(len(w))
                for item in w:
                    out += encode_varint(len(item)) + item
        out += struct.pack("<I", self.locktime)
        return bytes(out)

    def txid(self) -> bytes:
        # non-witness serialization
        raw = self.serialize(with_witness=False)
        return hash256(raw)

    def wtxid(self) -> bytes:
        return hash256(self.serialize(with_witness=True))


def build_p2wsh_complete_tx(
    *,
    prev_txid_hex: str,
    prev_vout: int,
    prev_amount: int,
    prev_spk: bytes,
    witness_stack: Sequence[bytes],
    outputs: Sequence[Tuple[int, bytes]],
    sequence: int,
    version: int = 2,
    locktime: int = 0,
) -> Transaction:
    """Build a single-input P2WSH spend (script path = witness stack ending with program)."""
    tin = TxIn(
        txid=txid_from_hex(prev_txid_hex),
        vout=prev_vout,
        script_sig=b"",
        sequence=sequence,
        witness=list(witness_stack),
    )
    touts = [TxOut(v, spk) for v, spk in outputs]
    return Transaction(version=version, inputs=[tin], outputs=touts, locktime=locktime)


# --- bech32 (segwit v0) for P2WSH address ---

_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"


def _bech32_polymod(values: List[int]) -> int:
    gens = [0x3B6A57B2, 0x26508E6D, 0x1EA119FA, 0x3D4233DD, 0x2A1462B3]
    chk = 1
    for v in values:
        b = chk >> 25
        chk = ((chk & 0x1FFFFFF) << 5) ^ v
        for i in range(5):
            chk ^= gens[i] if ((b >> i) & 1) else 0
    return chk


def _bech32_hrp_expand(hrp: str) -> List[int]:
    return [ord(x) >> 5 for x in hrp] + [0] + [ord(x) & 31 for x in hrp]


def _bech32_create_checksum(hrp: str, data: List[int]) -> List[int]:
    values = _bech32_hrp_expand(hrp) + data
    polymod = _bech32_polymod(values + [0, 0, 0, 0, 0, 0]) ^ 1
    return [(polymod >> 5 * (5 - i)) & 31 for i in range(6)]


def _convertbits(data: bytes, frombits: int, tobits: int, pad: bool = True) -> List[int]:
    acc = 0
    bits = 0
    ret: List[int] = []
    maxv = (1 << tobits) - 1
    for b in data:
        acc = (acc << frombits) | b
        bits += frombits
        while bits >= tobits:
            bits -= tobits
            ret.append((acc >> bits) & maxv)
    if pad and bits:
        ret.append((acc << (tobits - bits)) & maxv)
    return ret


def encode_p2wsh_address(spk: bytes, network: str = "regtest") -> str:
    """spk = 0x0020 || 32-byte witness program."""
    if len(spk) != 34 or spk[0] != 0x00 or spk[1] != 0x20:
        raise ValueError("expected P2WSH scriptPubKey")
    prog = spk[2:]
    hrp = {"mainnet": "bc", "testnet": "tb", "regtest": "bcrt"}.get(network, "bcrt")
    data = [0] + _convertbits(prog, 8, 5)
    combined = data + _bech32_create_checksum(hrp, data)
    return hrp + "1" + "".join(_CHARSET[d] for d in combined)
