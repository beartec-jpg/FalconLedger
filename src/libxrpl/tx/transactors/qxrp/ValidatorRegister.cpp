// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ValidatorRegister.h>

#include <xrpl/ledger/helpers/AccountRootHelpers.h>
#include <xrpl/ledger/helpers/DirectoryHelpers.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

namespace xrpl {

NotTEC
ValidatorRegister::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    return tesSUCCESS;
}

TER
ValidatorRegister::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];

    // Reject duplicate registration.
    if (ctx.view.read(keylet::validatorBond(account)))
        return tecDUPLICATE;

    return tesSUCCESS;
}

TER
ValidatorRegister::doApply()
{
    auto const account = ctx_.tx[sfAccount];

    auto sleBond = std::make_shared<SLE>(keylet::validatorBond(account));
    sleBond->setAccountID(sfAccount, account);
    sleBond->setFieldAmount(sfBondedAmount, STAmount{XRPAmount{0}});
    sleBond->setFieldU32(sfBondStatus, kBOND_STATUS_REGISTERED);
    sleBond->setFieldU32(sfSlashMultiplier, kBPS_DENOM);  // start at 10 000 = clean
    sleBond->setFieldU32(sfUptimeBps, 0);
    sleBond->setFieldU32(sfVoteAccuracyBps, 0);
    sleBond->setFieldU32(sfLatencyScoreBps, 0);
    sleBond->setFieldU32(sfConsistencyBps, 0);
    sleBond->setFieldU32(sfCompositeScore, 0);
    sleBond->setFieldU32(sfSlashCount, 0);
    sleBond->setFieldU32(sfLastClaimedEpoch, 0);
    sleBond->setFieldU32(sfUnbondingStartLedger, 0);
    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, view().seq());

    // Add to owner directory for reserve counting.
    auto const page = ctx_.view().dirInsert(
        keylet::ownerDir(account),
        sleBond->key(),
        describeOwnerDir(account));
    if (!page)
        return tecDIR_FULL;

    sleBond->setFieldU64(sfOwnerNode, *page);
    ctx_.view().insert(sleBond);

    // Bump owner count — the bond object counts against reserve.
    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const ownerCount = sleAccount->getFieldU32(sfOwnerCount);
    sleAccount->setFieldU32(sfOwnerCount, ownerCount + 1);
    ctx_.view().update(sleAccount);

    return tesSUCCESS;
}

void
ValidatorRegister::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ValidatorRegister::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
