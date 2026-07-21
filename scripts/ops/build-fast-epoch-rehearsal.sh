#!/usr/bin/env bash
# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Build a REHEARSAL-ONLY xrpld image with short epochs so emissions / ClaimReward
# can be proven in hours instead of ~8 weeks (mainnet quiet period).
#
# NEVER use this image for real mainnet T0.
# Production launch image: qxrp/xrpld:mainnet-v1 (default epoch 172800, first emission 8).
#
# Usage (Docker build host):
#   bash scripts/ops/build-fast-epoch-rehearsal.sh
#   bash scripts/ops/build-fast-epoch-rehearsal.sh 256 2
#
# Args:
#   $1  ledgers per epoch (default 256)
#   $2  first emission epoch (default 2)
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EPOCH_LEDGERS="${1:-256}"
FIRST_EMISSION="${2:-2}"
TAG="mainnet-rehearsal-fast-epoch"
IMAGE="qxrp/xrpld:${TAG}"
DOCKERFILE="$REPO_ROOT/docker/Dockerfile"

log() { echo "[fast-epoch] $*"; }
die() { echo "[fast-epoch] ERROR: $*" >&2; exit 1; }

command -v docker >/dev/null || die "docker required"
[[ -f "$DOCKERFILE" ]] || die "missing Dockerfile"

# Dockerfile must pass cmake args — use build-arg if supported; else patch note.
# Current Dockerfile does not expose qxrp_epoch_override; we inject via
# CMAKE_ARGS environment in a thin override file if present, else document
# manual one-liner below.

cd "$REPO_ROOT"
SHA="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
SHORT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"

log "Building REHEARSAL image only: $IMAGE"
log "epoch_ledgers=$EPOCH_LEDGERS first_emission=$FIRST_EMISSION commit=$SHORT"
log "This is NOT mainnet-v1."

export DOCKER_BUILDKIT=1

# Prefer docker/Dockerfile.fast-epoch if present; else inline build with
# --build-arg and a Dockerfile that honors QXRP_CMAKE_EXTRA.
if [[ -f "$REPO_ROOT/docker/Dockerfile.fast-epoch" ]]; then
  docker build \
    -t "$IMAGE" \
    -f "$REPO_ROOT/docker/Dockerfile.fast-epoch" \
    --build-arg CORES="${CORES:-2}" \
    --build-arg QXRP_EPOCH_LEDGERS="$EPOCH_LEDGERS" \
    --build-arg QXRP_FIRST_EMISSION_EPOCH="$FIRST_EMISSION" \
    --label "org.opencontainers.image.revision=${SHA}" \
    --label "falcon.image.purpose=rehearsal-fast-epoch" \
    --label "falcon.rehearsal.only=true" \
    --label "falcon.epoch.ledgers=${EPOCH_LEDGERS}" \
    --label "falcon.first.emission.epoch=${FIRST_EMISSION}" \
    "$REPO_ROOT"
else
  log "Creating temporary docker/Dockerfile.fast-epoch from main Dockerfile..."
  # Inject cmake -Dqxrp_epoch_override and -DQXRP_FIRST_EMISSION if CMake supports it.
  # BUILD.md: -Dqxrp_epoch_override=<N>
  sed \
    -e "s|-Dxrpld=ON \\\\|-Dxrpld=ON -Dqxrp_epoch_override=${EPOCH_LEDGERS} -DCMAKE_CXX_FLAGS=\"-DQXRP_FIRST_EMISSION_EPOCH=${FIRST_EMISSION}\" \\\\|" \
    "$DOCKERFILE" > "$REPO_ROOT/docker/Dockerfile.fast-epoch.generated"
  docker build \
    -t "$IMAGE" \
    -f "$REPO_ROOT/docker/Dockerfile.fast-epoch.generated" \
    --build-arg CORES="${CORES:-2}" \
    --label "org.opencontainers.image.revision=${SHA}" \
    --label "falcon.image.purpose=rehearsal-fast-epoch" \
    --label "falcon.rehearsal.only=true" \
    --label "falcon.epoch.ledgers=${EPOCH_LEDGERS}" \
    --label "falcon.first.emission.epoch=${FIRST_EMISSION}" \
    "$REPO_ROOT"
fi

log "built $IMAGE"
docker run --rm --entrypoint /usr/local/bin/xrpld "$IMAGE" --version || true
log "Use ONLY on private rehearsal nets. Pin mainnet-v1 for real T0."
log "Then: PUBLIC_RPC=... python3 scripts/ops/rehearsal-e2e.py --phases all --image $IMAGE"
