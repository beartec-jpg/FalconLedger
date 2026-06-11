#!/usr/bin/env python3
"""Falcon transaction signing proxy for qXRP.

Exposes POST /sign and POST /wallet_propose to Vercel (and other clients)
while keeping xrpld admin RPC (port 5005) on localhost inside the container.

Environment:
  SIGNER_PROXY_TOKEN  — required bearer token
  XRPLD_ADMIN_URL     — default http://127.0.0.1:5005 (use docker bridge IP from host)
  XRPLD_CONTAINER     — if set, run curl via `docker exec` (e.g. qxrp-full)
  LISTEN_PORT         — default 3001
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


TOKEN = os.environ.get("SIGNER_PROXY_TOKEN", "")
ADMIN_URL = os.environ.get("XRPLD_ADMIN_URL", "http://127.0.0.1:5005")
CONTAINER = os.environ.get("XRPLD_CONTAINER", "qxrp-full")
PORT = int(os.environ.get("LISTEN_PORT", "3001"))
NETWORK_ID = int(os.environ.get("NETWORK_ID", "1001"))


def admin_rpc(method: str, params: dict) -> dict:
    body = json.dumps({"method": method, "params": [params]})
    if CONTAINER:
        cmd = [
            "docker", "exec", CONTAINER,
            "curl", "-sf", "-X", "POST", ADMIN_URL,
            "-H", "Content-Type: application/json",
            "-d", body,
        ]
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    else:
        import urllib.request

        req = urllib.request.Request(
            ADMIN_URL,
            data=body.encode(),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        out = urllib.request.urlopen(req, timeout=30).read()

    data = json.loads(out)
    if "result" not in data:
        raise RuntimeError(data.get("error", "invalid RPC response"))
    result = data["result"]
    if result.get("error"):
        raise RuntimeError(result.get("error_message") or result.get("error"))
    return result


class Handler(BaseHTTPRequestHandler):
    server_version = "qxrp-falcon-signer/1.0"

    def log_message(self, fmt: str, *args) -> None:
        # Avoid logging request bodies (may contain secrets).
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _auth_ok(self) -> bool:
        if not TOKEN:
            return False
        auth = self.headers.get("Authorization", "")
        return auth == f"Bearer {TOKEN}"

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        return json.loads(raw or b"{}")

    def _json_response(self, status: int, payload: dict) -> None:
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path.rstrip("/") == "/health":
            self._json_response(200, {"ok": True})
            return
        self._json_response(404, {"error": "not found"})

    def do_POST(self) -> None:
        if not self._auth_ok():
            self._json_response(401, {"error": "unauthorized"})
            return

        path = self.path.rstrip("/")
        try:
            payload = self._read_json()
        except json.JSONDecodeError:
            self._json_response(400, {"error": "invalid JSON"})
            return

        try:
            if path == "/sign":
                self._handle_sign(payload)
            elif path == "/wallet_propose":
                self._handle_wallet_propose(payload)
            else:
                self._json_response(404, {"error": "not found"})
        except subprocess.CalledProcessError as e:
            self._json_response(502, {"error": f"admin RPC failed: {e.output.decode(errors='replace')[:200]}"})
        except RuntimeError as e:
            self._json_response(422, {"error": str(e)})

    def _handle_sign(self, payload: dict) -> None:
        tx_json = payload.get("tx_json")
        if not isinstance(tx_json, dict):
            self._json_response(400, {"error": "tx_json object required"})
            return

        secret = payload.get("falcon_secret") or payload.get("secret")
        if not secret or not isinstance(secret, str):
            self._json_response(400, {"error": "falcon_secret required"})
            return

        if NETWORK_ID > 1024 and "NetworkID" not in tx_json:  # noqa: PLR2004
            tx_json = {**tx_json, "NetworkID": NETWORK_ID}

        result = admin_rpc("sign", {"tx_json": tx_json, "falcon_secret": secret})
        self._json_response(200, {
            "tx_blob": result.get("tx_blob"),
            "hash": (result.get("tx_json") or {}).get("hash"),
        })

    def _handle_wallet_propose(self, payload: dict) -> None:
        params = {"key_type": payload.get("key_type", "falcon512")}
        result = admin_rpc("wallet_propose", params)
        self._json_response(200, {
            "account_id": result.get("account_id"),
            "public_key": result.get("public_key") or result.get("public_key_hex"),
            "key_type": result.get("key_type"),
            "falcon_secret": result.get("falcon_secret"),
        })


def main() -> None:
    if not TOKEN:
        print("FATAL: SIGNER_PROXY_TOKEN must be set", file=sys.stderr)
        sys.exit(1)

    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"qxrp-falcon-signer listening on :{PORT} (admin={ADMIN_URL}, container={CONTAINER or 'none'})")
    server.serve_forever()


if __name__ == "__main__":
    main()