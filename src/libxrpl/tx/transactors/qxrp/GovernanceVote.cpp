// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/GovernanceVote.h>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

#include <algorithm>
#include <cstring>

namespace xrpl {

// ── helpers ─────────────────────────────────────────────────────────────────

static constexpr std::size_t kACCOUNT_ID_BYTES = AccountID::kBYTES;  // 20

/// Check whether `voter` appears in the packed blob of 20-byte AccountIDs.
static bool
hasVoted(Blob const& voterList, AccountID const& voter)
{
    if (voterList.size() % kACCOUNT_ID_BYTES != 0)
        return false;  // corrupted; treat as empty
    for (std::size_t i = 0; i + kACCOUNT_ID_BYTES <= voterList.size();
         i += kACCOUNT_ID_BYTES)
    {
        if (std::memcmp(voterList.data() + i, voter.data(), kACCOUNT_ID_BYTES) == 0)
            return true;
    }
    return false;
}

/// Append a single AccountID to the end of the packed blob.
static Blob
appendVoter(Blob voterList, AccountID const& voter)
{
    voterList.insert(voterList.end(), voter.begin(), voter.end());
    return voterList;
}

// ── Transactor ───────────────────────────────────────────────────────────────

NotTEC
GovernanceVote::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    // sfVoteWeight reused: 1 = yes, 0 = no; any other value is malformed.
    auto const vote = ctx.tx.getFieldU32(sfVoteWeight);
    if (vote > 1)
        return temMALFORMED;

    return tesSUCCESS;
}

TER
GovernanceVote::preclaim(PreclaimContext const& ctx)
{
    auto const voter = ctx.tx[sfAccount];

    // Must be a bonded validator.
    auto const sleBond = ctx.view.read(keylet::validatorBond(voter));
    if (!sleBond)
        return tecNO_ENTRY;
    if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
        return tecNO_PERMISSION;

    // Proposal must exist and be open (state == 0).
    auto const proposalID = ctx.tx.getFieldH256(sfProposalID);
    auto const sleProposal = ctx.view.read(keylet::governanceProposal(proposalID));
    if (!sleProposal)
        return tecNO_ENTRY;
    if (sleProposal->getFieldU32(sfProposalState) != 0)
        return tecEXPIRED;

    // Proposal must not yet have expired (guard: if epoch close advanced it already).
    if (ctx.view.seq() >= sleProposal->getFieldU32(sfProposalExpiry))
        return tecEXPIRED;

    // Duplicate vote check.
    auto const voterList = sleProposal->getFieldVL(sfVoterList);
    if (hasVoted(voterList, voter))
        return tecDUPLICATE;

    return tesSUCCESS;
}

TER
GovernanceVote::doApply()
{
    auto const voter       = ctx_.tx[sfAccount];
    auto const proposalID  = ctx_.tx.getFieldH256(sfProposalID);
    auto const voteYes     = ctx_.tx.getFieldU32(sfVoteWeight) == 1;

    auto sleProposal = ctx_.view().peek(keylet::governanceProposal(proposalID));
    if (!sleProposal)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // Weight = voter's current composite score.
    auto const sleBond = ctx_.view().read(keylet::validatorBond(voter));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE
    auto const weight = sleBond->getFieldU32(sfCompositeScore);

    // Update tally.
    if (voteYes)
    {
        auto const prev = sleProposal->getFieldU32(sfVotedFor);
        sleProposal->setFieldU32(
            sfVotedFor,
            static_cast<std::uint32_t>(
                std::min(static_cast<std::uint64_t>(prev) + weight,
                         static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))));
    }
    else
    {
        auto const prev = sleProposal->getFieldU32(sfVotedAgainst);
        sleProposal->setFieldU32(
            sfVotedAgainst,
            static_cast<std::uint32_t>(
                std::min(static_cast<std::uint64_t>(prev) + weight,
                         static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))));
    }

    // Record voter to prevent double-voting.
    auto voterList = sleProposal->getFieldVL(sfVoterList);
    voterList = appendVoter(std::move(voterList), voter);
    sleProposal->setFieldVL(sfVoterList, voterList);

    sleProposal->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleProposal->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleProposal);

    return tesSUCCESS;
}

}  // namespace xrpl
