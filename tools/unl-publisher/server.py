#!/usr/bin/env python3
# =============================================================================
# UNL Publisher — serves a canonical qXRP validators list over HTTP
# =============================================================================
# Validators fetch this list to know which nodes to trust (UNL / Unique Node List).
#
# Usage:
#   pip install fastapi uvicorn
#   ADMIN_TOKEN=changeme python3 tools/unl-publisher/server.py
#
# API:
#   GET  /validators.txt           — plaintext list (used in xrpld.cfg)
#   GET  /validators.json          — JSON with metadata
#   GET  /health                   — health check
#   POST /admin/validators         — add a key  (requires X-Admin-Token header)
#   DELETE /admin/validators/{key} — remove a key
#
# xrpld.cfg integration (on each validator/full node):
#   [validators_file]
#   /etc/qxrp/validators.txt
#
#   Then run fetch-unl.sh via cron to keep it updated:
#   */10 * * * * bash /opt/qxrp/bin/fetch-unl.sh

import json
import os
import time
import hashlib
from pathlib import Path
from typing import Annotated

from fastapi import FastAPI, HTTPException, Depends, Header, Body
from fastapi.responses import PlainTextResponse
import uvicorn

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
ADMIN_TOKEN   = os.environ.get("ADMIN_TOKEN", "changeme")
DATA_FILE     = Path(os.environ.get("UNL_DATA_FILE", "data/validators.json"))
LISTEN_HOST   = os.environ.get("HOST", "0.0.0.0")
LISTEN_PORT   = int(os.environ.get("PORT", "8090"))
NETWORK_ID    = int(os.environ.get("NETWORK_ID", "999"))
NETWORK_NAME  = os.environ.get("NETWORK_NAME", "qXRP Testnet")

DATA_FILE.parent.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Persistence helpers
# ---------------------------------------------------------------------------
def load_data() -> dict:
    if DATA_FILE.exists():
        return json.loads(DATA_FILE.read_text())
    return {"validators": [], "sequence": 1, "updated_at": 0}

def save_data(data: dict) -> None:
    data["updated_at"] = int(time.time())
    data["sequence"]  += 1
    tmp = DATA_FILE.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, indent=2))
    tmp.rename(DATA_FILE)

def validators_txt(keys: list[str]) -> str:
    lines = ["[validators]"] + keys
    return "\n".join(lines) + "\n"

def checksum(keys: list[str]) -> str:
    return hashlib.sha256("\n".join(sorted(keys)).encode()).hexdigest()[:12]

# ---------------------------------------------------------------------------
# Auth
# ---------------------------------------------------------------------------
def require_admin(x_admin_token: Annotated[str | None, Header()] = None) -> None:
    if x_admin_token != ADMIN_TOKEN:
        raise HTTPException(status_code=401, detail="Invalid or missing X-Admin-Token")

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
app = FastAPI(title="qXRP UNL Publisher", docs_url="/docs")

@app.get("/health")
def health():
    data = load_data()
    return {
        "ok": True,
        "network_id": NETWORK_ID,
        "network_name": NETWORK_NAME,
        "validator_count": len(data["validators"]),
        "sequence": data["sequence"],
        "updated_at": data["updated_at"],
    }

@app.get("/validators.txt", response_class=PlainTextResponse)
def get_validators_txt():
    """Plaintext format — use directly in xrpld.cfg [validators_file]."""
    data = load_data()
    return PlainTextResponse(
        validators_txt(data["validators"]),
        headers={"X-Sequence": str(data["sequence"]), "X-Checksum": checksum(data["validators"])},
    )

@app.get("/validators.json")
def get_validators_json():
    """JSON format with metadata."""
    data = load_data()
    return {
        "network_id": NETWORK_ID,
        "network_name": NETWORK_NAME,
        "sequence": data["sequence"],
        "updated_at": data["updated_at"],
        "checksum": checksum(data["validators"]),
        "validators": data["validators"],
    }

@app.post("/admin/validators", dependencies=[Depends(require_admin)])
def add_validator(body: dict = Body(...)):
    """Add a validator public key. Body: { "key": "nXXXX..." }"""
    key = body.get("key", "").strip()
    if not key:
        raise HTTPException(400, "Missing 'key' field")
    if not key.startswith("n"):
        raise HTTPException(400, "Key must be a base58 validator public key starting with 'n'")
    data = load_data()
    if key in data["validators"]:
        return {"message": "Key already present", "key": key}
    data["validators"].append(key)
    save_data(data)
    return {"message": "Key added", "key": key, "total": len(data["validators"])}

@app.delete("/admin/validators/{key}", dependencies=[Depends(require_admin)])
def remove_validator(key: str):
    """Remove a validator public key."""
    data = load_data()
    if key not in data["validators"]:
        raise HTTPException(404, "Key not found")
    data["validators"].remove(key)
    save_data(data)
    return {"message": "Key removed", "key": key, "total": len(data["validators"])}

@app.get("/admin/validators", dependencies=[Depends(require_admin)])
def list_validators():
    data = load_data()
    return data

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    print(f"[unl-publisher] Starting on http://{LISTEN_HOST}:{LISTEN_PORT}")
    print(f"[unl-publisher] Network: {NETWORK_NAME} (ID {NETWORK_ID})")
    print(f"[unl-publisher] Data file: {DATA_FILE}")
    uvicorn.run("server:app", host=LISTEN_HOST, port=LISTEN_PORT, reload=False)
