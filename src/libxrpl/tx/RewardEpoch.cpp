// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/RewardEpoch.h>
#include <xrpl/tx/PoPLEmission.h>

#include <xrpl/basics/Log.h>
#include <xrpl/basics/WideArith.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/Rules.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace xrpl {

namespace {

std::uint64_t
aggregateVaultShareSupply(ReadView const& view)
{
    std::uint64_t total = 0;
    for (auto const& sle : view.sles)
    {
        if (!sle || sle->getType() != ltVAULT)
            continue;

        auto const shareMptID = sle->at(sfShareMPTID);
        if (auto const sleIssuance = view.read(keylet::mptIssuance(shareMptID)))
            total += sleIssuance->getFieldU64(sfOutstandingAmount);
    }
    return total;
}

std::uint32_t
countActiveLpProviders(ReadView const& view)
{
    std::unordered_set<MPTID> shareMptIDs;
    for (auto const& sle : view.sles)
    {
        if (!sle || sle->getType() != ltVAULT)
            continue;

        shareMptIDs.insert(sle->at(sfShareMPTID));
    }

    if (shareMptIDs.empty())
        return 0;

    std::unordered_set<AccountID> providers;
    for (auto const& sle : view.sles)
    {
        if (!sle || sle->getType() != ltMPTOKEN)
            continue;

        if (!shareMptIDs.count(sle->at(sfMPTokenIssuanceID)))
            continue;

        if (sle->getFieldU64(sfMPTAmount) == 0)
            continue;

        providers.insert(sle->getAccountID(sfAccount));
    }

    return static_cast<std::uint32_t>(providers.size());
}

}  // namespace

void
applyRewardEpoch(
    OpenView& view,
    LedgerIndex seq,
    Rules const& rules,
    beast::Journal j)
{
    // ── Gate on amendment and epoch boundary ─────────────────────────────
    if (!rules.enabled(featureProofOfParticipation))
        return;
    if (seq == 0 || seq % kQXRP_LEDGERS_PER_EPOCH != 0)
        return;

    // ── Epoch number (1-based: epoch 1 closes at seq == kQXRP_LEDGERS_PER_EPOCH) ──
    std::uint32_t const epochNum = seq / kQXRP_LEDGERS_PER_EPOCH;

    JLOG(j.info()) << "applyRewardEpoch: epoch " << epochNum
                   << " boundary at ledger " << seq;

    // ── Treasury balance ──────────────────────────────────────────────────
    // Computed once; used for both pool sizing and burn-pressure calculation.
    auto const& kTreasuryID = getTreasuryAccountID();

    std::int64_t treasuryDrops = 0;
    if (auto const sleTreasury = view.read(keylet::account(kTreasuryID)))
        treasuryDrops = sleTreasury->getFieldAmount(sfBalance).xrp().drops();

    if (treasuryDrops <= 0)
    {
        // Treasury is exhausted — nothing to emit; skip this epoch.
        JLOG(j.warn()) << "applyRewardEpoch: treasury empty at epoch " << epochNum;
        return;
    }

    // ── CID: yearly-average budget with per-epoch micro-decline ───────────
    // Bootstrap quiet period: no claimable pool until kQXRP_FIRST_EMISSION_EPOCH.
    std::uint32_t const emissionBps = (epochNum < kQXRP_FIRST_EMISSION_EPOCH)
        ? 0
        : cidEmissionBps(epochNum);
    std::uint32_t const lpProviderCount = countActiveLpProviders(view);
    std::uint32_t const lpAllocBps = poplLpParticipationBps(lpProviderCount);
    std::uint64_t const aggregateLPShares = aggregateVaultShareSupply(view);

    // ── Epoch pool balance ────────────────────────────────────────────────
    // The pool is a *commitment* from the treasury for this epoch window.
    // Drops are not physically moved here; ClaimReward draws from the treasury
    // account directly and checks total claimed vs. sfEpochPoolBalance.
    //
    // muldiv64(a, b, d): no overflow because emissionBps <= kBPS_DENOM.
    auto const poolDrops = emissionBps == 0
        ? std::uint64_t{0}
        : muldiv64(treasuryDrops, emissionBps, kBPS_DENOM);

    // ── Dynamic burn BPS from treasury fill pressure ──────────────────────
    // fillBps: fraction of the initial allocation still in the treasury.
    //   fillBps == kBPS_DENOM  → treasury is 100 % full (genesis)
    //   fillBps == 0           → treasury fully drained
    //
    // Overflow-free: since kQXRP_TREASURY_ALLOCATION is statically verified
    // to be exactly divisible by kBPS_DENOM, we compute:
    //   fillBps = treasuryDrops / (TREASURY / kBPS_DENOM)
    // Both operands fit in int64_t and the quotient is in [0, kBPS_DENOM].
    static constexpr std::int64_t kTreasuryPerBps =
        kQXRP_TREASURY_ALLOCATION.drops() / 10'000;  // == kBPS_DENOM
    auto const fillBps = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(treasuryDrops) / static_cast<std::uint64_t>(kTreasuryPerBps));

    auto const burnBps = std::clamp(
        kFEE_BURN_DEFAULT_BPS
            + static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(fillBps) * kFEE_TREASURY_SENSITIVITY_BPS)
                / kBPS_DENOM),
        kFEE_BURN_MIN_BPS,
        kFEE_BURN_MAX_BPS);

    // ── Build the new ltREWARD_EPOCH object ───────────────────────────────
    // sfFeeVolumeEMA is carried forward from the previous epoch as a
    // continuity anchor; aggregate score is re-zeroed each epoch (validators
    // must have an active score write-back before claiming).
    std::uint32_t prevFeeVolumeEMA = 0;
    bool epochExists = false;
    if (auto const slePrev = view.read(keylet::rewardEpoch()))
    {
        epochExists = true;
        prevFeeVolumeEMA = slePrev->getFieldU32(sfFeeVolumeEMA);
    }

    auto const k = keylet::rewardEpoch();
    auto sleEpoch = std::make_shared<SLE>(k);

    sleEpoch->setFieldU32(sfEpochNumber, epochNum);
    sleEpoch->setFieldU32(sfEpochStartLedger, seq);
    sleEpoch->setFieldAmount(
        sfEpochPoolBalance, STAmount{XRPAmount{poolDrops}});
    // sfEmissionRate records the original scheduled emission for this epoch.
    // (sfEpochPoolBalance shrinks as validators claim; this stays fixed.)
    sleEpoch->setFieldAmount(
        sfEmissionRate, STAmount{XRPAmount{poolDrops}});
    sleEpoch->setFieldU32(sfLPAllocationBps, lpAllocBps);
    if (aggregateLPShares != 0)
        sleEpoch->setFieldU64(sfAggregateLPShares, aggregateLPShares);
    sleEpoch->setFieldU32(sfCurrentBurnBps, burnBps);
    // sfFeeVolumeEMA and sfAggregateCompositeScore are SoeDefault (default=0).
    // Do NOT explicitly set them to 0 — applyTemplate will reject it.
    if (prevFeeVolumeEMA != 0)
        sleEpoch->setFieldU32(sfFeeVolumeEMA, prevFeeVolumeEMA);
    // sfAggregateCompositeScore intentionally left unset (defaults to 0).
    // Synthetic protocol operation — no driving transaction.
    sleEpoch->setFieldH256(sfPreviousTxnID, uint256{});
    sleEpoch->setFieldU32(sfPreviousTxnLgrSeq, seq);

    if (epochExists)
        view.rawReplace(sleEpoch);
    else
        view.rawInsert(sleEpoch);

    JLOG(j.info()) << "applyRewardEpoch: epoch=" << epochNum
                   << " pool=" << poolDrops << " drops"
                   << " emissionBps=" << emissionBps
                   << " lpProviders=" << lpProviderCount
                   << " lpAllocBps=" << lpAllocBps
                   << " aggregateLPShares=" << aggregateLPShares
                   << " burnBps=" << burnBps;
}

}  // namespace xrpl
