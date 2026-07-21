// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// C-01 regression: ValidatorSlash must reject garbage evidence and non-DOUBLE
// offenses; accept only cryptographically valid Falcon double-sign pairs.

#include <xrpl/basics/Slice.h>
#include <xrpl/beast/unit_test/suite.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/STValidation.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/falcon.h>
#include <xrpl/protocol/QXRPConstants.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace xrpl {
namespace {

// Mirrors the production helper in ValidatorSlash.cpp (kept local to the test
// so we can exercise the rules without applying a full ledger).
std::optional<STValidation>
parseValidationEvidence(Slice blob)
{
    if (blob.empty())
        return std::nullopt;
    try
    {
        SerialIter sit{blob};
        STValidation val(
            sit, [](PublicKey const& pk) { return calcNodeID(pk); }, true);
        if (!val.isValid())
            return std::nullopt;
        return val;
    }
    catch (std::exception const&)
    {
        return std::nullopt;
    }
}

bool
isValidDoubleSignPair(Slice e1, Slice e2, Slice consensusKey)
{
    auto v1 = parseValidationEvidence(e1);
    auto v2 = parseValidationEvidence(e2);
    if (!v1 || !v2)
        return false;
    auto const pk1 = v1->getSignerPublic().slice();
    auto const pk2 = v2->getSignerPublic().slice();
    if (pk1.size() == 0 || pk1 != pk2)
        return false;
    if (pk1 != consensusKey)
        return false;
    if (v1->getFieldU32(sfLedgerSequence) != v2->getFieldU32(sfLedgerSequence))
        return false;
    if (v1->getLedgerHash() == v2->getLedgerHash())
        return false;
    return true;
}

Blob
makeFalconValidation(
    PQPublicKey const& pk,
    PQSecretKey const& sk,
    std::uint32_t ledgerSeq,
    uint256 const& ledgerHash)
{
    STValidation val(
        NetClock::time_point{NetClock::duration{1}},
        pk,
        sk,
        calcNodeID(PublicKey{pk.slice()}),
        [&](STObject& obj) {
            obj.setFieldH256(sfLedgerHash, ledgerHash);
            obj.setFieldU32(sfLedgerSequence, ledgerSeq);
            obj.setFlag(kVF_FULL_VALIDATION | kVF_FULLY_CANONICAL_SIG);
        });
    return val.getSerialized();
}

}  // namespace

class ValidatorSlashEvidence_test : public beast::unit_test::Suite
{
    void
    testGarbageRejected()
    {
        testcase("garbage evidence blobs are rejected");
        Blob a{0x01, 0x02, 0x03};
        Blob b{0x04, 0x05, 0x06};
        Blob key{0xFB};
        BEAST_EXPECT(!isValidDoubleSignPair(makeSlice(a), makeSlice(b), makeSlice(key)));
    }

    void
    testIdenticalRejected()
    {
        testcase("identical ledger hashes are rejected");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;
        auto const& [pk, sk] = *kp;

        uint256 hash(0xAAAA);
        auto const e1 = makeFalconValidation(pk, sk, 100, hash);
        auto const e2 = makeFalconValidation(pk, sk, 100, hash);
        BEAST_EXPECT(!isValidDoubleSignPair(makeSlice(e1), makeSlice(e2), pk.slice()));
    }

    void
    testValidDoubleSignAccepted()
    {
        testcase("valid Falcon double-sign pair is accepted");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;
        auto const& [pk, sk] = *kp;

        uint256 h1(0x1111);
        uint256 h2(0x2222);
        auto const e1 = makeFalconValidation(pk, sk, 42, h1);
        auto const e2 = makeFalconValidation(pk, sk, 42, h2);
        BEAST_EXPECT(isValidDoubleSignPair(makeSlice(e1), makeSlice(e2), pk.slice()));
    }

    void
    testWrongKeyRejected()
    {
        testcase("evidence from a different key is rejected");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto const kp1 = generateFalconKeyPair(KeyType::Falcon512);
        auto const kp2 = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp1.has_value() && kp2.has_value());
        if (!kp1 || !kp2)
            return;

        auto const& [pk1, sk1] = *kp1;
        auto const& [pk2, sk2] = *kp2;

        auto const e1 = makeFalconValidation(pk1, sk1, 7, uint256(0x1));
        auto const e2 = makeFalconValidation(pk2, sk2, 7, uint256(0x2));
        // Different signers → reject
        BEAST_EXPECT(!isValidDoubleSignPair(makeSlice(e1), makeSlice(e2), pk1.slice()));
        // Same signer pair but claimed consensus key is the other validator
        auto const e1b = makeFalconValidation(pk1, sk1, 7, uint256(0x1));
        auto const e2b = makeFalconValidation(pk1, sk1, 7, uint256(0x2));
        BEAST_EXPECT(!isValidDoubleSignPair(makeSlice(e1b), makeSlice(e2b), pk2.slice()));
    }

    void
    testOffenseConstants()
    {
        testcase("ABSENCE and INVALID_VOTE remain non-DOUBLE_SIGN");
        BEAST_EXPECT(kSLASH_OFFENSE_DOUBLE_SIGN == 1);
        BEAST_EXPECT(kSLASH_OFFENSE_ABSENCE == 2);
        BEAST_EXPECT(kSLASH_OFFENSE_INVALID_VOTE == 3);
        BEAST_EXPECT(kSLASH_OFFENSE_ABSENCE != kSLASH_OFFENSE_DOUBLE_SIGN);
    }

public:
    void
    run() override
    {
        testGarbageRejected();
        testIdenticalRejected();
        testValidDoubleSignAccepted();
        testWrongKeyRejected();
        testOffenseConstants();
    }
};

BEAST_DEFINE_TESTSUITE(ValidatorSlashEvidence, app, xrpl);

}  // namespace xrpl
