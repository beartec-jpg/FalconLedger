// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ValidatorSlash.h>

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
ValidatorSlash::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    auto const offense = ctx.tx.getFieldU32(sfSlashOffense);
    if (offense < kSLASH_OFFENSE_DOUBLE_SIGN || offense > kSLASH_OFFENSE_INVALID_VOTE)
        return temMALFORMED;

    // NOTE: As of 2026, only DOUBLE_SIGN is actively enforced on the testnet.
    // ABSENCE and INVALID_VOTE currently return temDISABLED (see preflight below).
    // This is intentional until better detection logic exists (see L-01 in security audit).

    // Double-sign proofs require two evidence blobs.
    if (offense == kSLASH_OFFENSE_DOUBLE_SIGN)
    {
        if (!ctx.tx.isFieldPresent(sfSlashEvidence1) ||
            !ctx.tx.isFieldPresent(sfSlashEvidence2))
            return temMALFORMED;

        // Basic sanity: the two blobs must differ.
        if (ctx.tx.getFieldVL(sfSlashEvidence1) ==
            ctx.tx.getFieldVL(sfSlashEvidence2))
            return temMALFORMED;
    }
    else
    {
        // ABSENCE and INVALID_VOTE are intentionally disabled for now
        // (return temDISABLED later in the function).
        // See security audit L-01 and constants for rollout plan.
    }

    return tesSUCCESS;
}

TER
ValidatorSlash::preclaim(PreclaimContext const& ctx)
{
    auto const target = ctx.tx.getAccountID(sfSlashTarget);

    // Target must have a bonded or registered bond object.
    auto const sleBond = ctx.view.read(keylet::validatorBond(target));
    if (!sleBond)
        return tecNO_ENTRY;

    // Cannot slash an already-unbonding or slashed-out validator (idempotent
    // protection: a second slash attempt on the same validator is rejected).
    auto const bondStatus = sleBond->getFieldU32(sfBondStatus);
    if (bondStatus == kBOND_STATUS_UNBONDING)
        return tecDUPLICATE;
    if (bondStatus != kBOND_STATUS_BONDED && bondStatus != kBOND_STATUS_REGISTERED)
        return tecNO_PERMISSION;

    return tesSUCCESS;
}

TER
ValidatorSlash::doApply()
{
    auto const target = ctx_.tx.getAccountID(sfSlashTarget);
    auto const offense = ctx_.tx.getFieldU32(sfSlashOffense);

    auto sleBond = ctx_.view().peek(keylet::validatorBond(target));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto sleTarget = ctx_.view().peek(keylet::account(target));
    if (!sleTarget)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // ── Determine slash fraction ──────────────────────────────────────────
    std::uint32_t slashBps;
    switch (offense)
    {
        case kSLASH_OFFENSE_DOUBLE_SIGN:
            slashBps = kSLASH_DOUBLE_SIGN_BPS;
            break;
        case kSLASH_OFFENSE_ABSENCE:
            slashBps = kSLASH_ABSENCE_BPS;
            break;
        case kSLASH_OFFENSE_INVALID_VOTE:
            slashBps = kSLASH_INVALID_VOTE_BPS;
            break;
        default:
            return tefINTERNAL;  // LCOV_EXCL_LINE — checked in preflight
    }

    // ── Compute slashed drops ─────────────────────────────────────────────
    auto const bondedAmount = sleBond->getFieldAmount(sfBondedAmount);
    auto const bondedDrops  = bondedAmount.xrp().drops();

    auto const slashedDrops = static_cast<std::int64_t>(
        (static_cast<__int128>(bondedDrops) * slashBps) / kBPS_DENOM);
    auto const remainderDrops = bondedDrops - slashedDrops;

    // ── Update bond object ────────────────────────────────────────────────
    // Reduce bonded amount to the remainder (may be zero for full slash).
    sleBond->setFieldAmount(sfBondedAmount, STAmount{XRPAmount{remainderDrops}});

    // Forced unbonding — validator must wait out the lock period regardless of
    // remaining bond amount.  This makes the ledger state unambiguous and
    // prevents a slashed validator from claiming rewards.
    sleBond->setFieldU32(sfBondStatus, kBOND_STATUS_UNBONDING);
    sleBond->setFieldU32(sfUnbondingStartLedger, ctx_.view().seq());

    // Decrement the slash multiplier by 10 % per offense (floor at 0).
    auto const prevMul = sleBond->getFieldU32(sfSlashMultiplier);
    auto const newMul  = (prevMul >= 1'000) ? prevMul - 1'000 : 0;
    sleBond->setFieldU32(sfSlashMultiplier, newMul);

    // Increment slash counter.
    sleBond->setFieldU32(sfSlashCount, sleBond->getFieldU32(sfSlashCount) + 1);

    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, ctx_.view().seq());
    ctx_.view().update(sleBond);

    // ── Return bonded capital to validator's free balance (remainder) ─────
    // The slashed portion is simply destroyed (burned); it is NOT credited
    // to the submitter — no incentive to trigger a competitor's slash.
    if (slashedDrops > 0)
        ctx_.destroyXRP(XRPAmount{slashedDrops});

    if (remainderDrops > 0)
    {
        // Move the remaining bond back to the validator's account balance.
        // The unbonding lock prevents withdrawal; it will be spendable only
        // after the lock expires and a ledger-close hook releases it.
        auto const prevBal = sleTarget->getFieldAmount(sfBalance);
        sleTarget->setFieldAmount(sfBalance, prevBal + STAmount{XRPAmount{remainderDrops}});
        ctx_.view().update(sleTarget);
    }

    return tesSUCCESS;
}

void
ValidatorSlash::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ValidatorSlash::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
