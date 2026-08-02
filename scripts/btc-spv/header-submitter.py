#!/usr/bin/env python3
"""
Submit Bitcoin headers to Falcon BTCHeaderSubmit (permissionless).

Keeps Falcon BtcTipHeight near the BTC chain tip so deposit claims are not stuck.

Env:
  FALCON_RPC          default http://46.224.0.140:6005
  FALCON_SECRET       required (or KEYS_JSON with falcon_secret)
  KEYS_JSON           optional path to validator-keys.json
  BTC_API             default https://mempool.space/testnet/api
  BATCH               headers per tx (default 32, max 32)
  POLL_SEC            sleep between loops (default 30)
  ONCE                if 1, single catch-up then exit
"""
from __future__ import annotations

import json
import os
import sys
import time
import urllib.error
import urllib.request

RPC = os.environ.get("FALCON_RPC", "http://46.224.0.140:6005")
# If set (e.g. qxrp-full), sign/submit via docker exec … curl admin RPC (signing often disabled on :6005).
SIGN_DOCKER = os.environ.get("SIGN_DOCKER", "").strip()
SIGN_RPC = os.environ.get("SIGN_RPC", "http://127.0.0.1:5005")
BTC_API = os.environ.get("BTC_API", "https://mempool.space/testnet/api").rstrip("/")
BATCH = min(32, int(os.environ.get("BATCH", "32")))
POLL = int(os.environ.get("POLL_SEC", "30"))
ONCE = os.environ.get("ONCE", "0") in ("1", "true", "yes")
HEADER_SIZE = 80


def http_json(url: str, data: dict | None = None, timeout: int = 60):
    body = None if data is None else json.dumps(data).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json", "Accept": "application/json"},
        method="POST" if data is not None else "GET",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode())


def falcon(method: str, params: dict | None = None, *, for_sign: bool = False) -> dict:
    payload = {"method": method, "params": [params or {}]}
    if for_sign and SIGN_DOCKER:
        import subprocess

        out = subprocess.check_output(
            [
                "docker",
                "exec",
                SIGN_DOCKER,
                "curl",
                "-sf",
                "--max-time",
                "60",
                "-X",
                "POST",
                SIGN_RPC,
                "-H",
                "Content-Type: application/json",
                "-d",
                json.dumps(payload),
            ],
            text=True,
        )
        r = json.loads(out)
        return r.get("result") or r
    r = http_json(RPC, payload)
    return r.get("result") or r


def btc_get(path: str):
    url = f"{BTC_API}{path}"
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        raw = resp.read()
        # tip height is plain text
        if path.endswith("/height") or path.endswith("/tip/height"):
            return int(raw.decode().strip())
        try:
            return json.loads(raw.decode())
        except json.JSONDecodeError:
            return raw


def load_secret() -> tuple[str, str]:
    keys_path = os.environ.get("KEYS_JSON", "").strip()
    if keys_path:
        d = json.load(open(keys_path))
        sec = d.get("falcon_secret") or ""
        acct = d.get("account_address") or ""
        if sec and acct:
            return acct, sec
    sec = os.environ.get("FALCON_SECRET", "").strip()
    acct = os.environ.get("FALCON_ACCOUNT", "").strip()
    if not sec or not acct:
        # derive account via wallet_propose is wrong; require both
        raise SystemExit("Set KEYS_JSON or FALCON_ACCOUNT + FALCON_SECRET")
    return acct, sec


def falcon_tip() -> tuple[int, str]:
    r = falcon("ledger_entry", {"btc_bridge_state": True, "ledger_index": "validated"})
    node = r.get("node") or {}
    if r.get("error"):
        raise RuntimeError(r.get("error_message") or r.get("error"))
    return int(node.get("BtcTipHeight") or 0), str(node.get("BtcTipHash") or "")


def fetch_header_hex(height: int) -> str:
    # mempool.space: /block-height/{h} -> hash, /block/{hash}/header -> hex
    h = btc_get(f"/block-height/{height}")
    if isinstance(h, (bytes, bytearray)):
        h = h.decode().strip()
    elif not isinstance(h, str):
        h = str(h).strip()
    hdr = btc_get(f"/block/{h}/header")
    if isinstance(hdr, (bytes, bytearray)):
        return hdr.decode().strip()
    if isinstance(hdr, dict):
        raise RuntimeError(f"unexpected header json for {height}")
    return str(hdr).strip()


def submit_headers(account: str, secret: str, headers_hex: str) -> str:
    # sign via RPC (deprecated but works on fleet)
    blob_hex = headers_hex  # already concat hex of 80-byte headers
    # Fee scales with n headers; use generous fee
    n = len(blob_hex) // (HEADER_SIZE * 2)
    fee = str(max(12, 12 * n))
    sign = falcon(
        "sign",
        {
            "tx_json": {
                "TransactionType": "BTCHeaderSubmit",
                "Account": account,
                "Fee": fee,
                "BtcHeaders": blob_hex.upper(),
            },
            "falcon_secret": secret,
        },
        for_sign=True,
    )
    tx_blob = sign.get("tx_blob")
    if not tx_blob:
        raise RuntimeError(f"sign failed: {sign}")
    # Submit can use public RPC; prefer same path as sign for consistency.
    sub = falcon("submit", {"tx_blob": tx_blob}, for_sign=bool(SIGN_DOCKER))
    return str(sub.get("engine_result") or sub.get("error") or sub)


def btc_hash_at_height(height: int) -> str:
    h = btc_get(f"/block-height/{height}")
    if isinstance(h, (bytes, bytearray)):
        return h.decode().strip().lower()
    return str(h).strip().lower()


def find_best_chain_start(falcon_tip_hash: str, falcon_tip_h: int) -> int:
    """If Falcon tip is an orphan, return height to resume (common_ancestor + 1).

    Else return falcon_tip_h + 1.
    """
    tip_hash = falcon_tip_hash.lower().replace("0x", "")
    try:
        best = btc_hash_at_height(falcon_tip_h)
    except Exception:
        best = ""
    if best and best == tip_hash:
        return falcon_tip_h + 1

    print(
        f"[headers] tip hash mismatch at {falcon_tip_h}: falcon={tip_hash[:16]}… "
        f"btc_best={best[:16] if best else '?'}… — finding common ancestor",
        flush=True,
    )
    h = tip_hash
    for step in range(10_000):
        try:
            st = btc_get(f"/block/{h}/status")
        except Exception as e:
            print(f"[headers] ancestor walk failed at {h[:16]}…: {e}", flush=True)
            # Fall back: try a few heights below tip
            return max(1, falcon_tip_h - 64)
        if isinstance(st, dict) and st.get("in_best_chain"):
            ch = st.get("height")
            if ch is None:
                try:
                    b = btc_get(f"/block/{h}")
                    ch = b.get("height") if isinstance(b, dict) else None
                except Exception:
                    ch = None
            if ch is not None:
                print(f"[headers] common ancestor height={ch} hash={h[:16]}… steps={step}", flush=True)
                return int(ch) + 1
        try:
            b = btc_get(f"/block/{h}")
        except Exception as e:
            print(f"[headers] cannot fetch block {h[:16]}…: {e}", flush=True)
            return max(1, falcon_tip_h - 64)
        if not isinstance(b, dict):
            return max(1, falcon_tip_h - 64)
        h = (b.get("previousblockhash") or "").lower()
        if not h:
            break
    print("[headers] common ancestor not found — restarting near tip-128", flush=True)
    return max(1, falcon_tip_h - 128)


def catch_up(account: str, secret: str) -> None:
    tip_h, tip_hash = falcon_tip()
    btc_tip = int(btc_get("/blocks/tip/height"))
    start = find_best_chain_start(tip_hash, tip_h)
    lag = btc_tip - tip_h
    print(
        f"[headers] falcon_tip={tip_h} btc_tip={btc_tip} lag≈{lag} submit_from={start}",
        flush=True,
    )
    if start > btc_tip:
        print("[headers] already at / past tip", flush=True)
        return

    # Submit best-chain headers from `start` upward (links through common ancestor).
    height = start
    while height <= btc_tip:
        batch_end = min(btc_tip, height + BATCH - 1)
        parts = []
        for h in range(height, batch_end + 1):
            hx = fetch_header_hex(h)
            if len(hx) != HEADER_SIZE * 2:
                raise RuntimeError(f"bad header len at {h}: {len(hx)}")
            parts.append(hx)
        blob = "".join(parts)
        try:
            res = submit_headers(account, secret, blob)
        except Exception as e:
            print(f"[headers] submit {height}-{batch_end} ERR {e}", flush=True)
            time.sleep(5)
            tip_h, tip_hash = falcon_tip()
            height = find_best_chain_start(tip_hash, tip_h)
            btc_tip = int(btc_get("/blocks/tip/height"))
            continue
        tip_h, tip_hash = falcon_tip()
        print(
            f"[headers] submitted {height}-{batch_end} → {res} new_tip={tip_h} {tip_hash[:16]}…",
            flush=True,
        )
        if res in ("tesSUCCESS", "terQUEUED"):
            # Advance along best chain; tip height may jump if better work applied.
            height = batch_end + 1
        elif res == "tecNO_ENTRY":
            # Parent missing — re-resolve fork and retry
            time.sleep(3)
            height = find_best_chain_start(tip_hash, tip_h)
        else:
            time.sleep(8)
            height = find_best_chain_start(tip_hash, tip_h)
        btc_tip = int(btc_get("/blocks/tip/height"))
        time.sleep(1)


def main() -> None:
    account, secret = load_secret()
    print(f"[headers] account={account} rpc={RPC} btc={BTC_API}", flush=True)
    while True:
        try:
            catch_up(account, secret)
        except Exception as e:
            print(f"[headers] loop error: {e}", flush=True)
        if ONCE:
            break
        time.sleep(POLL)


if __name__ == "__main__":
    main()
