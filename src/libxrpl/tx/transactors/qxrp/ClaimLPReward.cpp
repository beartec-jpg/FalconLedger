// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ClaimLPReward.h>

#include <xrpl/basics/WideArith.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

#include <algorithm>
#include <cstdint>

namespace xrpl {

NotTEC
ClaimLPReward::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    if (!ctx.tx.isFieldPresent(sfVaultID))
        return temMALFORMED;

    return tesSUCCESS;
}

TER
ClaimLPReward::preclaim(PreclaimContext const& ctx)
{
    auto const vaultKey = ctx.tx[sfVaultID];
    auto const sleVault = ctx.view.read(keylet::vault(vaultKey));
    if (!sleVault)
        return tecNO_ENTRY;

    auto const sleEpoch = ctx.view.read(keylet::rewardEpoch());
    if (!sleEpoch)
        return tecNO_ENTRY;

    auto const lpAllocBps = sleEpoch->getFieldU32(sfLPAllocationBps);
    if (lpAllocBps == 0)
        return tecNO_PERMISSION;

    auto const aggregateLPShares = sleEpoch->getFieldU64(sfAggregateLPShares);
    if (aggregateLPShares == 0)
        return tecNO_PERMISSION;

    auto const shareMptID = sleVault->at(sfShareMPTID);
    auto const sleMpt = ctx.view.read(keylet::mptoken(shareMptID, ctx.tx[sfAccount]));
    if (!sleMpt || sleMpt->getFieldU64(sfMPTAmount) == 0)
        return tecNO_PERMISSION;

    auto const currentEpoch = sleEpoch->getFieldU32(sfEpochNumber);
    auto const stateKey = keylet::popLpState(ctx.tx[sfAccount], vaultKey);
    if (auto const sleState = ctx.view.read(stateKey))
    {
        if (sleState->getFieldU32(sfLastClaimedEpoch) >= currentEpoch)
            return tecDUPLICATE;
    }

    return tesSUCCESS;
}

TER
ClaimLPReward::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const vaultKey = ctx_.tx[sfVaultID];

    auto sleVault = ctx_.view().read(keylet::vault(vaultKey));
    if (!sleVault)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto sleEpoch = ctx_.view().peek(keylet::rewardEpoch());
    if (!sleEpoch)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const currentEpoch = sleEpoch->getFieldU32(sfEpochNumber);
    auto const lpAllocBps = sleEpoch->getFieldU32(sfLPAllocationBps);
    auto const aggregateLPShares = sleEpoch->getFieldU64(sfAggregateLPShares);
    auto const poolBalance = sleEpoch->getFieldAmount(sfEpochPoolBalance);
    auto const emissionRate = sleEpoch->getFieldAmount(sfEmissionRate);

    if (lpAllocBps == 0 || aggregateLPShares == 0)
        return tecNO_PERMISSION;

    auto const shareMptID = sleVault->at(sfShareMPTID);
    auto sleMpt = ctx_.view().read(keylet::mptoken(shareMptID, account));
    if (!sleMpt)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const userShares = sleMpt->getFieldU64(sfMPTAmount);
    if (userShares == 0)
        return tecNO_PERMISSION;

    auto const emissionDrops = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, emissionRate.xrp().drops()));
    auto const lpPoolDrops = muldivU64(emissionDrops, lpAllocBps, kBPS_DENOM);
    auto const shareDrops = muldivU64(lpPoolDrops, userShares, aggregateLPShares);

    if (shareDrops == 0)
        return tesSUCCESS;

    auto const& kTreasuryID = getTreasuryAccountID();

    auto sleTreasury = ctx_.view().peek(keylet::account(kTreasuryID));
    if (!sleTreasury)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const pay = STAmount{XRPAmount{static_cast<std::int64_t>(shareDrops)}};
    auto const treasuryBalance = sleTreasury->getFieldAmount(sfBalance);
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

    auto const stateKey = keylet::popLpState(account, vaultKey);
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
        sleNew->setFieldH256(sfVaultID, vaultKey);
        sleNew->setFieldU32(sfLastClaimedEpoch, currentEpoch);
        sleNew->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
        sleNew->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
        ctx_.view().insert(sleNew);
    }

    return tesSUCCESS;
}

void
ClaimLPReward::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ClaimLPReward::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl