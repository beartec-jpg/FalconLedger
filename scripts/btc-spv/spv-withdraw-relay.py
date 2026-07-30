#!/usr/bin/env python3
"""
spv-withdraw-relay.py
─────────────────────
SPV peg-out payout relay (testnet / ops).

After a user:
  1) BTCBridgeBurn (burns MPT FBTC, opens challenge window)
  2) BTCWithdrawFinalize (after ~32 Falcon ledgers)

this relay detects FINAL withdrawals and sends matching sats from the
Bitcoin custody wallet to BtcPayoutScript (P2PKH).

Usage:
  python3 scripts/btc-spv/spv-withdraw-relay.py --once
  python3 scripts/btc-spv/spv-withdraw-relay.py --loop --interval 20

Env:
  PUBLIC_RPC_URL              default http://127.0.0.1:6005
  BTC_CUSTODY_FILE            default /var/lib/qxrp-bridge/btc-custody.json
  SPV_WITHDRAW_STATE_FILE     default /var/lib/qxrp-bridge/spv_withdraw_state.json
  BTC_NETWORK                 testnet | mainnet
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
import time
import urllib.request
from pathlib import Path
from typing import Any, Optional

try:
    import ecdsa
    from ecdsa import SECP256k1, util
except ImportError:
    print("ERROR: python3-ecdsa required", file=sys.stderr)
    sys.exit(1)

# Import send helpers from custodial withdraw relay (same host layout)
sys.path.insert(0, "/var/lib/qxrp-bridge")
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

B58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
STATUS_PENDING = 0
STATUS_FINAL = 2
STATUS_PAID = 3  # off-chain only in our state file


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def rpc(url: str, method: str, params: Optional[dict] = None) -> dict:
    payload = json.dumps({"method": method, "params": [params or {}], "id": 1}).encode()
    req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        body = json.loads(resp.read().decode())
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body.get("result") or {}


def load_json(path: Path, default: Any) -> Any:
    if not path.is_file():
        return default
    try:
        return json.loads(path.read_text())
    except Exception:
        return default


def save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, indent=2))
    tmp.replace(path)


def p2pkh_address_from_script(spk_hex: str, network: str) -> str:
    spk = bytes.fromhex(spk_hex.replace("0x", ""))
    # OP_DUP OP_HASH160 0x14 <20> OP_EQUALVERIFY OP_CHECKSIG
    if len(spk) != 25 or spk[0:3] != bytes([0x76, 0xA9, 0x14]) or spk[23:25] != bytes([0x88, 0xAC]):
        raise ValueError(f"unsupported payout script: {spk_hex[:40]}…")
    h160 = spk[3:23]
    ver = 0x6F if network == "testnet" else 0x00
    payload = bytes([ver]) + h160
    chk = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    data = payload + chk
    # base58
    n = int.from_bytes(data, "big")
    out = ""
    while n > 0:
        n, r = divmod(n, 58)
        out = B58[r] + out
    pad = 0
    for b in data:
        if b == 0:
            pad += 1
        else:
            break
    return ("1" * pad) + out


def scan_burns(url: str, from_ledger: int, to_ledger: int) -> list[dict]:
    found: list[dict] = []
    for li in range(from_ledger, to_ledger + 1):
        try:
            led = rpc(
                url,
                "ledger",
                {
                    "ledger_index": li,
                    "transactions": True,
                    "expand": True,
                },
            )
        except Exception as e:
            log(f"ledger {li}: {e}")
            continue
        ledger = led.get("ledger") or {}
        txs = ledger.get("transactions") or []
        for tx in txs:
            if not isinstance(tx, dict):
                continue
            if tx.get("TransactionType") != "BTCBridgeBurn":
                continue
            meta = tx.get("metaData") or tx.get("meta") or {}
            if isinstance(meta, dict) and meta.get("TransactionResult") not in (None, "tesSUCCESS"):
                # expanded form sometimes nests differently
                if meta.get("TransactionResult") and meta.get("TransactionResult") != "tesSUCCESS":
                    continue
            # meta may be string in some modes
            if isinstance(meta, str) and "tesSUCCESS" not in meta:
                continue
            acct = tx.get("Account")
            seq = tx.get("Sequence")
            amt = tx.get("BtcWithdrawAmount")
            spk = tx.get("BtcPayoutScript")
            if not acct or seq is None or not spk:
                continue
            try:
                amount_sats = int(amt)
            except Exception:
                amount_sats = int(str(amt), 16) if isinstance(amt, str) else 0
            found.append(
                {
                    "account": acct,
                    "seq": int(seq),
                    "amount_sats": amount_sats,
                    "payout_script": str(spk).lower().replace("0x", ""),
                    "burn_ledger": li,
                    "burn_hash": tx.get("hash"),
                }
            )
    return found


def withdraw_status(url: str, account: str, seq: int) -> Optional[dict]:
    r = rpc(
        url,
        "ledger_entry",
        {"btc_withdrawal": {"account": account, "seq": seq}, "ledger_index": "validated"},
    )
    if r.get("error") or not r.get("node"):
        return None
    return r["node"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--loop", action="store_true")
    ap.add_argument("--interval", type=int, default=20)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--rpc", default=os.environ.get("PUBLIC_RPC_URL", "http://127.0.0.1:6005"))
    ap.add_argument(
        "--custody",
        default=os.environ.get("BTC_CUSTODY_FILE", "/var/lib/qxrp-bridge/btc-custody.json"),
    )
    ap.add_argument(
        "--state",
        default=os.environ.get(
            "SPV_WITHDRAW_STATE_FILE", "/var/lib/qxrp-bridge/spv_withdraw_state.json"
        ),
    )
    ap.add_argument("--network", default=os.environ.get("BTC_NETWORK", "testnet"))
    args = ap.parse_args()

    # Reuse custodial send_btc_p2pkh
    try:
        from importlib.machinery import SourceFileLoader

        withdraw_mod = SourceFileLoader(
            "btc_withdraw", "/var/lib/qxrp-bridge/bridge-btc-withdraw-relay.py"
        ).load_module()
    except Exception as e:
        log(f"cannot load withdraw helpers: {e}")
        return 1

    custody = load_json(Path(args.custody), {})
    if not custody.get("privateKeyHex"):
        log("custody key missing")
        return 1
    from_addr = custody.get("address_testnet") if args.network == "testnet" else custody.get(
        "address_mainnet"
    )
    if not from_addr:
        log("custody address missing")
        return 1

    state = load_json(Path(args.state), {"last_ledger": 0, "jobs": {}, "paid": {}})
    if not isinstance(state.get("jobs"), dict):
        state["jobs"] = {}
    if not isinstance(state.get("paid"), dict):
        state["paid"] = {}

    def tick() -> None:
        nonlocal state
        info = rpc(args.rpc, "server_info")
        cur = int((info.get("info") or {}).get("validated_ledger", {}).get("seq") or 0)
        if cur <= 0:
            log("no ledger")
            return
        start = max(int(state.get("last_ledger") or 0), cur - 200)
        if start >= cur:
            start = cur - 1
        # Scan new burns
        burns = scan_burns(args.rpc, start + 1, cur)
        for b in burns:
            key = f"{b['account']}:{b['seq']}"
            if key not in state["jobs"] and key not in state["paid"]:
                state["jobs"][key] = b
                log(f"tracked burn {key} amount={b['amount_sats']} sats")
        state["last_ledger"] = cur

        # Process jobs
        for key, job in list(state["jobs"].items()):
            if key in state["paid"]:
                del state["jobs"][key]
                continue
            node = withdraw_status(args.rpc, job["account"], job["seq"])
            if not node:
                continue
            st = int(node.get("BtcWithdrawStatus", 0))
            challenge_end = int(node.get("BtcChallengeEndLedger", 0))
            amt = int(node.get("BtcWithdrawAmount", job.get("amount_sats") or 0))
            spk = str(node.get("BtcPayoutScript") or job.get("payout_script") or "").lower()
            if st == STATUS_PENDING and cur <= challenge_end:
                log(f"{key} challenge {cur}/{challenge_end}")
                continue
            if st == STATUS_PENDING and cur > challenge_end:
                log(f"{key} ready for user finalize (still PENDING on ledger)")
                continue
            if st != STATUS_FINAL and st != STATUS_PAID:
                # 1 = challenged, etc.
                log(f"{key} status={st} skip")
                continue

            # FINAL → pay BTC
            try:
                to_addr = p2pkh_address_from_script(spk, args.network)
            except Exception as e:
                log(f"{key} bad payout script: {e}")
                continue
            fee_buffer = 500
            send_amt = max(546, amt - fee_buffer) if amt > fee_buffer + 546 else amt
            # Prefer full amt; send_btc_p2pkh takes fee from inputs
            send_amt = amt
            log(f"{key} paying {send_amt} sats → {to_addr}")
            if args.dry_run:
                state["paid"][key] = {"dry_run": True, "to": to_addr, "sats": send_amt}
                del state["jobs"][key]
                continue
            try:
                txid = withdraw_mod.send_btc_p2pkh(
                    priv_hex=custody["privateKeyHex"],
                    from_addr=from_addr,
                    to_addr=to_addr,
                    amount_sats=send_amt,
                    network=args.network,
                    fee_rate=2,
                )
                log(f"{key} BTC paid {txid}")
                state["paid"][key] = {
                    "btc_txid": txid,
                    "to": to_addr,
                    "sats": send_amt,
                    "at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                }
                del state["jobs"][key]
            except Exception as e:
                log(f"{key} pay failed: {e}")

        save_json(Path(args.state), state)

    if args.loop:
        log(f"spv-withdraw-relay loop interval={args.interval}s rpc={args.rpc}")
        while True:
            try:
                tick()
            except Exception as e:
                log(f"tick error: {e}")
            time.sleep(args.interval)
    else:
        tick()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
