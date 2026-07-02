# syntax=docker/dockerfile:1.4
# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Multi-stage build: compile xrpld, then copy binary into minimal runtime image.
#
# Requires BuildKit (incremental compile + Conan cache):
#   DOCKER_BUILDKIT=1 docker build -t qxrp/xrpld:latest -f docker/Dockerfile .
#
# Layer strategy:
#   1. Copy only Conan inputs first → deps layer survives source-only edits.
#   2. Cache-mount ~/.conan2 → Conan packages persist across failed builds.
#   3. Cache-mount /build → ninja recompiles only changed .cpp files after a fix.
#
# Run (single node, standalone mode):
#   docker run --rm -v "$PWD/cfg:/cfg" -p 5005:5005 -p 51235:51235 \
#     qxrp/xrpld:latest /cfg/xrpld.cfg

# ---------------------------------------------------------------------------
# Stage 1 – builder
# ---------------------------------------------------------------------------
FROM debian:bookworm AS builder

ARG CORES=4

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        curl \
        python3 \
        python3-pip \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Conan 2
RUN pip3 install --break-system-packages "conan>=2.17"

WORKDIR /src

# Copy dependency manifests BEFORE full source so conan install is cached
# until conanfile / profiles / version stamp change (not every .cpp edit).
COPY conanfile.py CMakeLists.txt ./
COPY src/libxrpl/protocol/BuildInfo.cpp ./src/libxrpl/protocol/BuildInfo.cpp
COPY conan/profiles/ ./conan/profiles/
COPY external/secp256k1-recipe/ ./external/secp256k1-recipe/
COPY external/ed25519-recipe/ ./external/ed25519-recipe/

# Profile + remote + deps all live in the Conan cache mount so they persist
# across failed builds and don't require a separate layer.
RUN --mount=type=cache,target=/root/.conan2,id=qxrp-conan \
    conan profile detect --force \
    && sed -i 's/compiler.cppstd=gnu17/compiler.cppstd=gnu20/' ~/.conan2/profiles/default \
    && conan remote add --force --index 0 xrplf https://conan.ripplex.io \
    && mkdir -p /build \
    && cd /build \
    && conan install /src --output-folder=. --build=missing \
       -s build_type=Release \
       -o "xrpl/*:tests=False" \
       -o "xrpl/*:xrpld=True"

# Full source tree (any .cpp/.h change invalidates only layers below)
COPY . .

# Re-run conan install here (fast — packages cached) so build/generators/ always
# exists in the /build mount when this layer re-runs after a source edit.
RUN --mount=type=cache,target=/root/.conan2,id=qxrp-conan \
    --mount=type=cache,target=/build,id=qxrp-build \
    cd /build \
    && conan install /src --output-folder=. --build=missing \
       -s build_type=Release \
       -o "xrpl/*:tests=False" \
       -o "xrpl/*:xrpld=True" \
    && cmake /src \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake \
       -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
       -Dxrpld=ON \
       -Dtests=OFF \
       -G Ninja \
    && cmake --build . --target xrpld --parallel ${CORES} \
    && cp /build/xrpld /tmp/xrpld

# ---------------------------------------------------------------------------
# Stage 2 – runtime
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 \
        libstdc++6 \
        curl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -u 1001 -s /bin/bash xrpld

COPY --from=builder /tmp/xrpld /usr/local/bin/xrpld

# Ports: 5005=RPC, 51235=peer, 8080=dashboard
EXPOSE 5005 51235 8080

USER xrpld
WORKDIR /home/xrpld

ENTRYPOINT ["/usr/local/bin/xrpld"]
CMD ["--conf", "/cfg/xrpld.cfg"]