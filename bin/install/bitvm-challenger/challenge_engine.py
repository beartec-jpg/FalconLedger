#!/usr/bin/env python3
"""
Protocol challenger engine — acts on dodgy peg-outs (no reserve spend keys).

What "challenger works" means here (pre–BitVM2 SNARK trees):

  1) Commit binding: wrong burn digest cannot unlock an instance.
  2) Race defense: after CSV, unlock knowledge is public (burn is public).
     A thief can COMPLETE with *wrong outputs*; an honest challenger races
     the *same* unlock with *correct* outputs + higher fee (RBF / replace).
  3) Auto-redeem: for PENDING Falcon burns, after CSV, broadcast honest COMPLETE
     so users are not stuck waiting for a private operator.
  4) Detect: flag confirmed spends that do not match any PENDING/PAID burn.

Full multi-round BitVM disprove trees remain future hardening. This engine is
the permissionless defense that works with current Bitcoin script.
"""

from __future__ import annotations

import hashlib
import json
import struct
import time
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

from bitvm_shared_pegout import (
    BitVmInstance,
    PegOutCommit,
    build_complete_outputs,
    complete_witness,
    fbto_payload,
    op_return_script,
    sha256,
)
from btc_tx import build_p2wsh_complete_tx, encode_p2wsh_address
from shared_reserve import ProtocolReserve, hold_complete_witness


def classic_to_account20(addr: str) -> bytes:
    alphabet = "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz"
    n = 0
    for c in addr:
        n = n * 58 + alphabet.index(c)
    raw = n.to_bytes((n.bit_length() + 7) // 8 or 1, "big")
    raw = b"\x00" * (25 - len(raw)) + raw
    return raw[1:21]


def commit_from_burn(
    account_r: str,
    withdraw_seq: int,
    amount_sats: int,
    payout_script_hex: str,
) -> PegOutCommit:
    return PegOutCommit(
        classic_to_account20(account_r),
        withdraw_seq,
        amount_sats,
        bytes.fromhex(payout_script_hex.replace("0x", "")),
    )


@dataclass
class PendingBurn:
    account: str
    withdraw_seq: int
    amount_sats: int
    payout_script_hex: str
    status: int  # 0 pending

    def commit(self) -> PegOutCommit:
        return commit_from_burn(
            self.account, self.withdraw_seq, self.amount_sats, self.payout_script_hex
        )


@dataclass
class UtxoRef:
    txid: str
    vout: int
    value_sats: int
    spk_hex: str


@dataclass
class ChallengerAction:
    kind: str  # honest_complete | race_replace | dodgy_detected | skip
    detail: str
    rawtx_hex: Optional[str] = None
    burn: Optional[PendingBurn] = None


@dataclass
class ChallengerEngine:
    """Pure logic; BTC broadcast injected via callbacks for regtest/live."""

    reserve: ProtocolReserve
    challenge_csv: int = 6
    fee_sats: int = 1500
    race_fee_sats: int = 3000  # higher fee to replace dodgy

    def instance_for(self, burn: PendingBurn) -> BitVmInstance:
        return self.reserve.pegout_instance(burn.commit())

    def build_honest_complete(
        self,
        burn: PendingBurn,
        utxo: UtxoRef,
        *,
        fee: Optional[int] = None,
        use_hold_program: bool = False,
    ) -> str:
        """Build COMPLETE raw tx paying the burner correctly + FBTO."""
        fee = fee if fee is not None else self.fee_sats
        commit = burn.commit()
        if use_hold_program:
            prog = self.reserve.hold_program
            spk = self.reserve.hold_spk
            wit = hold_complete_witness(self.reserve.setup_id, prog)
        else:
            inst = self.instance_for(burn)
            prog = inst.program
            spk = inst.spk
            wit = complete_witness(commit, prog)

        if utxo.value_sats < commit.amount_sats + fee:
            raise ValueError("utxo too small for amount+fee")

        change = utxo.value_sats - commit.amount_sats - fee
        outs: list[tuple[int, bytes]] = [
            (commit.amount_sats, commit.payout_script),
            (0, op_return_script(fbto_payload(commit.account20, commit.withdraw_seq))),
        ]
        if change >= 546:
            outs.append((change, self.reserve.hold_spk))

        tx = build_p2wsh_complete_tx(
            prev_txid_hex=utxo.txid,
            prev_vout=utxo.vout,
            prev_amount=utxo.value_sats,
            prev_spk=spk,
            witness_stack=wit,
            outputs=outs,
            sequence=self.challenge_csv,
        )
        return tx.serialize(with_witness=True).hex()

    def build_dodgy_steal(
        self,
        burn: PendingBurn,
        utxo: UtxoRef,
        thief_spk: bytes,
        *,
        use_hold_program: bool = False,
    ) -> str:
        """Attacker COMPLETE: same unlock, wrong payout (no matching FBTO/burn)."""
        commit = burn.commit()
        if use_hold_program:
            prog = self.reserve.hold_program
            spk = self.reserve.hold_spk
            wit = hold_complete_witness(self.reserve.setup_id, prog)
        else:
            inst = self.instance_for(burn)
            prog = inst.program
            spk = inst.spk
            wit = complete_witness(commit, prog)

        fee = self.fee_sats
        amount = utxo.value_sats - fee
        if amount < 546:
            raise ValueError("too small")
        outs = [(amount, thief_spk)]  # steal — no FBTO
        tx = build_p2wsh_complete_tx(
            prev_txid_hex=utxo.txid,
            prev_vout=utxo.vout,
            prev_amount=utxo.value_sats,
            prev_spk=spk,
            witness_stack=wit,
            outputs=outs,
            sequence=self.challenge_csv,
        )
        return tx.serialize(with_witness=True).hex()

    def is_honest_complete_shape(
        self, outs: list[tuple[int, bytes]], burn: PendingBurn
    ) -> bool:
        commit = burn.commit()
        # need payout >= amount and FBTO payload present
        has_pay = False
        has_fbto = False
        want_fbto = fbto_payload(commit.account20, commit.withdraw_seq)
        for v, spk in outs:
            if spk == commit.payout_script and v >= commit.amount_sats:
                has_pay = True
            if spk == op_return_script(want_fbto) or (
                len(spk) >= 2 and spk[0] == 0x6A and want_fbto in spk
            ):
                has_fbto = True
        return has_pay and has_fbto

    def evaluate_mempool_tx(
        self,
        *,
        burn: PendingBurn,
        utxo: UtxoRef,
        mempool_outs: list[tuple[int, bytes]],
        use_hold_program: bool = False,
    ) -> ChallengerAction:
        """If mempool COMPLETE is dodgy, race with honest replace."""
        if self.is_honest_complete_shape(mempool_outs, burn):
            return ChallengerAction("skip", "mempool already honest")
        raw = self.build_honest_complete(
            burn, utxo, fee=self.race_fee_sats, use_hold_program=use_hold_program
        )
        return ChallengerAction(
            "race_replace",
            "dodgy COMPLETE detected — racing honest payout+FBTO with higher fee",
            rawtx_hex=raw,
            burn=burn,
        )

    def plan_auto_redeem(
        self,
        burn: PendingBurn,
        utxo: UtxoRef,
        *,
        use_hold_program: bool = False,
    ) -> ChallengerAction:
        raw = self.build_honest_complete(
            burn, utxo, use_hold_program=use_hold_program
        )
        return ChallengerAction(
            "honest_complete",
            "auto-redeem PENDING burn after CSV",
            rawtx_hex=raw,
            burn=burn,
        )


def parse_pending_from_falcon_objects(objs: list[dict]) -> list[PendingBurn]:
    """Extract PENDING withdrawals from account_objects / ledger walk."""
    out: list[PendingBurn] = []
    for o in objs:
        if o.get("LedgerEntryType") not in ("BtcWithdrawal", "BTC_WITHDRAWAL", None):
            # field name varies
            pass
        status = o.get("BtcWithdrawStatus", o.get("btcWithdrawStatus"))
        if status is not None and int(status) != 0:
            continue
        acct = o.get("Account") or o.get("account")
        seq = o.get("BtcWithdrawSeq", o.get("btcWithdrawSeq"))
        amt = o.get("BtcWithdrawAmount", o.get("btcWithdrawAmount"))
        pay = o.get("BtcPayoutScript", o.get("btcPayoutScript"))
        if acct is None or seq is None or amt is None or pay is None:
            continue
        out.append(
            PendingBurn(
                account=str(acct),
                withdraw_seq=int(seq),
                amount_sats=int(amt),
                payout_script_hex=str(pay).replace("0x", ""),
                status=0,
            )
        )
    return out
