#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Build FalconLedger xrpld image from current git tip and record the digest
# for ceremony / soak (item 1 of PROTOCOL_READINESS_7_5_PLAN).
#
# Usage:
#   bash scripts/ops/build-and-pin-image.sh [tag]
#   bash scripts/ops/build-and-pin-image.sh mainnet-v1
#
# Requires: docker, git. Run on a build host (not required in CI sandbox).
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TAG="${1:-mainnet-v1}"
IMAGE="qxrp/xrpld:${TAG}"
CEREMONY="$REPO_ROOT/scripts/mainnet-ceremony"
DOCKERFILE="$REPO_ROOT/docker/Dockerfile"

log()  { echo "[build-pin] $*"; }
die()  { echo "[build-pin] ERROR: $*" >&2; exit 1; }

command -v docker >/dev/null || die "docker not found — run this on a Docker build host"
command -v git >/dev/null || die "git not found"
[[ -f "$DOCKERFILE" ]] || die "missing $DOCKERFILE"

cd "$REPO_ROOT"
SHA="$(git rev-parse HEAD)"
SHORT="$(git rev-parse --short HEAD)"
DIRTY="$(git status --porcelain | head -c1 || true)"
if [[ -n "$DIRTY" ]]; then
  log "WARN: working tree dirty — image will still build from Dockerfile context"
fi

log "commit=$SHORT ($SHA)"
log "image=$IMAGE"

export DOCKER_BUILDKIT=1
docker build \
  -t "$IMAGE" \
  -f "$DOCKERFILE" \
  --label "org.opencontainers.image.revision=${SHA}" \
  --label "falcon.freeze.commit=${SHA}" \
  "$REPO_ROOT"

log "built $IMAGE — verifying version"
docker run --rm --entrypoint xrpld "$IMAGE" --version || \
  docker run --rm "$IMAGE" --version || \
  log "WARN: could not run xrpld --version (check entrypoint)"

# Prefer RepoDigest after a push; locally use Image Id if not pushed yet
DIGEST=""
if docker push "$IMAGE" 2>/dev/null; then
  log "pushed $IMAGE"
  DIGEST="$(docker inspect "$IMAGE" --format '{{index .RepoDigests 0}}' 2>/dev/null || true)"
fi
if [[ -z "$DIGEST" || "$DIGEST" == "<no value>" ]]; then
  ID="$(docker inspect "$IMAGE" --format '{{.Id}}')"
  DIGEST="${IMAGE}@local-${ID#sha256:}"
  log "WARN: no remote digest (push failed or skipped). Local id recorded: $ID"
  log "After successful push, re-run: docker inspect $IMAGE --format '{{index .RepoDigests 0}}'"
fi

mkdir -p "$CEREMONY"
{
  echo "# Auto-written by build-and-pin-image.sh"
  echo "# git: $SHA"
  echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# tag: $IMAGE"
  echo "$DIGEST"
} | tee "$CEREMONY/IMAGE_DIGEST.txt"

{
  echo "# Protocol freeze tip for this image"
  echo "$SHA"
  echo "# short: $SHORT"
  echo "# image: $IMAGE"
  echo "# digest-file: IMAGE_DIGEST.txt"
} | tee "$CEREMONY/FREEZE_COMMIT.txt"

log "Wrote $CEREMONY/IMAGE_DIGEST.txt"
log "Wrote $CEREMONY/FREEZE_COMMIT.txt"
log "All soak / mainnet nodes must use:"
log "  export QXRP_XRPLD_IMAGE='$(cat "$CEREMONY/IMAGE_DIGEST.txt" | grep -v '^#' | tail -1)'"
log "Done (item 1)."
