// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace xrpl {

/** Double-SHA256 (Bitcoin style). Result is in internal hash byte order
    (same as Bitcoin Core's uint256 serialization — little-endian on wire for
    the 80-byte header fields, hash output as raw 32 bytes from double-SHA256). */
uint256
btcDoubleSha256(void const* data, std::size_t len);

inline uint256
btcDoubleSha256(Slice s)
{
    return btcDoubleSha256(s.data(), s.size());
}

struct BTCParsedHeader
{
    uint256 prevHash;     // internal order (as in header bytes 4..35, reversed for display)
    uint256 merkleRoot;
    std::uint32_t version = 0;
    std::uint32_t timestamp = 0;
    std::uint32_t bits = 0;
    std::uint32_t nonce = 0;
    uint256 blockHash;  // double-SHA256 of 80 bytes, internal order
};

/** Parse an 80-byte Bitcoin header. Returns nullopt if size != 80. */
std::optional<BTCParsedHeader>
btcParseHeader(Slice header80);

/** True if blockHash meets the compact nBits target (Bitcoin PoW). */
bool
btcHashMeetsTarget(uint256 const& blockHash, std::uint32_t nBits);

/** Work contribution of a block with given nBits (Bitcoin GetBlockProof). */
uint256
btcWorkFromBits(std::uint32_t nBits);

/** Chainwork = parentWork + workFromBits. */
uint256
btcAddWork(uint256 const& parentWork, uint256 const& blockWork);

/** Extract 32-byte hash field from header at offset (version=0, prev=4, merkle=36).
    Bitcoin header stores hashes in little-endian (internal) byte order. */
uint256
btcReadHashLE(Slice header80, std::size_t offset);

std::uint32_t
btcReadUint32LE(Slice header80, std::size_t offset);

/** Compare two chainworks / hashes for tip selection:
    return true if candidate should replace current tip. */
bool
btcIsBetterTip(
    uint256 const& candWork,
    std::uint32_t candHeight,
    uint256 const& candHash,
    uint256 const& tipWork,
    std::uint32_t tipHeight,
    uint256 const& tipHash);

}  // namespace xrpl
