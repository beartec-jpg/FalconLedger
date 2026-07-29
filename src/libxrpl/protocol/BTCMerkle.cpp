// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/protocol/BTCMerkle.h>
#include <xrpl/protocol/BTCHeader.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/digest.h>

#include <cstring>

namespace xrpl {
namespace {

// Hash pair in Bitcoin merkle style using raw 32-byte digests (not Falcon endian).
// We store hashes in Falcon big-endian numeric form; convert to Bitcoin internal
// (LE) before hashing, then back.

void
toBitcoinInternal(std::uint8_t out[32], uint256 const& h)
{
    for (int i = 0; i < 32; ++i)
        out[i] = h.data()[31 - i];
}

uint256
fromBitcoinInternal(std::uint8_t const in[32])
{
    uint256 out;
    for (int i = 0; i < 32; ++i)
        out.data()[i] = in[31 - i];
    return out;
}

uint256
hashNodes(uint256 const& left, uint256 const& right)
{
    std::uint8_t buf[64];
    toBitcoinInternal(buf, left);
    toBitcoinInternal(buf + 32, right);
    // double-SHA256 raw, then convert to Falcon form
    sha256_hasher h1;
    h1(buf, 64);
    auto const d1 = static_cast<sha256_hasher::result_type>(h1);
    sha256_hasher h2;
    h2(d1.data(), d1.size());
    auto const d2 = static_cast<sha256_hasher::result_type>(h2);
    return fromBitcoinInternal(d2.data());
}

}  // namespace

bool
btcVerifyMerkleProof(
    uint256 const& txidInternal,
    Slice proof,
    std::uint32_t txIndex,
    uint256 const& merkleRoot)
{
    if (proof.size() % 32 != 0)
        return false;
    auto const depth = proof.size() / 32;
    if (depth > kBTC_MAX_MERKLE_DEPTH)
        return false;

    uint256 hash = txidInternal;
    std::uint32_t idx = txIndex;
    for (std::size_t i = 0; i < depth; ++i)
    {
        uint256 sibling;
        // proof siblings are stored in Bitcoin internal byte order in the blob
        std::uint8_t const* p = proof.data() + i * 32;
        sibling = fromBitcoinInternal(p);
        if ((idx & 1u) == 0)
            hash = hashNodes(hash, sibling);
        else
            hash = hashNodes(sibling, hash);
        idx >>= 1;
    }
    return hash == merkleRoot;
}

}  // namespace xrpl
