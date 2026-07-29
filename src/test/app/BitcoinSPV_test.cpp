// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <test/jtx.h>

#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/BTCHeader.h>
#include <xrpl/protocol/BTCMerkle.h>
#include <xrpl/protocol/BTCTx.h>
#include <xrpl/protocol/BitcoinSPVConstants.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/jss.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace xrpl {
namespace test {

class BitcoinSPV_test : public beast::unit_test::Suite
{
    // Build a minimal valid-looking 80-byte header with given prev hash bytes (LE)
    // and nBits easy enough that double-SHA256 is likely to fail PoW unless nonce lucky.
    // For unit crypto tests we only parse; for PoW we set bits max target.
    static Blob
    makeHeader(std::uint32_t version, std::array<std::uint8_t, 32> const& prevLE, std::uint32_t bits)
    {
        Blob h(80, 0);
        // version LE
        h[0] = static_cast<std::uint8_t>(version);
        h[1] = static_cast<std::uint8_t>(version >> 8);
        h[2] = static_cast<std::uint8_t>(version >> 16);
        h[3] = static_cast<std::uint8_t>(version >> 24);
        std::memcpy(h.data() + 4, prevLE.data(), 32);
        // merkle zero, timestamp, bits, nonce
        h[72] = static_cast<std::uint8_t>(bits);
        h[73] = static_cast<std::uint8_t>(bits >> 8);
        h[74] = static_cast<std::uint8_t>(bits >> 16);
        h[75] = static_cast<std::uint8_t>(bits >> 24);
        return h;
    }

    void
    testCryptoHelpers()
    {
        testcase("btc crypto helpers");

        // Empty / wrong size
        BEAST_EXPECT(!btcParseHeader(Slice{}));
        Blob bad(79, 0);
        BEAST_EXPECT(!btcParseHeader(makeSlice(bad)));

        // Max target nBits (0x207fffff) — almost always meets
        std::array<std::uint8_t, 32> prev{};
        auto hdr = makeHeader(1, prev, 0x207fffff);
        // try nonces until PoW meets (regtest-easy)
        bool found = false;
        BTCParsedHeader parsed{};
        for (std::uint32_t nonce = 0; nonce < 100000; ++nonce)
        {
            hdr[76] = static_cast<std::uint8_t>(nonce);
            hdr[77] = static_cast<std::uint8_t>(nonce >> 8);
            hdr[78] = static_cast<std::uint8_t>(nonce >> 16);
            hdr[79] = static_cast<std::uint8_t>(nonce >> 24);
            auto p = btcParseHeader(makeSlice(hdr));
            BEAST_EXPECT(p);
            if (p && btcHashMeetsTarget(p->blockHash, p->bits))
            {
                parsed = *p;
                found = true;
                break;
            }
        }
        BEAST_EXPECT(found);
        if (found)
        {
            auto const work = btcWorkFromBits(parsed.bits);
            BEAST_EXPECT(work != uint256{});
            auto const sum = btcAddWork(work, work);
            BEAST_EXPECT(sum > work);
        }

        // Merkle: single leaf, empty proof, root = txid
        uint256 leaf;
        leaf.data()[31] = 0x42;
        BEAST_EXPECT(btcVerifyMerkleProof(leaf, Slice{}, 0, leaf));

        // Script hash deterministic
        Blob spk{0x00, 0x14};
        spk.resize(22, 0x11);
        auto const h1 = btcScriptHash(makeSlice(spk));
        auto const h2 = btcScriptHash(makeSlice(spk));
        BEAST_EXPECT(h1 == h2);
    }

    void
    testDisabled()
    {
        testcase("txs disabled without amendment");
        using namespace jtx;
        Env env{*this, testableAmendments() - featureBitcoinSPVBridge};

        Account const alice{"alice"};
        env.fund(XRP(10000), alice);
        env.close();

        // Submit txs — must be temDISABLED with feature off
        json::Value jv = json::ValueType::Object;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = "BTCBridgeActivate";
        jv["BtcChainId"] = 3;
        jv["BtcAnchorHash"] = to_string(uint256{});
        jv["BtcAnchorHeight"] = 0;
        jv["BtcAnchorWork"] = to_string(uint256{});
        jv["BtcMinConfirmations"] = 1;
        jv["BtcWatchScriptHash"] = to_string(uint256{});
        jv["BtcMintCap"] = 1000;
        jv["BtcHeaderBytes"] = strHex(Blob(80, 0));
        env(jv, Ter(temDISABLED));

        json::Value jh = json::ValueType::Object;
        jh[jss::Account] = alice.human();
        jh[jss::TransactionType] = "BTCHeaderSubmit";
        jh["BtcHeaders"] = strHex(Blob(80, 0));
        env(jh, Ter(temDISABLED));

        json::Value jc = json::ValueType::Object;
        jc[jss::Account] = alice.human();
        jc[jss::TransactionType] = "BTCDepositClaim";
        jc[jss::Destination] = alice.human();
        jc["BtcRawTx"] = "00";
        jc["BtcMerkleProof"] = "";
        jc["BtcTxIndex"] = 0;
        jc["BtcBlockHash"] = to_string(uint256{});
        jc["BtcVout"] = 0;
        env(jc, Ter(temDISABLED));

        json::Value jb = json::ValueType::Object;
        jb[jss::Account] = alice.human();
        jb[jss::TransactionType] = "BTCBridgeBurn";
        jb["BtcWithdrawAmount"] = 1;
        jb["BtcPayoutScript"] = "00";
        jb["BtcBurnPreimage"] = strHex(Blob(32, 0xab));
        env(jb, Ter(temDISABLED));

        json::Value jf = json::ValueType::Object;
        jf[jss::Account] = alice.human();
        jf[jss::TransactionType] = "BTCWithdrawFinalize";
        jf["BtcWithdrawSeq"] = 1;
        env(jf, Ter(temDISABLED));
    }

    void
    testActivateAndHeaders()
    {
        testcase("activate + header extend on regtest params");
        using namespace jtx;

        // feature on; network id 0 allows activate in preclaim
        Env env{*this, testableAmendments() | featureBitcoinSPVBridge | featureMPTokensV1};

        Account const alice{"alice"};
        env.fund(XRP(100000), alice);
        env.close();

        // Mine a regtest-easy header (prev all zero = genesis-style parentless anchor)
        std::array<std::uint8_t, 32> prev{};
        auto hdr = makeHeader(1, prev, 0x207fffff);
        BTCParsedHeader parsed{};
        bool found = false;
        for (std::uint32_t nonce = 0; nonce < 200000; ++nonce)
        {
            hdr[76] = static_cast<std::uint8_t>(nonce);
            hdr[77] = static_cast<std::uint8_t>(nonce >> 8);
            hdr[78] = static_cast<std::uint8_t>(nonce >> 16);
            hdr[79] = static_cast<std::uint8_t>(nonce >> 24);
            auto p = btcParseHeader(makeSlice(hdr));
            if (p && btcHashMeetsTarget(p->blockHash, p->bits))
            {
                parsed = *p;
                found = true;
                break;
            }
        }
        BEAST_EXPECT(found);
        if (!found)
            return;

        uint256 work = btcWorkFromBits(parsed.bits);

        json::Value jv = json::ValueType::Object;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = "BTCBridgeActivate";
        jv["BtcChainId"] = json::UInt{kBTC_CHAIN_REGTEST};
        jv["BtcAnchorHash"] = to_string(parsed.blockHash);
        jv["BtcAnchorHeight"] = 0;
        jv["BtcAnchorWork"] = to_string(work);
        jv["BtcMinConfirmations"] = 1;
        jv["BtcWatchScriptHash"] = to_string(uint256{});
        // Cap fits in 32-bit for test; field is U64 on ledger
        jv["BtcMintCap"] = json::UInt{
            static_cast<json::UInt>(std::min<std::uint64_t>(
                kBTC_ISOLATED_RECOMMENDED_MINT_CAP, json::Value::kMAX_UINT))};
        jv["BtcHeaderBytes"] = strHex(hdr);
        env(jv);
        env.close();

        // Second activate → tecDUPLICATE
        env(jv, Ter(tecDUPLICATE));

        // Child header linking to anchor
        std::array<std::uint8_t, 32> prev2{};
        // prev in header is LE of block hash
        for (int i = 0; i < 32; ++i)
            prev2[i] = parsed.blockHash.data()[31 - i];

        auto hdr2 = makeHeader(1, prev2, 0x207fffff);
        BTCParsedHeader parsed2{};
        found = false;
        for (std::uint32_t nonce = 0; nonce < 200000; ++nonce)
        {
            hdr2[76] = static_cast<std::uint8_t>(nonce);
            hdr2[77] = static_cast<std::uint8_t>(nonce >> 8);
            hdr2[78] = static_cast<std::uint8_t>(nonce >> 16);
            hdr2[79] = static_cast<std::uint8_t>(nonce >> 24);
            auto p = btcParseHeader(makeSlice(hdr2));
            if (p && btcHashMeetsTarget(p->blockHash, p->bits) && p->prevHash == parsed.blockHash)
            {
                parsed2 = *p;
                found = true;
                break;
            }
        }
        BEAST_EXPECT(found);
        if (!found)
            return;

        json::Value jh = json::ValueType::Object;
        jh[jss::Account] = alice.human();
        jh[jss::TransactionType] = "BTCHeaderSubmit";
        jh["BtcHeaders"] = strHex(hdr2);
        // fee auto-scales; pay enough
        env(jh, Fee(XRP(1)));
        env.close();
    }

public:
    void
    run() override
    {
        testCryptoHelpers();
        testDisabled();
        testActivateAndHeaders();
    }
};

BEAST_DEFINE_TESTSUITE(BitcoinSPV, app, xrpl);

}  // namespace test
}  // namespace xrpl
