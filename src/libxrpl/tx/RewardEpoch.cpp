// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/RewardEpoch.h>

#include <xrpl/basics/Log.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/Rules.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>

#include <algorithm>
#include <cstdint>

namespace xrpl {

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
    static auto const kTreasuryID = calcAccountID(
        generateKeyPair(KeyType::Secp256k1, generateSeed(kQXRP_TREASURY_SEED)).first);

    std::int64_t treasuryDrops = 0;
    if (auto const sleTreasury = view.read(keylet::account(kTreasuryID)))
        treasuryDrops = sleTreasury->getFieldAmount(sfBalance).xrp().drops();

    if (treasuryDrops <= 0)
    {
        // Treasury is exhausted — nothing to emit; skip this epoch.
        JLOG(j.warn()) << "applyRewardEpoch: treasury empty at epoch " << epochNum;
        return;
    }

    // ── Emission rate (halved every kQXRP_EPOCHS_PER_HALVING epochs) ─────
    // halvings uses (epochNum - 1) so that epoch 1 uses the initial rate.
    std::uint32_t const halvings =
        static_cast<std::uint32_t>((epochNum - 1) / kQXRP_EPOCHS_PER_HALVING);

    // Guard against shift UB: after 31 halvings the rate is effectively 0.
    std::uint32_t emissionBps;
    if (halvings >= 31u)
        emissionBps = kQXRP_MIN_EMISSION_BPS;
    else
        emissionBps = std::max(
            static_cast<std::uint32_t>(kQXRP_INITIAL_EMISSION_BPS >> halvings),
            kQXRP_MIN_EMISSION_BPS);

    // ── Epoch pool balance ────────────────────────────────────────────────
    // The pool is a *commitment* from the treasury for this epoch window.
    // Drops are not physically moved here; ClaimReward draws from the treasury
    // account directly and checks total claimed vs. sfEpochPoolBalance.
    auto const poolDrops = static_cast<std::int64_t>(
        (static_cast<__int128>(treasuryDrops) * emissionBps) / kBPS_DENOM);

    // ── Dynamic burn BPS from treasury fill pressure ──────────────────────
    // fillBps: fraction of the initial allocation still in the treasury.
    //   fillBps == kBPS_DENOM  → treasury is 100 % full (genesis)
    //   fillBps == 0           → treasury fully drained
    //
    // Higher fill → higher burn fraction so deflation keeps pace with rewards.
    auto const fillBps = static_cast<std::uint32_t>(
        (static_cast<__int128>(treasuryDrops) * kBPS_DENOM)
        / kQXRP_TREASURY_ALLOCATION.drops());

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
    sleEpoch->setFieldU32(sfCurrentBurnBps, burnBps);
    sleEpoch->setFieldU32(sfFeeVolumeEMA, prevFeeVolumeEMA);
    sleEpoch->setFieldU32(sfAggregateCompositeScore, 0);
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
                   << " burnBps=" << burnBps
                   << " halvings=" << halvings;
}

}  // namespace xrpl
