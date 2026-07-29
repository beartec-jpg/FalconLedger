// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/btc_spv/BTCBridgeActivate.h>

#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/core/NetworkIDService.h>
#include <xrpl/core/ServiceRegistry.h>
#include <xrpl/ledger/helpers/AccountRootHelpers.h>
#include <xrpl/protocol/BTCHeader.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFlags.h>
#include <xrpl/tx/transactors/token/MPTokenIssuanceCreate.h>

#include <cstring>
#include <string>

namespace xrpl {

NotTEC
BTCBridgeActivate::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureBitcoinSPVBridge))
        return temDISABLED;

    auto const header = ctx.tx.getFieldVL(sfBtcHeaderBytes);
    if (header.size() != kBTC_HEADER_SIZE)
        return temMALFORMED;

    auto const chainId = ctx.tx.getFieldU32(sfBtcChainId);
    if (chainId > kBTC_CHAIN_REGTEST)
        return temMALFORMED;

    auto const minConf = ctx.tx.getFieldU32(sfBtcMinConfirmations);
    if (minConf < btcMinConfFloor(chainId))
        return temMALFORMED;

    auto const mintCap = ctx.tx.getFieldU64(sfBtcMintCap);
    if (mintCap == 0 || mintCap > kMAX_MP_TOKEN_AMOUNT)
        return temMALFORMED;

    auto const parsed = btcParseHeader(makeSlice(header));
    if (!parsed)
        return temMALFORMED;

    if (parsed->blockHash != ctx.tx.getFieldH256(sfBtcAnchorHash))
        return temMALFORMED;

    // Regtest/prototype: require PoW meets bits (trivial on regtest).
    if (!btcHashMeetsTarget(parsed->blockHash, parsed->bits))
        return temMALFORMED;

    return tesSUCCESS;
}

TER
BTCBridgeActivate::preclaim(PreclaimContext const& ctx)
{
    if (ctx.view.read(keylet::btcBridgeState()))
        return tecDUPLICATE;

    // Prototype: allow activate on isolated net 1101 or networkID 0 (unit tests).
    // Production mainnet must use genesis injection / ceremony — not permissionless.
    std::uint32_t nid = 0;
    try
    {
        nid = ctx.registry.get().getNetworkIDService().getNetworkID();
    }
    catch (...)
    {
        nid = 0;
    }
    if (nid != 0 && nid != kBTC_SPV_ISOLATED_NETWORK_ID)
        return tecNO_PERMISSION;

    auto const stateKey = keylet::btcBridgeState().key;
    if (pseudoAccountAddress(ctx.view, stateKey) == beast::kZERO)
        return terADDRESS_COLLISION;

    return tesSUCCESS;
}

TER
BTCBridgeActivate::doApply()
{
    auto const& tx = ctx_.tx;
    auto const headerBytes = tx.getFieldVL(sfBtcHeaderBytes);
    auto const parsed = btcParseHeader(makeSlice(headerBytes));
    if (!parsed)
        return tecINTERNAL;

    auto const stateKeylet = keylet::btcBridgeState();
    auto const chainId = tx.getFieldU32(sfBtcChainId);
    auto const anchorHeight = tx.getFieldU32(sfBtcAnchorHeight);
    auto minConf = tx.getFieldU32(sfBtcMinConfirmations);
    auto const floor = btcMinConfFloor(chainId);
    if (minConf < floor)
        minConf = floor;

    auto const mintCap = tx.getFieldU64(sfBtcMintCap);
    auto const watchHash = tx.getFieldH256(sfBtcWatchScriptHash);
    auto const anchorWork = tx.getFieldH256(sfBtcAnchorWork);

    auto maybePseudo = createPseudoAccount(view(), stateKeylet.key, sfBtcBridgeID);
    if (!maybePseudo)
        return maybePseudo.error();
    auto& pseudo = *maybePseudo;
    // ValueProxy<STAccount> — same pattern as VaultCreate / LoanBrokerSet
    auto const pseudoId = pseudo->at(sfAccount);
    AccountID const issuerAccount = *pseudoId;
    if (!issuerAccount || issuerAccount == noAccount())
        return tefINTERNAL;

    Blob meta;
    if (tx.isFieldPresent(sfMPTokenMetadata))
        meta = tx.getFieldVL(sfMPTokenMetadata);
    else
    {
        auto const* d = kBTC_FBTC_METADATA_DISCLAIMER;
        meta.assign(d, d + std::strlen(d));
    }
    std::optional<Slice> const metaOpt{makeSlice(meta)};

    auto maybeShare = MPTokenIssuanceCreate::create(
        view(),
        j_,
        {
            .priorBalance = std::nullopt,
            .account = issuerAccount,
            .sequence = 1,
            .flags = lsfMPTCanTransfer | lsfMPTCanTrade | lsfMPTCanEscrow,
            .maxAmount = mintCap,
            .assetScale = 0,
            .transferFee = std::nullopt,
            .metadata = metaOpt,
            .domainId = std::nullopt,
            .mutableFlags = std::nullopt,
        });
    if (!maybeShare)
        return maybeShare.error();
    auto const mptIssuanceID = *maybeShare;

    auto state = std::make_shared<SLE>(stateKeylet);
    state->setFieldU32(sfBtcChainId, chainId);
    state->setFieldH256(sfBtcAnchorHash, parsed->blockHash);
    state->setFieldU32(sfBtcAnchorHeight, anchorHeight);
    state->setFieldH256(sfBtcTipHash, parsed->blockHash);
    state->setFieldU32(sfBtcTipHeight, anchorHeight);
    state->setFieldH256(sfBtcTipWork, anchorWork);
    state->setFieldU32(sfBtcMinConfirmations, minConf);
    state->setFieldH256(sfBtcWatchScriptHash, watchHash);
    state->setFieldU64(sfBtcMintCap, mintCap);
    // sfBtcPaused default 0
    state->setAccountID(sfAccount, issuerAccount);
    state->at(sfMPTokenIssuanceID) = mptIssuanceID;
    state->setFieldU64(sfBtcTotalMinted, 0);
    state->setFieldH256(sfPreviousTxnID, tx.getTransactionID());
    state->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().insert(state);

    // Anchor header SLE
    auto hdr = std::make_shared<SLE>(keylet::btcHeader(parsed->blockHash));
    hdr->setFieldH256(sfBtcBlockHash, parsed->blockHash);
    hdr->setFieldU32(sfBtcHeight, anchorHeight);
    hdr->setFieldVL(sfBtcHeaderBytes, headerBytes);
    hdr->setFieldH256(sfBtcChainWork, anchorWork);
    hdr->setFieldH256(sfBtcPrevBlockHash, parsed->prevHash);
    hdr->setFieldH256(sfBtcMerkleRoot, parsed->merkleRoot);
    hdr->setFieldH256(sfPreviousTxnID, tx.getTransactionID());
    hdr->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().insert(hdr);

    // Height index
    auto hi = std::make_shared<SLE>(keylet::btcHeight(anchorHeight));
    hi->setFieldU32(sfBtcHeight, anchorHeight);
    hi->setFieldH256(sfBtcBlockHash, parsed->blockHash);
    hi->setFieldH256(sfBtcChainWork, anchorWork);
    hi->setFieldH256(sfPreviousTxnID, tx.getTransactionID());
    hi->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().insert(hi);

    return tesSUCCESS;
}

void
BTCBridgeActivate::visitInvariantEntry(
    bool isDelete,
    std::shared_ptr<SLE const> const& before,
    std::shared_ptr<SLE const> const& after)
{
    inv_.visitEntry(isDelete, before, after);
}

bool
BTCBridgeActivate::finalizeInvariants(
    STTx const& tx,
    TER result,
    XRPAmount fee,
    ReadView const& view,
    beast::Journal const& j)
{
    return inv_.finalize(tx, result, fee, view, j);
}

}  // namespace xrpl
