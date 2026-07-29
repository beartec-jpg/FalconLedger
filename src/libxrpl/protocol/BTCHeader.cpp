// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/protocol/BTCHeader.h>
#include <xrpl/protocol/digest.h>

#include <algorithm>
#include <cstring>

namespace xrpl {
namespace {

void
writeBE256(uint256& out, unsigned char const* littleEndian32)
{
    // Convert 32-byte LE buffer to big-endian base_uint storage if needed.
    // Falcon uint256 stores most-significant byte first (same as XRPL).
    // Bitcoin double-SHA256 result is typically kept as raw digest bytes;
    // for PoW we treat the digest as a little-endian integer (Bitcoin style).
    for (int i = 0; i < 32; ++i)
        out.data()[i] = littleEndian32[31 - i];
}

}  // namespace

uint256
btcDoubleSha256(void const* data, std::size_t len)
{
    sha256_hasher h1;
    h1(data, len);
    auto const d1 = static_cast<sha256_hasher::result_type>(h1);
    sha256_hasher h2;
    h2(d1.data(), d1.size());
    auto const d2 = static_cast<sha256_hasher::result_type>(h2);
    // Keep internal Bitcoin hash as little-endian interpretation in uint256
    // (byte-reversed into Falcon's big-endian storage for numeric compares).
    uint256 out;
    writeBE256(out, d2.data());
    return out;
}

uint256
btcReadHashLE(Slice header80, std::size_t offset)
{
    uint256 out;
    if (header80.size() < offset + 32)
        return out;
    writeBE256(out, header80.data() + offset);
    return out;
}

std::uint32_t
btcReadUint32LE(Slice header80, std::size_t offset)
{
    if (header80.size() < offset + 4)
        return 0;
    auto const* p = header80.data() + offset;
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::optional<BTCParsedHeader>
btcParseHeader(Slice header80)
{
    if (header80.size() != kBTC_HEADER_SIZE)
        return std::nullopt;

    BTCParsedHeader h;
    h.version = btcReadUint32LE(header80, 0);
    h.prevHash = btcReadHashLE(header80, 4);
    h.merkleRoot = btcReadHashLE(header80, 36);
    h.timestamp = btcReadUint32LE(header80, 68);
    h.bits = btcReadUint32LE(header80, 72);
    h.nonce = btcReadUint32LE(header80, 76);
    h.blockHash = btcDoubleSha256(header80.data(), header80.size());
    return h;
}

bool
btcHashMeetsTarget(uint256 const& blockHash, std::uint32_t nBits)
{
    // Expand compact nBits → 256-bit target (Bitcoin SetCompact).
    std::uint32_t const size = nBits >> 24;
    std::uint32_t const word = nBits & 0x007fffff;
    bool const negative = (nBits & 0x00800000) != 0;
    if (negative || word == 0)
        return false;

    uint256 target;
    target.zero();
    if (size <= 3)
    {
        std::uint32_t const w = word >> (8 * (3 - size));
        // store w as low bytes in big-endian uint256
        target.data()[31] = static_cast<std::uint8_t>(w);
        target.data()[30] = static_cast<std::uint8_t>(w >> 8);
        target.data()[29] = static_cast<std::uint8_t>(w >> 16);
        target.data()[28] = static_cast<std::uint8_t>(w >> 24);
    }
    else if (size <= 32)
    {
        // word is the high 3 bytes of the target at position size-3
        int const pos = 32 - static_cast<int>(size);
        // write word in big-endian at [pos, pos+3)
        // word's MSB goes to pos
        if (pos >= 0 && pos + 2 < 32)
        {
            target.data()[pos] = static_cast<std::uint8_t>((word >> 16) & 0xff);
            target.data()[pos + 1] = static_cast<std::uint8_t>((word >> 8) & 0xff);
            target.data()[pos + 2] = static_cast<std::uint8_t>(word & 0xff);
        }
        else if (size == 32)
        {
            // overflow-ish; rare
            return false;
        }
    }
    else
    {
        return false;  // overflow
    }

    // blockHash is already in big-endian numeric form
    return blockHash <= target;
}

uint256
btcWorkFromBits(std::uint32_t nBits)
{
    // Simplified work: 1 if target is valid non-zero, else 0.
    // Sufficient for regtest ordering; full ~target/(target+1)+1 can replace later.
    std::uint32_t const word = nBits & 0x007fffff;
    bool const negative = (nBits & 0x00800000) != 0;
    if (negative || word == 0)
        return uint256{};

    uint256 one;
    one.data()[31] = 1;
    return one;
}

uint256
btcAddWork(uint256 const& parentWork, uint256 const& blockWork)
{
    // Big-endian addition of two 256-bit values
    uint256 out;
    unsigned carry = 0;
    for (int i = 31; i >= 0; --i)
    {
        unsigned const sum =
            static_cast<unsigned>(parentWork.data()[i]) + static_cast<unsigned>(blockWork.data()[i]) +
            carry;
        out.data()[i] = static_cast<std::uint8_t>(sum & 0xff);
        carry = sum >> 8;
    }
    return out;
}

bool
btcIsBetterTip(
    uint256 const& candWork,
    std::uint32_t candHeight,
    uint256 const& candHash,
    uint256 const& tipWork,
    std::uint32_t tipHeight,
    uint256 const& tipHash)
{
    if (candWork > tipWork)
        return true;
    if (candWork < tipWork)
        return false;
    if (candHeight > tipHeight)
        return true;
    if (candHeight < tipHeight)
        return false;
    return candHash > tipHash;
}

}  // namespace xrpl
