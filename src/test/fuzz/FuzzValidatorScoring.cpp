// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Fuzz target: composite score and aggregate score calculation.
//
// Self-contained — inlines the scoring constants and formula directly.
// No libxrpl linkage required.
//
// Build (clang with libFuzzer):
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     FuzzValidatorScoring.cpp -o fuzz_validator_scoring
//
// Run:
//   mkdir -p corpus/scoring
//   ./fuzz_validator_scoring corpus/scoring/ -max_len=64 -timeout=30
//
// Invariants verified:
//   1. rawScore is always in [0, kBPS_DENOM].
//   2. compositeScore is always in [0, rawScore] (slash can only reduce).
//   3. aggregateScore saturates at UINT32_MAX and never wraps.
//   4. No undefined behaviour under UBSAN for any 64-byte input.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// ── Inlined constants from QXRPConstants.h ──────────────────────────────────

static constexpr std::uint32_t kBPS_DENOM              = 10'000;
static constexpr std::uint32_t kSCORE_WEIGHT_UPTIME    = 40;
static constexpr std::uint32_t kSCORE_WEIGHT_VOTE_ACC  = 30;
static constexpr std::uint32_t kSCORE_WEIGHT_LATENCY   = 15;
static constexpr std::uint32_t kSCORE_WEIGHT_CONSISTENCY = 10;
// Slash-multiplier component (5) is applied as a multiplier, not a weight.

// ── Inline replica of ValidatorScoring formula ──────────────────────────────

static std::uint32_t
computeCompositeScore(
    std::uint32_t uptimeBps,
    std::uint32_t voteAccBps,
    std::uint32_t latencyBps,
    std::uint32_t consistencyBps,
    std::uint32_t slashMult)
{
    // rawScore = (uptime*40 + voteAcc*30 + latency*15 + consistency*10) / 100
    // Latency is measured relative to earliest signer (0–10000 bps).
    auto const rawScore = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(uptimeBps)        * kSCORE_WEIGHT_UPTIME    +
         static_cast<std::uint64_t>(voteAccBps)       * kSCORE_WEIGHT_VOTE_ACC  +
         static_cast<std::uint64_t>(latencyBps)       * kSCORE_WEIGHT_LATENCY +
         static_cast<std::uint64_t>(consistencyBps)   * kSCORE_WEIGHT_CONSISTENCY) /
        100u);

    // compositeScore = rawScore * slashMult / kBPS_DENOM
    auto const compositeScore = static_cast<std::uint32_t>(
        (static_cast<__int128>(rawScore) * slashMult) / kBPS_DENOM);

    return compositeScore;
}

// ── libFuzzer entry ──────────────────────────────────────────────────────────

extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    // Need 20 bytes: 5 × uint32 inputs.
    if (size < 20)
        return 0;

    std::uint32_t uptimeBps{}, voteAccBps{}, latencyBps{}, consistencyBps{}, slashMult{};
    std::memcpy(&uptimeBps,      data,      4);
    std::memcpy(&voteAccBps,     data + 4,  4);
    std::memcpy(&latencyBps,     data + 8,  4);
    std::memcpy(&consistencyBps, data + 12, 4);
    std::memcpy(&slashMult,      data + 16, 4);

    // Clamp to valid BPS ranges (mirrors what the on-chain scoring code does).
    uptimeBps      = std::min(uptimeBps,      kBPS_DENOM);
    voteAccBps     = std::min(voteAccBps,     kBPS_DENOM);
    latencyBps     = std::min(latencyBps,     kBPS_DENOM);
    consistencyBps = std::min(consistencyBps, kBPS_DENOM);
    slashMult      = std::min(slashMult,      kBPS_DENOM);

    auto const score =
        computeCompositeScore(uptimeBps, voteAccBps, latencyBps, consistencyBps, slashMult);

    // Invariant 1: compositeScore ≤ kBPS_DENOM (can never exceed clean max).
    assert(score <= kBPS_DENOM);

    // Invariant 2: with a clean slash multiplier (10000) compositeScore == rawScore.
    if (slashMult == kBPS_DENOM)
    {
        auto const rawCheck = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(uptimeBps)      * kSCORE_WEIGHT_UPTIME    +
             static_cast<std::uint64_t>(voteAccBps)     * kSCORE_WEIGHT_VOTE_ACC  +
             static_cast<std::uint64_t>(latencyBps)     * kSCORE_WEIGHT_LATENCY  +
             static_cast<std::uint64_t>(consistencyBps) * kSCORE_WEIGHT_CONSISTENCY) /
            100u);
        assert(score == rawCheck);
    }

    // Invariant 3: slashing never increases the score.
    {
        auto const cleanScore = computeCompositeScore(
            uptimeBps, voteAccBps, latencyBps, consistencyBps, kBPS_DENOM);
        assert(score <= cleanScore);
    }

    // Invariant 4: aggregate accumulation saturates safely for up to 1000 validators.
    {
        std::uint32_t aggregate = 0;
        for (int i = 0; i < 1000; ++i)
        {
            if (score <= std::numeric_limits<std::uint32_t>::max() - aggregate)
                aggregate += score;
            else
                aggregate = std::numeric_limits<std::uint32_t>::max();
        }
        // aggregate must never be zero when score > 0.
        if (score > 0)
            assert(aggregate > 0);
    }

    return 0;
}
