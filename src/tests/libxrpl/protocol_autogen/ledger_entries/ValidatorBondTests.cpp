// Auto-generated unit tests for ledger entry ValidatorBond


#include <gtest/gtest.h>

#include <protocol_autogen/TestHelpers.h>

#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol_autogen/ledger_entries/ValidatorBond.h>
#include <xrpl/protocol_autogen/ledger_entries/Ticket.h>

#include <string>

namespace xrpl::ledger_entries {

// 1 & 4) Set fields via builder setters, build, then read them back via
// wrapper getters. After build(), validate() should succeed for both the
// builder's STObject and the wrapper's SLE.
TEST(ValidatorBondTests, BuilderSettersRoundTrip)
{
    uint256 const index{1u};

    auto const accountValue = canonical_ACCOUNT();
    auto const publicKeyValue = canonical_VL();
    auto const consensusKeyValue = canonical_VL();
    auto const bondedAmountValue = canonical_AMOUNT();
    auto const bondStatusValue = canonical_UINT32();
    auto const uptimeBpsValue = canonical_UINT32();
    auto const voteAccuracyBpsValue = canonical_UINT32();
    auto const latencyScoreBpsValue = canonical_UINT32();
    auto const consistencyBpsValue = canonical_UINT32();
    auto const slashMultiplierValue = canonical_UINT32();
    auto const compositeScoreValue = canonical_UINT32();
    auto const slashCountValue = canonical_UINT32();
    auto const lastClaimedEpochValue = canonical_UINT32();
    auto const unbondingStartLedgerValue = canonical_UINT32();
    auto const ownerNodeValue = canonical_UINT64();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    ValidatorBondBuilder builder{
        accountValue,
        publicKeyValue,
        consensusKeyValue,
        bondedAmountValue,
        bondStatusValue,
        slashMultiplierValue,
        ownerNodeValue,
        previousTxnIDValue,
        previousTxnLgrSeqValue
    };

    builder.setUptimeBps(uptimeBpsValue);
    builder.setVoteAccuracyBps(voteAccuracyBpsValue);
    builder.setLatencyScoreBps(latencyScoreBpsValue);
    builder.setConsistencyBps(consistencyBpsValue);
    builder.setCompositeScore(compositeScoreValue);
    builder.setSlashCount(slashCountValue);
    builder.setLastClaimedEpoch(lastClaimedEpochValue);
    builder.setUnbondingStartLedger(unbondingStartLedgerValue);

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
        auto const& expected = publicKeyValue;
        auto const actual = entry.getPublicKey();
        expectEqualField(expected, actual, "sfPublicKey");
    }

    {
        auto const& expected = consensusKeyValue;
        auto const actual = entry.getConsensusKey();
        expectEqualField(expected, actual, "sfConsensusKey");
    }

    {
        auto const& expected = bondedAmountValue;
        auto const actual = entry.getBondedAmount();
        expectEqualField(expected, actual, "sfBondedAmount");
    }

    {
        auto const& expected = bondStatusValue;
        auto const actual = entry.getBondStatus();
        expectEqualField(expected, actual, "sfBondStatus");
    }

    {
        auto const& expected = slashMultiplierValue;
        auto const actual = entry.getSlashMultiplier();
        expectEqualField(expected, actual, "sfSlashMultiplier");
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
        auto const& expected = uptimeBpsValue;
        auto const actualOpt = entry.getUptimeBps();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfUptimeBps");
        EXPECT_TRUE(entry.hasUptimeBps());
    }

    {
        auto const& expected = voteAccuracyBpsValue;
        auto const actualOpt = entry.getVoteAccuracyBps();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfVoteAccuracyBps");
        EXPECT_TRUE(entry.hasVoteAccuracyBps());
    }

    {
        auto const& expected = latencyScoreBpsValue;
        auto const actualOpt = entry.getLatencyScoreBps();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfLatencyScoreBps");
        EXPECT_TRUE(entry.hasLatencyScoreBps());
    }

    {
        auto const& expected = consistencyBpsValue;
        auto const actualOpt = entry.getConsistencyBps();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfConsistencyBps");
        EXPECT_TRUE(entry.hasConsistencyBps());
    }

    {
        auto const& expected = compositeScoreValue;
        auto const actualOpt = entry.getCompositeScore();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfCompositeScore");
        EXPECT_TRUE(entry.hasCompositeScore());
    }

    {
        auto const& expected = slashCountValue;
        auto const actualOpt = entry.getSlashCount();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfSlashCount");
        EXPECT_TRUE(entry.hasSlashCount());
    }

    {
        auto const& expected = lastClaimedEpochValue;
        auto const actualOpt = entry.getLastClaimedEpoch();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfLastClaimedEpoch");
        EXPECT_TRUE(entry.hasLastClaimedEpoch());
    }

    {
        auto const& expected = unbondingStartLedgerValue;
        auto const actualOpt = entry.getUnbondingStartLedger();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfUnbondingStartLedger");
        EXPECT_TRUE(entry.hasUnbondingStartLedger());
    }

    EXPECT_TRUE(entry.hasLedgerIndex());
    auto const ledgerIndex = entry.getLedgerIndex();
    ASSERT_TRUE(ledgerIndex.has_value());
    EXPECT_EQ(*ledgerIndex, index);
    EXPECT_EQ(entry.getKey(), index);
}

// 2 & 4) Start from an SLE, set fields directly on it, construct a builder
// from that SLE, build a new wrapper, and verify all fields (and validate()).
TEST(ValidatorBondTests, BuilderFromSleRoundTrip)
{
    uint256 const index{2u};

    auto const accountValue = canonical_ACCOUNT();
    auto const publicKeyValue = canonical_VL();
    auto const consensusKeyValue = canonical_VL();
    auto const bondedAmountValue = canonical_AMOUNT();
    auto const bondStatusValue = canonical_UINT32();
    auto const uptimeBpsValue = canonical_UINT32();
    auto const voteAccuracyBpsValue = canonical_UINT32();
    auto const latencyScoreBpsValue = canonical_UINT32();
    auto const consistencyBpsValue = canonical_UINT32();
    auto const slashMultiplierValue = canonical_UINT32();
    auto const compositeScoreValue = canonical_UINT32();
    auto const slashCountValue = canonical_UINT32();
    auto const lastClaimedEpochValue = canonical_UINT32();
    auto const unbondingStartLedgerValue = canonical_UINT32();
    auto const ownerNodeValue = canonical_UINT64();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    auto sle = std::make_shared<SLE>(ValidatorBond::entryType, index);

    sle->at(sfAccount) = accountValue;
    sle->at(sfPublicKey) = publicKeyValue;
    sle->at(sfConsensusKey) = consensusKeyValue;
    sle->at(sfBondedAmount) = bondedAmountValue;
    sle->at(sfBondStatus) = bondStatusValue;
    sle->at(sfUptimeBps) = uptimeBpsValue;
    sle->at(sfVoteAccuracyBps) = voteAccuracyBpsValue;
    sle->at(sfLatencyScoreBps) = latencyScoreBpsValue;
    sle->at(sfConsistencyBps) = consistencyBpsValue;
    sle->at(sfSlashMultiplier) = slashMultiplierValue;
    sle->at(sfCompositeScore) = compositeScoreValue;
    sle->at(sfSlashCount) = slashCountValue;
    sle->at(sfLastClaimedEpoch) = lastClaimedEpochValue;
    sle->at(sfUnbondingStartLedger) = unbondingStartLedgerValue;
    sle->at(sfOwnerNode) = ownerNodeValue;
    sle->at(sfPreviousTxnID) = previousTxnIDValue;
    sle->at(sfPreviousTxnLgrSeq) = previousTxnLgrSeqValue;

    ValidatorBondBuilder builderFromSle{sle};
    EXPECT_TRUE(builderFromSle.validate());

    auto const entryFromBuilder = builderFromSle.build(index);

    ValidatorBond entryFromSle{sle};
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
        auto const& expected = publicKeyValue;

        auto const fromSle = entryFromSle.getPublicKey();
        auto const fromBuilder = entryFromBuilder.getPublicKey();

        expectEqualField(expected, fromSle, "sfPublicKey");
        expectEqualField(expected, fromBuilder, "sfPublicKey");
    }

    {
        auto const& expected = consensusKeyValue;

        auto const fromSle = entryFromSle.getConsensusKey();
        auto const fromBuilder = entryFromBuilder.getConsensusKey();

        expectEqualField(expected, fromSle, "sfConsensusKey");
        expectEqualField(expected, fromBuilder, "sfConsensusKey");
    }

    {
        auto const& expected = bondedAmountValue;

        auto const fromSle = entryFromSle.getBondedAmount();
        auto const fromBuilder = entryFromBuilder.getBondedAmount();

        expectEqualField(expected, fromSle, "sfBondedAmount");
        expectEqualField(expected, fromBuilder, "sfBondedAmount");
    }

    {
        auto const& expected = bondStatusValue;

        auto const fromSle = entryFromSle.getBondStatus();
        auto const fromBuilder = entryFromBuilder.getBondStatus();

        expectEqualField(expected, fromSle, "sfBondStatus");
        expectEqualField(expected, fromBuilder, "sfBondStatus");
    }

    {
        auto const& expected = slashMultiplierValue;

        auto const fromSle = entryFromSle.getSlashMultiplier();
        auto const fromBuilder = entryFromBuilder.getSlashMultiplier();

        expectEqualField(expected, fromSle, "sfSlashMultiplier");
        expectEqualField(expected, fromBuilder, "sfSlashMultiplier");
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
        auto const& expected = uptimeBpsValue;

        auto const fromSleOpt = entryFromSle.getUptimeBps();
        auto const fromBuilderOpt = entryFromBuilder.getUptimeBps();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfUptimeBps");
        expectEqualField(expected, *fromBuilderOpt, "sfUptimeBps");
    }

    {
        auto const& expected = voteAccuracyBpsValue;

        auto const fromSleOpt = entryFromSle.getVoteAccuracyBps();
        auto const fromBuilderOpt = entryFromBuilder.getVoteAccuracyBps();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfVoteAccuracyBps");
        expectEqualField(expected, *fromBuilderOpt, "sfVoteAccuracyBps");
    }

    {
        auto const& expected = latencyScoreBpsValue;

        auto const fromSleOpt = entryFromSle.getLatencyScoreBps();
        auto const fromBuilderOpt = entryFromBuilder.getLatencyScoreBps();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfLatencyScoreBps");
        expectEqualField(expected, *fromBuilderOpt, "sfLatencyScoreBps");
    }

    {
        auto const& expected = consistencyBpsValue;

        auto const fromSleOpt = entryFromSle.getConsistencyBps();
        auto const fromBuilderOpt = entryFromBuilder.getConsistencyBps();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfConsistencyBps");
        expectEqualField(expected, *fromBuilderOpt, "sfConsistencyBps");
    }

    {
        auto const& expected = compositeScoreValue;

        auto const fromSleOpt = entryFromSle.getCompositeScore();
        auto const fromBuilderOpt = entryFromBuilder.getCompositeScore();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfCompositeScore");
        expectEqualField(expected, *fromBuilderOpt, "sfCompositeScore");
    }

    {
        auto const& expected = slashCountValue;

        auto const fromSleOpt = entryFromSle.getSlashCount();
        auto const fromBuilderOpt = entryFromBuilder.getSlashCount();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfSlashCount");
        expectEqualField(expected, *fromBuilderOpt, "sfSlashCount");
    }

    {
        auto const& expected = lastClaimedEpochValue;

        auto const fromSleOpt = entryFromSle.getLastClaimedEpoch();
        auto const fromBuilderOpt = entryFromBuilder.getLastClaimedEpoch();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfLastClaimedEpoch");
        expectEqualField(expected, *fromBuilderOpt, "sfLastClaimedEpoch");
    }

    {
        auto const& expected = unbondingStartLedgerValue;

        auto const fromSleOpt = entryFromSle.getUnbondingStartLedger();
        auto const fromBuilderOpt = entryFromBuilder.getUnbondingStartLedger();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfUnbondingStartLedger");
        expectEqualField(expected, *fromBuilderOpt, "sfUnbondingStartLedger");
    }

    EXPECT_EQ(entryFromSle.getKey(), index);
    EXPECT_EQ(entryFromBuilder.getKey(), index);
}

// 3) Verify wrapper throws when constructed from wrong ledger entry type.
TEST(ValidatorBondTests, WrapperThrowsOnWrongEntryType)
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

    EXPECT_THROW(ValidatorBond{wrongEntry.getSle()}, std::runtime_error);
}

// 4) Verify builder throws when constructed from wrong ledger entry type.
TEST(ValidatorBondTests, BuilderThrowsOnWrongEntryType)
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

    EXPECT_THROW(ValidatorBondBuilder{wrongEntry.getSle()}, std::runtime_error);
}

// 5) Build with only required fields and verify optional fields return nullopt.
TEST(ValidatorBondTests, OptionalFieldsReturnNullopt)
{
    uint256 const index{3u};

    auto const accountValue = canonical_ACCOUNT();
    auto const publicKeyValue = canonical_VL();
    auto const consensusKeyValue = canonical_VL();
    auto const bondedAmountValue = canonical_AMOUNT();
    auto const bondStatusValue = canonical_UINT32();
    auto const slashMultiplierValue = canonical_UINT32();
    auto const ownerNodeValue = canonical_UINT64();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    ValidatorBondBuilder builder{
        accountValue,
        publicKeyValue,
        consensusKeyValue,
        bondedAmountValue,
        bondStatusValue,
        slashMultiplierValue,
        ownerNodeValue,
        previousTxnIDValue,
        previousTxnLgrSeqValue
    };

    auto const entry = builder.build(index);

    // Verify optional fields are not present
    EXPECT_FALSE(entry.hasUptimeBps());
    EXPECT_FALSE(entry.getUptimeBps().has_value());
    EXPECT_FALSE(entry.hasVoteAccuracyBps());
    EXPECT_FALSE(entry.getVoteAccuracyBps().has_value());
    EXPECT_FALSE(entry.hasLatencyScoreBps());
    EXPECT_FALSE(entry.getLatencyScoreBps().has_value());
    EXPECT_FALSE(entry.hasConsistencyBps());
    EXPECT_FALSE(entry.getConsistencyBps().has_value());
    EXPECT_FALSE(entry.hasCompositeScore());
    EXPECT_FALSE(entry.getCompositeScore().has_value());
    EXPECT_FALSE(entry.hasSlashCount());
    EXPECT_FALSE(entry.getSlashCount().has_value());
    EXPECT_FALSE(entry.hasLastClaimedEpoch());
    EXPECT_FALSE(entry.getLastClaimedEpoch().has_value());
    EXPECT_FALSE(entry.hasUnbondingStartLedger());
    EXPECT_FALSE(entry.getUnbondingStartLedger().has_value());
}
}
