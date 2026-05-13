// Auto-generated unit tests for ledger entry RewardEpoch


#include <gtest/gtest.h>

#include <protocol_autogen/TestHelpers.h>

#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol_autogen/ledger_entries/RewardEpoch.h>
#include <xrpl/protocol_autogen/ledger_entries/Ticket.h>

#include <string>

namespace xrpl::ledger_entries {

// 1 & 4) Set fields via builder setters, build, then read them back via
// wrapper getters. After build(), validate() should succeed for both the
// builder's STObject and the wrapper's SLE.
TEST(RewardEpochTests, BuilderSettersRoundTrip)
{
    uint256 const index{1u};

    auto const epochNumberValue = canonical_UINT32();
    auto const epochStartLedgerValue = canonical_UINT32();
    auto const epochPoolBalanceValue = canonical_AMOUNT();
    auto const emissionRateValue = canonical_AMOUNT();
    auto const currentBurnBpsValue = canonical_UINT32();
    auto const feeVolumeEMAValue = canonical_UINT32();
    auto const aggregateCompositeScoreValue = canonical_UINT32();
    auto const proposalsValue = canonical_VECTOR256();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    RewardEpochBuilder builder{
        epochNumberValue,
        epochStartLedgerValue,
        epochPoolBalanceValue,
        emissionRateValue,
        currentBurnBpsValue,
        previousTxnIDValue,
        previousTxnLgrSeqValue
    };

    builder.setFeeVolumeEMA(feeVolumeEMAValue);
    builder.setAggregateCompositeScore(aggregateCompositeScoreValue);
    builder.setProposals(proposalsValue);

    builder.setLedgerIndex(index);
    builder.setFlags(0x1u);

    EXPECT_TRUE(builder.validate());

    auto const entry = builder.build(index);

    EXPECT_TRUE(entry.validate());

    {
        auto const& expected = epochNumberValue;
        auto const actual = entry.getEpochNumber();
        expectEqualField(expected, actual, "sfEpochNumber");
    }

    {
        auto const& expected = epochStartLedgerValue;
        auto const actual = entry.getEpochStartLedger();
        expectEqualField(expected, actual, "sfEpochStartLedger");
    }

    {
        auto const& expected = epochPoolBalanceValue;
        auto const actual = entry.getEpochPoolBalance();
        expectEqualField(expected, actual, "sfEpochPoolBalance");
    }

    {
        auto const& expected = emissionRateValue;
        auto const actual = entry.getEmissionRate();
        expectEqualField(expected, actual, "sfEmissionRate");
    }

    {
        auto const& expected = currentBurnBpsValue;
        auto const actual = entry.getCurrentBurnBps();
        expectEqualField(expected, actual, "sfCurrentBurnBps");
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
        auto const& expected = feeVolumeEMAValue;
        auto const actualOpt = entry.getFeeVolumeEMA();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfFeeVolumeEMA");
        EXPECT_TRUE(entry.hasFeeVolumeEMA());
    }

    {
        auto const& expected = aggregateCompositeScoreValue;
        auto const actualOpt = entry.getAggregateCompositeScore();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfAggregateCompositeScore");
        EXPECT_TRUE(entry.hasAggregateCompositeScore());
    }

    {
        auto const& expected = proposalsValue;
        auto const actualOpt = entry.getProposals();
        ASSERT_TRUE(actualOpt.has_value());
        expectEqualField(expected, *actualOpt, "sfProposals");
        EXPECT_TRUE(entry.hasProposals());
    }

    EXPECT_TRUE(entry.hasLedgerIndex());
    auto const ledgerIndex = entry.getLedgerIndex();
    ASSERT_TRUE(ledgerIndex.has_value());
    EXPECT_EQ(*ledgerIndex, index);
    EXPECT_EQ(entry.getKey(), index);
}

// 2 & 4) Start from an SLE, set fields directly on it, construct a builder
// from that SLE, build a new wrapper, and verify all fields (and validate()).
TEST(RewardEpochTests, BuilderFromSleRoundTrip)
{
    uint256 const index{2u};

    auto const epochNumberValue = canonical_UINT32();
    auto const epochStartLedgerValue = canonical_UINT32();
    auto const epochPoolBalanceValue = canonical_AMOUNT();
    auto const emissionRateValue = canonical_AMOUNT();
    auto const currentBurnBpsValue = canonical_UINT32();
    auto const feeVolumeEMAValue = canonical_UINT32();
    auto const aggregateCompositeScoreValue = canonical_UINT32();
    auto const proposalsValue = canonical_VECTOR256();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    auto sle = std::make_shared<SLE>(RewardEpoch::entryType, index);

    sle->at(sfEpochNumber) = epochNumberValue;
    sle->at(sfEpochStartLedger) = epochStartLedgerValue;
    sle->at(sfEpochPoolBalance) = epochPoolBalanceValue;
    sle->at(sfEmissionRate) = emissionRateValue;
    sle->at(sfCurrentBurnBps) = currentBurnBpsValue;
    sle->at(sfFeeVolumeEMA) = feeVolumeEMAValue;
    sle->at(sfAggregateCompositeScore) = aggregateCompositeScoreValue;
    sle->at(sfProposals) = proposalsValue;
    sle->at(sfPreviousTxnID) = previousTxnIDValue;
    sle->at(sfPreviousTxnLgrSeq) = previousTxnLgrSeqValue;

    RewardEpochBuilder builderFromSle{sle};
    EXPECT_TRUE(builderFromSle.validate());

    auto const entryFromBuilder = builderFromSle.build(index);

    RewardEpoch entryFromSle{sle};
    EXPECT_TRUE(entryFromBuilder.validate());
    EXPECT_TRUE(entryFromSle.validate());

    {
        auto const& expected = epochNumberValue;

        auto const fromSle = entryFromSle.getEpochNumber();
        auto const fromBuilder = entryFromBuilder.getEpochNumber();

        expectEqualField(expected, fromSle, "sfEpochNumber");
        expectEqualField(expected, fromBuilder, "sfEpochNumber");
    }

    {
        auto const& expected = epochStartLedgerValue;

        auto const fromSle = entryFromSle.getEpochStartLedger();
        auto const fromBuilder = entryFromBuilder.getEpochStartLedger();

        expectEqualField(expected, fromSle, "sfEpochStartLedger");
        expectEqualField(expected, fromBuilder, "sfEpochStartLedger");
    }

    {
        auto const& expected = epochPoolBalanceValue;

        auto const fromSle = entryFromSle.getEpochPoolBalance();
        auto const fromBuilder = entryFromBuilder.getEpochPoolBalance();

        expectEqualField(expected, fromSle, "sfEpochPoolBalance");
        expectEqualField(expected, fromBuilder, "sfEpochPoolBalance");
    }

    {
        auto const& expected = emissionRateValue;

        auto const fromSle = entryFromSle.getEmissionRate();
        auto const fromBuilder = entryFromBuilder.getEmissionRate();

        expectEqualField(expected, fromSle, "sfEmissionRate");
        expectEqualField(expected, fromBuilder, "sfEmissionRate");
    }

    {
        auto const& expected = currentBurnBpsValue;

        auto const fromSle = entryFromSle.getCurrentBurnBps();
        auto const fromBuilder = entryFromBuilder.getCurrentBurnBps();

        expectEqualField(expected, fromSle, "sfCurrentBurnBps");
        expectEqualField(expected, fromBuilder, "sfCurrentBurnBps");
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
        auto const& expected = feeVolumeEMAValue;

        auto const fromSleOpt = entryFromSle.getFeeVolumeEMA();
        auto const fromBuilderOpt = entryFromBuilder.getFeeVolumeEMA();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfFeeVolumeEMA");
        expectEqualField(expected, *fromBuilderOpt, "sfFeeVolumeEMA");
    }

    {
        auto const& expected = aggregateCompositeScoreValue;

        auto const fromSleOpt = entryFromSle.getAggregateCompositeScore();
        auto const fromBuilderOpt = entryFromBuilder.getAggregateCompositeScore();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfAggregateCompositeScore");
        expectEqualField(expected, *fromBuilderOpt, "sfAggregateCompositeScore");
    }

    {
        auto const& expected = proposalsValue;

        auto const fromSleOpt = entryFromSle.getProposals();
        auto const fromBuilderOpt = entryFromBuilder.getProposals();

        ASSERT_TRUE(fromSleOpt.has_value());
        ASSERT_TRUE(fromBuilderOpt.has_value());

        expectEqualField(expected, *fromSleOpt, "sfProposals");
        expectEqualField(expected, *fromBuilderOpt, "sfProposals");
    }

    EXPECT_EQ(entryFromSle.getKey(), index);
    EXPECT_EQ(entryFromBuilder.getKey(), index);
}

// 3) Verify wrapper throws when constructed from wrong ledger entry type.
TEST(RewardEpochTests, WrapperThrowsOnWrongEntryType)
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

    EXPECT_THROW(RewardEpoch{wrongEntry.getSle()}, std::runtime_error);
}

// 4) Verify builder throws when constructed from wrong ledger entry type.
TEST(RewardEpochTests, BuilderThrowsOnWrongEntryType)
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

    EXPECT_THROW(RewardEpochBuilder{wrongEntry.getSle()}, std::runtime_error);
}

// 5) Build with only required fields and verify optional fields return nullopt.
TEST(RewardEpochTests, OptionalFieldsReturnNullopt)
{
    uint256 const index{3u};

    auto const epochNumberValue = canonical_UINT32();
    auto const epochStartLedgerValue = canonical_UINT32();
    auto const epochPoolBalanceValue = canonical_AMOUNT();
    auto const emissionRateValue = canonical_AMOUNT();
    auto const currentBurnBpsValue = canonical_UINT32();
    auto const previousTxnIDValue = canonical_UINT256();
    auto const previousTxnLgrSeqValue = canonical_UINT32();

    RewardEpochBuilder builder{
        epochNumberValue,
        epochStartLedgerValue,
        epochPoolBalanceValue,
        emissionRateValue,
        currentBurnBpsValue,
        previousTxnIDValue,
        previousTxnLgrSeqValue
    };

    auto const entry = builder.build(index);

    // Verify optional fields are not present
    EXPECT_FALSE(entry.hasFeeVolumeEMA());
    EXPECT_FALSE(entry.getFeeVolumeEMA().has_value());
    EXPECT_FALSE(entry.hasAggregateCompositeScore());
    EXPECT_FALSE(entry.getAggregateCompositeScore().has_value());
    EXPECT_FALSE(entry.hasProposals());
    EXPECT_FALSE(entry.getProposals().has_value());
}
}
