// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/btc_spv/BTCHeaderSubmit.h>

#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/BTCHeader.h>

#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/applySteps.h>

#include <chrono>

namespace xrpl {

XRPAmount
BTCHeaderSubmit::calculateBaseFee(ReadView const& view, STTx const& tx)
{
    // Auto-scale: base * max(1, numHeaders)  (K21)
    auto const base = Transactor::calculateBaseFee(view, tx);
    auto const blob = tx.getFieldVL(sfBtcHeaders);
    auto n = blob.size() / kBTC_HEADER_SIZE;
    if (n == 0)
        n = 1;
    if (n > kBTC_MAX_HEADERS_PER_TX)
        n = kBTC_MAX_HEADERS_PER_TX;
    return base * static_cast<std::uint32_t>(n);
}

NotTEC
BTCHeaderSubmit::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureBitcoinSPVBridge))
        return temDISABLED;

    auto const blob = ctx.tx.getFieldVL(sfBtcHeaders);
    if (blob.empty() || blob.size() % kBTC_HEADER_SIZE != 0)
        return temMALFORMED;
    auto const n = blob.size() / kBTC_HEADER_SIZE;
    if (n == 0 || n > kBTC_MAX_HEADERS_PER_TX)
        return temMALFORMED;

    for (std::size_t i = 0; i < n; ++i)
    {
        Slice const h{blob.data() + i * kBTC_HEADER_SIZE, kBTC_HEADER_SIZE};
        if (!btcParseHeader(h))
            return temMALFORMED;
    }
    return tesSUCCESS;
}

TER
BTCHeaderSubmit::preclaim(PreclaimContext const& ctx)
{
    auto const state = ctx.view.read(keylet::btcBridgeState());
    if (!state)
        return tecNO_ENTRY;
    if (state->getFieldU32(sfBtcPaused) != 0)
        return tecNO_PERMISSION;
    return tesSUCCESS;
}

TER
BTCHeaderSubmit::doApply()
{
    auto state = view().peek(keylet::btcBridgeState());
    if (!state)
        return tecNO_ENTRY;

    auto tipHash = state->getFieldH256(sfBtcTipHash);
    auto tipHeight = state->getFieldU32(sfBtcTipHeight);
    auto tipWork = state->getFieldH256(sfBtcTipWork);

    auto const blob = ctx_.tx.getFieldVL(sfBtcHeaders);
    auto const n = blob.size() / kBTC_HEADER_SIZE;
    auto const closeTime = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                                               view().header().closeTime.time_since_epoch())
                                                           .count());

    for (std::size_t i = 0; i < n; ++i)
    {
        Blob headerBytes(blob.begin() + static_cast<std::ptrdiff_t>(i * kBTC_HEADER_SIZE),
            blob.begin() + static_cast<std::ptrdiff_t>((i + 1) * kBTC_HEADER_SIZE));
        auto const parsed = btcParseHeader(makeSlice(headerBytes));
        if (!parsed)
            return tecINTERNAL;

        // Idempotent re-submit
        if (view().exists(keylet::btcHeader(parsed->blockHash)))
            continue;

        // Parent must exist (any known header)
        auto const parent = view().read(keylet::btcHeader(parsed->prevHash));
        if (!parent)
            return tecNO_ENTRY;

        if (parent->getFieldH256(sfBtcBlockHash) != parsed->prevHash)
            return tecNO_ENTRY;

        if (!btcHashMeetsTarget(parsed->blockHash, parsed->bits))
            return temMALFORMED;

        // Soft timestamp: not more than 2h ahead of Falcon close (seconds)
        if (parsed->timestamp > static_cast<std::uint32_t>(closeTime) + kBTC_MAX_TIMESTAMP_AHEAD_SEC)
            return temMALFORMED;

        auto const parentWork = parent->getFieldH256(sfBtcChainWork);
        auto const parentHeight = parent->getFieldU32(sfBtcHeight);
        auto const blockWork = btcWorkFromBits(parsed->bits);
        auto const chainWork = btcAddWork(parentWork, blockWork);
        auto const height = parentHeight + 1;

        auto hdr = std::make_shared<SLE>(keylet::btcHeader(parsed->blockHash));
        hdr->setFieldH256(sfBtcBlockHash, parsed->blockHash);
        hdr->setFieldU32(sfBtcHeight, height);
        hdr->setFieldVL(sfBtcHeaderBytes, headerBytes);
        hdr->setFieldH256(sfBtcChainWork, chainWork);
        hdr->setFieldH256(sfBtcPrevBlockHash, parsed->prevHash);
        hdr->setFieldH256(sfBtcMerkleRoot, parsed->merkleRoot);
        hdr->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
        hdr->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
        view().insert(hdr);

        // Incremental tip update (K22/K2)
        if (btcIsBetterTip(
                chainWork, height, parsed->blockHash, tipWork, tipHeight, tipHash))
        {
            tipHash = parsed->blockHash;
            tipHeight = height;
            tipWork = chainWork;

            // Update / insert height index for tip lineage
            auto hKey = keylet::btcHeight(height);
            if (auto existing = view().peek(hKey))
            {
                existing->setFieldH256(sfBtcBlockHash, parsed->blockHash);
                existing->setFieldH256(sfBtcChainWork, chainWork);
                existing->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
                existing->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
                view().update(existing);
            }
            else
            {
                auto hi = std::make_shared<SLE>(hKey);
                hi->setFieldU32(sfBtcHeight, height);
                hi->setFieldH256(sfBtcBlockHash, parsed->blockHash);
                hi->setFieldH256(sfBtcChainWork, chainWork);
                hi->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
                hi->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
                view().insert(hi);
            }
        }
    }

    state->setFieldH256(sfBtcTipHash, tipHash);
    state->setFieldU32(sfBtcTipHeight, tipHeight);
    state->setFieldH256(sfBtcTipWork, tipWork);
    state->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    state->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().update(state);

    return tesSUCCESS;
}

}  // namespace xrpl
