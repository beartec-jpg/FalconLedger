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
    /** True if payment is BitVM vault P2WSH (not legacy fixed watch P2PKH). */
    bool isVault = false;
    /** SHA256(preimage) embedded in vault script (for client cross-check). */
    uint256 vaultCommit;
};

/** True if scriptPubKey is witness v0 P2WSH (0x00 0x20 || 32). */
bool
btcIsP2WSH(Slice scriptPubKey);

/**
 * Validate BitVM-class vault witness script (CSV + hashlock + CHECKSIG).
 * On success sets @p commitOut to the 32-byte burn commit in the script.
 */
bool
btcParseBitvmVaultScript(Slice witnessScript, uint256& commitOut);

/** Find watch payment + OP_RETURN FALC||AccountID.
    Accepts:
      (1) legacy fixed watchScriptHash (P2PKH custody-era), or
      (2) P2WSH vault when @p vaultWitnessScript is provided and matches template.
    Requires exactly one OP_RETURN with magic payload.
    Rejects ambiguous multi OP_RETURN. */
std::optional<BTCDepositExtract>
btcExtractDeposit(
    BTCParsedTx const& tx,
    uint256 const& watchScriptHash,
    std::uint32_t preferredVout,
    Slice vaultWitnessScript = {});

}  // namespace xrpl
