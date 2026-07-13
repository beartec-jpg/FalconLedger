#!/usr/bin/env bash
# Build qXRP xrpld on memory-constrained hosts (~8 GB RAM).
# Uses 2 parallel compile jobs and skips regtest smoke.
#
# Usage:
#   ./scripts/build-release-8gb.sh
#   ./scripts/build-release-8gb.sh --install-deps

set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
BUILD_DIR="$REPO/.build"
JOBS="${BUILD_JOBS:-2}"

log() { echo -e "\n\033[1;34m[8gb-build] $*\033[0m"; }
die() { echo -e "\033[1;31mERROR: $*\033[0m" >&2; exit 1; }

INSTALL_DEPS=false
for arg in "$@"; do
  [[ "$arg" == "--install-deps" ]] && INSTALL_DEPS=true
done

if $INSTALL_DEPS; then
  log "Installing build packages (Debian/Ubuntu)"
  sudo apt-get update -qq
  sudo apt-get install -y build-essential cmake ninja-build python3-pip git pkg-config \
    libssl-dev libprotobuf-dev protobuf-compiler libboost-all-dev
fi

log "Conan"
if ! command -v conan &>/dev/null; then
  pip3 install --user "conan>=2.17"
  export PATH="$HOME/.local/bin:$PATH"
fi
conan config install conan/profiles/ -tf "$(conan config home)/profiles/" 2>/dev/null || true
PROFILE="$(conan config home)/profiles/default"
[[ -f "$PROFILE" ]] || conan profile detect --name default
sed -i 's/^compiler\.cppstd=.*/compiler.cppstd=20/' "$PROFILE" 2>/dev/null || true
sed -i 's/^compiler\.libcxx=.*/compiler.libcxx=libstdc++11/' "$PROFILE" 2>/dev/null || true

conan remote add --index 0 xrplf https://conan.ripplex.io 2>/dev/null || \
  conan remote update xrplf --url https://conan.ripplex.io
conan export "$REPO/external/secp256k1-recipe" --version=0.7.1 2>/dev/null || true
conan export "$REPO/external/ed25519-recipe" --version=2015.03 2>/dev/null || true

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

log "conan install (jobs=$JOBS)"
conan install "$REPO" \
  --output-folder . \
  --build missing \
  --settings build_type=Release \
  -c tools.build:jobs="$JOBS" \
  -c tools.cmake.cmake_layout:build_folder_vars="['settings.build_type']"

log "cmake configure"
cmake "$REPO" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -Dxrpld=ON \
  -Dtests=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

log "cmake build (-j$JOBS)"
cmake --build . -j"$JOBS"

BINARY="$BUILD_DIR/xrpld"
[[ -x "$BINARY" ]] || die "xrpld not found at $BINARY"

log "Build complete: $BINARY ($(du -sh "$BINARY" | cut -f1))"
log "Next: deploy xrpld to fleet, then: bash scripts/enable-lending-collateral-fleet.sh"