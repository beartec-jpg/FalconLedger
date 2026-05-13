// Auto-generated unit tests for transaction ValidatorSlash


#include <gtest/gtest.h>

#include <protocol_autogen/TestHelpers.h>

#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol_autogen/transactions/ValidatorSlash.h>
#include <xrpl/protocol_autogen/transactions/AccountSet.h>

#include <string>

namespace xrpl::transactions {

// 1 & 4) Set fields via builder setters, build, then read them back via
// wrapper getters. After build(), validate() should succeed.
TEST(TransactionsValidatorSlashTests, BuilderSettersRoundTrip)
{
    // Generate a deterministic keypair for signing
    auto const [publicKey, secretKey] =
        generateKeyPair(KeyType::Secp256k1, generateSeed("testValidatorSlash"));

    // Common transaction fields
    auto const accountValue = calcAccountID(publicKey);
    std::uint32_t const sequenceValue = 1;
    auto const feeValue = canonical_AMOUNT();

    // Transaction-specific field values
    auto const slashTargetValue = canonical_ACCOUNT();
    auto const slashOffenseValue = canonical_UINT32();
    auto const slashEvidence1Value = canonical_VL();
    auto const slashEvidence2Value = canonical_VL();

    ValidatorSlashBuilder builder{
        accountValue,
        slashTargetValue,
        slashOffenseValue,
        sequenceValue,
        feeValue
    };

    // Set optional fields
    builder.setSlashEvidence1(slashEvidence1Value);
    builder.setSlashEvidence2(slashEvidence2Value);

    auto tx = builder.build(publicKey, secretKey);

    std::string reason;
    EXPECT_TRUE(tx.validate(reason)) << reason;

    // Verify signing was applied
    EXPECT_FALSE(tx.getSigningPubKey().empty());
    EXPECT_TRUE(tx.hasTxnSignature());

    // Verify common fields
    EXPECT_EQ(tx.getAccount(), accountValue);
    EXPECT_EQ(tx.getSequence(), sequenceValue);
    EXPECT_EQ(tx.getFee(), feeValue);

    // Verify required fields
    {
        auto const& expected = slashTargetValue;
        auto const actual = tx.getSlashTarget();
        expectEqualField(expected, actual, "sfSlashTarget");
    }

    {
        auto const& expected = slashOffenseValue;
        auto const actual = tx.getSlashOffense();
        expectEqualField(expected, actual, "sfSlashOffense");
    }

    // Verify optional fields
    {
        auto const& expected = slashEvidence1Value;
        auto const actualOpt = tx.getSlashEvidence1();
        ASSERT_TRUE(actualOpt.has_value()) << "Optional field sfSlashEvidence1 should be present";
        expectEqualField(expected, *actualOpt, "sfSlashEvidence1");
        EXPECT_TRUE(tx.hasSlashEvidence1());
    }

    {
        auto const& expected = slashEvidence2Value;
        auto const actualOpt = tx.getSlashEvidence2();
        ASSERT_TRUE(actualOpt.has_value()) << "Optional field sfSlashEvidence2 should be present";
        expectEqualField(expected, *actualOpt, "sfSlashEvidence2");
        EXPECT_TRUE(tx.hasSlashEvidence2());
    }

}

// 2 & 4) Start from an STTx, construct a builder from it, build a new wrapper,
// and verify all fields match.
TEST(TransactionsValidatorSlashTests, BuilderFromStTxRoundTrip)
{
    // Generate a deterministic keypair for signing
    auto const [publicKey, secretKey] =
        generateKeyPair(KeyType::Secp256k1, generateSeed("testValidatorSlashFromTx"));

    // Common transaction fields
    auto const accountValue = calcAccountID(publicKey);
    std::uint32_t const sequenceValue = 2;
    auto const feeValue = canonical_AMOUNT();

    // Transaction-specific field values
    auto const slashTargetValue = canonical_ACCOUNT();
    auto const slashOffenseValue = canonical_UINT32();
    auto const slashEvidence1Value = canonical_VL();
    auto const slashEvidence2Value = canonical_VL();

    // Build an initial transaction
    ValidatorSlashBuilder initialBuilder{
        accountValue,
        slashTargetValue,
        slashOffenseValue,
        sequenceValue,
        feeValue
    };

    initialBuilder.setSlashEvidence1(slashEvidence1Value);
    initialBuilder.setSlashEvidence2(slashEvidence2Value);

    auto initialTx = initialBuilder.build(publicKey, secretKey);

    // Create builder from existing STTx
    ValidatorSlashBuilder builderFromTx{initialTx.getSTTx()};

    auto rebuiltTx = builderFromTx.build(publicKey, secretKey);

    std::string reason;
    EXPECT_TRUE(rebuiltTx.validate(reason)) << reason;

    // Verify common fields
    EXPECT_EQ(rebuiltTx.getAccount(), accountValue);
    EXPECT_EQ(rebuiltTx.getSequence(), sequenceValue);
    EXPECT_EQ(rebuiltTx.getFee(), feeValue);

    // Verify required fields
    {
        auto const& expected = slashTargetValue;
        auto const actual = rebuiltTx.getSlashTarget();
        expectEqualField(expected, actual, "sfSlashTarget");
    }

    {
        auto const& expected = slashOffenseValue;
        auto const actual = rebuiltTx.getSlashOffense();
        expectEqualField(expected, actual, "sfSlashOffense");
    }

    // Verify optional fields
    {
        auto const& expected = slashEvidence1Value;
        auto const actualOpt = rebuiltTx.getSlashEvidence1();
        ASSERT_TRUE(actualOpt.has_value()) << "Optional field sfSlashEvidence1 should be present";
        expectEqualField(expected, *actualOpt, "sfSlashEvidence1");
    }

    {
        auto const& expected = slashEvidence2Value;
        auto const actualOpt = rebuiltTx.getSlashEvidence2();
        ASSERT_TRUE(actualOpt.has_value()) << "Optional field sfSlashEvidence2 should be present";
        expectEqualField(expected, *actualOpt, "sfSlashEvidence2");
    }

}

// 3) Verify wrapper throws when constructed from wrong transaction type.
TEST(TransactionsValidatorSlashTests, WrapperThrowsOnWrongTxType)
{
    // Build a valid transaction of a different type
    auto const [pk, sk] =
        generateKeyPair(KeyType::Secp256k1, generateSeed("testWrongType"));
    auto const account = calcAccountID(pk);

    AccountSetBuilder wrongBuilder{account, 1, canonical_AMOUNT()};
    auto wrongTx = wrongBuilder.build(pk, sk);

    EXPECT_THROW(ValidatorSlash{wrongTx.getSTTx()}, std::runtime_error);
}

// 4) Verify builder throws when constructed from wrong transaction type.
TEST(TransactionsValidatorSlashTests, BuilderThrowsOnWrongTxType)
{
    // Build a valid transaction of a different type
    auto const [pk, sk] =
        generateKeyPair(KeyType::Secp256k1, generateSeed("testWrongTypeBuilder"));
    auto const account = calcAccountID(pk);

    AccountSetBuilder wrongBuilder{account, 1, canonical_AMOUNT()};
    auto wrongTx = wrongBuilder.build(pk, sk);

    EXPECT_THROW(ValidatorSlashBuilder{wrongTx.getSTTx()}, std::runtime_error);
}

// 5) Build with only required fields and verify optional fields return nullopt.
TEST(TransactionsValidatorSlashTests, OptionalFieldsReturnNullopt)
{
    // Generate a deterministic keypair for signing
    auto const [publicKey, secretKey] =
        generateKeyPair(KeyType::Secp256k1, generateSeed("testValidatorSlashNullopt"));

    // Common transaction fields
    auto const accountValue = calcAccountID(publicKey);
    std::uint32_t const sequenceValue = 3;
    auto const feeValue = canonical_AMOUNT();

    // Transaction-specific required field values
    auto const slashTargetValue = canonical_ACCOUNT();
    auto const slashOffenseValue = canonical_UINT32();

    ValidatorSlashBuilder builder{
        accountValue,
        slashTargetValue,
        slashOffenseValue,
        sequenceValue,
        feeValue
    };

    // Do NOT set optional fields

    auto tx = builder.build(publicKey, secretKey);

    // Verify optional fields are not present
    EXPECT_FALSE(tx.hasSlashEvidence1());
    EXPECT_FALSE(tx.getSlashEvidence1().has_value());
    EXPECT_FALSE(tx.hasSlashEvidence2());
    EXPECT_FALSE(tx.getSlashEvidence2().has_value());
}

}
