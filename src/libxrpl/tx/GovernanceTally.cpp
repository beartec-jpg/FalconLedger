// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/GovernanceTally.h>

#include <xrpl/basics/Log.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/Rules.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>

#include <cstdint>
#include <vector>

namespace xrpl {

void
applyGovernanceTally(
    OpenView& view,
    LedgerIndex seq,
    Rules const& rules,
    beast::Journal j,
    std::vector<uint256> const& openProposalKeys)
{
    if (!rules.enabled(featureProofOfParticipation))
        return;
    if (openProposalKeys.empty())
        return;

    // Read the aggregate composite score to use as the denominator.
    std::uint32_t aggregateScore = 0;
    if (auto sleEpoch = view.read(keylet::rewardEpoch()))
        aggregateScore = sleEpoch->getFieldU32(sfAggregateCompositeScore);

    for (auto const& key : openProposalKeys)
    {
        auto sleProposal = view.peek(keylet::governanceProposal(key));
        if (!sleProposal)
            continue;

        // Only tally proposals that are open AND have expired.
        if (sleProposal->getFieldU32(sfProposalState) != 0)
            continue;
        if (seq < sleProposal->getFieldU32(sfProposalExpiry))
            continue;

        auto const votedFor = sleProposal->getFieldU32(sfVotedFor);

        // passed = votedFor * kBPS_DENOM >= aggregateScore * kGOVERNANCE_SUPERMAJORITY_BPS
        bool const passed =
            aggregateScore > 0 &&
            (static_cast<__int128>(votedFor) * kBPS_DENOM >=
             static_cast<__int128>(aggregateScore) * kGOVERNANCE_SUPERMAJORITY_BPS);

        JLOG(j.info()) << "qXRP GovernanceTally: proposal=" << key
                       << " votedFor=" << votedFor
                       << " aggregate=" << aggregateScore
                       << " outcome=" << (passed ? "PASSED" : "REJECTED");

        if (passed)
        {
            auto const proposalType  = sleProposal->getFieldU32(sfProposalType);
            auto const proposalValue = sleProposal->getFieldU32(sfProposalValue);

            auto sleParams = view.peek(keylet::governanceParams());
            if (!sleParams)
            {
                sleParams = std::make_shared<SLE>(keylet::governanceParams());
                sleParams->setFieldH256(sfPreviousTxnID,    key);
                sleParams->setFieldU32(sfPreviousTxnLgrSeq, seq);
                view.insert(sleParams);
            }

            if (proposalType == kPROPOSAL_TYPE_BURN_BPS)
            {
                sleParams->setFieldU32(sfCurrentBurnBps, proposalValue);
                sleParams->setFieldH256(sfPreviousTxnID,    key);
                sleParams->setFieldU32(sfPreviousTxnLgrSeq, seq);
                view.update(sleParams);

                JLOG(j.info()) << "qXRP GovernanceTally: wrote sfCurrentBurnBps="
                               << proposalValue;
            }
        }

        // Mark proposal as resolved.
        sleProposal->setFieldU32(sfProposalState, passed ? 1 : 2);
        sleProposal->setFieldH256(sfPreviousTxnID,    key);
        sleProposal->setFieldU32(sfPreviousTxnLgrSeq, seq);
        view.update(sleProposal);
    }
}

}  // namespace xrpl
