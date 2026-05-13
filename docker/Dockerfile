# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only
#
# Multi-stage build: compile xrpld, then copy binary into minimal runtime image.
#
# Build:
#   docker build -t qxrp/xrpld:latest .
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
COPY . .

# Set up Conan profile
RUN conan profile detect --force \
    && sed -i 's/compiler.cppstd=gnu17/compiler.cppstd=gnu20/' ~/.conan2/profiles/default

# Add XRPLF remote
RUN conan remote add --force --index 0 xrplf https://conan.ripplex.io

# Install dependencies and configure
RUN mkdir -p /build \
    && cd /build \
    && conan install /src --output-folder=. --build=missing \
       -s build_type=Release \
       -o "xrpl/*:tests=False" \
       -o "xrpl/*:xrpld=True"

# CMake configure + build
RUN cd /build \
    && cmake /src \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake \
       -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
       -Dxrpld=ON \
       -Dtests=OFF \
       -G Ninja \
    && cmake --build . --target xrpld --parallel ${CORES}

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

COPY --from=builder /build/xrpld /usr/local/bin/xrpld

# Ports: 5005=RPC, 51235=peer, 8080=dashboard
EXPOSE 5005 51235 8080

USER xrpld
WORKDIR /home/xrpld

ENTRYPOINT ["/usr/local/bin/xrpld"]
CMD ["--conf", "/cfg/xrpld.cfg"]
