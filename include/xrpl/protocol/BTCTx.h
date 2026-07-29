// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace xrpl {

struct BTCTxOutput
{
    std::uint64_t valueSats = 0;
    Blob scriptPubKey;
};

struct BTCParsedTx
{
    uint256 txid;  // internal order (non-witness txid)
    std::vector<BTCTxOutput> outputs;
    bool isSegwit = false;
};

/** Parse a raw Bitcoin transaction (supports legacy + segwit wire format).
    Computes non-witness txid (BIP141). */
std::optional<BTCParsedTx>
btcParseTx(Slice rawTx);

/** SHA256 of scriptPubKey (single hash) — matches sfBtcWatchScriptHash convention. */
uint256
btcScriptHash(Slice scriptPubKey);

struct BTCDepositExtract
{
    std::uint32_t watchVout = 0;
    std::uint64_t valueSats = 0;
    AccountID destination;
};

/** Find watch payment + OP_RETURN FALC||AccountID.
    Requires exactly one OP_RETURN with magic payload and ≥1 watch match;
    uses @p preferredVout if it is a watch output, else first watch output.
    Rejects ambiguous multi OP_RETURN. */
std::optional<BTCDepositExtract>
btcExtractDeposit(
    BTCParsedTx const& tx,
    uint256 const& watchScriptHash,
    std::uint32_t preferredVout);

}  // namespace xrpl
