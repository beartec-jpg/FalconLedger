// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/NameRelease.h>

#include <xrpl/ledger/ApplyView.h>
#include <xrpl/ledger/helpers/DirectoryHelpers.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>
#include <xrpl/tx/transactors/qxrp/AccountNameHelpers.h>

#include <algorithm>

namespace xrpl {

NotTEC
NameRelease::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureAccountNames))
        return temDISABLED;

    if (!ctx.tx.isFieldPresent(sfName))
        return temMALFORMED;

    auto const name = account_names::normalizeName(ctx.tx.getFieldVL(sfName));
    if (!name)
        return temMALFORMED;

    auto const raw = ctx.tx.getFieldVL(sfName);
    if (raw.size() != name->size() ||
        !std::equal(raw.begin(), raw.end(), name->begin()))
        return temMALFORMED;

    return tesSUCCESS;
}

TER
NameRelease::preclaim(PreclaimContext const& ctx)
{
    auto const name = *account_names::normalizeName(ctx.tx.getFieldVL(sfName));
    auto const sleName = ctx.view.read(keylet::accountName(makeSlice(name)));
    if (!sleName)
        return tecNO_ENTRY;

    if (sleName->getFieldU32(sfNameStatus) != kNAME_STATUS_RELEASING)
        return tecNO_PERMISSION;

    auto const startLedger = sleName->getFieldU32(sfUnbondingStartLedger);
    if (ctx.view.seq() < startLedger + kNAME_UNBOND_LEDGERS)
        return tecTOO_SOON;

    return tesSUCCESS;
}

TER
NameRelease::doApply()
{
    auto const name = *account_names::normalizeName(ctx_.tx.getFieldVL(sfName));
    auto sleName = ctx_.view().peek(keylet::accountName(makeSlice(name)));
    if (!sleName)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const owner = sleName->getAccountID(sfAccount);
    auto sleAccount = ctx_.view().peek(keylet::account(owner));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // Return bond capital to owner.
    auto const bondedAmount = sleName->getFieldAmount(sfBondedAmount);
    if (bondedAmount > beast::kZERO)
    {
        auto const prev = sleAccount->getFieldAmount(sfBalance);
        sleAccount->setFieldAmount(sfBalance, prev + bondedAmount);
    }

    // Remove from owner directory.
    auto const page = sleName->getFieldU64(sfOwnerNode);
    if (!ctx_.view().dirRemove(
            keylet::ownerDir(owner), page, sleName->key(), /*keepRoot=*/false))
        return tefBAD_LEDGER;

    // Clear reverse pointer if it still points at this name.
    if (sleAccount->isFieldPresent(sfAccountName) &&
        sleAccount->getFieldH256(sfAccountName) == sleName->key())
    {
        sleAccount->makeFieldAbsent(sfAccountName);
    }

    auto const ownerCount = sleAccount->getFieldU32(sfOwnerCount);
    if (ownerCount > 0)
        sleAccount->setFieldU32(sfOwnerCount, ownerCount - 1);
    ctx_.view().update(sleAccount);

    ctx_.view().erase(sleName);

    return tesSUCCESS;
}

void
NameRelease::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
NameRelease::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
