#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""
Light network traffic generator for live testnet activity.

Creates WALLETS_PER_SERVER deterministic wallets per fleet host, funds from
genesis, runs a low baseline of random Payments with occasional bursts, and
refills wallets before they run dry.

Designed to run on the coordinator (46.224.0.140) with admin RPC via docker.

Usage:
  python3 scripts/network-traffic.py --dry-run
  python3 scripts/network-traffic.py --daemon
"""
from __future__ import annotations

import argparse
import json
import os
import random
import subprocess
import sys
import time
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

DROPS_PER_QXRP = 1_000_000
GENESIS_ACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
GENESIS_SECRET = "masterpassphrase"
FEE_DROPS = 12
PAYMENT_DROPS = 10_000  # 0.01 qXRP
FUND_DROPS = 50 * DROPS_PER_QXRP
REFILL_DROPS = 30 * DROPS_PER_QXRP
MIN_BALANCE_DROPS = 5 * DROPS_PER_QXRP
MAX_LEDGER_DELTA = 20

DEFAULT_SERVERS = [
    "val2",
    "v167",
    "v204",
    "v89",
    "val5",
    "nyc",
]

DEFAULT_ADMIN = "http://127.0.0.1:5005"
DEFAULT_PUBLIC = "http://46.224.0.140:6005"
DEFAULT_CONTAINER = "qxrp-full"
DEFAULT_DATA = "/var/lib/qxrp-traffic"


@dataclass
class Wallet:
    server: str
    index: int
    address: str
    falcon_secret: str
    seq: int = 0


@dataclass
class TrafficStats:
    started_at: float = field(default_factory=time.time)
    submitted: int = 0
    validated: int = 0
    errors: int = 0
    refills: int = 0
    pumps: int = 0
    last_tx_at: float | None = None
    in_pump: bool = False
    baseline_interval_s: float = 0.0

    def to_dict(self, wallets: int) -> dict[str, Any]:
        uptime = max(1.0, time.time() - self.started_at)
        return {
            "updated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "uptime_seconds": int(uptime),
            "wallets": wallets,
            "submitted": self.submitted,
            "validated": self.validated,
            "errors": self.errors,
            "refills": self.refills,
            "pumps": self.pumps,
            "tx_per_min": round(self.validated / uptime * 60, 2),
            "in_pump": self.in_pump,
            "baseline_interval_s": round(self.baseline_interval_s, 1),
            "last_tx_at": (
                time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(self.last_tx_at))
                if self.last_tx_at
                else None
            ),
        }


class RpcClient:
    def __init__(self, admin_url: str, public_url: str, container: str = ""):
        self.admin_url = admin_url
        self.public_url = public_url
        self.container = container

    def _post(self, url: str, method: str, params: dict | None = None) -> dict:
        payload = json.dumps({"method": method, "params": [params or {}]}).encode()
        if self.container:
            cmd = [
                "docker",
                "exec",
                self.container,
                "curl",
                "-sf",
                "-X",
                "POST",
                url,
                "-H",
                "Content-Type: application/json",
                "-d",
                payload.decode(),
            ]
            out = subprocess.check_output(cmd, text=True)
            body = json.loads(out)
        else:
            req = urllib.request.Request(
                url,
                data=payload,
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=20) as resp:
                body = json.loads(resp.read())
        if body.get("error"):
            raise RuntimeError(body["error"])
        return body.get("result", body)

    def admin(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.admin_url, method, params)

    def public(self, method: str, params: dict | None = None) -> dict:
        return self._post(self.public_url, method, params)

    def submit_signed(self, result: dict) -> str:
        blob = result.get("tx_blob")
        if not blob:
            eng = result.get("engine_result", "no_blob")
            raise RuntimeError(eng)
        sub = self.public("submit", {"tx_blob": blob})
        return sub.get("engine_result", sub.get("error", "unknown"))


class NetworkTraffic:
    def __init__(
        self,
        rpc: RpcClient,
        data_dir: Path,
        servers: list[str],
        wallets_per_server: int,
        baseline_min: float,
        baseline_max: float,
        pump_min_interval: float,
        pump_max_interval: float,
        pump_size_min: int,
        pump_size_max: int,
    ):
        self.rpc = rpc
        self.data_dir = data_dir
        self.servers = servers
        self.wallets_per_server = wallets_per_server
        self.baseline_min = baseline_min
        self.baseline_max = baseline_max
        self.pump_min_interval = pump_min_interval
        self.pump_max_interval = pump_max_interval
        self.pump_size_min = pump_size_min
        self.pump_size_max = pump_size_max
        self.wallets: list[Wallet] = []
        self.stats = TrafficStats()
        self.genesis_seq = 1
        self._next_pump_at = time.time() + random.uniform(
            pump_min_interval, pump_max_interval
        )
        self._pending_hashes: dict[str, float] = {}

        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.wallets_file = self.data_dir / "wallets.json"
        self.stats_file = self.data_dir / "stats.json"

    def _save_wallets(self) -> None:
        payload = [
            {
                "server": w.server,
                "index": w.index,
                "address": w.address,
                "falcon_secret": w.falcon_secret,
                "seq": w.seq,
            }
            for w in self.wallets
        ]
        self.wallets_file.write_text(json.dumps(payload, indent=2))
        os.chmod(self.wallets_file, 0o600)

    def _write_stats(self) -> None:
        self.stats_file.write_text(
            json.dumps(self.stats.to_dict(len(self.wallets)), indent=2)
        )

    def _ledger_seq(self) -> int:
        info = self.rpc.admin("server_info")["info"]
        return int(info.get("validated_ledger", {}).get("seq", 0))

    def _account_balance(self, address: str) -> int:
        try:
            r = self.rpc.public(
                "account_info",
                {"account": address, "ledger_index": "validated"},
            )
            return int(r["account_data"]["Balance"])
        except Exception:
            return 0

    def _account_seq(self, address: str) -> int:
        try:
            r = self.rpc.public(
                "account_info",
                {"account": address, "ledger_index": "validated"},
            )
            return int(r["account_data"]["Sequence"])
        except Exception:
            return 0

    def _derive_wallets(self) -> list[Wallet]:
        wallets: list[Wallet] = []
        for server in self.servers:
            for i in range(self.wallets_per_server):
                wp = self.rpc.admin("wallet_propose", {"key_type": "falcon512"})
                secret = wp.get("falcon_secret") or wp.get("master_key")
                if not secret or "account_id" not in wp:
                    raise RuntimeError(f"wallet_propose failed for {server}-{i}: {wp}")
                wallets.append(
                    Wallet(
                        server=server,
                        index=i,
                        address=wp["account_id"],
                        falcon_secret=secret,
                    )
                )
                time.sleep(0.05)
        return wallets

    def _load_or_create_wallets(self) -> None:
        if self.wallets_file.exists():
            raw = json.loads(self.wallets_file.read_text())
            self.wallets = [Wallet(**row) for row in raw]
            for w in self.wallets:
                w.seq = self._account_seq(w.address) or w.seq or 1
            print(f"Loaded {len(self.wallets)} wallets from {self.wallets_file}")
            return

        self.wallets = self._derive_wallets()
        print(f"Derived {len(self.wallets)} wallets")

    def _fund_wallet(self, wallet: Wallet, amount: int) -> bool:
        tx = {
            "TransactionType": "Payment",
            "Account": GENESIS_ACCOUNT,
            "Destination": wallet.address,
            "Amount": str(amount),
            "Sequence": self.genesis_seq,
            "Fee": str(FEE_DROPS),
        }
        signed = self.rpc.admin(
            "sign",
            {"tx_json": tx, "secret": GENESIS_SECRET},
        )
        eng = self.rpc.submit_signed(signed)
        if eng in ("tesSUCCESS", "terQUEUED"):
            self.genesis_seq += 1
            return True
        print(f"  fund failed {wallet.server}-{wallet.index}: {eng}")
        return False

    def _initial_fund_all(self) -> None:
        genesis = self.rpc.public(
            "account_info",
            {"account": GENESIS_ACCOUNT, "ledger_index": "validated"},
        )
        self.genesis_seq = int(genesis["account_data"]["Sequence"])
        need = [w for w in self.wallets if self._account_balance(w.address) < MIN_BALANCE_DROPS]
        if not need:
            print("All wallets already funded")
            return
        print(f"Funding {len(need)} wallets ({FUND_DROPS // DROPS_PER_QXRP} qXRP each)...")
        for w in need:
            if self._fund_wallet(w, FUND_DROPS):
                w.seq = 1
            time.sleep(0.15)
        time.sleep(3)
        for w in self.wallets:
            w.seq = self._account_seq(w.address) or w.seq
        self._save_wallets()

    def _refill_low_wallets(self) -> None:
        for w in self.wallets:
            bal = self._account_balance(w.address)
            if bal >= MIN_BALANCE_DROPS:
                continue
            print(f"Refill {w.server}-{w.index} (balance {bal / DROPS_PER_QXRP:.2f} qXRP)")
            if self._fund_wallet(w, REFILL_DROPS):
                self.stats.refills += 1
                w.seq = self._account_seq(w.address) or w.seq
            time.sleep(0.2)
        self._save_wallets()

    def _poll_validated(self) -> None:
        if not self._pending_hashes:
            return
        done = []
        for tx_hash, submitted_at in list(self._pending_hashes.items()):
            try:
                r = self.rpc.public(
                    "tx",
                    {"transaction": tx_hash, "binary": False},
                )
                if r.get("validated"):
                    self.stats.validated += 1
                    done.append(tx_hash)
            except Exception:
                if time.time() - submitted_at > 120:
                    done.append(tx_hash)
        for h in done:
            self._pending_hashes.pop(h, None)

    def _send_payment(self, src: Wallet, dst: Wallet) -> bool:
        lseq = self._ledger_seq()
        tx = {
            "TransactionType": "Payment",
            "Account": src.address,
            "Destination": dst.address,
            "Amount": str(PAYMENT_DROPS),
            "Fee": str(FEE_DROPS),
            "Sequence": src.seq,
            "LastLedgerSequence": lseq + MAX_LEDGER_DELTA,
        }
        signed = self.rpc.admin(
            "sign", {"tx_json": tx, "falcon_secret": src.falcon_secret}
        )
        eng = self.rpc.submit_signed(signed)
        if eng in ("tesSUCCESS", "terQUEUED"):
            src.seq += 1
            self.stats.submitted += 1
            self.stats.last_tx_at = time.time()
            tx_hash = signed.get("tx_json", {}).get("hash")
            if tx_hash:
                self._pending_hashes[tx_hash] = time.time()
            return True
        if eng in ("tecUNFUNDED_PAYMENT", "tecINSUFFICIENT_RESERVE"):
            src.seq = self._account_seq(src.address) or src.seq
        self.stats.errors += 1
        return False

    def _random_payment(self) -> None:
        if len(self.wallets) < 2:
            return
        src, dst = random.sample(self.wallets, 2)
        self._send_payment(src, dst)

    def _run_pump(self, count: int) -> None:
        self.stats.in_pump = True
        self.stats.pumps += 1
        self._write_stats()
        print(f"Pump: {count} txs")
        for _ in range(count):
            self._random_payment()
            self._poll_validated()
            time.sleep(random.uniform(0.3, 0.8))
        self.stats.in_pump = False

    def run(self, daemon: bool) -> None:
        self._load_or_create_wallets()
        self._initial_fund_all()
        self._save_wallets()
        self._write_stats()

        if not daemon:
            print("Dry run complete — wallets ready, no traffic loop")
            return

        print(
            f"Traffic daemon started: baseline {self.baseline_min}-{self.baseline_max}s, "
            f"{len(self.wallets)} wallets"
        )
        next_baseline = time.time() + random.uniform(self.baseline_min, self.baseline_max)
        next_refill = time.time() + 120

        while True:
            now = time.time()
            self._poll_validated()

            if now >= next_refill:
                self._refill_low_wallets()
                next_refill = now + 180

            if now >= self._next_pump_at:
                count = random.randint(self.pump_size_min, self.pump_size_max)
                self._run_pump(count)
                self._next_pump_at = now + random.uniform(
                    self.pump_min_interval, self.pump_max_interval
                )

            if now >= next_baseline:
                self._random_payment()
                interval = random.uniform(self.baseline_min, self.baseline_max)
                self.stats.baseline_interval_s = interval
                next_baseline = now + interval

            self._save_wallets()
            self._write_stats()
            time.sleep(1)


def main() -> None:
    p = argparse.ArgumentParser(description="qXRP testnet traffic generator")
    p.add_argument("--daemon", action="store_true", help="Run continuous traffic loop")
    p.add_argument("--dry-run", action="store_true", help="Setup wallets only")
    p.add_argument("--data-dir", default=os.environ.get("TRAFFIC_DATA_DIR", DEFAULT_DATA))
    p.add_argument("--admin-url", default=os.environ.get("ADMIN_RPC_URL", DEFAULT_ADMIN))
    p.add_argument("--public-url", default=os.environ.get("PUBLIC_RPC_URL", DEFAULT_PUBLIC))
    p.add_argument("--container", default=os.environ.get("XRPLD_CONTAINER", DEFAULT_CONTAINER))
    p.add_argument("--wallets-per-server", type=int, default=5)
    p.add_argument("--baseline-min", type=float, default=8.0)
    p.add_argument("--baseline-max", type=float, default=25.0)
    p.add_argument("--pump-interval-min", type=float, default=300.0)
    p.add_argument("--pump-interval-max", type=float, default=900.0)
    p.add_argument("--pump-size-min", type=int, default=6)
    p.add_argument("--pump-size-max", type=int, default=18)
    p.add_argument(
        "--servers",
        default=",".join(DEFAULT_SERVERS),
        help="Comma-separated server labels (6 × 5 = 30 wallets)",
    )
    args = p.parse_args()

    rpc = RpcClient(args.admin_url, args.public_url, args.container)
    traffic = NetworkTraffic(
        rpc=rpc,
        data_dir=Path(args.data_dir),
        servers=[s.strip() for s in args.servers.split(",") if s.strip()],
        wallets_per_server=args.wallets_per_server,
        baseline_min=args.baseline_min,
        baseline_max=args.baseline_max,
        pump_min_interval=args.pump_interval_min,
        pump_max_interval=args.pump_interval_max,
        pump_size_min=args.pump_size_min,
        pump_size_max=args.pump_size_max,
    )
    traffic.run(daemon=args.daemon and not args.dry_run)


if __name__ == "__main__":
    main()