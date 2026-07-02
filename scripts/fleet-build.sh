#!/usr/bin/env bash
# fleet-build.sh — incremental xrpld build for fleet build hosts.
#
# Usage:
#   ./scripts/fleet-build.sh          # full image build (Conan cached after first run)
#   ./scripts/fleet-build.sh compile  # recompile only — use after fixing a .cpp error
#   ./scripts/fleet-build.sh image    # package /tmp/xrpld into runtime image (after compile)
#
# Requires: docker with BuildKit, repo at /root/qXRP-falcon (or set SRC_DIR).
set -euo pipefail

SRC_DIR="${SRC_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_LOG="${BUILD_LOG:-/root/falcon-build.log}"
CORES="${CORES:-$(nproc)}"
TAGS=(-t qxrp/xrpld:falcon-only -t qxrp/xrpld:latest)

export DOCKER_BUILDKIT=1

log() { echo "[fleet-build] $*"; }

compile_only() {
    log "Compile-only: reusing /build + ~/.conan2 cache mounts (ninja incremental)"
    docker build "${TAGS[@]}" \
        --build-arg "CORES=${CORES}" \
        -f "${SRC_DIR}/docker/Dockerfile" \
        "${SRC_DIR}" 2>&1 | tee "${BUILD_LOG}"
}

full_build() {
    log "Full image build (Conan + compile; caches persist via BuildKit mounts)"
    compile_only
}

package_image() {
    log "Building runtime image from builder stage"
    docker build "${TAGS[@]}" \
        --build-arg "CORES=${CORES}" \
        -f "${SRC_DIR}/docker/Dockerfile" \
        "${SRC_DIR}"
}

case "${1:-full}" in
    compile|c)
        compile_only
        ;;
    image|i)
        package_image
        ;;
    full|f|"")
        full_build
        ;;
    *)
        echo "Usage: $0 [full|compile|image]" >&2
        exit 1
        ;;
esac

if docker run --rm qxrp/xrpld:latest xrpld --version; then
    log "Done."
else
    log "Build finished but version check failed — inspect ${BUILD_LOG}"
    exit 1
fi