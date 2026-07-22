// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/NameSet.h>

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
NameSet::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureAccountNames))
        return temDISABLED;

    if (!ctx.tx.isFieldPresent(sfName))
        return temMALFORMED;

    auto const name = account_names::normalizeName(ctx.tx.getFieldVL(sfName));
    if (!name)
        return temMALFORMED;

    // Require the submitted form to already match normalized lowercase.
    auto const raw = ctx.tx.getFieldVL(sfName);
    if (raw.size() != name->size() ||
        !std::equal(raw.begin(), raw.end(), name->begin()))
        return temMALFORMED;

    return tesSUCCESS;
}

TER
NameSet::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const name = *account_names::normalizeName(ctx.tx.getFieldVL(sfName));
    auto const nameKey = keylet::accountName(makeSlice(name));

    if (ctx.view.read(nameKey))
        return tecDUPLICATE;

    auto const sleAccount = ctx.view.read(keylet::account(account));
    if (!sleAccount)
        return terNO_ACCOUNT;

    // One name per account (active or releasing).
    if (sleAccount->isFieldPresent(sfAccountName))
        return tecDUPLICATE;

    auto const balance = sleAccount->getFieldAmount(sfBalance);
    auto const bond = STAmount{XRPAmount{kNAME_BOND_DROPS}};
    if (balance < bond)
        return tecUNFUNDED;

    return tesSUCCESS;
}

TER
NameSet::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const name = *account_names::normalizeName(ctx_.tx.getFieldVL(sfName));
    auto const bond = STAmount{XRPAmount{kNAME_BOND_DROPS}};

    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const balance = sleAccount->getFieldAmount(sfBalance);
    if (balance < bond)
        return tecUNFUNDED;

    sleAccount->setFieldAmount(sfBalance, balance - bond);

    auto sleName = std::make_shared<SLE>(keylet::accountName(makeSlice(name)));
    sleName->setFieldVL(sfName, makeSlice(name));
    sleName->setAccountID(sfAccount, account);
    sleName->setFieldAmount(sfBondedAmount, bond);
    sleName->setFieldU32(sfNameStatus, kNAME_STATUS_ACTIVE);
    sleName->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleName->setFieldU32(sfPreviousTxnLgrSeq, view().seq());

    auto const page = ctx_.view().dirInsert(
        keylet::ownerDir(account), sleName->key(), describeOwnerDir(account));
    if (!page)
        return tecDIR_FULL;

    sleName->setFieldU64(sfOwnerNode, *page);
    ctx_.view().insert(sleName);

    // Reverse pointer for O(1) one-name-per-account checks.
    sleAccount->setFieldH256(sfAccountName, sleName->key());
    auto const ownerCount = sleAccount->getFieldU32(sfOwnerCount);
    sleAccount->setFieldU32(sfOwnerCount, ownerCount + 1);
    ctx_.view().update(sleAccount);

    return tesSUCCESS;
}

void
NameSet::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
NameSet::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
