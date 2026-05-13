// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Portable wide-integer arithmetic helpers.
//
// Provides muldiv64(a, b, d) — an overflow-safe integer computation of
// (a * b) / d for cases where the intermediate product a * b may exceed the
// range of std::int64_t.
//
// Preconditions:
//   0 <= b <= d          (the result fits in the same range as a)
//   d != 0
//   a, b, d all non-negative
//
// The implementation uses only 64-bit arithmetic and truncates the fractional
// remainder (i.e., floor division), which is correct for discrete drop
// accounting on the ledger.

#pragma once

#include <cstdint>

namespace xrpl {

/// Compute (a * b) / d without overflow.
///
/// Algorithm: express a = q*d + r, then
///   (a * b) / d  =  q*b  +  r*b/d
///
/// Safety:
///   q*b  : q = a/d, and since b <= d we have q*b <= q*d <= a — no overflow.
///   r*b  : r = a%d < d, b fits in uint32, r < d fits in uint32 → r*b < d*d.
///          With d <= kBPS_DENOM (10 000) that is at most 10^8, safe in int64.
///
/// @param a  Multiplicand (int64_t, non-negative).
/// @param b  Multiplier   (uint32_t; must satisfy b <= d).
/// @param d  Divisor      (uint32_t, non-zero).
/// @return   floor(a * b / d) as int64_t.
[[nodiscard]] inline std::int64_t
muldiv64(std::int64_t a, std::uint32_t b, std::uint32_t d) noexcept
{
    auto const q = a / static_cast<std::int64_t>(d);
    auto const r = a % static_cast<std::int64_t>(d);
    return q * static_cast<std::int64_t>(b) +
           r * static_cast<std::int64_t>(b) / static_cast<std::int64_t>(d);
}

}  // namespace xrpl
