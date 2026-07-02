// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Consensus-level test for H-01 remediation: Falcon-signed validator proposals
// must verify through RCLCxPeerPos::checkSign without hitting secp256k1 paths.

#include <xrpld/app/consensus/RCLCxPeerPos.h>

#include <xrpl/basics/Slice.h>
#include <xrpl/beast/unit_test/suite.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/falcon.h>

#include <cstdint>
#include <vector>

namespace xrpl {

class FalconProposalSignature_test : public beast::unit_test::Suite
{
    void
    testFalconProposalSignVerify()
    {
        testcase("RCLCxPeerPos::checkSign accepts Falcon-512 proposals");

        if (!falconAvailable(KeyType::Falcon512))
        {
            log << "liboqs Falcon-512 unavailable; skipping";
            return;
        }

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        BEAST_EXPECT(kp.has_value());
        if (!kp)
            return;

        auto const& [pqPk, pqSk] = *kp;
        PublicKey const publicKey(pqPk.slice());
        NodeID const nodeID = calcNodeID(publicKey);

        uint256 const prevLedger(0x1111);
        uint256 const position(0x2222);
        auto const closeTime = NetClock::now();
        auto const now = NetClock::now();

        RCLCxPeerPos::Proposal const proposal(
            prevLedger,
            RCLCxPeerPos::Proposal::kSEQ_JOIN + 1,
            position,
            closeTime,
            now,
            nodeID);

        auto const hash = proposal.signingHash();
        auto const hashSlice = Slice(hash.data(), hash.size());
        auto sig = signFalcon(pqSk, hashSlice);
        BEAST_EXPECT(!sig.empty());

        uint256 const suppression = proposalUniqueId(
            position,
            prevLedger,
            proposal.proposeSeq(),
            closeTime,
            publicKey.slice(),
            Slice(sig.data(), sig.size()));

        RCLCxPeerPos const peerPos(
            publicKey,
            Slice(sig.data(), sig.size()),
            suppression,
            proposal);

        BEAST_EXPECT(peerPos.checkSign());

        // Classical keys must not verify (H-01: no logicError / no false accept).
        auto const classical = randomKeyPair(KeyType::Secp256k1);
        RCLCxPeerPos const classicalPos(
            classical.first,
            Slice(sig.data(), sig.size()),
            suppression,
            proposal);
        BEAST_EXPECT(!classicalPos.checkSign());
    }

    void
    testTamperedFalconProposalRejected()
    {
        testcase("RCLCxPeerPos::checkSign rejects tampered Falcon signatures");

        if (!falconAvailable(KeyType::Falcon512))
            return;

        auto const kp = generateFalconKeyPair(KeyType::Falcon512);
        if (!kp)
            return;

        auto const& [pqPk, pqSk] = *kp;
        PublicKey const publicKey(pqPk.slice());
        NodeID const nodeID = calcNodeID(publicKey);

        RCLCxPeerPos::Proposal const proposal(
            uint256(0x3333),
            2,
            uint256(0x4444),
            NetClock::now(),
            NetClock::now(),
            nodeID);

        auto sig = signFalcon(pqSk, Slice(proposal.signingHash().data(), sizeof(uint256)));
        if (sig.empty())
            return;

        sig[0] ^= 0x01;

        RCLCxPeerPos const peerPos(
            publicKey,
            Slice(sig.data(), sig.size()),
            uint256(0x5555),
            proposal);

        BEAST_EXPECT(!peerPos.checkSign());
    }

public:
    void
    run() override
    {
        testFalconProposalSignVerify();
        testTamperedFalconProposalRejected();
    }
};

BEAST_DEFINE_TESTSUITE(FalconProposalSignature, app, xrpl);

}  // namespace xrpl