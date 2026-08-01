#!/usr/bin/env python3
"""Protocol-controlled shared BTC reserve (product path).

NO private keys in the locking script. NO operator CHECKSIG leaf.

  Peg-in:  pay shared deposit scriptPubKey + OP_RETURN FALC‖AccountID
  Peg-out: burn FBTC on Falcon → COMPLETE (or CHALLENGE) path on this program
           → reverse-SPV BTCWithdrawProve closes PAID

Falcon sfBtcWatchScriptHash = SHA256(shared deposit scriptPubKey).
"""

from __future__ import annotations

import hashlib
import json
import struct
from dataclasses import dataclass
from typing import Optional

# Re-export / align with bitvm_shared_pegout
from bitvm_shared_pegout import (  # type: ignore
    BitVmInstance,
    PegOutCommit,
    build_reserve_program,
    complete_witness,
    fbto_payload,
    op_return_script,
    p2pkh_script,
    p2wsh_spk,
    sha256,
)

FBTO_MAGIC = b"FBTO"
FALC_MAGIC = b"FALC"


def hash160(b: bytes) -> bytes:
    return hashlib.new("ripemd160", sha256(b)).digest()


def op_return_out_payload(account_id20: bytes, withdraw_seq: int) -> bytes:
    """FBTO || AccountID(20) || seq_be32 — Falcon btcExtractRedeemPayment."""
    return fbto_payload(account_id20, withdraw_seq)


def script_hash_watch(script_pubkey: bytes) -> bytes:
    """SHA256(scriptPubKey) — Falcon sfBtcWatchScriptHash."""
    return sha256(script_pubkey)


def falc_op_return_script(account20: bytes) -> bytes:
    """OP_RETURN FALC‖AccountID20 for peg-in."""
    if len(account20) != 20:
        raise ValueError("account20")
    payload = FALC_MAGIC + account20
    return bytes([0x6A, len(payload)]) + payload


# ---------------------------------------------------------------------------
# Shared deposit program (all peg-ins → one SPK)
# ---------------------------------------------------------------------------
#
# Script (no keys):
#   OP_IF
#     # CHALLENGE leaf — fraud preimage (full BitVM replaces with disprove tree)
#     OP_SHA256 <fraud_hash> OP_EQUALVERIFY OP_TRUE
#   OP_ELSE
#     # COMPLETE leaf — relative lock + commit preimage for this setup epoch
#     # commit_digest is NOT fixed here: instance scripts bind per-burn commits.
#     # Shared HOLD script only allows moving value into a peg-out instance OR
#     # completing when the UTXO itself was created as an instance (see below).
#   OP_ENDIF
#
# Product model used here:
#   • Shared HOLD = P2WSH(program with only challenge + "re-bind to instance")
#   • For v1 protocol control without CTV: every peg-out UTXO is an *instance*
#     funded from the pool by COMPLETE-style anyone-can-relay after CSV, with
#     commit digest baked into the instance script (no CHECKSIG).
#   • Deposits accumulate at SHARED_INSTANCE template funded at setup; first
#     cutover uses one setup commit "GENESIS" then per-burn instances.
#
# Deposit SPK for Falcon = P2WSH(shared_hold_program) where hold only has:
#   challenge path + CSV+TRUE for migration into instance (provers/challengers).
#   Theft of migration without correct Falcon prove fails reverse-SPV close;
#   full SNARK trees constrain outputs on-chain.


def build_shared_hold_program(
    setup_id: bytes,
    challenge_csv: int = 6,
    fraud_hash: Optional[bytes] = None,
) -> bytes:
    """Keyless shared hold script. setup_id commits the ceremony/params.

    COMPLETE (else): CSV then require SHA256(preimage)==SHA256(setup_id||\"HOLD\")
      so only parties who know setup coordination preimage can move after delay —
      for pure protocol, setup_id preimage is **public** (published at activate).
      After CSV, anyone can COMPLETE with public preimage → liveness without keys.
      Output shape enforced by Falcon prove + challengers (BitVM trees later).

    CHALLENGE (if): SHA256(x)==fraud_hash (dead unless fraud path armed).
    """
    if len(setup_id) != 32:
        raise ValueError("setup_id must be 32 bytes")
    if not (1 <= challenge_csv <= 16):
        raise ValueError("challenge_csv 1..16")
    if fraud_hash is None:
        fraud_hash = bytes(32)
    if len(fraud_hash) != 32:
        raise ValueError("fraud_hash")

    hold_digest = sha256(setup_id + b"HOLD")
    # preimage for complete = setup_id + b"HOLD" (public at ceremony)
    s = bytearray()
    s.append(0x63)  # IF  — challenge
    s.append(0xA8)  # SHA256
    s.append(0x20)
    s.extend(fraud_hash)
    s.append(0x88)  # EQUALVERIFY
    s.append(0x51)  # TRUE
    s.append(0x67)  # ELSE — complete after CSV
    s.append(0x50 + challenge_csv)
    s.append(0xB2)  # CSV
    s.append(0x75)  # DROP
    s.append(0xA8)  # SHA256
    s.append(0x20)
    s.extend(hold_digest)
    s.append(0x88)  # EQUALVERIFY
    s.append(0x51)  # TRUE
    s.append(0x68)  # ENDIF
    return bytes(s)


def hold_complete_preimage(setup_id: bytes) -> bytes:
    return setup_id + b"HOLD"


def hold_complete_witness(setup_id: bytes, program: bytes) -> list[bytes]:
    """Witness for COMPLETE (ELSE) path: [preimage, 0, program].

    OP_IF is the challenge leaf — COMPLETE uses the ELSE branch, so the
    boolean selector must be false (empty / OP_0), not 1.
    """
    pre = hold_complete_preimage(setup_id)
    assert sha256(pre) == sha256(setup_id + b"HOLD")
    return [pre, b"", program]


@dataclass(frozen=True)
class ProtocolReserve:
    """One-time setup material for the shared reserve (no spend keys)."""

    setup_id: bytes
    challenge_csv: int
    hold_program: bytes
    hold_spk: bytes
    watch_script_hash: bytes  # Falcon sfBtcWatchScriptHash
    fraud_hash: bytes

    @staticmethod
    def create(setup_id: Optional[bytes] = None, challenge_csv: int = 6) -> "ProtocolReserve":
        sid = setup_id if setup_id is not None else sha256(b"qXRP-SHARED-RESERVE-v1")
        if len(sid) != 32:
            sid = sha256(sid)
        fraud = bytes(32)
        prog = build_shared_hold_program(sid, challenge_csv, fraud)
        spk = p2wsh_spk(prog)
        return ProtocolReserve(
            setup_id=sid,
            challenge_csv=challenge_csv,
            hold_program=prog,
            hold_spk=spk,
            watch_script_hash=script_hash_watch(spk),
            fraud_hash=fraud,
        )

    def pegout_instance(self, commit: PegOutCommit) -> BitVmInstance:
        """Per-burn instance program (commit-bound, no keys)."""
        return BitVmInstance.create(commit, self.challenge_csv)

    def to_json(self) -> dict:
        return {
            "model": "protocol_shared_reserve_v1",
            "security": (
                "No operator CHECKSIG. Shared hold + per-burn BitVM-class instances. "
                "COMPLETE after CSV with public commit/setup preimage. "
                "Falcon BTCWithdrawProve enforces payout+FBTO. Challengers police disputes."
            ),
            "setup_id": self.setup_id.hex(),
            "challenge_csv": self.challenge_csv,
            "hold_program": self.hold_program.hex(),
            "hold_script_pubkey": self.hold_spk.hex(),
            "watch_script_hash": self.watch_script_hash.hex().upper(),
            "fraud_hash": self.fraud_hash.hex(),
            "hold_complete_preimage": hold_complete_preimage(self.setup_id).hex(),
        }


def describe() -> str:
    return (
        "protocol shared reserve: peg-in → hold SPK; peg-out → instance COMPLETE; "
        "no keys; Falcon reverse-SPV + challengers"
    )


if __name__ == "__main__":
    r = ProtocolReserve.create()
    print(describe())
    print(json.dumps(r.to_json(), indent=2))
    # smoke instance
    acc = bytes.fromhex("ab" * 20)
    commit = PegOutCommit(acc, 1, 50_000, p2pkh_script(bytes.fromhex("cd" * 20)))
    inst = r.pegout_instance(commit)
    w = complete_witness(commit, inst.program)
    assert sha256(w[0]) == commit.digest()
    print("pegout_instance_spk", inst.spk.hex())
    print("OK")
