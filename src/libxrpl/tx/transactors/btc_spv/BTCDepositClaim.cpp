// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/btc_spv/BTCDepositClaim.h>

#include <xrpl/basics/Slice.h>
#include <xrpl/ledger/helpers/MPTokenHelpers.h>
#include <xrpl/ledger/helpers/TokenHelpers.h>
#include <xrpl/protocol/BTCHeader.h>
#include <xrpl/protocol/BTCMerkle.h>
#include <xrpl/protocol/BTCTx.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>

namespace xrpl {

NotTEC
BTCDepositClaim::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureBitcoinSPVBridge))
        return temDISABLED;

    if (ctx.tx[sfAccount] != ctx.tx[sfDestination])
        return temMALFORMED;

    auto const raw = ctx.tx.getFieldVL(sfBtcRawTx);
    if (raw.empty() || raw.size() > kBTC_MAX_TX_BLOB)
        return temMALFORMED;

    auto const proof = ctx.tx.getFieldVL(sfBtcMerkleProof);
    if (proof.size() % 32 != 0 || proof.size() / 32 > kBTC_MAX_MERKLE_DEPTH)
        return temMALFORMED;

    return tesSUCCESS;
}

TER
BTCDepositClaim::preclaim(PreclaimContext const& ctx)
{
    auto const state = ctx.view.read(keylet::btcBridgeState());
    if (!state)
        return tecNO_ENTRY;
    if (state->getFieldU32(sfBtcPaused) != 0)
        return tecNO_PERMISSION;

    auto const dest = ctx.tx[sfDestination];
    if (!ctx.view.exists(keylet::account(dest)))
        return tecNO_DST;

    auto const blockHash = ctx.tx.getFieldH256(sfBtcBlockHash);
    if (!ctx.view.exists(keylet::btcHeader(blockHash)))
        return tecNO_ENTRY;

    return tesSUCCESS;
}

TER
BTCDepositClaim::doApply()
{
    auto state = view().peek(keylet::btcBridgeState());
    if (!state)
        return tecNO_ENTRY;

    auto const dest = ctx_.tx[sfDestination];
    if (ctx_.tx[sfAccount] != dest)
        return temMALFORMED;

    auto const rawTx = ctx_.tx.getFieldVL(sfBtcRawTx);
    auto const proof = ctx_.tx.getFieldVL(sfBtcMerkleProof);
    auto const txIndex = ctx_.tx.getFieldU32(sfBtcTxIndex);
    auto const blockHash = ctx_.tx.getFieldH256(sfBtcBlockHash);
    auto const preferredVout = ctx_.tx.getFieldU32(sfBtcVout);

    auto const parsedTx = btcParseTx(makeSlice(rawTx));
    if (!parsedTx)
        return temMALFORMED;

    auto const hdr = view().read(keylet::btcHeader(blockHash));
    if (!hdr)
        return tecNO_ENTRY;

    auto const merkleRoot = hdr->getFieldH256(sfBtcMerkleRoot);
    if (!btcVerifyMerkleProof(parsedTx->txid, makeSlice(proof), txIndex, merkleRoot))
        return temMALFORMED;

    // Best-chain membership + depth
    auto const tipHash = state->getFieldH256(sfBtcTipHash);
    auto const tipHeight = state->getFieldU32(sfBtcTipHeight);
    auto const inclHeight = hdr->getFieldU32(sfBtcHeight);
    auto const minConf = state->getFieldU32(sfBtcMinConfirmations);

    // Walk from tip along prev links; must find blockHash within walk cap
    {
        uint256 cur = tipHash;
        bool found = false;
        std::uint32_t walked = 0;
        for (; walked < kBTC_MAX_BEST_CHAIN_WALK; ++walked)
        {
            if (cur == blockHash)
            {
                found = true;
                break;
            }
            auto const curHdr = view().read(keylet::btcHeader(cur));
            if (!curHdr)
                break;
            if (curHdr->getFieldU32(sfBtcHeight) <= inclHeight && cur != blockHash)
            {
                // walked past inclusion height without finding → orphan
                break;
            }
            cur = curHdr->getFieldH256(sfBtcPrevBlockHash);
            if (cur == uint256{})
                break;
        }
        if (!found)
            return tecNO_ENTRY;

        // depth including inclusion block
        if (tipHeight < inclHeight)
            return tecNO_ENTRY;
        std::uint32_t const depth = tipHeight - inclHeight + 1;
        if (depth < minConf)
            return tecTOO_SOON;
    }

    auto const watchHash = state->getFieldH256(sfBtcWatchScriptHash);
    // Optional BitVM vault witness script (P2WSH multi-user peg-in)
    Slice vaultScript{};
    Blob vaultBlob;
    if (ctx_.tx.isFieldPresent(sfBtcVaultScript))
    {
        vaultBlob = ctx_.tx.getFieldVL(sfBtcVaultScript);
        vaultScript = makeSlice(vaultBlob);
    }
    auto const extracted =
        btcExtractDeposit(*parsedTx, watchHash, preferredVout, vaultScript);
    if (!extracted)
        return temMALFORMED;
    if (extracted->destination != dest)
        return temMALFORMED;

    auto const V = extracted->valueSats;
    auto const chainId = state->getFieldU32(sfBtcChainId);
    if (V < btcDustFloor(chainId) || V > kMAX_MP_TOKEN_AMOUNT)
        return temMALFORMED;

    auto const total = state->getFieldU64(sfBtcTotalMinted);
    auto const cap = state->getFieldU64(sfBtcMintCap);
    if (V > cap || total > cap - V)
        return tecNO_PERMISSION;

    auto const vout = extracted->watchVout;
    auto const depKey = keylet::btcDeposit(parsedTx->txid, vout);
    if (view().exists(depKey))
        return tecDUPLICATE;

    uint192 const issuanceID = state->at(sfMPTokenIssuanceID);
    AccountID const issuer = state->at(sfAccount);

    // Ensure holder MPToken (VaultDeposit pattern)
    if (!view().exists(keylet::mptoken(issuanceID, dest)))
    {
        if (auto const err =
                authorizeMPToken(view(), preFeeBalance_, issuanceID, dest, ctx_.journal);
            !isTesSuccess(err))
            return err;
    }

    MPTIssue const issue{issuanceID};
    STAmount amt{issue, V};
    if (auto const err =
            accountSend(view(), issuer, dest, amt, ctx_.journal, WaiveTransferFee::Yes);
        !isTesSuccess(err))
        return err;

    auto sleDep = std::make_shared<SLE>(depKey);
    sleDep->setFieldH256(sfBtcTxID, parsedTx->txid);
    sleDep->setFieldU32(sfBtcVout, vout);
    sleDep->setFieldU64(sfBtcAmount, V);
    sleDep->setAccountID(sfDestination, dest);
    sleDep->setFieldH256(sfBtcBlockHash, blockHash);
    sleDep->setFieldU32(sfBtcHeight, inclHeight);
    sleDep->setFieldH256(sfBtcMerkleRoot, merkleRoot);
    sleDep->setFieldU32(sfBtcDepositStatus, kBTC_DEPOSIT_MINTED);
    sleDep->at(sfMPTokenIssuanceID) = issuanceID;
    sleDep->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleDep->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().insert(sleDep);

    state->setFieldU64(sfBtcTotalMinted, total + V);
    state->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    state->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().update(state);

    return tesSUCCESS;
}

void
BTCDepositClaim::visitInvariantEntry(
    bool isDelete,
    std::shared_ptr<SLE const> const& before,
    std::shared_ptr<SLE const> const& after)
{
    inv_.visitEntry(isDelete, before, after);
}

bool
BTCDepositClaim::finalizeInvariants(
    STTx const& tx,
    TER result,
    XRPAmount fee,
    ReadView const& view,
    beast::Journal const& j)
{
    return inv_.finalize(tx, result, fee, view, j);
}

}  // namespace xrpl
