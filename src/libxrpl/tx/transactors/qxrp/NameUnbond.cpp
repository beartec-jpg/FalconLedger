// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/NameUnbond.h>

#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>
#include <xrpl/tx/transactors/qxrp/AccountNameHelpers.h>

#include <algorithm>

namespace xrpl {

namespace {

std::optional<Keylet>
resolveNameKeylet(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    if (ctx.tx.isFieldPresent(sfName))
    {
        auto const name = account_names::normalizeName(ctx.tx.getFieldVL(sfName));
        if (!name)
            return std::nullopt;
        return keylet::accountName(makeSlice(*name));
    }

    auto const sleAccount = ctx.view.read(keylet::account(account));
    if (!sleAccount || !sleAccount->isFieldPresent(sfAccountName))
        return std::nullopt;

    return keylet::accountName(sleAccount->getFieldH256(sfAccountName));
}

std::optional<Keylet>
resolveNameKeyletApply(ApplyContext& ctx)
{
    auto const account = ctx.tx[sfAccount];
    if (ctx.tx.isFieldPresent(sfName))
    {
        auto const name = account_names::normalizeName(ctx.tx.getFieldVL(sfName));
        if (!name)
            return std::nullopt;
        return keylet::accountName(makeSlice(*name));
    }

    auto const sleAccount = ctx.view().read(keylet::account(account));
    if (!sleAccount || !sleAccount->isFieldPresent(sfAccountName))
        return std::nullopt;

    return keylet::accountName(sleAccount->getFieldH256(sfAccountName));
}

}  // namespace

NotTEC
NameUnbond::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureAccountNames))
        return temDISABLED;

    if (ctx.tx.isFieldPresent(sfName))
    {
        auto const name = account_names::normalizeName(ctx.tx.getFieldVL(sfName));
        if (!name)
            return temMALFORMED;
        auto const raw = ctx.tx.getFieldVL(sfName);
        if (raw.size() != name->size() ||
            !std::equal(raw.begin(), raw.end(), name->begin()))
            return temMALFORMED;
    }

    return tesSUCCESS;
}

TER
NameUnbond::preclaim(PreclaimContext const& ctx)
{
    auto const kl = resolveNameKeylet(ctx);
    if (!kl)
        return tecNO_ENTRY;

    auto const sleName = ctx.view.read(*kl);
    if (!sleName)
        return tecNO_ENTRY;

    if (sleName->getAccountID(sfAccount) != ctx.tx[sfAccount])
        return tecNO_PERMISSION;

    if (sleName->getFieldU32(sfNameStatus) != kNAME_STATUS_ACTIVE)
        return tecNO_PERMISSION;

    return tesSUCCESS;
}

TER
NameUnbond::doApply()
{
    auto const kl = resolveNameKeyletApply(ctx_);
    if (!kl)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto sleName = ctx_.view().peek(*kl);
    if (!sleName)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    sleName->setFieldU32(sfNameStatus, kNAME_STATUS_RELEASING);
    sleName->setFieldU32(sfUnbondingStartLedger, view().seq());
    sleName->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleName->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleName);

    return tesSUCCESS;
}

void
NameUnbond::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
NameUnbond::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
