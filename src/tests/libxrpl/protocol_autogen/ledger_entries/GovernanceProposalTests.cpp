// Auto-generated unit tests for ledger entry GovernanceProposal


#include <gtest/gtest.h>

#include <protocol_autogen/TestHelpers.h>

#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol_autogen/ledger_entries/GovernanceProposal.h>
#include <xrpl/protocol_autogen/ledger_entries/Ticket.h>

#include <string>

namespace xrpl::ledger_entries {

// 1 & 4) Set fields via builder setters, build, then read them back via
// wrapper getters. After build(), validate() should succeed for both the
// builder's STObject and the wrapper's SLE.
TEST(GovernanceProposalTests, BuilderSettersRoundTrip)
{
    uint256 const index{1u};

    auto const accountValue = canonical_ACCOUNT();
    auto const proposalTypeValue = canonical_UINT32();
    auto const proposalValueValue = canonical_UINT32();
    auto const proposalExpiryValue = canonical_UINT32();
    auto const proposalStateValue = canonical_UINT32();
    auto const votedForValue = canonical_UINT32();
    auto const votedAgainstValue = canonical_UINT32();
    auto const voterListValue = canonical_VL();
    auto const ownerNodeValue = canonical_UINT64();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    GovernanceProposalBuilder builder{
        accountValue,
        proposalTypeValue,
        proposalValueValue,
        proposalExpiryValue,
        proposalStateValue,
        ownerNodeValue,
        previousTxnIDValue,
        previousTxnLgrSeqValue
    };

    builder.setVotedFor(votedForValue);
    builder.setVotedAgainst(votedAgainstValue);
    builder.setVoterList(voterListValue);

    builder.setLedgerIndex(index);
    builder.setFlags(0x1u);

    EXPECT_TRUE(builder.validate());

    auto const entry = builder.build(index);

    EXPECT_TRUE(entry.validate());

    {
        auto const& expected = accountValue;
        auto const actual = entry.getAccount();
        expectEqualField(expected, actual, "sfAccount");
    }

    {
        auto const& expected = proposalTypeValue;
        auto const actual = entry.getProposalType();
        expectEqualField(expected, actual, "sfProposalType");
    }

    {
        auto const& expected = proposalValueValue;
        auto const actual = entry.getProposalValue();
        expectEqualField(expected, actual, "sfProposalValue");
    }

    {
        auto const& expected = proposalExpiryValue;
        auto const actual = entry.getProposalExpiry();
        expectEqualField(expected, actual, "sfProposalExpiry");
    }

    {
        auto const& expected = proposalStateValue;
        auto const actual = entry.getProposalState();
        expectEqualField(expected, actual, "sfProposalState");
    }

    {
        auto const& expected = ownerNodeValue;
        auto const actual = entry.getOwnerNode();
        expectEqualField(expected, actual, "sfOwnerNode");
    }

    {
        auto const& expected = previousTxnIDValue;
        auto const actual = entry.getPreviousTxnID();
        expectEqualField(expected, actual, "sfPreviousTxnID");
    }

    {
        auto const& expected = previousTxnLgrSeqValue;
        auto const actual = entry.getPreviousTxnLgrSeq();
        expectEqualField(expected, actual, "sfPreviousTxnLgrSeq");
    }

    {
        auto const& expected = votedForValue;
        auto const actualOpt = entry.getVotedFor();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfVotedFor");
        EXPECT_TRUE(entry.hasVotedFor());
    }

    {
        auto const& expected = votedAgainstValue;
        auto const actualOpt = entry.getVotedAgainst();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfVotedAgainst");
        EXPECT_TRUE(entry.hasVotedAgainst());
    }

    {
        auto const& expected = voterListValue;
        auto const actualOpt = entry.getVoterList();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfVoterList");
        EXPECT_TRUE(entry.hasVoterList());
    }

    EXPECT_TRUE(entry.hasLedgerIndex());
    auto const ledgerIndex = entry.getLedgerIndex();
    ASSERT_TRUE(ledgerIndex.has_value());
    EXPECT_EQ(*ledgerIndex, index);
    EXPECT_EQ(entry.getKey(), index);
}

// 2 & 4) Start from an SLE, set fields directly on it, construct a builder
// from that SLE, build a new wrapper, and verify all fields (and validate()).
TEST(GovernanceProposalTests, BuilderFromSleRoundTrip)
{
    uint256 const index{2u};

    auto const accountValue = canonical_ACCOUNT();
    auto const proposalTypeValue = canonical_UINT32();
    auto const proposalValueValue = canonical_UINT32();
    auto const proposalExpiryValue = canonical_UINT32();
    auto const proposalStateValue = canonical_UINT32();
    auto const votedForValue = canonical_UINT32();
    auto const votedAgainstValue = canonical_UINT32();
    auto const voterListValue = canonical_VL();
    auto const ownerNodeValue = canonical_UINT64();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    auto sle = std::make_shared<SLE>(GovernanceProposal::entryType, index);

    sle->at(sfAccount) = accountValue;
    sle->at(sfProposalType) = proposalTypeValue;
    sle->at(sfProposalValue) = proposalValueValue;
    sle->at(sfProposalExpiry) = proposalExpiryValue;
    sle->at(sfProposalState) = proposalStateValue;
    sle->at(sfVotedFor) = votedForValue;
    sle->at(sfVotedAgainst) = votedAgainstValue;
    sle->at(sfVoterList) = voterListValue;
    sle->at(sfOwnerNode) = ownerNodeValue;
    sle->at(sfPreviousTxnID) = previousTxnIDValue;
    sle->at(sfPreviousTxnLgrSeq) = previousTxnLgrSeqValue;

    GovernanceProposalBuilder builderFromSle{sle};
    EXPECT_TRUE(builderFromSle.validate());

    auto const entryFromBuilder = builderFromSle.build(index);

    GovernanceProposal entryFromSle{sle};
    EXPECT_TRUE(entryFromBuilder.validate());
    EXPECT_TRUE(entryFromSle.validate());

    {
        auto const& expected = accountValue;

        auto const fromSle = entryFromSle.getAccount();
        auto const fromBuilder = entryFromBuilder.getAccount();

        expectEqualField(expected, fromSle, "sfAccount");
        expectEqualField(expected, fromBuilder, "sfAccount");
    }

    {
        auto const& expected = proposalTypeValue;

        auto const fromSle = entryFromSle.getProposalType();
        auto const fromBuilder = entryFromBuilder.getProposalType();

        expectEqualField(expected, fromSle, "sfProposalType");
        expectEqualField(expected, fromBuilder, "sfProposalType");
    }

    {
        auto const& expected = proposalValueValue;

        auto const fromSle = entryFromSle.getProposalValue();
        auto const fromBuilder = entryFromBuilder.getProposalValue();

        expectEqualField(expected, fromSle, "sfProposalValue");
        expectEqualField(expected, fromBuilder, "sfProposalValue");
    }

    {
        auto const& expected = proposalExpiryValue;

        auto const fromSle = entryFromSle.getProposalExpiry();
        auto const fromBuilder = entryFromBuilder.getProposalExpiry();

        expectEqualField(expected, fromSle, "sfProposalExpiry");
        expectEqualField(expected, fromBuilder, "sfProposalExpiry");
    }

    {
        auto const& expected = proposalStateValue;

        auto const fromSle = entryFromSle.getProposalState();
        auto const fromBuilder = entryFromBuilder.getProposalState();

        expectEqualField(expected, fromSle, "sfProposalState");
        expectEqualField(expected, fromBuilder, "sfProposalState");
    }

    {
        auto const& expected = ownerNodeValue;

        auto const fromSle = entryFromSle.getOwnerNode();
        auto const fromBuilder = entryFromBuilder.getOwnerNode();

        expectEqualField(expected, fromSle, "sfOwnerNode");
        expectEqualField(expected, fromBuilder, "sfOwnerNode");
    }

    {
        auto const& expected = previousTxnIDValue;

        auto const fromSle = entryFromSle.getPreviousTxnID();
        auto const fromBuilder = entryFromBuilder.getPreviousTxnID();

        expectEqualField(expected, fromSle, "sfPreviousTxnID");
        expectEqualField(expected, fromBuilder, "sfPreviousTxnID");
    }

    {
        auto const& expected = previousTxnLgrSeqValue;

        auto const fromSle = entryFromSle.getPreviousTxnLgrSeq();
        auto const fromBuilder = entryFromBuilder.getPreviousTxnLgrSeq();

        expectEqualField(expected, fromSle, "sfPreviousTxnLgrSeq");
        expectEqualField(expected, fromBuilder, "sfPreviousTxnLgrSeq");
    }

    {
        auto const& expected = votedForValue;

        auto const fromSleOpt = entryFromSle.getVotedFor();
        auto const fromBuilderOpt = entryFromBuilder.getVotedFor();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfVotedFor");
        expectEqualField(expected, *fromBuilderOpt, "sfVotedFor");
    }

    {
        auto const& expected = votedAgainstValue;

        auto const fromSleOpt = entryFromSle.getVotedAgainst();
        auto const fromBuilderOpt = entryFromBuilder.getVotedAgainst();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfVotedAgainst");
        expectEqualField(expected, *fromBuilderOpt, "sfVotedAgainst");
    }

    {
        auto const& expected = voterListValue;

        auto const fromSleOpt = entryFromSle.getVoterList();
        auto const fromBuilderOpt = entryFromBuilder.getVoterList();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfVoterList");
        expectEqualField(expected, *fromBuilderOpt, "sfVoterList");
    }

    EXPECT_EQ(entryFromSle.getKey(), index);
    EXPECT_EQ(entryFromBuilder.getKey(), index);
}

// 3) Verify wrapper throws when constructed from wrong ledger entry type.
TEST(GovernanceProposalTests, WrapperThrowsOnWrongEntryType)
{
    uint256 const index{3u};

    // Build a valid ledger entry of a different type
    // Ticket requires: Account, OwnerNode, TicketSequence, PreviousTxnID, PreviousTxnLgrSeq
    // Check requires: Account, Destination, SendMax, Sequence, OwnerNode, DestinationNode, PreviousTxnID, PreviousTxnLgrSeq
    TicketBuilder wrongBuilder{
        canonical_ACCOUNT(),
        canonical_UINT64(),
        canonical_UINT32(),
        canonical_UINT256(),
        canonical_UINT32()};
    auto wrongEntry = wrongBuilder.build(index);

    EXPECT_THROW(GovernanceProposal{wrongEntry.getSle()}, std::runtime_error);
}

// 4) Verify builder throws when constructed from wrong ledger entry type.
TEST(GovernanceProposalTests, BuilderThrowsOnWrongEntryType)
{
    uint256 const index{4u};

    // Build a valid ledger entry of a different type
    TicketBuilder wrongBuilder{
        canonical_ACCOUNT(),
        canonical_UINT64(),
        canonical_UINT32(),
        canonical_UINT256(),
        canonical_UINT32()};
    auto wrongEntry = wrongBuilder.build(index);

    EXPECT_THROW(GovernanceProposalBuilder{wrongEntry.getSle()}, std::runtime_error);
}

// 5) Build with only required fields and verify optional fields return nullopt.
TEST(GovernanceProposalTests, OptionalFieldsReturnNullopt)
{
    uint256 const index{3u};

    auto const accountValue = canonical_ACCOUNT();
    auto const proposalTypeValue = canonical_UINT32();
    auto const proposalValueValue = canonical_UINT32();
    auto const proposalExpiryValue = canonical_UINT32();
    auto const proposalStateValue = canonical_UINT32();
    auto const ownerNodeValue = canonical_UINT64();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    GovernanceProposalBuilder builder{
        accountValue,
        proposalTypeValue,
        proposalValueValue,
        proposalExpiryValue,
        proposalStateValue,
        ownerNodeValue,
        previousTxnIDValue,
        previousTxnLgrSeqValue
    };

    auto const entry = builder.build(index);

    // Verify optional fields are not present
    EXPECT_FALSE(entry.hasVotedFor());
    EXPECT_FALSE(entry.getVotedFor().has_value());
    EXPECT_FALSE(entry.hasVotedAgainst());
    EXPECT_FALSE(entry.getVotedAgainst().has_value());
    EXPECT_FALSE(entry.hasVoterList());
    EXPECT_FALSE(entry.getVoterList().has_value());
}
}
