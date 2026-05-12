// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Fuzz target: fee-split computation (burnBps formula).
//
// Build:
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     -I <repo>/include \
//     FuzzFeeSplit.cpp \
//     -o fuzz_fee_split \
//     -L<build>/lib -lxrpl
//
// Run:
//   ./fuzz_fee_split corpus/feesplit/ -max_len=64 -timeout=10
//
// This target exercises the burnBps clamping formula with arbitrary 64-bit
// inputs to catch any overflow or out-of-bounds behaviour.

#include <xrpl/protocol/QXRPConstants.h>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>

// Inline replication of the computeBurnBps logic so we can fuzz it
// independently of the full Apply stack.
static std::uint32_t
computeBurnBps(
    std::int64_t treasuryBalance,
    std::int64_t totalSupply,
    std::uint32_t feeVolumeEMA_bps)
{
    using namespace xrpl;

    if (totalSupply <= 0)
        return kFEE_BURN_DEFAULT_BPS;

    std::int64_t treasuryPressure =
        (treasuryBalance * static_cast<std::int64_t>(kBPS_DENOM)) / totalSupply;

    std::int64_t raw = static_cast<std::int64_t>(kFEE_BURN_DEFAULT_BPS)
        + (treasuryPressure *
               static_cast<std::int64_t>(kFEE_TREASURY_SENSITIVITY_BPS) /
               static_cast<std::int64_t>(kBPS_DENOM))
        - (static_cast<std::int64_t>(feeVolumeEMA_bps) *
               static_cast<std::int64_t>(kFEE_USAGE_SENSITIVITY_BPS) /
               static_cast<std::int64_t>(kBPS_DENOM));

    if (raw < static_cast<std::int64_t>(kFEE_BURN_MIN_BPS))
        raw = kFEE_BURN_MIN_BPS;
    if (raw > static_cast<std::int64_t>(kFEE_BURN_MAX_BPS))
        raw = kFEE_BURN_MAX_BPS;

    return static_cast<std::uint32_t>(raw);
}

extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    if (size < 20)
        return 0;

    std::int64_t treasury{};
    std::int64_t supply{};
    std::uint32_t ema{};

    std::memcpy(&treasury, data,      8);
    std::memcpy(&supply,   data + 8,  8);
    std::memcpy(&ema,      data + 16, 4);

    std::uint32_t bps = computeBurnBps(treasury, supply, ema);

    // Hard invariant: result must always be within the declared bounds.
    assert(bps >= xrpl::kFEE_BURN_MIN_BPS);
    assert(bps <= xrpl::kFEE_BURN_MAX_BPS);

    return 0;
}
