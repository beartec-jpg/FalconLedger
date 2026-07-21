// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Fuzz target: epoch claim hard-cap (C-02) — pay never exceeds remaining pool.
//
// Build:
//   clang++ -std=c++20 -fsanitize=fuzzer,address,undefined \
//     FuzzEpochPoolCap.cpp -o fuzz_epoch_pool_cap

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

static std::uint64_t
muldivU64(std::uint64_t a, std::uint64_t b, std::uint64_t d)
{
    if (d == 0 || a == 0 || b == 0)
        return 0;
    return static_cast<std::uint64_t>(
        (static_cast<unsigned __int128>(a) * static_cast<unsigned __int128>(b)) /
        static_cast<unsigned __int128>(d));
}

/** Mirrors claim path: computed share then min(share, remaining pool). */
static std::uint64_t
cappedPay(
    std::uint64_t emission,
    std::uint64_t allocBps,
    std::uint64_t userShare,
    std::uint64_t denom,
    std::uint64_t poolRemaining)
{
    constexpr std::uint64_t kBPS = 10'000;
    if (denom == 0 || poolRemaining == 0)
        return 0;
    auto const basket = muldivU64(emission, allocBps, kBPS);
    auto share = muldivU64(basket, userShare, denom);
    if (share > poolRemaining)
        share = poolRemaining;
    return share;
}

extern "C" int
LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    if (size < 40)
        return 0;

    std::uint64_t emission{}, user{}, denom{}, pool{}, alloc{};
    std::memcpy(&emission, data, 8);
    std::memcpy(&user, data + 8, 8);
    std::memcpy(&denom, data + 16, 8);
    std::memcpy(&pool, data + 24, 8);
    std::memcpy(&alloc, data + 32, 8);

    // Keep inputs in plausible ranges.
    emission %= 196'000'000'000ULL * 1'000'000ULL + 1;
    alloc %= 10'001;
    if (denom == 0)
        denom = 1;
    if (user > denom * 2)
        user = denom;  // can exceed snapshot; live denom path still caps

    auto const pay = cappedPay(emission, alloc, user, denom, pool);

    // Invariant: never exceed remaining pool.
    assert(pay <= pool);

    // Invariant: never exceed emission (when alloc <= 100%).
    if (alloc <= 10'000)
        assert(pay <= emission || emission == 0);

    // Sequential claims against shrinking pool never overdraw.
    std::uint64_t remaining = pool;
    for (int i = 0; i < 8; ++i)
    {
        auto const p = cappedPay(emission, alloc, user, denom, remaining);
        assert(p <= remaining);
        remaining -= p;
    }

    return 0;
}
