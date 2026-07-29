// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/btc_spv/BTCBridgeBurn.h>

#include <xrpl/basics/Slice.h>
#include <xrpl/ledger/helpers/DirectoryHelpers.h>
#include <xrpl/ledger/helpers/TokenHelpers.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/digest.h>

namespace xrpl {
namespace {

uint256
sha256Commit(Slice preimage)
{
    sha256_hasher h;
    h(preimage.data(), preimage.size());
    auto const d = static_cast<sha256_hasher::result_type>(h);
    uint256 out;
    // store as big-endian numeric
    for (int i = 0; i < 32; ++i)
        out.data()[i] = d[i];
    return out;
}

}  // namespace

NotTEC
BTCBridgeBurn::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureBitcoinSPVBridge))
        return temDISABLED;

    auto const amount = ctx.tx.getFieldU64(sfBtcWithdrawAmount);
    if (amount == 0)
        return temMALFORMED;

    auto const payout = ctx.tx.getFieldVL(sfBtcPayoutScript);
    if (payout.empty() || payout.size() > 100)
        return temMALFORMED;

    auto const pre = ctx.tx.getFieldVL(sfBtcBurnPreimage);
    if (pre.size() < 32 || pre.size() > 128)
        return temMALFORMED;

    return tesSUCCESS;
}

TER
BTCBridgeBurn::preclaim(PreclaimContext const& ctx)
{
    auto const state = ctx.view.read(keylet::btcBridgeState());
    if (!state)
        return tecNO_ENTRY;

    auto const amount = ctx.tx.getFieldU64(sfBtcWithdrawAmount);
    auto const total = state->getFieldU64(sfBtcTotalMinted);
    if (amount > total)
        return tecINSUFFICIENT_FUNDS;

    auto const account = ctx.tx[sfAccount];
    auto const issuanceID = state->at(sfMPTokenIssuanceID);
    auto const mpt = ctx.view.read(keylet::mptoken(issuanceID, account));
    if (!mpt)
        return tecNO_ENTRY;
    if (mpt->getFieldU64(sfMPTAmount) < amount)
        return tecINSUFFICIENT_FUNDS;

    auto const seq = ctx.tx.getSeqValue();
    if (ctx.view.exists(keylet::btcWithdraw(account, seq)))
        return tecDUPLICATE;

    return tesSUCCESS;
}

TER
BTCBridgeBurn::doApply()
{
    auto state = view().peek(keylet::btcBridgeState());
    if (!state)
        return tecNO_ENTRY;

    auto const amount = ctx_.tx.getFieldU64(sfBtcWithdrawAmount);
    auto const payout = ctx_.tx.getFieldVL(sfBtcPayoutScript);
    auto const preimage = ctx_.tx.getFieldVL(sfBtcBurnPreimage);
    auto const commit = sha256Commit(makeSlice(preimage));

    auto const issuanceID = state->at(sfMPTokenIssuanceID);
    auto const issuer = state->at(sfAccount);
    auto const account = account_;
    auto const seq = ctx_.tx.getSeqValue();

    // Burn: holder → pseudo issuer (reduces outstanding)
    MPTIssue const issue{issuanceID};
    STAmount amt{issue, amount};
    if (auto const err = accountSend(view(), account, issuer, amt, ctx_.journal, WaiveTransferFee::Yes);
        !isTesSuccess(err))
        return err;

    auto const total = state->getFieldU64(sfBtcTotalMinted);
    if (amount > total)
        return tecINTERNAL;
    state->setFieldU64(sfBtcTotalMinted, total - amount);
    state->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    state->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().update(state);

    auto w = std::make_shared<SLE>(keylet::btcWithdraw(account, seq));
    w->setAccountID(sfAccount, account);
    w->setFieldU64(sfBtcWithdrawAmount, amount);
    w->setFieldVL(sfBtcPayoutScript, payout);
    w->setFieldH256(sfBtcBurnCommit, commit);
    w->setFieldU32(sfBtcWithdrawStatus, kBTC_WITHDRAW_PENDING);
    w->setFieldU32(sfBtcChallengeEndLedger, view().seq() + kBTC_CHALLENGE_LEDGERS_DEFAULT);
    w->setFieldU32(sfBtcWithdrawSeq, seq);
    w->at(sfMPTokenIssuanceID) = issuanceID;
    w->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    w->setFieldU32(sfPreviousTxnLgrSeq, view().seq());

    if (auto const ter = dirLink(view(), account, w))
        return ter;

    view().insert(w);
    return tesSUCCESS;
}

}  // namespace xrpl
