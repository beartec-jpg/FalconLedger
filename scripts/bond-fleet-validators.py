#!/usr/bin/env python3
"""Fund and bond all Falcon fleet validators after a migration reset."""
import hashlib
import json
import subprocess
import sys
import time
import urllib.request

PUBLIC_RPC = "http://46.224.0.140:6005"
GENESIS_ACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
GENESIS_SECRET = "masterpassphrase"
DROPS_PER_QXRP = 1_000_000
MIN_BOND_DROPS = 1_000 * DROPS_PER_QXRP
FUND_DROPS = 2_000 * DROPS_PER_QXRP
SSH = ["ssh", "-i", "/home/scott/.ssh/id_ed25519", "-o", "StrictHostKeyChecking=no"]

VALIDATORS = [
    {
        "host": "46.224.0.140",
        "container": "qxrp-val2",
        "cfg": "/var/lib/qxrp-val2/config/xrpld.cfg",
        "name": "val2",
    },
    {
        "host": "167.233.55.43",
        "container": "qxrp-validator",
        "cfg": "/var/lib/qxrp-validator/config/xrpld.cfg",
        "name": "v1",
    },
    {
        "host": "204.168.175.194",
        "container": "qxrp-validator",
        "cfg": "/var/lib/qxrp-validator/config/xrpld.cfg",
        "name": "v2",
    },
    {
        "host": "89.167.109.241",
        "container": "qxrp-validator",
        "cfg": "/var/lib/qxrp-validator/config/xrpld.cfg",
        "name": "v3",
    },
]


def rpc(url, method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(
        url, data=body, headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read())


def ssh(host, cmd):
    out = subprocess.check_output(SSH + [f"root@{host}", cmd], text=True)
    return out.strip()


def falcon_pubkey_from_secret(falcon_secret: str) -> str:
    raw = bytes.fromhex(falcon_secret)
    pub_len = 898 if raw[0] == 0xFB else 1794
    return raw[:pub_len].hex().upper()


def falcon_account_from_secret(falcon_secret: str) -> str:
    pub = bytes.fromhex(falcon_pubkey_from_secret(falcon_secret))
    account_bytes = hashlib.new("ripemd160", hashlib.sha256(pub).digest()).digest()
    alphabet = "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz"
    payload = b"\x00" + account_bytes
    checksum = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    data = payload + checksum
    num = int.from_bytes(data, "big")
    chars = []
    while num:
        num, rem = divmod(num, 58)
        chars.append(alphabet[rem])
    for b in data:
        if b == 0:
            chars.append(alphabet[0])
        else:
            break
    return "".join(reversed(chars))


def read_falcon_secret(host, cfg):
    out = ssh(host, f"awk '/^\\[validation_falcon_secret\\]/{{getline; print; exit}}' {cfg}")
    secret = out.splitlines()[-1].strip()
    if not secret.startswith("FB"):
        raise RuntimeError(f"bad falcon secret on {host}")
    return secret


def admin_rpc(host, container, method, params):
    payload = json.dumps({"method": method, "params": [params]})
    cmd = (
        f"docker exec {container} curl -sf -X POST http://127.0.0.1:5005 "
        f"-H 'Content-Type: application/json' -d {json.dumps(payload)}"
    )
    out = ssh(host, cmd)
    data = json.loads(out)
    if "result" not in data:
        raise RuntimeError(f"{method} on {host}/{container}: {data}")
    return data["result"]


def sign_on_node(host, container, tx_json, falcon_secret):
    return admin_rpc(
        host, container, "sign", {"tx_json": tx_json, "falcon_secret": falcon_secret}
    )


def submit_public(result):
    blob = result.get("tx_blob")
    if not blob:
        return result
    r = rpc(PUBLIC_RPC, "submit", {"tx_blob": blob})
    return r.get("result", r)


def submit_signed(result):
    """Sign RPC returns tx_blob; submit and return engine_result."""
    sub = submit_public(result)
    eng = sub.get("engine_result")
    if eng:
        return eng, sub
    if sub.get("error"):
        return sub.get("error"), sub
    return sub.get("status", "unknown"), sub


def wait_tx(tx_hash, timeout=90):
    for _ in range(timeout):
        try:
            r = rpc(PUBLIC_RPC, "tx", {"transaction": tx_hash, "binary": False})
            res = r.get("result", {})
            if res.get("validated"):
                return res.get("meta", {}).get("TransactionResult", "UNKNOWN")
        except Exception:
            pass
        time.sleep(1)
    return "TIMEOUT"


def account_balance(account):
    try:
        r = rpc(
            PUBLIC_RPC,
            "account_info",
            {"account": account, "ledger_index": "validated"},
        )
        return int(r["result"]["account_data"]["Balance"])
    except Exception:
        return 0


def bond_status(account):
    try:
        r = rpc(
            PUBLIC_RPC,
            "ledger_entry",
            {"validator_bond": {"account": account}, "ledger_index": "validated"},
        )
        node = r["result"]["node"]
        return node.get("BondStatus"), node.get("BondedAmount")
    except Exception:
        return None, None


def main():
    print("=== Fleet validator bonding ===\n")
    info = rpc(PUBLIC_RPC, "server_info")["result"]["info"]
    print(f"Network state: {info['server_state']}  ledger: {info.get('validated_ledger', {}).get('seq')}")

    genesis_seq = rpc(
        PUBLIC_RPC,
        "account_info",
        {"account": GENESIS_ACCOUNT, "ledger_index": "validated"},
    )["result"]["account_data"]["Sequence"]
    print(f"Genesis sequence: {genesis_seq}\n")

    for v in VALIDATORS:
        print(f"── {v['name']} @ {v['host']} ──")
        secret = read_falcon_secret(v["host"], v["cfg"])
        pk = falcon_pubkey_from_secret(secret)
        account = falcon_account_from_secret(secret)
        print(f"  account: {account}")
        print(f"  falcon:  {pk[:24]}…")

        status, bonded = bond_status(account)
        if status == 1:  # kBOND_STATUS_BONDED
            print(f"  already bonded ({bonded} drops) ✓\n")
            continue

        bal = account_balance(account)
        if bal < MIN_BOND_DROPS + 20_000_000:
            print(f"  funding {FUND_DROPS} drops from genesis…")
            pay = admin_rpc(
                "46.224.0.140",
                "qxrp-full",
                "sign",
                {
                    "tx_json": {
                        "TransactionType": "Payment",
                        "Account": GENESIS_ACCOUNT,
                        "Destination": account,
                        "Amount": str(FUND_DROPS),
                        "Sequence": genesis_seq,
                        "Fee": "12",
                    },
                    "secret": GENESIS_SECRET,
                },
            )
            eng, sub = submit_signed(pay)
            if eng not in ("tesSUCCESS", "terQUEUED"):
                print(f"  FUND FAILED: {eng} {sub.get('engine_result_message', '')}")
                sys.exit(1)
            genesis_seq += 1
            print(f"  fund: {eng} → {wait_tx(pay['tx_json']['hash'])}")
        else:
            print(f"  balance ok: {bal} drops")

        if status is None:
            print("  ValidatorRegister…")
            reg = sign_on_node(
                v["host"],
                v["container"],
                {
                    "TransactionType": "ValidatorRegister",
                    "Account": account,
                    "PublicKey": pk,
                    "ConsensusKey": pk,
                    "Fee": "12",
                },
                secret,
            )
            eng, sub = submit_signed(reg)
            if eng not in ("tesSUCCESS", "tecDUPLICATE"):
                print(f"  REGISTER FAILED: {eng} {sub.get('engine_result_message', '')}")
                sys.exit(1)
            if eng == "tesSUCCESS":
                print(f"  register: {eng} → {wait_tx(reg['tx_json']['hash'])}")
            else:
                print(f"  register: {eng}")

        print(f"  ValidatorBond ({MIN_BOND_DROPS} drops)…")
        bond = sign_on_node(
            v["host"],
            v["container"],
            {
                "TransactionType": "ValidatorBond",
                "Account": account,
                "ConsensusKey": pk,
                "BondedAmount": str(MIN_BOND_DROPS),
                "Fee": "12",
            },
            secret,
        )
        eng, sub = submit_signed(bond)
        if eng == "tecNO_PERMISSION":
            status, bonded = bond_status(account)
            if status == 1:
                print(f"  bond: already bonded ({bonded}) ✓")
            else:
                print(f"  bond: tecNO_PERMISSION (status={status})")
                sys.exit(1)
        elif eng != "tesSUCCESS":
            print(f"  BOND FAILED: {eng} {sub.get('engine_result_message', '')}")
            sys.exit(1)
        else:
            print(f"  bond: {eng} → {wait_tx(bond['tx_json']['hash'])}")
        print()

    print("=== All validators bonded ===")
    for v in VALIDATORS:
        secret = read_falcon_secret(v["host"], v["cfg"])
        account = falcon_account_from_secret(secret)
        status, bonded = bond_status(account)
        print(f"  {v['name']}: {account} status={status} bonded={bonded}")


if __name__ == "__main__":
    main()