// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/Slice.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace xrpl {

/** Verify a Bitcoin Merkle branch.
    @param txidInternal  double-SHA256(tx) in internal byte order
    @param proof         concatenated 32-byte sibling hashes (leaf → root)
    @param txIndex       leaf index in the tree
    @param merkleRoot    from header (internal order)
*/
bool
btcVerifyMerkleProof(
    uint256 const& txidInternal,
    Slice proof,
    std::uint32_t txIndex,
    uint256 const& merkleRoot);

}  // namespace xrpl
