#!/usr/bin/env python3
"""Fund and bond all Falcon fleet validators after genesis wipe."""
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
SSH = ["ssh", "-i", "/home/scott/.ssh/id_ed25519", "-o", "StrictHostKeyChecking=no", "-o", "ConnectTimeout=20"]

VALIDATORS = [
    {"host": "46.224.0.140", "container": "qxrp-val2", "dir": "/var/lib/qxrp-val2", "name": "val2"},
    {"host": "167.233.55.43", "container": "qxrp-validator", "dir": "/var/lib/qxrp-validator", "name": "v1"},
    {"host": "204.168.175.194", "container": "qxrp-validator", "dir": "/var/lib/qxrp-validator", "name": "v2"},
    {"host": "89.167.109.241", "container": "qxrp-validator", "dir": "/var/lib/qxrp-validator", "name": "v3"},
    {"host": "5.78.142.246", "container": "qxrp-validator", "dir": "/var/lib/qxrp-validator", "name": "v4"},
    {"host": "192.241.247.158", "container": "qxrp-validator", "dir": "/var/lib/qxrp-validator", "name": "v5"},
]


def rpc(url, method, params=None):
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read())


def ssh(host, cmd):
    return subprocess.check_output(SSH + [f"root@{host}", cmd], text=True).strip()


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


def read_falcon_secret(host, vdir):
    keys_path = f"{vdir}/validator-keys.json"
    master_path = f"{vdir}/validator-master-seed"
    cfg = f"{vdir}/config/xrpld.cfg"

    try:
        keys = json.loads(ssh(host, f"cat {keys_path}"))
        if keys.get("falcon_secret"):
            return keys["falcon_secret"].strip()
    except Exception:
        keys = None

    try:
        master = ssh(host, f"cat {master_path}").strip()
        if master.startswith("FB"):
            return master
    except Exception:
        pass

    out = ssh(host, f"awk '/^\\[validation_falcon_secret\\]/{{getline; print; exit}}' {cfg}")
    secret = out.splitlines()[-1].strip()
    if not secret.startswith("FB"):
        raise RuntimeError(f"no falcon secret for {host}")
    return secret


def read_validator_meta(host, vdir):
    keys_path = f"{vdir}/validator-keys.json"
    falcon_secret = read_falcon_secret(host, vdir)
    falcon_pk = falcon_pubkey_from_secret(falcon_secret)

    try:
        keys = json.loads(ssh(host, f"cat {keys_path}"))
    except Exception:
        keys = {}

    falcon_account = falcon_account_from_secret(falcon_secret)
    keys_account = keys.get("account_address")
    account = keys_account or falcon_account
    falcon_pk = keys.get("falcon_public_key_hex") or falcon_pk
    consensus = keys.get("consensus_key_hex") or falcon_pk
    # Protocol requires identical Falcon keys for PublicKey and ConsensusKey.
    if not (consensus.startswith("FB") and len(consensus) >= 1796):
        consensus = falcon_pk

    # Legacy nodes stored a classical r-address; bond the Falcon operating account.
    if keys_account and keys_account != falcon_account:
        account = falcon_account

    sign_param, sign_value = "falcon_secret", falcon_secret
    return account, consensus, falcon_pk, sign_param, sign_value


def admin_rpc(host, container, method, params):
    payload = json.dumps({"method": method, "params": [params]})
    cmd = (
        f"docker exec {container} curl -sf -X POST http://127.0.0.1:5005 "
        f"-H 'Content-Type: application/json' -d {json.dumps(payload)}"
    )
    data = json.loads(ssh(host, cmd))
    if "result" not in data:
        raise RuntimeError(f"{method} on {host}/{container}: {data}")
    return data["result"]


def sign_on_node(host, container, tx_json, sign_param, sign_value):
    return admin_rpc(
        host, container, "sign", {"tx_json": tx_json, sign_param: sign_value}
    )


def submit_public(result):
    blob = result.get("tx_blob")
    if not blob:
        return result
    r = rpc(PUBLIC_RPC, "submit", {"tx_blob": blob})
    return r.get("result", r)


def submit_signed(result):
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
        r = rpc(PUBLIC_RPC, "account_info", {"account": account, "ledger_index": "validated"})
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
        try:
            r = rpc(
                PUBLIC_RPC,
                "account_objects",
                {"account": account, "ledger_index": "validated"},
            )
            for obj in r["result"].get("account_objects", []):
                if obj.get("LedgerEntryType") == "ValidatorBond":
                    return obj.get("BondStatus"), obj.get("BondedAmount")
        except Exception:
            pass
        return None, None


def wait_local_sync(host, container, min_seq=3):
    for _ in range(60):
        try:
            info = admin_rpc(host, container, "server_info", {})
            seq = int(info.get("info", {}).get("validated_ledger", {}).get("seq", 0))
            if seq >= min_seq:
                return seq
        except Exception:
            pass
        time.sleep(3)
    raise RuntimeError(f"{host}/{container} not synced")


def main():
    print("=== Fleet genesis bonding ===\n")
    info = rpc(PUBLIC_RPC, "server_info")["result"]["info"]
    print(f"Network: {info['server_state']}  ledger: {info.get('validated_ledger', {}).get('seq')}")

    genesis_seq = rpc(
        PUBLIC_RPC,
        "account_info",
        {"account": GENESIS_ACCOUNT, "ledger_index": "validated"},
    )["result"]["account_data"]["Sequence"]
    print(f"Genesis sequence: {genesis_seq}\n")

    for v in VALIDATORS:
        print(f"── {v['name']} @ {v['host']} ──")
        account, consensus, falcon_pk, sign_param, sign_value = read_validator_meta(
            v["host"], v["dir"]
        )
        print(f"  account:   {account}")
        print(f"  consensus: {consensus[:24]}…")
        print(f"  falcon:    {falcon_pk[:24]}…")

        wait_local_sync(v["host"], v["container"])

        status, bonded = bond_status(account)
        if status == 1:
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
            time.sleep(2)
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
                    "PublicKey": falcon_pk,
                    "ConsensusKey": consensus,
                    "Fee": "12",
                },
                sign_param,
                sign_value,
            )
            eng, sub = submit_signed(reg)
            if eng not in ("tesSUCCESS", "tecDUPLICATE", "terQUEUED"):
                print(f"  REGISTER FAILED: {eng} {sub.get('engine_result_message', '')}")
                sys.exit(1)
            if eng == "tesSUCCESS":
                print(f"  register: {eng} → {wait_tx(reg['tx_json']['hash'])}")
            else:
                print(f"  register: {eng}")
            time.sleep(3)

        print(f"  ValidatorBond ({MIN_BOND_DROPS} drops)…")
        bond = sign_on_node(
            v["host"],
            v["container"],
            {
                "TransactionType": "ValidatorBond",
                "Account": account,
                "ConsensusKey": consensus,
                "BondedAmount": str(MIN_BOND_DROPS),
                "Fee": "12",
            },
            sign_param,
            sign_value,
        )
        eng, sub = submit_signed(bond)
        if eng == "tecNO_PERMISSION":
            status, bonded = bond_status(account)
            if status == 1:
                print(f"  bond: already bonded ({bonded}) ✓")
            else:
                print(f"  bond: tecNO_PERMISSION (status={status})")
                sys.exit(1)
        elif eng not in ("tesSUCCESS", "terQUEUED"):
            status, bonded = bond_status(account)
            if status == 1:
                print(f"  bond: already bonded ({bonded}) ✓")
            else:
                print(f"  BOND FAILED: {eng} {sub.get('engine_result_message', '')}")
                sys.exit(1)
        else:
            print(f"  bond: {eng} → {wait_tx(bond['tx_json']['hash'])}")
        print()

    print("=== Bond summary ===")
    for v in VALIDATORS:
        account, _, _, _, _ = read_validator_meta(v["host"], v["dir"])
        status, bonded = bond_status(account)
        label = "BONDED" if status == 1 else f"status={status}"
        print(f"  {v['name']}: {account} {label} bonded={bonded}")


if __name__ == "__main__":
    main()