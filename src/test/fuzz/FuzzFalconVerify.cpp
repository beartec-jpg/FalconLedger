// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Fuzz target: Falcon post-quantum signature verification.
//
// Build (requires liboqs to be present):
//
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     -I <repo>/include \
//     FuzzFalconVerify.cpp \
//     -o fuzz_falcon_verify \
//     -L<build>/lib -lxrpl -loqs
//
// Run:
//   ./fuzz_falcon_verify corpus/falcon/ -max_len=8192 -timeout=30
//
// Seed corpus: generate a few valid signatures with the unit test helper and
// place the raw (publicKey || signature || message) blobs in corpus/falcon/.

#include <xrpl/protocol/falcon.h>
#include <xrpl/basics/Slice.h>

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>

// Input layout (all lengths big-endian uint16):
//   [2 bytes] pubKeyLen
//   [pubKeyLen bytes] publicKey
//   [2 bytes] sigLen
//   [sigLen bytes] signature
//   [remaining bytes] message
extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    // Need at least 4 bytes for the two length fields.
    if (size < 4)
        return 0;

    auto readU16 = [&](std::size_t offset) -> std::uint16_t {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(data[offset]) << 8) |
             static_cast<std::uint16_t>(data[offset + 1]));
    };

    std::size_t offset = 0;
    std::uint16_t pubKeyLen = readU16(offset);
    offset += 2;

    if (offset + pubKeyLen > size)
        return 0;
    xrpl::Slice pubKey{data + offset, pubKeyLen};
    offset += pubKeyLen;

    if (offset + 2 > size)
        return 0;
    std::uint16_t sigLen = readU16(offset);
    offset += 2;

    if (offset + sigLen > size)
        return 0;
    xrpl::Slice sig{data + offset, sigLen};
    offset += sigLen;

    xrpl::Slice msg{data + offset, size - offset};

    try
    {
        // verifyFalcon must not crash, leak, or invoke UB on any input.
        // It should return false for invalid inputs.
        (void)xrpl::verifyFalcon(pubKey, msg, sig);
    }
    catch (std::exception const&)
    {
        // Exceptions are acceptable; crashes are not.
    }
    return 0;
}
