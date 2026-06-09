// Copyright (c) 2026 Falcon Ledger Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Tests for post-quantum Falcon transaction-signature support: the
// signing-key primitives that allow qXRP transactions to be signed and
// verified with Falcon-512 / Falcon-1024 keys.

#include <xrpl/basics/Slice.h>
#include <xrpl/beast/unit_test/suite.h>
#include <xrpl/beast/hash/uhash.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PQPublicKey.h>
#include <xrpl/protocol/PQSecretKey.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/Rules.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxFlags.h>
#include <xrpl/protocol/falcon.h>

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace xrpl {

class FalconTxnSignature_test : public beast::unit_test::Suite
{
    static Slice
    asSlice(std::string const& s)
    {
        return Slice(reinterpret_cast<std::uint8_t const*>(s.data()), s.size());
    }

    void
    testSigningPubKeyType()
    {
        testcase("signingPubKeyType recognizes Falcon and classical keys");

        // Classical keys still recognized.
        auto const classical = derivePublicKey(
            KeyType::Ed25519, generateSecretKey(KeyType::Ed25519, generateSeed("masterpassphrase")));
        BEAST_EXPECT(signingPubKeyType(classical.slice()) == KeyType::Ed25519);

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping Falcon checks";
            return;
        }

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;

        BEAST_EXPECT(signingPubKeyType(kp->first.slice()) == KeyType::Falcon512);

        // An empty or junk blob is not a signing key.
        BEAST_EXPECT(!signingPubKeyType(Slice(nullptr, 0)));
        std::vector<std::uint8_t> junk(40, 0x11);
        BEAST_EXPECT(!signingPubKeyType(Slice(junk.data(), junk.size())));
    }

    void
    testVerifyRoundTrip()
    {
        testcase("Falcon verify() round-trip via slice overload");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;
        auto const& pk = kp->first;
        auto const& sk = kp->second;

        std::string const msg = "quantum-resistant transaction payload";
        auto const sigVec = signFalcon(sk, asSlice(msg));
        Slice const sig(sigVec.data(), sigVec.size());

        // Valid signature verifies through the generic slice-based verify().
        BEAST_EXPECT(verify(pk.slice(), asSlice(msg), sig));

        // Tampered message must fail.
        std::string const badMsg = "quantum-resistant transaction payloaX";
        BEAST_EXPECT(!verify(pk.slice(), asSlice(badMsg), sig));

        // Tampered signature must fail.
        auto tampered = sigVec;
        if (!tampered.empty())
            tampered[0] ^= 0xFF;
        BEAST_EXPECT(!verify(pk.slice(), asSlice(msg), Slice(tampered.data(), tampered.size())));

        // Empty signature must fail (and not throw).
        BEAST_EXPECT(!verify(pk.slice(), asSlice(msg), Slice(nullptr, 0)));

        // A signature from a different key must fail.
        auto const kp2 = generateFalconKeyPair(KeyType::Falcon512);
        if (kp2)
            BEAST_EXPECT(!verify(kp2->first.slice(), asSlice(msg), sig));
    }

    void
    testAccountIDDerivation()
    {
        testcase("calcAccountID(Slice) is deterministic and matches bond ID");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;

        auto const id1 = calcAccountID(kp->first.slice());
        auto const id2 = calcAccountID(kp->first.slice());
        BEAST_EXPECT(id1 == id2);

        // Same RIPEMD160(SHA256(blob)) transform as the validator bond ID.
        BEAST_EXPECT(id1 == calcValidatorBondID(kp->first.slice()));

        // For a classical key, the slice overload agrees with the PublicKey
        // overload, proving the two derivations are consistent.
        auto const classical = derivePublicKey(
            KeyType::Secp256k1,
            generateSecretKey(KeyType::Secp256k1, generateSeed("masterpassphrase")));
        BEAST_EXPECT(calcAccountID(classical.slice()) == calcAccountID(classical));
    }

    void
    testClassicalStillVerifies()
    {
        testcase("Classical signatures still verify via slice overload");

        auto const sk = generateSecretKey(KeyType::Ed25519, generateSeed("masterpassphrase"));
        auto const pk = derivePublicKey(KeyType::Ed25519, sk);

        std::string const msg = "classical payload";
        auto const sig = sign(pk, sk, asSlice(msg));
        BEAST_EXPECT(verify(pk.slice(), asSlice(msg), Slice(sig.data(), sig.size())));
        BEAST_EXPECT(!verify(pk.slice(), asSlice("other"), Slice(sig.data(), sig.size())));
    }

    void
    testSTTxFalconRoundTrip()
    {
        testcase("STTx single-sign with Falcon: checkSign + serialization");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;
        auto const& pk = kp->first;
        auto const& sk = kp->second;

        // The account's master key is the Falcon public key.
        auto const acct = calcAccountID(pk.slice());

        STTx tx(ttACCOUNT_SET, [&](auto& obj) {
            obj.setAccountID(sfAccount, acct);
            obj.setFieldVL(sfMessageKey, pk.slice());
        });

        // sign() sets both sfSigningPubKey (Falcon blob) and sfTxnSignature.
        tx.sign(pk, sk);

        // The embedded signing key is recognized as a Falcon key.
        BEAST_EXPECT(signingPubKeyType(makeSlice(tx.getSigningPubKey())) == KeyType::Falcon512);

        std::unordered_set<uint256, beast::Uhash<>> const presets;
        Rules const defaultRules{presets};

        // A correctly Falcon-signed transaction passes the cryptographic check.
        BEAST_EXPECT(static_cast<bool>(tx.checkSign(defaultRules)));

        // Serialize / deserialize round-trip preserves the Falcon transaction.
        Serializer rawTxn;
        tx.add(rawTxn);
        SerialIter sit(rawTxn.slice());
        STTx const copy(sit);
        BEAST_EXPECT(copy == tx);
        BEAST_EXPECT(static_cast<bool>(copy.checkSign(defaultRules)));

        // Tampering with the signature must make the check fail.
        {
            STTx bad(tx);
            Blob sig = bad.getFieldVL(sfTxnSignature);
            BEAST_EXPECT(!sig.empty());
            sig[0] ^= 0xFF;
            bad.setFieldVL(sfTxnSignature, sig);
            BEAST_EXPECT(!static_cast<bool>(bad.checkSign(defaultRules)));
        }

        // Substituting a different Falcon key (that did not sign) must fail.
        {
            auto kp2 = generateFalconKeyPair(KeyType::Falcon512);
            if (kp2)
            {
                STTx bad(tx);
                bad.setFieldVL(sfSigningPubKey, kp2->first.slice());
                BEAST_EXPECT(!static_cast<bool>(bad.checkSign(defaultRules)));
            }
        }
    }

public:
    void
    run() override
    {
        testSigningPubKeyType();
        testVerifyRoundTrip();
        testAccountIDDerivation();
        testClassicalStillVerifies();
        testSTTxFalconRoundTrip();
    }
};

BEAST_DEFINE_TESTSUITE(FalconTxnSignature, protocol, xrpl);

}  // namespace xrpl
