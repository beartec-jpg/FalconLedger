// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ValidatorBond.h>

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
ValidatorBond::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    // sfBondedAmount must be XRP, positive, and meet the minimum.
    auto const bondAmount = ctx.tx[sfBondedAmount];
    if (!bondAmount.native())
        return temBAD_AMOUNT;
    if (bondAmount.signum() <= 0)
        return temBAD_AMOUNT;
    if (bondAmount.mantissa() < static_cast<std::uint64_t>(kQXRP_MIN_BOND_DROPS))
        return temBAD_AMOUNT;

    return tesSUCCESS;
}

TER
ValidatorBond::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];

    auto sleBond = ctx.view.read(keylet::validatorBond(account));
    if (!sleBond)
        return tecNO_ENTRY;  // must ValidatorRegister first

    if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_REGISTERED)
        return tecNO_PERMISSION;  // already bonded or unbonding

    return tesSUCCESS;
}

TER
ValidatorBond::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const bondAmount = ctx_.tx[sfBondedAmount];

    // Deduct bond from account balance.
    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const balance = sleAccount->getFieldAmount(sfBalance);
    if (balance < bondAmount)
        return tecUNFUNDED;

    sleAccount->setFieldAmount(sfBalance, balance - bondAmount);
    ctx_.view().update(sleAccount);

    // Update bond object.
    auto sleBond = ctx_.view().peek(keylet::validatorBond(account));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    sleBond->setFieldAmount(sfBondedAmount, bondAmount);
    sleBond->setFieldU32(sfBondStatus, kBOND_STATUS_BONDED);
    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleBond);

    return tesSUCCESS;
}

void
ValidatorBond::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ValidatorBond::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
