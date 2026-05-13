// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/GovernanceProposal.h>

#include <xrpl/basics/Blob.h>
#include <xrpl/ledger/helpers/AccountRootHelpers.h>
#include <xrpl/ledger/helpers/DirectoryHelpers.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

namespace xrpl {

NotTEC
GovernanceProposal::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    auto const proposalType = ctx.tx.getFieldU32(sfProposalType);
    if (proposalType != kPROPOSAL_TYPE_BURN_BPS)
        return temMALFORMED;

    // Validate the proposed value is within hard limits for the burnBps param.
    auto const value = ctx.tx.getFieldU32(sfProposalValue);
    if (value < kFEE_BURN_MIN_BPS || value > kFEE_BURN_MAX_BPS)
        return temBAD_AMOUNT;

    return tesSUCCESS;
}

TER
GovernanceProposal::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];

    // Only bonded validators may propose.
    auto const sleBond = ctx.view.read(keylet::validatorBond(account));
    if (!sleBond)
        return tecNO_ENTRY;
    if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
        return tecNO_PERMISSION;

    return tesSUCCESS;
}

TER
GovernanceProposal::doApply()
{
    auto const account    = ctx_.tx[sfAccount];
    auto const seq        = view().seq();

    auto const proposalKeylet = keylet::governanceProposal(account, ctx_.tx[sfSequence]);

    auto sleProposal = std::make_shared<SLE>(proposalKeylet);

    sleProposal->setAccountID(sfAccount,       account);
    sleProposal->setFieldU32(sfProposalType,   ctx_.tx.getFieldU32(sfProposalType));
    sleProposal->setFieldU32(sfProposalValue,  ctx_.tx.getFieldU32(sfProposalValue));
    sleProposal->setFieldU32(sfProposalExpiry, seq + kGOVERNANCE_VOTING_LEDGERS);
    sleProposal->setFieldU32(sfProposalState,  0);  // open
    sleProposal->setFieldU32(sfVotedFor,       0);
    sleProposal->setFieldU32(sfVotedAgainst,   0);
    sleProposal->setFieldVL(sfVoterList,       Blob{});
    sleProposal->setFieldH256(sfPreviousTxnID,      ctx_.tx.getTransactionID());
    sleProposal->setFieldU32(sfPreviousTxnLgrSeq,   seq);

    // Add to proposer's owner directory for reserve accounting.
    auto const page = ctx_.view().dirInsert(
        keylet::ownerDir(account),
        sleProposal->key(),
        describeOwnerDir(account));
    if (!page)
        return tecDIR_FULL;

    sleProposal->setFieldU64(sfOwnerNode, *page);
    ctx_.view().insert(sleProposal);

    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    sleAccount->setFieldU32(sfOwnerCount, sleAccount->getFieldU32(sfOwnerCount) + 1);
    ctx_.view().update(sleAccount);

    // Register the proposal key in ltREWARD_EPOCH::sfProposals so that the
    // epoch-boundary tally loop knows about it without scanning the whole state.
    if (auto sleEpoch = ctx_.view().peek(keylet::rewardEpoch()))
    {
        auto proposals = sleEpoch->getFieldV256(sfProposals).value();
        proposals.push_back(proposalKeylet.key);
        sleEpoch->setFieldV256(sfProposals, STVector256{proposals});
        sleEpoch->setFieldH256(sfPreviousTxnID,    ctx_.tx.getTransactionID());
        sleEpoch->setFieldU32(sfPreviousTxnLgrSeq, seq);
        ctx_.view().update(sleEpoch);
    }
    // If ltREWARD_EPOCH doesn't exist yet (pre-epoch-1) we still succeed;
    // the proposal simply won't be tallied until the next epoch creates it.

    return tesSUCCESS;
}

void
GovernanceProposal::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
GovernanceProposal::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
