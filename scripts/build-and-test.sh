#!/usr/bin/env bash
# build-and-test.sh  –  Full build + regtest smoke test for qXRP.
# Run this directly in your terminal (not via Copilot agent).
#
# Usage:
#   chmod +x scripts/build-and-test.sh
#   ./scripts/build-and-test.sh
#
# Steps performed:
#   1. Install Conan 2 via pip if missing
#   2. Set up Conan profile (GCC 13, C++20, libstdc++11)
#   3. Add XRPLF Conan remote
#   4. conan install (fetch / build dependencies)
#   5. cmake configure
#   6. cmake --build  (using all available cores)
#   7. Quick unit-test smoke run  (xrpld --unittest)
#   8. Start 5-validator regtest network and confirm convergence

set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
BUILD_DIR="$REPO/.build"
CORES=$(nproc)

log()  { echo -e "\n\033[1;34m[build] $*\033[0m"; }
die()  { echo -e "\033[1;31mERROR: $*\033[0m" >&2; exit 1; }

# ---------------------------------------------------------------------------
# 1. Conan
# ---------------------------------------------------------------------------
log "Step 1/8 – Install Conan"
if ! command -v conan &>/dev/null; then
    pip3 install --user "conan>=2.17"
    # Make sure the user-install bin dir is on PATH
    export PATH="$HOME/.local/bin:$PATH"
    hash -r
fi
conan --version

# ---------------------------------------------------------------------------
# 2. Conan profile
# ---------------------------------------------------------------------------
log "Step 2/8 – Configure Conan profile"

# Install the project's default profile templates
conan config install conan/profiles/ -tf "$(conan config home)/profiles/"

PROFILE="$(conan config home)/profiles/default"

if [[ ! -f "$PROFILE" ]]; then
    conan profile detect --name default
fi

# Ensure C++20 and libstdc++11
sed -i 's/^compiler\.cppstd=.*/compiler.cppstd=20/'   "$PROFILE" || true
sed -i 's/^compiler\.libcxx=.*/compiler.libcxx=libstdc++11/' "$PROFILE" || true

# Show final profile
conan profile show

# ---------------------------------------------------------------------------
# 3. XRPLF remote
# ---------------------------------------------------------------------------
log "Step 3/8 – Add XRPLF Conan remote"
conan remote add --index 0 xrplf https://conan.ripplex.io 2>/dev/null || \
    conan remote update xrplf --url https://conan.ripplex.io

# ---------------------------------------------------------------------------
# 3b. Export bundled secp256k1 and ed25519 recipes into the local Conan cache.
#
# These packages are hosted on conan.ripplex.io but may be unavailable in
# air-gapped or offline environments.  Exporting the local recipes ensures
# that "conan install --build missing" (step 4) can build them from source if
# no pre-built binary is found on the remote.  The export is fast (no
# compilation) and idempotent.
# ---------------------------------------------------------------------------
log "Step 3b/8 – Export bundled secp256k1 and ed25519 Conan recipes"
conan export "$REPO/external/secp256k1-recipe" --version=0.7.1 || \
    log "  WARN: secp256k1 recipe export failed — will rely on remote"
conan export "$REPO/external/ed25519-recipe" --version=2015.03 || \
    log "  WARN: ed25519 recipe export failed — will rely on remote"

# ---------------------------------------------------------------------------
# 4. conan install
# ---------------------------------------------------------------------------
log "Step 4/8 – conan install (this may take a while first time)"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

conan install "$REPO" \
    --output-folder . \
    --build missing \
    --settings build_type=RelWithDebInfo \
    -c tools.cmake.cmake_layout:build_folder_vars="['settings.build_type']"

# ---------------------------------------------------------------------------
# 5. cmake configure
# ---------------------------------------------------------------------------
log "Step 5/8 – CMake configure"
cmake "$REPO" \
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -Dxrpld=ON \
    -Dtests=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# ---------------------------------------------------------------------------
# 6. Build
# ---------------------------------------------------------------------------
log "Step 6/8 – cmake --build  (using $CORES cores)"
cmake --build . --parallel "$CORES" 2>&1 | tee "$REPO/build.log"
BINARY="$BUILD_DIR/xrpld"
[[ -x "$BINARY" ]] || die "Build finished but xrpld not found at $BINARY"
log "Binary: $BINARY  ($(du -sh "$BINARY" | cut -f1))"

# ---------------------------------------------------------------------------
# 7. Unit-test smoke run
# ---------------------------------------------------------------------------
log "Step 7/8 – Unit test smoke run (Protocol suite only)"
"$BINARY" --unittest Protocol --unittest-jobs "$CORES" 2>&1 | tail -20
echo ""
log "Full unit tests: $BINARY --unittest --unittest-jobs $CORES"

# ---------------------------------------------------------------------------
# 8. Regtest smoke test
# ---------------------------------------------------------------------------
log "Step 8/8 – Starting 5-validator regtest network"
cd "$REPO"

# Remove any stale regtest data so we get a clean bootstrap
rm -rf data/regtest

chmod +x scripts/start-regtest.sh scripts/stop-regtest.sh
./scripts/start-regtest.sh "$BINARY" &
REGTEST_PID=$!

# Give it up to 3 minutes to print "converged" or bail
CONVERGED=false
for i in $(seq 1 36); do
    if wait "$REGTEST_PID" 2>/dev/null; then
        break
    fi
    # Check v1 log for convergence
    LOG="data/regtest/v1/debug.log"
    if [[ -f "$LOG" ]] && grep -q '"LCL.*seq.*[3-9]\|validated_ledger.*[3-9]' "$LOG" 2>/dev/null; then
        CONVERGED=true
        break
    fi
    sleep 5
done

if $CONVERGED; then
    log "Regtest converged successfully!"
else
    log "Regtest started; check data/regtest/v1/debug.log for progress."
    log "Stop with: ./scripts/stop-regtest.sh"
fi

echo ""
log "============================================"
log "Build complete!"
log "  Binary  : $BINARY"
log "  Regtest : ./scripts/start-regtest.sh $BINARY"
log "  Stop    : ./scripts/stop-regtest.sh"
log "============================================"
