#!/usr/bin/env bash
# Daily airdrop score snapshot → portal admin API.
# Requires: AIRDROP_ADMIN_TOKEN, PORTAL_BASE_URL
# Optional: NETWORK (default mainnet), SNAP_DAY (UTC YYYY-MM-DD, default today)
set -euo pipefail

PORTAL_BASE_URL="${PORTAL_BASE_URL:-https://falcon.example}"
AIRDROP_ADMIN_TOKEN="${AIRDROP_ADMIN_TOKEN:?Set AIRDROP_ADMIN_TOKEN}"
NETWORK="${NETWORK:-mainnet}"
SNAP_DAY="${SNAP_DAY:-$(date -u +%Y-%m-%d)}"

url="${PORTAL_BASE_URL%/}/api/airdrop/snapshot?network=${NETWORK}"

echo "[airdrop-cron] $(date -u +%Y-%m-%dT%H:%M:%SZ) snapshot day=${SNAP_DAY} network=${NETWORK}"

code=$(curl -sS -o /tmp/airdrop-snapshot-resp.json -w '%{http_code}' \
  -X POST "$url" \
  -H "Authorization: Bearer ${AIRDROP_ADMIN_TOKEN}" \
  -H "Content-Type: application/json" \
  -d "{\"day\":\"${SNAP_DAY}\"}")

echo "[airdrop-cron] HTTP ${code}"
cat /tmp/airdrop-snapshot-resp.json
echo

if [[ "$code" != "200" && "$code" != "201" ]]; then
  exit 1
fi
