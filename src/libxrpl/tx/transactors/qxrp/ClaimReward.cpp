// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ClaimReward.h>

#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

namespace xrpl {

NotTEC
ClaimReward::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    return tesSUCCESS;
}

TER
ClaimReward::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];

    // Must have a registered + bonded bond object.
    auto sleBond = ctx.view.read(keylet::validatorBond(account));
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

    auto sleBond = ctx_.view().peek(keylet::validatorBond(account));
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
    // share = poolBalance * compositeScore / aggregateCompositeScore
    // All arithmetic is integer; no floating point.
    auto const compositeScore     = sleBond->getFieldU32(sfCompositeScore);
    auto const aggregateScore     = sleEpoch->getFieldU32(sfAggregateCompositeScore);
    auto const poolBalance        = sleEpoch->getFieldAmount(sfEpochPoolBalance);

    if (aggregateScore == 0)
        return tecNO_PERMISSION;  // no eligible validators — shouldn't reach here

    // Use 128-bit-safe multiply: drops * bps / bps stays within int64 range
    // because poolBalance <= 200B * 1e6 drops ~ 2e17, compositeScore <= 1e4,
    // ratio <= 1, so the product before division <= 2e21 — needs care.
    // We compute: share = (poolBalance.drops() * compositeScore) / aggregateScore
    // To avoid overflow, cap: poolBalance.drops() fits in int64 (~2e17),
    // compositeScore <= 10000, product <= 2e21 which overflows int64.
    // Use __int128 or sequential division to stay safe:
    auto const poolDrops   = poolBalance.xrp().drops();
    auto const shareDrops  = static_cast<std::int64_t>(
        (static_cast<__int128>(poolDrops) * compositeScore) / aggregateScore);

    if (shareDrops <= 0)
        return tesSUCCESS;  // rounding to zero — no reward to distribute

    // ── Transfer: treasury → validator ───────────────────────────────────
    // Read treasury account using the deterministic treasury seed.
    static auto const kTreasuryID =
        calcAccountID(generateKeyPair(KeyType::Secp256k1, generateSeed(kQXRP_TREASURY_SEED)).first);

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

}  // namespace xrpl
