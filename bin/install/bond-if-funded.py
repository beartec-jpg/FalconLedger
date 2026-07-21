#!/usr/bin/env python3
"""Wait for funding + local sync, then bond via sign (local) + submit (public RPC)."""
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request

KEYS_FILE = "/var/lib/qxrp-validator/validator-keys.json"
PUBLIC_RPC = __import__("os").environ.get("QXRP_PUBLIC_RPC", "http://46.224.0.140:6005")
MIN_FUND = 1_100_000_000
MIN_BOND = 1_000_000_000
MIN_SYNC_SEQ = 1000


def rpc(url, method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read())


def rpc_local(method, params=None):
    payload = json.dumps({"method": method, "params": [params or {}]})
    out = subprocess.check_output(
        ["docker", "exec", "qxrp-validator", "curl", "-sf", "-X", "POST",
         "http://127.0.0.1:5005", "-H", "Content-Type: application/json", "-d", payload],
        text=True,
    )
    return json.loads(out)


def local_server_info():
    try:
        return rpc_local("server_info", {}).get("result", {}).get("info", {}) or {}
    except Exception:
        return {}


def local_sync_progress():
    """Return (seq, detail, state) for bond readiness.

    Falcon joiners often report validated_ledger=null and complete_ledgers=empty
    while still proposing/full with a high closed_ledger. Signing only needs a
    live admin RPC + peers; use closed_ledger as a fallback so bonding is not
    blocked forever.
    """
    info = local_server_info()
    state = str(info.get("server_state") or "")
    peers = int(info.get("peers") or 0)
    ledgers = info.get("complete_ledgers") or ""
    v = info.get("validated_ledger") or {}
    c = info.get("closed_ledger") or {}
    seq = 0
    if isinstance(v, dict) and v.get("seq") is not None:
        seq = int(v.get("seq") or 0)
        detail = ledgers or f"validated:{seq}"
    elif isinstance(c, dict) and c.get("seq") is not None:
        seq = int(c.get("seq") or 0)
        detail = ledgers if ledgers and ledgers != "empty" else f"closed:{seq}"
    return seq, detail, state, peers


def wait_for_local_sync():
    print("Waiting for local ledger (validated preferred; closed+peers OK for Falcon joiners)...")
    ready_states = {"proposing", "full", "tracking", "syncing", "connected"}
    for i in range(120):
        seq, detail, state, peers = local_sync_progress()
        # Prefer real complete range; otherwise accept high closed seq while peered.
        has_range = bool(detail) and detail != "empty" and not detail.startswith("closed:")
        closed_ok = seq > MIN_SYNC_SEQ and peers >= 1 and state in ready_states
        if (has_range and seq > MIN_SYNC_SEQ) or closed_ok:
            print(f"  Local ledger ready (seq {seq}, {detail}, state={state}, peers={peers}).")
            return
        if i % 6 == 0:
            print(
                f"  … seq {seq}, {detail or 'no ledger'}, state={state}, "
                f"peers={peers} (need seq > {MIN_SYNC_SEQ} + peers)"
            )
        time.sleep(5)
    print("ERROR: local node has no usable ledger — check: docker logs qxrp-validator")
    sys.exit(1)


def sign_and_submit_public(tx_json, falcon_secret):
    """Sign on local admin RPC; broadcast signed blob via public RPC."""
    sign = rpc_local("sign", {"tx_json": tx_json, "falcon_secret": falcon_secret})
    result = sign.get("result", sign)
    if result.get("error"):
        err = result.get("error", "error")
        msg = result.get("error_message", "")
        raise RuntimeError(f"{err}: {msg}")

    blob = result["tx_blob"]
    sign_eng = result.get("engine_result", "")

    sub = rpc(PUBLIC_RPC, "submit", {"tx_blob": blob})
    sres = sub.get("result", sub)
    if sres.get("error") and not sres.get("engine_result"):
        raise RuntimeError(f"{sres.get('error')}: {sres.get('error_message', '')}")

    eng = sres.get("engine_result", sign_eng or "unknown")
    msg = sres.get("engine_result_message", result.get("engine_result_message", ""))
    return eng, msg


def main():
    keys = json.load(open(KEYS_FILE))
    account = keys["account_address"]
    falcon_secret = keys["falcon_secret"]
    consensus = keys["consensus_key_hex"]
    falcon_pk = keys["falcon_public_key_hex"]

    print(f"Waiting for ≥1,100 qXRP on {account}...")
    bal = 0
    for i in range(180):
        try:
            info = rpc(PUBLIC_RPC, "account_info", {"account": account, "ledger_index": "validated"})
            bal = int(info["result"]["account_data"]["Balance"])
            if bal >= MIN_FUND:
                print(f"Funded: {bal / 1_000_000} qXRP")
                break
        except (urllib.error.URLError, KeyError, ValueError):
            bal = 0
        if i % 6 == 0:
            print(f"  … balance {bal} drops (need {MIN_FUND})")
        time.sleep(10)
    else:
        print("Timed out — run again after funding.")
        return

    wait_for_local_sync()

    print("Submitting ValidatorRegister...")
    try:
        eng, msg = sign_and_submit_public({
            "TransactionType": "ValidatorRegister",
            "Account": account,
            "PublicKey": falcon_pk,
            "ConsensusKey": consensus,
            "Fee": "12",
        }, falcon_secret)
    except RuntimeError as e:
        print(f"  ValidatorRegister: error — {e}")
        sys.exit(1)
    print(f"  ValidatorRegister: {eng}" + (f" — {msg}" if msg else ""))
    if eng not in ("tesSUCCESS", "tecDUPLICATE", "terQUEUED"):
        sys.exit(1)
    time.sleep(5)

    print("Submitting ValidatorBond (1,000 qXRP)...")
    try:
        eng, msg = sign_and_submit_public({
            "TransactionType": "ValidatorBond",
            "Account": account,
            "ConsensusKey": consensus,
            "BondedAmount": str(MIN_BOND),
            "Fee": "12",
        }, falcon_secret)
    except RuntimeError as e:
        print(f"  ValidatorBond: error — {e}")
        sys.exit(1)
    print(f"  ValidatorBond: {eng}" + (f" — {msg}" if msg else ""))
    if eng == "tecNO_PERMISSION":
        print("  Already bonded.")
    elif eng not in ("tesSUCCESS", "terQUEUED"):
        sys.exit(1)
    else:
        print("Bond complete.")


if __name__ == "__main__":
    main()
