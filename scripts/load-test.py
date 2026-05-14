#!/usr/bin/env python3
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Transaction throughput benchmark for qXRP regtest.
# Submits parallel Payment transactions across all 5 validators,
# measures TPS and per-ledger latency.
#
# Usage:
#   python3 scripts/load-test.py                         # 20 accounts, 60 s
#   python3 scripts/load-test.py --accounts 50 --duration 120
#   python3 scripts/load-test.py --accounts 10 --rate 50 # cap at 50 tx/s

import argparse
import json
import sys
import time
import threading
import urllib.request
from concurrent.futures import ThreadPoolExecutor

# ── constants ──────────────────────────────────────────────────────────────────
DROPS_PER_QXRP   = 1_000_000
BASE_PORT        = 5005
N_PORTS          = 5
GENESIS_SECRET   = "masterpassphrase"
GENESIS_ACCOUNT  = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
PAYMENT_DROPS    = 1_000          # 0.001 qXRP per hop
FEE_DROPS        = 12
FUND_DROPS       = 50 * DROPS_PER_QXRP   # 50 qXRP per account
MAX_LEDGER_DELTA = 10             # LastLedgerSequence guard

# ── RPC ────────────────────────────────────────────────────────────────────────
def rpc(port, method, params=None):
    url  = f"http://127.0.0.1:{port}"
    body = json.dumps({"method": method, "params": [params or {}]}).encode()
    req  = urllib.request.Request(
        url, data=body, headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read())

def validated_ledger(port=BASE_PORT):
    return rpc(port, "server_info")["result"]["info"].get(
        "validated_ledger", {}
    ).get("seq", 0)

def account_sequence(port, address):
    r = rpc(port, "account_info", {
        "account": address, "ledger_index": "current"
    })
    return r["result"]["account_data"]["Sequence"]

# ── account setup ──────────────────────────────────────────────────────────────
def setup_accounts(n):
    """Derive and fund n load accounts from genesis. Returns list of dicts."""
    print(f"\nSetting up {n} load accounts...")
    genesis_seq = account_sequence(BASE_PORT, GENESIS_ACCOUNT)
    accounts = []
    for i in range(n):
        # Use passphrase-style key derivation; wallet_propose returns master_seed
        # (base58) which we store for use in submit.
        passphrase = f"qxrp-load-acct-{i}-regtest"
        wp = rpc(BASE_PORT, "wallet_propose",
                 {"passphrase": passphrase, "key_type": "secp256k1"})[
            "result"
        ]
        if "account_id" not in wp:
            print(f"  wallet_propose failed for account {i}: {wp.get('error_message', wp)}")
            sys.exit(1)
        accounts.append({
            "address": wp["account_id"],
            "secret":  wp["master_seed"],  # base58 seed for submit
            "seq":     1,
        })

    # Batch-fund: submit all from genesis without waiting
    hashes = []
    for i, acc in enumerate(accounts):
        r = rpc(BASE_PORT, "submit", {"tx_json": {
            "TransactionType": "Payment",
            "Account":         GENESIS_ACCOUNT,
            "Destination":     acc["address"],
            "Amount":          str(FUND_DROPS),
            "Fee":             str(FEE_DROPS),
            "Sequence":        genesis_seq + i,
        }, "secret": GENESIS_SECRET})["result"]
        eng = r.get("engine_result", "")
        if eng not in ("tesSUCCESS", "terQUEUED"):
            print(f"  fund {i}: {eng} — aborting")
            sys.exit(1)
        hashes.append(r["tx_json"]["hash"])
        if (i + 1) % 10 == 0:
            print(f"  funded {i + 1}/{n}...")

    # Wait for the last batch tx to validate
    print(f"  Waiting for funding txs to validate...", end="", flush=True)
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        try:
            r = rpc(BASE_PORT, "tx", {"transaction": hashes[-1], "binary": False})
            if r["result"].get("validated"):
                print(" done")
                break
        except Exception:
            pass
        time.sleep(1)
        print(".", end="", flush=True)
    else:
        print("\n  WARNING: timed out waiting for funding")

    # Read actual starting sequences for all accounts
    for acc in accounts:
        try:
            acc["seq"] = account_sequence(BASE_PORT, acc["address"])
        except Exception:
            acc["seq"] = 1

    return accounts

# ── load runner ────────────────────────────────────────────────────────────────
class LoadRunner:
    def __init__(self, accounts, duration, rate_cap):
        self.accounts   = accounts
        self.n          = len(accounts)
        self.duration   = duration
        self.rate_cap   = rate_cap   # tx/s total, 0 = unlimited
        self.stop       = threading.Event()

        # Stats
        self.submitted  = 0
        self.validated  = 0
        self.errors     = 0   # txs that used a seq slot and failed (fatal)
        self.retries    = 0   # transient failures (queue full, network)
        self.latencies  = []         # ledger deltas for confirmed txs
        self.lock       = threading.Lock()

        # pending hash → submit_ledger
        self.pending    = {}

        # Per-account seq lock
        self.seq_locks  = [threading.Lock() for _ in range(self.n)]
        self.seqs       = [acc["seq"] for acc in accounts]

    # ── submission worker (one per account) ───────────────────────────────────
    def _submit_worker(self, idx):
        acc      = self.accounts[idx]
        port     = BASE_PORT + (idx % N_PORTS)
        dest     = self.accounts[(idx + 1) % self.n]["address"]
        interval = (self.n / self.rate_cap) if self.rate_cap > 0 else 0

        while not self.stop.is_set():
            t0 = time.monotonic()

            # Read seq without advancing — only advance on success
            with self.seq_locks[idx]:
                seq = self.seqs[idx]

            # Compute LastLedgerSequence guard
            try:
                lls = validated_ledger(port) + MAX_LEDGER_DELTA
            except Exception:
                lls = 0

            tx = {
                "TransactionType":   "Payment",
                "Account":           acc["address"],
                "Destination":       dest,
                "Amount":            str(PAYMENT_DROPS),
                "Fee":               str(FEE_DROPS),
                "Sequence":          seq,
            }
            if lls:
                tx["LastLedgerSequence"] = lls

            try:
                r   = rpc(port, "submit", {"tx_json": tx, "secret": acc["secret"]})["result"]
                eng = r.get("engine_result", "")

                if eng in ("tesSUCCESS", "terQUEUED"):
                    # Success — advance seq and track hash
                    with self.seq_locks[idx]:
                        self.seqs[idx] += 1
                    with self.lock:
                        self.submitted += 1
                        self.pending[r["tx_json"]["hash"]] = validated_ledger(port)

                elif eng in ("telCAN_NOT_QUEUE_FULL", "telCAN_NOT_QUEUE",
                             "telINSUF_FEE_P"):
                    # Queue is full — backpressure, retry same seq after brief wait
                    with self.lock:
                        self.retries += 1
                    time.sleep(0.3)

                elif eng in ("tefPAST_SEQ", "tefALREADY"):
                    # Seq already used — advance and count as error
                    with self.seq_locks[idx]:
                        self.seqs[idx] += 1
                    with self.lock:
                        self.errors += 1

                elif eng.startswith("ter"):
                    # Retryable (no seq consumed) — brief wait, retry same seq
                    with self.lock:
                        self.retries += 1
                    time.sleep(0.1)

                else:
                    # Fatal error consuming a seq slot
                    with self.seq_locks[idx]:
                        self.seqs[idx] += 1
                    with self.lock:
                        self.errors += 1

            except Exception:
                with self.lock:
                    self.retries += 1
                time.sleep(0.1)

            if interval > 0:
                elapsed = time.monotonic() - t0
                remaining = interval - elapsed
                if remaining > 0:
                    time.sleep(remaining)

    # ── validation poller: reads each new ledger's tx list ───────────────────
    def _validation_poller(self):
        last_lseq = validated_ledger()

        while not self.stop.is_set() or self.pending:
            try:
                lseq = validated_ledger()
                if lseq > last_lseq:
                    for ls in range(last_lseq + 1, lseq + 1):
                        try:
                            r    = rpc(BASE_PORT, "ledger", {
                                "ledger_index": ls,
                                "transactions": True,
                                "expand":       False,
                            })
                            txs  = r.get("result", {}).get(
                                "ledger", {}
                            ).get("transactions", [])
                            with self.lock:
                                for h in txs:
                                    if h in self.pending:
                                        submit_ls = self.pending.pop(h)
                                        delta     = ls - submit_ls
                                        self.validated += 1
                                        if 0 <= delta <= 20:
                                            self.latencies.append(delta)
                        except Exception:
                            pass
                    last_lseq = lseq
            except Exception:
                pass
            time.sleep(0.3)

    # ── main run loop ─────────────────────────────────────────────────────────
    def run(self):
        rate_str = f"{self.rate_cap} tx/s cap" if self.rate_cap else "unlimited rate"
        print(
            f"\nLoad test: {self.n} accounts  ×  {N_PORTS} ports  |  "
            f"{self.duration}s  |  {rate_str}\n"
        )
        hdr = f"{'Elapsed':>8}  {'Submitted':>10}  {'Validated':>10}  {'Errors':>7}  "
        hdr += f"{'Sub/s':>7}  {'Val/s':>7}  {'Pending':>8}  {'Ledger':>7}"
        print(hdr)
        print("─" * len(hdr))

        poller = threading.Thread(target=self._validation_poller, daemon=True)
        poller.start()

        with ThreadPoolExecutor(max_workers=self.n) as pool:
            for i in range(self.n):
                pool.submit(self._submit_worker, i)

            t_start   = time.monotonic()
            prev_sub  = 0
            prev_val  = 0
            prev_t    = t_start

            while time.monotonic() - t_start < self.duration:
                time.sleep(2)
                now     = time.monotonic()
                elapsed = now - t_start
                dt      = now - prev_t

                with self.lock:
                    sub     = self.submitted
                    val     = self.validated
                    err     = self.errors
                    pending = len(self.pending)

                try:
                    lseq = validated_ledger()
                except Exception:
                    lseq = 0

                sub_rate = (sub - prev_sub) / dt
                val_rate = (val - prev_val) / dt

                print(
                    f"{elapsed:7.1f}s  {sub:>10,}  {val:>10,}  {err:>7,}  "
                    f"{sub_rate:>7.1f}  {val_rate:>7.1f}  {pending:>8,}  {lseq:>7}"
                )
                prev_sub = sub
                prev_val = val
                prev_t   = now

            self.stop.set()

        # Drain remaining validations (up to 2 ledger closes)
        print("\nDraining final validations (≤ 15 s)...")
        drain_deadline = time.monotonic() + 15
        while self.pending and time.monotonic() < drain_deadline:
            time.sleep(1)

        self._summary()

    def _summary(self):
        with self.lock:
            sub  = self.submitted
            val  = self.validated
            err  = self.errors
            ret  = self.retries
            lats = sorted(self.latencies)

        print()
        print("═" * 58)
        print("  qXRP Load Test — Final Results")
        print("═" * 58)
        print(f"  Total submitted  : {sub:,}")
        print(f"  Total validated  : {val:,}  ({100*val/sub:.1f}% of submitted)" if sub else "")
        print(f"  Fatal errors     : {err:,}  (seq slot consumed, tx failed)")
        print(f"  Retries          : {ret:,}  (queue-full / network transient)")
        print(f"  Unconfirmed      : {sub - val - err:,}  (in-flight or expired)")
        if lats:
            avg = sum(lats) / len(lats)
            p50 = lats[int(len(lats) * 0.50)]
            p95 = lats[int(len(lats) * 0.95)]
            p99 = lats[int(len(lats) * 0.99)]
            print(f"  Latency (ledgers):")
            print(f"    avg={avg:.2f}  min={lats[0]}  p50={p50}  p95={p95}  p99={p99}  max={lats[-1]}")
            print(f"    (1 ledger ≈ 3.5 s  →  avg ≈ {avg*3.5:.1f} s)")
        print("═" * 58)


# ── entry point ────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(
        description="qXRP regtest transaction load tester"
    )
    ap.add_argument(
        "--accounts", type=int, default=20,
        help="Number of concurrent sender accounts (default: 20)"
    )
    ap.add_argument(
        "--duration", type=int, default=60,
        help="Test duration in seconds (default: 60)"
    )
    ap.add_argument(
        "--rate", type=int, default=0,
        help="Max combined tx/s to submit; 0 = unlimited (default: 0)"
    )
    args = ap.parse_args()

    # Sanity check
    try:
        info   = rpc(BASE_PORT, "server_info")["result"]["info"]
        lseq   = info.get("validated_ledger", {}).get("seq", 0)
        state  = info.get("server_state", "unknown")
        queued = info.get("load_factor", 1)
        print(f"Network ready: ledger={lseq}  state={state}  load_factor={queued}")
    except Exception as e:
        print(f"Cannot reach validator on port {BASE_PORT}: {e}")
        sys.exit(1)

    accounts = setup_accounts(args.accounts)
    runner   = LoadRunner(accounts, args.duration, args.rate)
    runner.run()


if __name__ == "__main__":
    main()
