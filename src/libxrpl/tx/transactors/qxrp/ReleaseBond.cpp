// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ReleaseBond.h>

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

namespace xrpl {

NotTEC
ReleaseBond::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    return tesSUCCESS;
}

TER
ReleaseBond::preclaim(PreclaimContext const& ctx)
{
    auto const target = ctx.tx.getAccountID(sfSlashTarget);

    auto const sleBond = ctx.view.read(keylet::validatorBond(target));
    if (!sleBond)
        return tecNO_ENTRY;

    // Only bonds in the unbonding state can be released.
    if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_UNBONDING)
        return tecNO_PERMISSION;

    // The lock period must have elapsed.
    auto const startLedger = sleBond->getFieldU32(sfUnbondingStartLedger);
    if (ctx.view.seq() < startLedger + kUNBONDING_LOCK_LEDGERS)
        return tecTOO_SOON;

    return tesSUCCESS;
}

TER
ReleaseBond::doApply()
{
    auto const target = ctx_.tx.getAccountID(sfSlashTarget);

    auto sleBond = ctx_.view().peek(keylet::validatorBond(target));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // The bond's sfAccount is the account that registered this validator.
    // sfSlashTarget is the consensus-key-derived ID (== bond keylet key) which
    // may differ from the registrant's actual XRPL account if they used a
    // different signing account at registration time.
    auto const bondOwner = sleBond->getFieldAccountID(sfAccount);
    auto sleAccount = ctx_.view().peek(keylet::account(bondOwner));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // ── Return remaining bond capital to validator ────────────────────────
    auto const bondedAmount = sleBond->getFieldAmount(sfBondedAmount);
    if (bondedAmount > beast::kZERO)
    {
        auto const prev = sleAccount->getFieldAmount(sfBalance);
        sleAccount->setFieldAmount(sfBalance, prev + bondedAmount);
        ctx_.view().update(sleAccount);
    }

    // ── Remove bond from owner directory ─────────────────────────────────
    auto const page = sleBond->getFieldU64(sfOwnerNode);
    if (!ctx_.view().dirRemove(
            keylet::ownerDir(bondOwner), page, sleBond->key(), /*keepRoot=*/false))
        return tefBAD_LEDGER;

    // ── Decrement owner count ─────────────────────────────────────────────
    auto const ownerCount = sleAccount->getFieldU32(sfOwnerCount);
    if (ownerCount > 0)
        sleAccount->setFieldU32(sfOwnerCount, ownerCount - 1);
    ctx_.view().update(sleAccount);

    // ── Erase the bond ledger object ──────────────────────────────────────
    ctx_.view().erase(sleBond);

    return tesSUCCESS;
}

void
ReleaseBond::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ReleaseBond::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
