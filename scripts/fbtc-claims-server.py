#!/usr/bin/env python3
"""
Minimal HTTP claims intake for FBTC (runs on coordinator).
POST /claim  JSON { falcon_account, btc_txid, amount_sats }
GET  /health

Env:
  FBTC_CLAIMS_FILE  default /var/lib/qxrp-bridge/fbtc_claims.json
  LISTEN_HOST       default 127.0.0.1
  LISTEN_PORT       default 8099
  CLAIMS_TOKEN      optional Bearer token
"""

from __future__ import annotations

import json
import os
import re
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlparse

CLAIMS = Path(os.environ.get("FBTC_CLAIMS_FILE", "/var/lib/qxrp-bridge/fbtc_claims.json"))
TOKEN = os.environ.get("CLAIMS_TOKEN", "").strip()
FALCON_RE = re.compile(r"^r[1-9A-HJ-NP-Za-km-z]{24,34}$")
TXID_RE = re.compile(r"^[0-9a-fA-F]{64}$")


def load() -> dict:
    if not CLAIMS.exists():
        return {"pending": {}}
    return json.loads(CLAIMS.read_text())


def save(data: dict) -> None:
    CLAIMS.parent.mkdir(parents=True, exist_ok=True)
    CLAIMS.write_text(json.dumps(data, indent=2) + "\n")


class Handler(BaseHTTPRequestHandler):
    def _json(self, code: int, obj: dict) -> None:
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _auth(self) -> bool:
        if not TOKEN:
            return True
        h = self.headers.get("Authorization", "")
        return h == f"Bearer {TOKEN}"

    def do_GET(self) -> None:
        if urlparse(self.path).path in ("/health", "/"):
            self._json(200, {"ok": True, "service": "fbtc-claims"})
            return
        self._json(404, {"error": "not found"})

    def do_POST(self) -> None:
        if urlparse(self.path).path not in ("/claim", "/"):
            self._json(404, {"error": "not found"})
            return
        if not self._auth():
            self._json(401, {"error": "unauthorized"})
            return
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n) if n else b"{}"
        try:
            body = json.loads(raw.decode())
        except Exception:
            self._json(400, {"error": "invalid json"})
            return
        falcon = str(body.get("falcon_account") or "").strip()
        txid = str(body.get("btc_txid") or "").strip().lower()
        try:
            amount_sats = int(body.get("amount_sats") or 0)
        except Exception:
            amount_sats = 0
        if not FALCON_RE.match(falcon):
            self._json(400, {"error": "invalid falcon_account"})
            return
        if not TXID_RE.match(txid):
            self._json(400, {"error": "invalid btc_txid"})
            return
        if amount_sats < 546:
            self._json(400, {"error": "amount_sats dust"})
            return
        data = load()
        pending = data.setdefault("pending", {})
        if txid not in pending:
            from time import strftime, gmtime
            pending[txid] = {
                "falcon_account": falcon,
                "btc_txid": txid,
                "amount_sats": amount_sats,
                "queued_at": strftime("%Y-%m-%dT%H:%M:%SZ", gmtime()),
            }
            save(data)
        self._json(200, {"ok": True, "claim_id": txid})

    def log_message(self, fmt: str, *args) -> None:
        print(f"[claims] {args[0] if args else fmt}", flush=True)


def main() -> None:
    host = os.environ.get("LISTEN_HOST", "127.0.0.1")
    port = int(os.environ.get("LISTEN_PORT", "8099"))
    print(f"FBTC claims listening on {host}:{port} file={CLAIMS}", flush=True)
    HTTPServer((host, port), Handler).serve_forever()


if __name__ == "__main__":
    main()
