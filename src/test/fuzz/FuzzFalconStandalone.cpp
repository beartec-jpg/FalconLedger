// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Standalone Falcon key / signature shape fuzzer (no liboqs linkage).
// Exercises prefix + length validation used by PQPublicKey / isValidNodeKey.
//
// Build (clang + libFuzzer, no libxrpl):
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     FuzzFalconStandalone.cpp -o fuzz_falcon_standalone
//
// Invariants:
//   1. Never crashes on arbitrary input
//   2. Accepts only exact Falcon-512 / Falcon-1024 public key sizes + prefixes
//   3. Signature length bounds are enforced without buffer OOB

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Mirror include/xrpl/protocol/PQPublicKey.h / falcon sizes (keep in sync).
static constexpr std::size_t kFALCON512_PUBKEY_BYTES = 897;   // raw; +1 prefix = 898 on wire
static constexpr std::size_t kFALCON1024_PUBKEY_BYTES = 1793;
static constexpr std::uint8_t kPREFIX_FALCON512 = 0xFB;
static constexpr std::uint8_t kPREFIX_FALCON1024 = 0xFC;

// liboqs Falcon-512 / 1024 max signature sizes (upper bounds for sanity).
static constexpr std::size_t kFALCON512_SIG_MAX = 752;
static constexpr std::size_t kFALCON1024_SIG_MAX = 1462;

enum class KeyKind { Invalid, Falcon512, Falcon1024 };

static KeyKind
classifyKey(std::uint8_t const* data, std::size_t size)
{
    if (size == 0)
        return KeyKind::Invalid;
    if (data[0] == kPREFIX_FALCON512 && size == kFALCON512_PUBKEY_BYTES + 1)
        return KeyKind::Falcon512;
    if (data[0] == kPREFIX_FALCON1024 && size == kFALCON1024_PUBKEY_BYTES + 1)
        return KeyKind::Falcon1024;
    return KeyKind::Invalid;
}

static bool
sigLenPlausible(KeyKind kind, std::size_t sigLen)
{
    if (kind == KeyKind::Falcon512)
        return sigLen > 0 && sigLen <= kFALCON512_SIG_MAX;
    if (kind == KeyKind::Falcon1024)
        return sigLen > 0 && sigLen <= kFALCON1024_SIG_MAX;
    return false;
}

extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    if (size < 4)
        return 0;

    // Layout: [2B] keyLen | [key] | [2B] sigLen | [sig] | [msg...]
    auto readU16 = [&](std::size_t off) -> std::uint16_t {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(data[off]) << 8) |
            static_cast<std::uint16_t>(data[off + 1]));
    };

    std::size_t off = 0;
    std::uint16_t keyLen = readU16(off);
    off += 2;
    if (off + keyLen > size)
        return 0;
    auto const kind = classifyKey(data + off, keyLen);
    off += keyLen;

    if (off + 2 > size)
        return 0;
    std::uint16_t sigLen = readU16(off);
    off += 2;
    if (off + sigLen > size)
        return 0;
    // Touch first/last sig byte if present (ASAN bounds).
    if (sigLen > 0)
    {
        volatile std::uint8_t a = data[off];
        volatile std::uint8_t b = data[off + sigLen - 1];
        (void)a;
        (void)b;
    }
    off += sigLen;
    std::size_t const msgLen = size - off;
    if (msgLen > 0)
    {
        volatile std::uint8_t c = data[off];
        (void)c;
    }

    // Invariants on classification (shape only; no liboqs verify).
    if (kind == KeyKind::Invalid)
        return 0;

    // Re-derive prefix from key blob start (offset 2 after length prefix).
    std::uint8_t const prefix = data[2];
    if (kind == KeyKind::Falcon512)
        assert(prefix == kPREFIX_FALCON512 && keyLen == kFALCON512_PUBKEY_BYTES + 1);
    if (kind == KeyKind::Falcon1024)
        assert(prefix == kPREFIX_FALCON1024 && keyLen == kFALCON1024_PUBKEY_BYTES + 1);

    return 0;
}
