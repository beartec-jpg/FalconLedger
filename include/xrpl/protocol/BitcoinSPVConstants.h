// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xrpl {

// Bitcoin chain identifiers stored in sfBtcChainId
constexpr std::uint32_t kBTC_CHAIN_MAINNET = 0;
constexpr std::uint32_t kBTC_CHAIN_TESTNET3 = 1;
constexpr std::uint32_t kBTC_CHAIN_SIGNET = 2;
constexpr std::uint32_t kBTC_CHAIN_REGTEST = 3;

constexpr std::uint32_t kBTC_MAX_HEADERS_PER_TX = 32;
constexpr std::uint32_t kBTC_HEADER_SIZE = 80;
constexpr std::uint32_t kBTC_MAX_MERKLE_DEPTH = 32;
constexpr std::uint32_t kBTC_MAX_TX_BLOB = 100'000;
constexpr std::uint32_t kBTC_MAX_BEST_CHAIN_WALK = 4096;

constexpr std::uint32_t kBTC_MIN_CONF_REGTEST = 1;
constexpr std::uint32_t kBTC_MIN_CONF_TESTNET = 6;
constexpr std::uint32_t kBTC_MIN_CONF_MAINNET = 12;

// Future-block limit analog (seconds) vs Falcon close time
constexpr std::uint32_t kBTC_MAX_TIMESTAMP_AHEAD_SEC = 2 * 60 * 60;

// Isolated SPV research / prototype network
constexpr std::uint32_t kBTC_SPV_ISOLATED_NETWORK_ID = 1101;

// Mint caps (sats)
constexpr std::uint64_t kBTC_DEFAULT_MINT_CAP_SATS = 21'000'000ULL * 100'000'000ULL;
constexpr std::uint64_t kBTC_ISOLATED_RECOMMENDED_MINT_CAP = 10ULL * 100'000'000ULL;

constexpr std::uint64_t kBTC_DUST_REGTEST = 1;
constexpr std::uint64_t kBTC_DUST_MAINNET_DESIGN = 546;

// OP_RETURN magic "FALC" || AccountID(20)
constexpr std::array<std::uint8_t, 4> kBTC_OP_RETURN_MAGIC{{0x46, 0x41, 0x4c, 0x43}};
constexpr std::size_t kBTC_OP_RETURN_PAYLOAD_LEN = 24;  // 4 + 20

constexpr std::uint32_t kBTC_DEPOSIT_MINTED = 1;

// BitVM peg-out withdraw status
constexpr std::uint32_t kBTC_WITHDRAW_PENDING = 0;
constexpr std::uint32_t kBTC_WITHDRAW_CHALLENGED = 1;
constexpr std::uint32_t kBTC_WITHDRAW_FINAL = 2;
constexpr std::uint32_t kBTC_WITHDRAW_PAID = 3;

// Falcon challenge window (ledgers) before BTC claim is authorized
constexpr std::uint32_t kBTC_CHALLENGE_LEDGERS_DEFAULT = 32;
// Bitcoin CSV relative locktime (blocks) for vault happy-path claim
constexpr std::uint32_t kBTC_VAULT_CSV_BLOCKS = 6;

// Auto-scale fee: base fee units * (1 + nHeaders) — applied as multiplier on reference fee
// Implementation: calculateBaseFee returns view.fees().base * max(1, n)

// FBTC MPT metadata disclaimer (ASCII)
inline constexpr char const* kBTC_FBTC_METADATA_DISCLAIMER =
    "FBTC-SPV-ATTESTATION-NONREDEEMABLE-V1";

inline constexpr std::uint32_t
btcMinConfFloor(std::uint32_t chainId) noexcept
{
    switch (chainId)
    {
        case kBTC_CHAIN_REGTEST:
            return kBTC_MIN_CONF_REGTEST;
        case kBTC_CHAIN_TESTNET3:
        case kBTC_CHAIN_SIGNET:
            return kBTC_MIN_CONF_TESTNET;
        case kBTC_CHAIN_MAINNET:
        default:
            return kBTC_MIN_CONF_MAINNET;
    }
}

inline constexpr std::uint64_t
btcDustFloor(std::uint32_t chainId) noexcept
{
    return (chainId == kBTC_CHAIN_MAINNET) ? kBTC_DUST_MAINNET_DESIGN : kBTC_DUST_REGTEST;
}

}  // namespace xrpl
