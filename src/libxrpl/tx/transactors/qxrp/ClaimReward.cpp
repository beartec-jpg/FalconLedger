// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ClaimReward.h>

#include <xrpl/basics/WideArith.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/tx/ApplyContext.h>

namespace xrpl {

NotTEC
ClaimReward::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    // sfConsensusKey must be a classical secp256k1 or ed25519 node key.
    auto const ckBlob = ctx.tx.getFieldVL(sfConsensusKey);
    if (!publicKeyType(makeSlice(ckBlob)))
        return temINVALID_FLAG;

    return tesSUCCESS;
}

TER
ClaimReward::preclaim(PreclaimContext const& ctx)
{
    auto const ckBlob = ctx.tx.getFieldVL(sfConsensusKey);

    // Must have a registered + bonded bond object.
    auto sleBond = ctx.view.read(keylet::validatorBond(calcValidatorBondID(makeSlice(ckBlob))));
    if (!sleBond)
        return tecNO_ENTRY;

    if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
        return tecNO_PERMISSION;

    // Composite score must clear the minimum threshold.
    if (sleBond->getFieldU32(sfCompositeScore) < kMIN_COMPOSITE_SCORE_BPS)
        return tecINSUFF_FEE;  // repurposed: insufficient quality

    // Epoch tracker must exist.
    if (!ctx.view.read(keylet::rewardEpoch()))
        return tecNO_ENTRY;

    return tesSUCCESS;
}

TER
ClaimReward::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const ckBlob = ctx_.tx.getFieldVL(sfConsensusKey);

    auto sleBond = ctx_.view().peek(keylet::validatorBond(calcValidatorBondID(makeSlice(ckBlob))));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto sleEpoch = ctx_.view().peek(keylet::rewardEpoch());
    if (!sleEpoch)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const currentEpoch = sleEpoch->getFieldU32(sfEpochNumber);
    auto const lastClaimed  = sleBond->getFieldU32(sfLastClaimedEpoch);

    // Duplicate claim within the same epoch.
    if (lastClaimed >= currentEpoch)
        return tecDUPLICATE;

    // ── Proportional share computation ───────────────────────────────────
    // share = emissionRate * compositeScore / aggregateCompositeScore
    // All arithmetic is integer; no floating point.
    //
    // sfEmissionRate is the FIXED original pool committed at epoch start.
    // sfEpochPoolBalance decreases as validators claim — using it for share
    // computation would cause first-claimers to receive disproportionately
    // large rewards (early claim unfairness bug).  Use the fixed emission
    // rate so every validator's share is based on the same denominator,
    // and shares across all validators sum to exactly sfEmissionRate.
    auto const compositeScore     = sleBond->getFieldU32(sfCompositeScore);
    auto const aggregateScore     = sleEpoch->getFieldU32(sfAggregateCompositeScore);
    auto const poolBalance        = sleEpoch->getFieldAmount(sfEpochPoolBalance);
    auto const emissionRate       = sleEpoch->getFieldAmount(sfEmissionRate);

    if (aggregateScore == 0)
        return tecNO_PERMISSION;  // no eligible validators — shouldn't reach here

    // Use muldiv64: no overflow because compositeScore <= aggregateScore
    // (one validator's score vs. the sum of all bonded validators' scores).
    // Both arguments fit in uint32; the result fits in int64.
    //
    // SECURITY NOTE (2026 audit item 2.5): For very large validator sets
    // the aggregateScore can grow, but the design + muldiv64 keeps it safe.
    // A deeper review for >1000 validators is still recommended.
    auto const emissionDrops = emissionRate.xrp().drops();
    auto const shareDrops = muldiv64(emissionDrops, compositeScore, aggregateScore);

    if (shareDrops <= 0)
        return tesSUCCESS;  // rounding to zero — no reward to distribute

    // ── Transfer: treasury → validator ───────────────────────────────────
    auto const& kTreasuryID = getTreasuryAccountID();

    auto sleTreasury = ctx_.view().peek(keylet::account(kTreasuryID));
    if (!sleTreasury)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const treasuryBalance = sleTreasury->getFieldAmount(sfBalance);
    if (treasuryBalance < STAmount{XRPAmount{shareDrops}})
        return tecUNFUNDED;

    sleTreasury->setFieldAmount(sfBalance, treasuryBalance - STAmount{XRPAmount{shareDrops}});
    ctx_.view().update(sleTreasury);

    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const valBalance = sleAccount->getFieldAmount(sfBalance);
    sleAccount->setFieldAmount(sfBalance, valBalance + STAmount{XRPAmount{shareDrops}});
    ctx_.view().update(sleAccount);

    // ── Deduct from epoch pool ────────────────────────────────────────────
    sleEpoch->setFieldAmount(
        sfEpochPoolBalance,
        poolBalance - STAmount{XRPAmount{shareDrops}});
    sleEpoch->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleEpoch->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleEpoch);

    // ── Mark epoch claimed on bond ────────────────────────────────────────
    sleBond->setFieldU32(sfLastClaimedEpoch, currentEpoch);
    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleBond);

    return tesSUCCESS;
}

void
ClaimReward::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ClaimReward::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
