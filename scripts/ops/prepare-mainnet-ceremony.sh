#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Scaffold / refresh the offline mainnet ceremony pack from this repo.
# Does NOT generate secrets. Does NOT push images. Safe to re-run.
#
# Usage:
#   bash scripts/ops/prepare-mainnet-ceremony.sh
#   bash scripts/ops/prepare-mainnet-ceremony.sh /path/to/offline/ceremony-pack
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST="${1:-$REPO_ROOT/scripts/mainnet-ceremony}"
SRC="$REPO_ROOT/scripts/mainnet-ceremony"

log()  { echo "[ceremony] $*"; }
warn() { echo "[ceremony] WARN: $*" >&2; }

mkdir -p \
  "$DEST/wallets" \
  "$DEST/validators/secrets" \
  "$DEST/dry-runs" \
  "$DEST/roles" \
  "$DEST/cfg"

# Copy templates if missing (never overwrite real secrets)
copy_if_absent() {
  local from="$1" to="$2"
  if [[ -e "$to" ]]; then
    log "keep existing $to"
  else
    cp -a "$from" "$to"
    log "created $to"
  fi
}

# Refresh non-secret docs (skip self-copy when DEST is the in-repo pack)
same_pack=0
if [[ "$(cd "$DEST" && pwd)" == "$(cd "$SRC" && pwd)" ]]; then
  same_pack=1
fi

if [[ "$same_pack" -eq 0 ]]; then
  cp -a "$SRC/README.md" "$DEST/README.md"
  cp -a "$SRC/T0-RUNBOOK.md" "$DEST/T0-RUNBOOK.md"
  [[ -f "$SRC/DEV_CUSTODY.md" ]] && cp -a "$SRC/DEV_CUSTODY.md" "$DEST/DEV_CUSTODY.md"
  cp -a "$SRC/NETWORK_ID.txt" "$DEST/NETWORK_ID.txt"
  [[ -f "$SRC/DNS_RPC.md" ]] && cp -a "$SRC/DNS_RPC.md" "$DEST/DNS_RPC.md"
  copy_if_absent "$SRC/portal.env.mainnet.example" "$DEST/portal.env.mainnet.example"
  copy_if_absent "$SRC/IMAGE_DIGEST.txt.example" "$DEST/IMAGE_DIGEST.txt.example"
  copy_if_absent "$SRC/wallets/ADDRESSES.txt" "$DEST/wallets/ADDRESSES.txt"
  copy_if_absent "$SRC/validators/unl-public.txt.example" "$DEST/validators/unl-public.txt.example"
  copy_if_absent "$SRC/validators/peers.txt.example" "$DEST/validators/peers.txt.example"
  copy_if_absent "$SRC/roles/ROLES.md" "$DEST/roles/ROLES.md"
  copy_if_absent "$SRC/dry-runs/notes.md" "$DEST/dry-runs/notes.md"
fi

cp -a "$REPO_ROOT/docs/MAINNET_GO_LIVE_CHECKLIST.md" "$DEST/CHECKLIST.md"
cp -a "$REPO_ROOT/docs/MAINNET_SECURITY_FREEZE.md" "$DEST/SECURITY_FREEZE.md"
[[ -f "$REPO_ROOT/docs/MAINNET_REHEARSAL.md" ]] && cp -a "$REPO_ROOT/docs/MAINNET_REHEARSAL.md" "$DEST/REHEARSAL.md"
[[ -f "$REPO_ROOT/docs/ops/MAINNET_OPS_RUNBOOK.md" ]] && cp -a "$REPO_ROOT/docs/ops/MAINNET_OPS_RUNBOOK.md" "$DEST/OPS_RUNBOOK.md"

# Mainnet cfg templates
cp -a "$REPO_ROOT/cfg/mainnet/falcon-validator.cfg.example" "$DEST/cfg/" 2>/dev/null || true
cp -a "$REPO_ROOT/cfg/mainnet/validators.txt.example" "$DEST/cfg/" 2>/dev/null || true

# Record freeze commit from current repo
SHA="$(git -C "$REPO_ROOT" rev-parse HEAD)"
SHORT="$(git -C "$REPO_ROOT" rev-parse --short HEAD)"
cat > "$DEST/FREEZE_COMMIT.txt" <<EOF
# Auto-written by prepare-mainnet-ceremony.sh — update after final merge.
# Image build must use this commit (or later freeze tag).
$SHA
# short: $SHORT
# date:  $(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF
log "FREEZE_COMMIT.txt = $SHORT"

# Network id check
NID="$(grep -E '^[0-9]+$' "$DEST/NETWORK_ID.txt" | head -1 || true)"
if [[ -z "${NID:-}" ]]; then
  warn "NETWORK_ID.txt has no numeric id"
else
  log "network id = $NID"
  if [[ "$NID" == "1001" ]]; then
    warn "1001 is testnet — mainnet should be 1026 (or your locked id)"
  fi
fi

# Offline split plan — sample XRPL-shaped addresses prove script path works.
# Replace with real ceremony addresses when wallets/ADDRESSES.txt is filled.
if [[ -x "$(command -v python3)" ]]; then
  python3 "$REPO_ROOT/scripts/mainnet-genesis-split.py" --offline-plan \
    --genesis rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh \
    --airdrop rN7n7otQDd6FczFgLdLN8Nua9eK2vmH1i1 \
    --faucet rLNaPoKeeBjZe2qs6x52yVPZpZ8td4dc6w \
    --dev rPEPPER7kfTD9w2To4CQk6UCfuHM9c6GDY \
    | tee "$DEST/dry-runs/genesis-split-offline-plan.out.txt" || true
  log "wrote dry-runs/genesis-split-offline-plan.out.txt (sample addrs — re-run with real keys)"
fi

# Secret hygiene
if [[ -f "$DEST/wallets/GENESIS.secret" ]] || [[ -f "$DEST/wallets/DEV.secret" ]]; then
  warn "secret files present under $DEST/wallets — ensure this path is NOT a public git repo"
fi

# .gitignore hint for offline pack copy
cat > "$DEST/.gitignore" <<'EOF'
# Never commit secrets if this folder is inside a git worktree
*.secret
portal.env.mainnet
IMAGE_DIGEST.txt
validators/secrets/**
!validators/secrets/.gitkeep
EOF
mkdir -p "$DEST/validators/secrets"
touch "$DEST/validators/secrets/.gitkeep"

log "Ceremony pack ready at: $DEST"
log "Next (human / offline machine):"
log "  1. Build image from FREEZE_COMMIT → write IMAGE_DIGEST.txt"
log "  2. Generate Falcon wallets on freeze image → fill wallets/ADDRESSES.txt + *.secret"
log "  3. Generate N validator falcon secrets → validators/secrets/ + unl-public.txt"
log "  4. Fill portal.env.mainnet (LIVE=false) + Neon schema docs/sql/airdrop-schema.sql"
log "  5. Private dress rehearsal (MAINNET_REHEARSAL.md) then wipe"
log "  6. T0: T0-RUNBOOK.md"
