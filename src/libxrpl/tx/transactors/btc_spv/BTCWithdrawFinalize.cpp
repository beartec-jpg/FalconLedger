// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/btc_spv/BTCWithdrawFinalize.h>

#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>

namespace xrpl {

NotTEC
BTCWithdrawFinalize::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureBitcoinSPVBridge))
        return temDISABLED;
    return tesSUCCESS;
}

TER
BTCWithdrawFinalize::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const seq = ctx.tx.getFieldU32(sfBtcWithdrawSeq);
    auto const w = ctx.view.read(keylet::btcWithdraw(account, seq));
    if (!w)
        return tecNO_ENTRY;

    if (w->getAccountID(sfAccount) != account)
        return tecNO_PERMISSION;

    auto const status = w->getFieldU32(sfBtcWithdrawStatus);
    if (status == kBTC_WITHDRAW_FINAL || status == kBTC_WITHDRAW_PAID)
        return tecDUPLICATE;
    if (status == kBTC_WITHDRAW_CHALLENGED)
        return tecNO_PERMISSION;
    if (status != kBTC_WITHDRAW_PENDING)
        return tecNO_ENTRY;

    if (ctx.view.seq() <= w->getFieldU32(sfBtcChallengeEndLedger))
        return tecTOO_SOON;

    return tesSUCCESS;
}

TER
BTCWithdrawFinalize::doApply()
{
    auto const seq = ctx_.tx.getFieldU32(sfBtcWithdrawSeq);
    auto w = view().peek(keylet::btcWithdraw(account_, seq));
    if (!w)
        return tecNO_ENTRY;

    w->setFieldU32(sfBtcWithdrawStatus, kBTC_WITHDRAW_FINAL);
    w->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    w->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    view().update(w);
    return tesSUCCESS;
}

}  // namespace xrpl
