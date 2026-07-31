#!/usr/bin/env python3
"""Full two-way peg e2e: BTC deposit → FBTC mint → burn → finalize → vault claim.

Requires:
  - Falcon 1101 standalone (BitcoinSPVBridge in [features])
  - Bitcoin regtest (Docker btc-spv-regtest)
  - Local venv with ecdsa: scripts/btc-spv/.venv

No public testnet BTC.
"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import subprocess
import sys
import urllib.request
from typing import Any, Optional

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BITVM = os.path.join(os.path.dirname(__file__), "bitvm")
sys.path.insert(0, BITVM)

FALCON_URL = os.environ.get("FALCON_SPV_URL", "http://127.0.0.1:5115")
GENESIS_SECRET = "snoPBrXtMeMyMHUVTgbuqAfg1SUTb"
GENESIS_ACCT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
NETWORK_ID = 1101
BTC_CHAIN_REGTEST = 3
CHALLENGE_LEDGERS = 32
CSV_BLOCKS = 6

# Prefer project venv for ecdsa
_VENV_PY = os.path.join(os.path.dirname(__file__), ".venv/bin/python")
if os.path.isfile(_VENV_PY) and os.path.realpath(sys.executable) != os.path.realpath(_VENV_PY):
    # re-exec under venv if ecdsa missing
    try:
        import ecdsa  # noqa: F401
    except ImportError:
        os.execv(_VENV_PY, [_VENV_PY, __file__] + sys.argv[1:])

import ecdsa  # type: ignore
from ecdsa import SECP256k1, util

from vault import (
    VaultParams,
    bitcoin_cli,
    build_vault_script,
    ensure_wallet,
    mine,
    script_to_p2wsh_address,
)


def log(msg: str) -> None:
    print(msg, flush=True)


def falcon_rpc(method: str, params: Optional[dict] = None) -> dict:
    payload = {"method": method, "params": [params or {}], "id": 1}
    data = json.dumps(payload).encode()
    req = urllib.request.Request(
        FALCON_URL,
        data=data,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            body = json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        err_body = e.read().decode(errors="replace")
        raise RuntimeError(f"HTTP {e.code} {method}: {err_body[:800]}") from e
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body.get("result") or {}


def ledger_accept() -> None:
    falcon_rpc("ledger_accept")


def account_seq(acct: str) -> int:
    return int(
        falcon_rpc("account_info", {"account": acct, "ledger_index": "current"})[
            "account_data"
        ]["Sequence"]
    )


def submit_genesis_payment(tx: dict) -> dict:
    return falcon_rpc("submit", {"tx_json": tx, "secret": GENESIS_SECRET})


def submit_falcon(tx: dict, secret: str) -> dict:
    return falcon_rpc("submit", {"tx_json": tx, "falcon_secret": secret})


def require_tes(r: dict, label: str) -> None:
    eng = r.get("engine_result") or r.get("error_message") or r.get("error") or "?"
    if not str(eng).startswith("tes"):
        raise RuntimeError(f"{label}: {eng}\n{json.dumps(r)[:800]}")
    log(f"  PASS {label}: {eng}")


def double_sha256(b: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


def falcon_block_hash_hex(header80: bytes) -> str:
    return double_sha256(header80)[::-1].hex().upper()


def falcon_hash_field_from_header_le(header80: bytes, offset: int) -> str:
    return header80[offset : offset + 32][::-1].hex().upper()


def falcon_script_hash_hex(spk: bytes) -> str:
    # Raw SHA256(scriptPubKey) — matches btcScriptHash / design (no LE reverse)
    return hashlib.sha256(spk).digest().hex().upper()


def classic_account_id(addr: str) -> bytes:
    alphabet = "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz"
    n = 0
    for c in addr:
        n = n * 58 + alphabet.index(c)
    full = n.to_bytes(25, "big")
    return full[1:21]


def generate_secp_keypair() -> tuple[bytes, bytes, bytes]:
    """Return (priv32, compressed_pub33, p2wpkh_scriptPubKey)."""
    sk = ecdsa.SigningKey.generate(curve=SECP256k1)
    priv = sk.to_string()
    vk = sk.get_verifying_key()
    x = vk.pubkey.point.x().to_bytes(32, "big")
    y = vk.pubkey.point.y()
    pub = (b"\x02" if y % 2 == 0 else b"\x03") + x
    h160 = hashlib.new("ripemd160", hashlib.sha256(pub).digest()).digest()
    spk = b"\x00\x14" + h160
    return priv, pub, spk


def ser_compact_size(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", n)
    if n <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", n)
    return b"\xff" + struct.pack("<Q", n)


def bip143_sighash(
    *,
    version: int,
    prev_txid_le: bytes,
    prev_vout: int,
    sequence: int,
    amount_sats: int,
    script_code: bytes,
    outputs: list[tuple[int, bytes]],
    locktime: int = 0,
    sighash_type: int = 1,
) -> bytes:
    """BIP143 sighash for single P2WSH input (SIGHASH_ALL)."""
    assert len(prev_txid_le) == 32
    hash_prevouts = double_sha256(prev_txid_le + struct.pack("<I", prev_vout))
    hash_sequence = double_sha256(struct.pack("<I", sequence))
    outs = b""
    for val, spk in outputs:
        outs += struct.pack("<Q", val)
        outs += ser_compact_size(len(spk)) + spk
    hash_outputs = double_sha256(outs)

    preimage = b""
    preimage += struct.pack("<I", version)
    preimage += hash_prevouts
    preimage += hash_sequence
    preimage += prev_txid_le + struct.pack("<I", prev_vout)
    preimage += ser_compact_size(len(script_code)) + script_code
    preimage += struct.pack("<Q", amount_sats)
    preimage += struct.pack("<I", sequence)
    preimage += hash_outputs
    preimage += struct.pack("<I", locktime)
    preimage += struct.pack("<I", sighash_type)
    return double_sha256(preimage)


def priv_to_wif_regtest(priv: bytes) -> str:
    """Compressed regtest/testnet WIF."""
    alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
    payload = b"\xef" + priv + b"\x01"
    checksum = double_sha256(payload)[:4]
    data = payload + checksum
    n = int.from_bytes(data, "big")
    res = ""
    while n > 0:
        n, r = divmod(n, 58)
        res = alphabet[r] + res
    # leading zeros
    for b in data:
        if b == 0:
            res = "1" + res
        else:
            break
    return res


def sign_sighash(priv: bytes, sighash: bytes) -> bytes:
    sk = ecdsa.SigningKey.from_string(priv, curve=SECP256k1)
    # Low-S DER; avoid non-minimal integer encodings
    order = SECP256k1.order
    sig_der = sk.sign_digest_deterministic(
        sighash, hashfunc=hashlib.sha256, sigencode=util.sigencode_der_canonize
    )
    # Ensure low S
    r, s = util.sigdecode_der(sig_der, order)
    if s > order // 2:
        s = order - s
        sig_der = util.sigencode_der(r, s, order)
    return sig_der + b"\x01"  # SIGHASH_ALL


def build_claim_tx(
    *,
    funding_txid: str,
    funding_vout: int,
    amount_sats: int,
    witness_script: bytes,
    preimage: bytes,
    priv: bytes,
    payout_spk: bytes,
    csv_blocks: int,
    fee_sats: int = 1000,
) -> str:
    """Build + sign P2WSH vault claim (ELSE path: sig, preimage, 0, script)."""
    # prev txid LE
    prev_le = bytes.fromhex(funding_txid)[::-1]
    sequence = csv_blocks  # relative locktime
    send_sats = amount_sats - fee_sats
    if send_sats <= 0:
        raise ValueError("amount too small for fee")
    outputs = [(send_sats, payout_spk)]

    sighash = bip143_sighash(
        version=2,
        prev_txid_le=prev_le,
        prev_vout=funding_vout,
        sequence=sequence,
        amount_sats=amount_sats,
        script_code=witness_script,
        outputs=outputs,
        locktime=0,
        sighash_type=1,
    )
    sig = sign_sighash(priv, sighash)

    # Serialize tx
    # version 2
    tx = struct.pack("<I", 2)
    # marker/flag segwit
    tx += b"\x00\x01"
    # 1 input
    tx += ser_compact_size(1)
    tx += prev_le + struct.pack("<I", funding_vout)
    tx += ser_compact_size(0)  # empty scriptSig
    tx += struct.pack("<I", sequence)
    # outputs
    tx += ser_compact_size(len(outputs))
    for val, spk in outputs:
        tx += struct.pack("<Q", val)
        tx += ser_compact_size(len(spk)) + spk
    # witness for input: 4 stack items
    # stack bottom→top: sig, preimage, OP_IF cond, witness_script
    # Minimal false for OP_IF is empty vector (not 0x00) — BIP62 / SCRIPT_VERIFY_MINIMALIF
    items = [sig, preimage, b"", witness_script]
    tx += ser_compact_size(len(items))
    for it in items:
        tx += ser_compact_size(len(it)) + it
    tx += struct.pack("<I", 0)  # locktime
    return tx.hex()


def merkle_proof_for_txid(blockhash: str, txid: str) -> tuple[bytes, int]:
    block = json.loads(bitcoin_cli("getblock", blockhash, "2"))
    txids = [t["txid"] if isinstance(t, dict) else t for t in block["tx"]]
    tx_index = txids.index(txid)
    leaves = [bytes.fromhex(t)[::-1] for t in txids]

    proof = []
    layer = list(leaves)
    idx = tx_index
    while len(layer) > 1:
        if len(layer) % 2 == 1:
            layer.append(layer[-1])
        sibling = layer[idx ^ 1]
        proof.append(sibling)
        next_layer = []
        for i in range(0, len(layer), 2):
            next_layer.append(double_sha256(layer[i] + layer[i + 1]))
        layer = next_layer
        idx //= 2
    return b"".join(proof), tx_index


def main() -> int:
    log("=== Full peg e2e: mint + burn + finalize + vault claim ===")
    ensure_wallet("bitvm")

    # ── Operator ──────────────────────────────────────────────────────────
    w = falcon_rpc("wallet_propose", {"key_type": "falcon512"})
    op = w["account_id"]
    fsec = w["falcon_secret"]
    log(f"Operator {op}")

    gseq = account_seq(GENESIS_ACCT)
    r = submit_genesis_payment(
        {
            "TransactionType": "Payment",
            "Account": GENESIS_ACCT,
            "Destination": op,
            "Amount": "100000000000",
            "Fee": "12",
            "Sequence": gseq,
            "NetworkID": NETWORK_ID,
        }
    )
    require_tes(r, "fund operator")
    ledger_accept()

    # ── Vault (peg collateral + watch target) ─────────────────────────────
    mine_addr = bitcoin_cli("getnewaddress", wallet="bitvm")
    mine(110, mine_addr)

    # Claim keypair we control (descriptor wallets block dumpprivkey)
    user_priv, user_pub, payout_spk = generate_secp_keypair()

    preimage = hashlib.sha256(b"e2e-full-peg" + os.urandom(16)).digest()
    # Falcon BTCBridgeBurn stores commit as raw SHA256 bytes (no reverse)
    burn_commit = hashlib.sha256(preimage).digest()

    params = VaultParams(
        csv_blocks=CSV_BLOCKS,
        burn_commit=burn_commit,
        user_pubkey=user_pub,
        preimage=preimage,
    )
    witness_script = build_vault_script(params)
    vault_addr = script_to_p2wsh_address(witness_script)
    # P2WSH scriptPubKey
    wit_prog = hashlib.sha256(witness_script).digest()
    vault_spk = bytes([0x00, 0x20]) + wit_prog
    watch_hash = falcon_script_hash_hex(vault_spk)
    log(f"Vault {vault_addr}")
    log(f"  commit={burn_commit.hex()[:16]}… preimage={preimage.hex()[:16]}…")

    # ── Activate on recent tip ────────────────────────────────────────────
    height = int(bitcoin_cli("getblockcount"))
    anchor_h = max(0, height - 2)
    anchor_hash = bitcoin_cli("getblockhash", str(anchor_h))
    hdr_hex = bitcoin_cli("getblockheader", anchor_hash, "false")
    hdr = bytes.fromhex(hdr_hex)
    block_hash = falcon_block_hash_hex(hdr)

    seq = account_seq(op)
    r = submit_falcon(
        {
            "TransactionType": "BTCBridgeActivate",
            "Account": op,
            "Fee": "1000000",
            "Sequence": seq,
            "NetworkID": NETWORK_ID,
            "BtcChainId": BTC_CHAIN_REGTEST,
            "BtcAnchorHash": block_hash,
            "BtcAnchorHeight": anchor_h,
            "BtcAnchorWork": "00" * 31 + "01",
            "BtcMinConfirmations": 1,
            "BtcWatchScriptHash": watch_hash,
            # JSON UInt32 max on some parsers; keep mint cap within 2^32-1
            "BtcMintCap": 100_000_000,  # 1 BTC in sats
            "BtcHeaderBytes": hdr_hex.upper(),
        },
        fsec,
    )
    require_tes(r, "BTCBridgeActivate")
    ledger_accept()

    # ── Deposit to vault + OP_RETURN ──────────────────────────────────────
    deposit_sats = 1_000_000  # 0.01 BTC
    deposit_btc = deposit_sats / 1e8
    acct_id = classic_account_id(op)
    op_return = b"FALC" + acct_id

    utxos = json.loads(bitcoin_cli("listunspent", wallet="bitvm"))
    utxo = max(utxos, key=lambda u: u["amount"])
    fee = 0.0001
    change = float(utxo["amount"]) - deposit_btc - fee
    vin = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
    vout: dict[str, Any] = {vault_addr: deposit_btc, "data": op_return.hex()}
    if change > 0.00001:
        vout[bitcoin_cli("getrawchangeaddress", wallet="bitvm")] = round(change, 8)

    raw = bitcoin_cli(
        "createrawtransaction",
        json.dumps(vin),
        json.dumps(vout),
        wallet="bitvm",
    )
    signed = json.loads(
        bitcoin_cli("signrawtransactionwithwallet", raw, wallet="bitvm")
    )
    if not signed.get("complete"):
        raise RuntimeError(f"sign deposit failed: {signed}")
    dep_txid = bitcoin_cli("sendrawtransaction", signed["hex"])
    mine(3, mine_addr)
    log(f"  Deposit txid={dep_txid}")

    rawv = json.loads(bitcoin_cli("getrawtransaction", dep_txid, "true"))
    blockhash = rawv["blockhash"]
    bh_info = json.loads(bitcoin_cli("getblockheader", blockhash, "true"))
    dep_height = bh_info["height"]

    # Find vault vout
    decoded = json.loads(bitcoin_cli("decoderawtransaction", signed["hex"]))
    vault_vout = None
    for o in decoded["vout"]:
        spk = o.get("scriptPubKey", {})
        if vault_addr in (spk.get("addresses") or []) or spk.get("address") == vault_addr:
            vault_vout = o["n"]
            break
        if spk.get("hex", "").lower() == vault_spk.hex().lower():
            vault_vout = o["n"]
            break
    if vault_vout is None:
        raise RuntimeError("vault vout not found")

    # Submit headers anchor+1 .. dep_height
    for h in range(anchor_h + 1, dep_height + 1):
        bh = bitcoin_cli("getblockhash", str(h))
        hx = bitcoin_cli("getblockheader", bh, "false")
        seq = account_seq(op)
        r = submit_falcon(
            {
                "TransactionType": "BTCHeaderSubmit",
                "Account": op,
                "Fee": "2000000",
                "Sequence": seq,
                "NetworkID": NETWORK_ID,
                "BtcHeaders": hx.upper(),
            },
            fsec,
        )
        eng = r.get("engine_result") or ""
        if str(eng).startswith("tes"):
            ledger_accept()
        elif "tec" in eng.lower() or eng == "tefPAST_SEQ":
            pass
        else:
            # duplicate headers ok-ish
            if eng not in ("temREDUNDANT",):
                log(f"  header {h}: {eng}")

    hx = bitcoin_cli("getblockheader", blockhash, "false")
    hdr_bytes = bytes.fromhex(hx)
    proof_bytes, tx_index = merkle_proof_for_txid(blockhash, dep_txid)

    seq = account_seq(op)
    r = submit_falcon(
        {
            "TransactionType": "BTCDepositClaim",
            "Account": op,
            "Destination": op,
            "Fee": "1000000",
            "Sequence": seq,
            "NetworkID": NETWORK_ID,
            "BtcRawTx": signed["hex"].upper(),
            "BtcMerkleProof": proof_bytes.hex().upper(),
            "BtcTxIndex": tx_index,
            "BtcBlockHash": falcon_block_hash_hex(hdr_bytes),
            "BtcVout": vault_vout,
        },
        fsec,
    )
    require_tes(r, "BTCDepositClaim (mint FBTC)")
    ledger_accept()
    burn_seq = seq  # burn will use next sequence; withdraw keyed by burn tx sequence

    # ── Burn ──────────────────────────────────────────────────────────────
    seq = account_seq(op)
    burn_seq = seq

    r = submit_falcon(
        {
            "TransactionType": "BTCBridgeBurn",
            "Account": op,
            "Fee": "1000000",
            "Sequence": seq,
            "NetworkID": NETWORK_ID,
            "BtcWithdrawAmount": deposit_sats,
            "BtcPayoutScript": payout_spk.hex().upper(),
            "BtcBurnPreimage": preimage.hex().upper(),
        },
        fsec,
    )
    require_tes(r, "BTCBridgeBurn")
    ledger_accept()

    # Challenge window: need view.seq() > challengeEnd = burn_ledger + 32
    log(f"  Waiting {CHALLENGE_LEDGERS + 2} Falcon ledgers (challenge window)…")
    for _ in range(CHALLENGE_LEDGERS + 2):
        ledger_accept()

    # ── Finalize ──────────────────────────────────────────────────────────
    seq = account_seq(op)
    r = submit_falcon(
        {
            "TransactionType": "BTCWithdrawFinalize",
            "Account": op,
            "Fee": "1000000",
            "Sequence": seq,
            "NetworkID": NETWORK_ID,
            "BtcWithdrawSeq": burn_seq,
        },
        fsec,
    )
    require_tes(r, "BTCWithdrawFinalize")
    ledger_accept()

    # ── Bitcoin CSV + vault claim ─────────────────────────────────────────
    log(f"  Mining {CSV_BLOCKS + 1} CSV blocks…")
    mine(CSV_BLOCKS + 1, mine_addr)

    # Amount locked = deposit_sats (vault output)
    claim_hex = build_claim_tx(
        funding_txid=dep_txid,
        funding_vout=vault_vout,
        amount_sats=deposit_sats,
        witness_script=witness_script,
        preimage=preimage,
        priv=user_priv,
        payout_spk=payout_spk,
        csv_blocks=CSV_BLOCKS,
        fee_sats=1000,
    )
    # Broadcast
    try:
        claim_txid = bitcoin_cli("sendrawtransaction", claim_hex)
        log(f"  PASS vault claim broadcast: {claim_txid}")
    except RuntimeError as e:
        # Debug with testmempoolaccept
        try:
            acc = bitcoin_cli("testmempoolaccept", json.dumps([claim_hex]))
            log(f"  mempoolaccept: {acc}")
        except Exception as e2:
            log(f"  mempoolaccept failed: {e2}")
        raise RuntimeError(f"vault claim broadcast failed: {e}") from e

    mine(1, mine_addr)
    conf = json.loads(bitcoin_cli("getrawtransaction", claim_txid, "true"))
    if not conf.get("confirmations"):
        raise RuntimeError("claim tx not confirmed")
    log(f"  PASS vault claim confirmed (conf={conf.get('confirmations')})")
    log(f"  Claim outputs: {json.dumps(conf.get('vout', [])[:2])[:200]}")

    log("")
    log("=" * 60)
    log("FULL PEG E2E PASSED")
    log("  BTC → FBTC (SPV mint) → burn → finalize → BTC vault claim")
    log("  No intermediate custodian on mint; BitVM-class vault on claim.")
    log("=" * 60)
    return 0


if __name__ == "__main__":
    bindir = os.path.join(ROOT, "data/btc-spv-1101/bin")
    if os.path.isdir(bindir):
        os.environ["PATH"] = bindir + os.pathsep + os.environ.get("PATH", "")
    try:
        sys.exit(main())
    except Exception as e:
        log(f"FAIL: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)
