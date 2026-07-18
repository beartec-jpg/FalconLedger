// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ClaimAmmLpReward.h>

#include <xrpl/basics/WideArith.h>
#include <xrpl/ledger/helpers/AMMHelpers.h>
#include <xrpl/protocol/AMMCore.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

#include <algorithm>
#include <cstdint>

namespace xrpl {
namespace {

/** Snapshot TVL weight for a native AMM: XRP leg on the AMM pseudo-account. */
std::uint64_t
nativeAmmTvlDrops(ReadView const& view, SLE const& ammSle)
{
    auto const ammAccount = ammSle.at(sfAccount);
    if (auto const sleAmmAcct = view.read(keylet::account(ammAccount)))
    {
        auto const drops = sleAmmAcct->getFieldAmount(sfBalance).xrp().drops();
        if (drops > 0)
            return static_cast<std::uint64_t>(drops);
    }
    // Fallback: LP supply mantissa as relative weight (never zero when pool exists).
    auto const lpBal = ammSle.getFieldAmount(sfLPTokenBalance);
    auto const m = lpBal.mantissa();
    return m > 0 ? static_cast<std::uint64_t>(m) : 1;
}

std::uint64_t
positiveMantissa(STAmount const& amt)
{
    if (amt <= beast::kZERO)
        return 0;
    auto const m = amt.mantissa();
    return m > 0 ? static_cast<std::uint64_t>(m) : 0;
}

}  // namespace

NotTEC
ClaimAmmLpReward::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    if (!ctx.tx.isFieldPresent(sfAsset) || !ctx.tx.isFieldPresent(sfAsset2))
        return temMALFORMED;

    // Same asset pair rules as other AMM txs (order-independent keylet).
    if (ctx.tx[sfAsset] == ctx.tx[sfAsset2])
        return temBAD_AMM_TOKENS;

    return tesSUCCESS;
}

TER
ClaimAmmLpReward::preclaim(PreclaimContext const& ctx)
{
    auto const asset = ctx.tx[sfAsset];
    auto const asset2 = ctx.tx[sfAsset2];

    // Emissions only for native FALCON pairs (one side is XRP/FALCON).
    if (!asset.native() && !asset2.native())
        return tecNO_PERMISSION;

    auto const ammSle = ctx.view.read(keylet::amm(asset, asset2));
    if (!ammSle)
        return tecNO_ENTRY;

    auto const sleEpoch = ctx.view.read(keylet::rewardEpoch());
    if (!sleEpoch)
        return tecNO_ENTRY;

    auto const ammAllocBps = sleEpoch->isFieldPresent(sfAmmLPAllocationBps)
        ? sleEpoch->getFieldU32(sfAmmLPAllocationBps)
        : 0;
    if (ammAllocBps == 0)
        return tecNO_PERMISSION;

    auto const aggregateAmmTvl = sleEpoch->isFieldPresent(sfAggregateAmmTvlDrops)
        ? sleEpoch->getFieldU64(sfAggregateAmmTvlDrops)
        : 0;
    if (aggregateAmmTvl == 0)
        return tecNO_PERMISSION;

    auto const lpTokens = ammLPHolds(ctx.view, *ammSle, ctx.tx[sfAccount], ctx.j);
    if (lpTokens <= beast::kZERO)
        return tecNO_PERMISSION;

    auto const totalLp = ammSle->getFieldAmount(sfLPTokenBalance);
    if (totalLp <= beast::kZERO)
        return tecNO_PERMISSION;

    auto const currentEpoch = sleEpoch->getFieldU32(sfEpochNumber);
    auto const ammKey = keylet::amm(asset, asset2).key;
    auto const stateKey = keylet::popLpState(ctx.tx[sfAccount], ammKey);
    if (auto const sleState = ctx.view.read(stateKey))
    {
        if (sleState->getFieldU32(sfLastClaimedEpoch) >= currentEpoch)
            return tecDUPLICATE;
    }

    return tesSUCCESS;
}

TER
ClaimAmmLpReward::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const asset = ctx_.tx[sfAsset];
    auto const asset2 = ctx_.tx[sfAsset2];

    auto const ammSle = ctx_.view().read(keylet::amm(asset, asset2));
    if (!ammSle)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto sleEpoch = ctx_.view().peek(keylet::rewardEpoch());
    if (!sleEpoch)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const currentEpoch = sleEpoch->getFieldU32(sfEpochNumber);
    auto const ammAllocBps = sleEpoch->isFieldPresent(sfAmmLPAllocationBps)
        ? sleEpoch->getFieldU32(sfAmmLPAllocationBps)
        : 0;
    auto const aggregateAmmTvl = sleEpoch->isFieldPresent(sfAggregateAmmTvlDrops)
        ? sleEpoch->getFieldU64(sfAggregateAmmTvlDrops)
        : 0;
    auto const poolBalance = sleEpoch->getFieldAmount(sfEpochPoolBalance);
    auto const emissionRate = sleEpoch->getFieldAmount(sfEmissionRate);

    if (ammAllocBps == 0 || aggregateAmmTvl == 0)
        return tecNO_PERMISSION;

    auto const lpTokens = ammLPHolds(ctx_.view(), *ammSle, account, ctx_.journal);
    auto const totalLp = ammSle->getFieldAmount(sfLPTokenBalance);
    auto const userMant = positiveMantissa(lpTokens);
    auto const totalMant = positiveMantissa(totalLp);
    if (userMant == 0 || totalMant == 0)
        return tecNO_PERMISSION;

    auto const poolTvl = nativeAmmTvlDrops(ctx_.view(), *ammSle);

    auto const emissionDrops = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, emissionRate.xrp().drops()));
    // ammBasket = emission * ammAllocBps / 10000
    auto const ammBasket = muldivU64(emissionDrops, ammAllocBps, kBPS_DENOM);
    // poolBasket = ammBasket * poolTvl / aggregateTvl
    auto const poolBasket = muldivU64(ammBasket, poolTvl, aggregateAmmTvl);
    // share = poolBasket * userLp / totalLp
    auto const shareDrops = muldivU64(poolBasket, userMant, totalMant);

    if (shareDrops == 0)
        return tesSUCCESS;

    auto const& kTreasuryID = getTreasuryAccountID();

    auto sleTreasury = ctx_.view().peek(keylet::account(kTreasuryID));
    if (!sleTreasury)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const treasuryBalance = sleTreasury->getFieldAmount(sfBalance);
    auto const pay = STAmount{XRPAmount{static_cast<std::int64_t>(shareDrops)}};
    if (treasuryBalance < pay)
        return tecUNFUNDED;

    sleTreasury->setFieldAmount(sfBalance, treasuryBalance - pay);
    ctx_.view().update(sleTreasury);

    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const acctBalance = sleAccount->getFieldAmount(sfBalance);
    sleAccount->setFieldAmount(sfBalance, acctBalance + pay);
    ctx_.view().update(sleAccount);

    sleEpoch->setFieldAmount(sfEpochPoolBalance, poolBalance - pay);
    sleEpoch->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleEpoch->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleEpoch);

    // Reuse PopLpState keyed by (account, AMMID) — sfVaultID stores the AMM key.
    auto const ammKey = keylet::amm(asset, asset2).key;
    auto const stateKey = keylet::popLpState(account, ammKey);
    if (auto sleState = ctx_.view().peek(stateKey))
    {
        sleState->setFieldU32(sfLastClaimedEpoch, currentEpoch);
        sleState->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
        sleState->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
        ctx_.view().update(sleState);
    }
    else
    {
        auto sleNew = std::make_shared<SLE>(stateKey);
        sleNew->setAccountID(sfAccount, account);
        sleNew->setFieldH256(sfVaultID, ammKey);
        sleNew->setFieldU32(sfLastClaimedEpoch, currentEpoch);
        sleNew->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
        sleNew->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
        ctx_.view().insert(sleNew);
    }

    return tesSUCCESS;
}

void
ClaimAmmLpReward::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ClaimAmmLpReward::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
