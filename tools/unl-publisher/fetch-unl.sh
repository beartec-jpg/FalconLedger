#!/usr/bin/env bash
# fetch-unl.sh — fetch the latest validators list from the UNL publisher
# Run via cron every 10 minutes on each node:
#   */10 * * * * bash /opt/qxrp/bin/fetch-unl.sh
#
# After the file is updated xrpld picks it up automatically on next read
# (no restart needed — it re-reads validators_file at each consensus round).

set -euo pipefail

UNL_URL="${UNL_URL:-https://unl.qxrp.network/validators.txt}"
LOCAL_FILE="${LOCAL_FILE:-/var/lib/qxrp/validator/validators.txt}"
BACKUP_FILE="${LOCAL_FILE}.bak"

# Download to a temp file first, then atomically replace
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

if curl -fsSL --max-time 10 "$UNL_URL" -o "$TMP"; then
    # Basic sanity check — must contain [validators]
    grep -q '^\[validators\]' "$TMP" || {
        echo "[fetch-unl] Invalid response from $UNL_URL — keeping existing file"
        exit 1
    }
    # Backup existing
    [[ -f "$LOCAL_FILE" ]] && cp "$LOCAL_FILE" "$BACKUP_FILE"
    # Atomic replace
    mv "$TMP" "$LOCAL_FILE"
    echo "[fetch-unl] Updated $LOCAL_FILE ($(grep -c '^n' "$LOCAL_FILE") keys)"
else
    echo "[fetch-unl] Failed to fetch $UNL_URL — keeping existing file"
    exit 1
fi
