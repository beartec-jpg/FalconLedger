// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Fuzz target: ClaimReward share computation (muldiv64 proportional formula).
//
// Self-contained — inlines the share formula directly.
// No libxrpl linkage required.
//
// Build (clang with libFuzzer):
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     FuzzClaimReward.cpp -o fuzz_claim_reward
//
// Run:
//   mkdir -p corpus/claim
//   ./fuzz_claim_reward corpus/claim/ -max_len=64 -timeout=30
//
// Invariants verified:
//   1. shareDrops is always in [0, emissionDrops] (can never overpay).
//   2. When compositeScore == aggregateScore (sole validator), share == emission.
//   3. Sum of all validator shares never exceeds emissionDrops.
//   4. No division-by-zero (guarded by aggregateScore == 0 check).
//   5. No overflow under UBSAN for any 64-byte input.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// ── muldiv64: (a * b) / c using __int128 ────────────────────────────────────
// Mirrors xrpl::muldiv64 (WideArith.h).

static std::int64_t
muldiv64(std::int64_t a, std::uint32_t b, std::uint32_t c)
{
    if (c == 0)
        return 0;  // caller must guard; replicated here for fuzz safety
    return static_cast<std::int64_t>(
        (static_cast<__int128>(a) * b) / c);
}

// ── libFuzzer entry ──────────────────────────────────────────────────────────

extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    // Layout: 8B emissionDrops + 4B numValidators (1-16) + N×8B scores.
    if (size < 12)
        return 0;

    std::int64_t emissionDrops{};
    std::memcpy(&emissionDrops, data, 8);

    // Only positive emission makes sense; clamp to [0, 196e18 drops].
    if (emissionDrops < 0)
        emissionDrops = -emissionDrops;
    constexpr std::int64_t kMaxTreasuryDrops =
        static_cast<std::int64_t>(196'000'000'000LL) * 1'000'000LL;
    if (emissionDrops > kMaxTreasuryDrops)
        emissionDrops = kMaxTreasuryDrops;

    std::uint32_t numValidators{};
    std::memcpy(&numValidators, data + 8, 4);
    numValidators = (numValidators % 16) + 1;  // clamp to [1, 16]

    // Read up to numValidators composite scores from remaining bytes.
    // Each score is a uint32 in [0, 10000].
    constexpr std::uint32_t kBPS_DENOM = 10'000;
    std::vector<std::uint32_t> scores(numValidators, 0);
    for (std::uint32_t i = 0; i < numValidators; ++i)
    {
        std::size_t offset = 12 + i * 4;
        if (offset + 4 <= size)
            std::memcpy(&scores[i], data + offset, 4);
        scores[i] = scores[i] % (kBPS_DENOM + 1);  // clamp to [0, 10000]
    }

    // Compute aggregate (with saturation, mirrors ValidatorScoring.cpp).
    std::uint32_t aggregateScore = 0;
    for (auto s : scores)
    {
        if (s <= static_cast<std::uint32_t>(0xFFFFFFFFu) - aggregateScore)
            aggregateScore += s;
        else
            aggregateScore = 0xFFFFFFFFu;
    }

    // Guard: if no eligible validators, nothing to distribute.
    if (aggregateScore == 0)
        return 0;

    // Compute each validator's share and sum them.
    std::int64_t totalDistributed = 0;
    for (auto s : scores)
    {
        auto const shareDrops = muldiv64(emissionDrops, s, aggregateScore);

        // Invariant 1: each share is non-negative and ≤ total emission.
        assert(shareDrops >= 0);
        assert(shareDrops <= emissionDrops);

        totalDistributed += shareDrops;
    }

    // Invariant 2: sum of all shares never exceeds emission (integer rounding
    // may mean it's slightly less, never more).
    assert(totalDistributed >= 0);
    assert(totalDistributed <= emissionDrops);

    // Invariant 3: sole-validator edge case — one validator with all the score
    // gets the entire emission (allowing for integer rounding down by at most 1).
    if (numValidators == 1 && scores[0] == aggregateScore && emissionDrops > 0)
    {
        auto const share = muldiv64(emissionDrops, scores[0], aggregateScore);
        assert(share == emissionDrops);  // compositeScore/aggregateScore == 1 exactly
    }

    return 0;
}
