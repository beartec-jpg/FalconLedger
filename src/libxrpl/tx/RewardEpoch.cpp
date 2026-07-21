// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/RewardEpoch.h>
#include <xrpl/tx/PoPLEmission.h>

#include <xrpl/basics/Log.h>
#include <xrpl/basics/WideArith.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/AMMCore.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
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
#include <vector>

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

/** Native (FALCON/XRP) AMMs only — asset1 or asset2 is XRP. */
struct NativeAmmInfo
{
    AccountID ammAccount;
    Currency lpCurrency;
    std::int64_t xrpDrops{0};
};

std::vector<NativeAmmInfo>
listNativeAmms(ReadView const& view)
{
    std::vector<NativeAmmInfo> out;
    for (auto const& sle : view.sles)
    {
        if (!sle || sle->getType() != ltAMM)
            continue;

        Asset const a1 = sle->at(sfAsset);
        Asset const a2 = sle->at(sfAsset2);
        bool const a1Xrp = a1.native();
        bool const a2Xrp = a2.native();
        if (!a1Xrp && !a2Xrp)
            continue;

        auto const ammAccount = sle->at(sfAccount);
        auto const lpCur = ammLPTCurrency(a1, a2);
        std::int64_t xrpDrops = 0;
        // Reserves: amount fields on AMM vary by amendment; use LP balance as fallback weight.
        auto const lpBal = sle->getFieldAmount(sfLPTokenBalance);
        // Prefer XRP amount from amount/amount2 if present via balances on AMM account.
        // Weight by XRP held on AMM pseudo-account when available.
        if (auto const sleAmmAcct = view.read(keylet::account(ammAccount)))
            xrpDrops = sleAmmAcct->getFieldAmount(sfBalance).xrp().drops();
        if (xrpDrops <= 0)
        {
            // Fallback: use mantissa of LP supply as relative weight.
            xrpDrops = std::max<std::int64_t>(1, static_cast<std::int64_t>(lpBal.mantissa()));
        }
        out.push_back(NativeAmmInfo{ammAccount, lpCur, xrpDrops});
    }
    return out;
}

std::pair<std::uint32_t, std::uint64_t>
countAmmLpProvidersAndTvl(ReadView const& view)
{
    auto const amms = listNativeAmms(view);
    if (amms.empty())
        return {0, 0};

    std::unordered_set<AccountID> providers;
    std::uint64_t tvl = 0;
    for (auto const& a : amms)
        tvl += static_cast<std::uint64_t>(std::max<std::int64_t>(0, a.xrpDrops));

    for (auto const& sle : view.sles)
    {
        if (!sle || sle->getType() != ltRIPPLE_STATE)
            continue;

        // Trust line: LP tokens use the AMM account as issuer.
        auto const bal = sle->getFieldAmount(sfBalance);
        if (bal.native() || bal == beast::kZERO)
            continue;

        auto const& issue = bal.get<Issue>();
        for (auto const& a : amms)
        {
            if (issue.currency != a.lpCurrency)
                continue;
            if (issue.account != a.ammAccount)
                continue;

            AccountID const lo = sle->getFieldAmount(sfLowLimit).getIssuer();
            AccountID const hi = sle->getFieldAmount(sfHighLimit).getIssuer();
            AccountID const holder = (lo == a.ammAccount) ? hi : lo;
            if (holder == a.ammAccount || holder == beast::kZERO)
                continue;
            providers.insert(holder);
            break;
        }
    }

    return {static_cast<std::uint32_t>(providers.size()), tvl};
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
    auto const [ammProviderCount, aggregateAmmTvl] = countAmmLpProvidersAndTvl(view);
    std::uint32_t const ammAllocBps = poplAmmLpParticipationBps(ammProviderCount);

    // ── Epoch pool balance ────────────────────────────────────────────────
    // The pool is a *commitment* from the treasury for this epoch window.
    // Drops are not physically moved here; claim txs draw from the treasury
    // and hard-cap each payout to remaining sfEpochPoolBalance (C-02).
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
    if (ammAllocBps != 0)
        sleEpoch->setFieldU32(sfAmmLPAllocationBps, ammAllocBps);
    if (aggregateAmmTvl != 0)
        sleEpoch->setFieldU64(sfAggregateAmmTvlDrops, aggregateAmmTvl);
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
                   << " vaultLpProviders=" << lpProviderCount
                   << " vaultLpAllocBps=" << lpAllocBps
                   << " ammLpProviders=" << ammProviderCount
                   << " ammLpAllocBps=" << ammAllocBps
                   << " aggregateLPShares=" << aggregateLPShares
                   << " aggregateAmmTvlDrops=" << aggregateAmmTvl
                   << " burnBps=" << burnBps;
}

}  // namespace xrpl
