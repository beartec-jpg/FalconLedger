// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/protocol/QXRPConstants.h>

#include <algorithm>
#include <cstdint>

namespace xrpl {

namespace detail {

[[nodiscard]] inline std::uint32_t
cidEpochBpsNumerator(std::uint32_t epochNum) noexcept
{
    return kQXRP_CID_YEAR1_AVG_BPS * kQXRP_CID_DECLINE_DEN +
        1'326 * kQXRP_CID_DECLINE_NUM -
        kQXRP_CID_DECLINE_NUM * (epochNum - 1) * kQXRP_EPOCHS_PER_YEAR;
}

[[nodiscard]] constexpr std::uint32_t
cidEpochBpsDenominator() noexcept
{
    return kQXRP_EPOCHS_PER_YEAR * kQXRP_CID_DECLINE_DEN;
}

}  // namespace detail

/** Yearly-average CID emission (bps of treasury per calendar year) for a 1-based epoch. */
[[nodiscard]] inline std::uint32_t
cidYearlyAvgBps(std::uint32_t epochNum) noexcept
{
    if (epochNum == 0)
        return kQXRP_CID_YEAR1_AVG_BPS;

    auto const year0 = (epochNum - 1) / kQXRP_EPOCHS_PER_YEAR;
    auto const denom = detail::cidEpochBpsDenominator();
    auto const numer =
        kQXRP_EPOCHS_PER_YEAR *
            (kQXRP_CID_YEAR1_AVG_BPS * kQXRP_CID_DECLINE_DEN +
             1'326 * kQXRP_CID_DECLINE_NUM) -
        kQXRP_EPOCHS_PER_YEAR * kQXRP_CID_DECLINE_NUM *
            (kQXRP_EPOCHS_PER_YEAR * kQXRP_EPOCHS_PER_YEAR * year0 + 1'326);

    auto const yearlyBps = (numer + denom / 2) / denom;
    return std::max(kQXRP_CID_YEARLY_FLOOR_BPS, yearlyBps);
}

/** Per-epoch CID emission rate (bps of treasury) for a 1-based epoch.

    Declines linearly every epoch (no year-end reset) from ~25 bps at epoch 1
    toward the 1.5 % yearly floor reached around year 7. */
[[nodiscard]] inline std::uint32_t
cidEmissionBps(std::uint32_t epochNum) noexcept
{
    if (epochNum == 0)
        epochNum = 1;

    auto const denom = detail::cidEpochBpsDenominator();
    auto const numer = detail::cidEpochBpsNumerator(epochNum);
    auto const rounded = (numer + denom / 2) / denom;

    return std::max(kQXRP_CID_EPOCH_FLOOR_BPS, rounded);
}

/** PoPL LP basket as bps of total epoch emission from active provider count.

    Each distinct vault depositor (non-zero share MPT) adds 1 % (100 bps) to the
    LP basket, capped at 50 providers → 50 %. Validators receive the remainder. */
[[nodiscard]] inline std::uint32_t
poplLpParticipationBps(std::uint32_t providerCount) noexcept
{
    if (providerCount == 0)
        return 0;

    auto const capped = std::min(providerCount, kQXRP_POPL_LP_MAX_PROVIDERS);
    auto const bps = capped * kQXRP_POPL_LP_BPS_PER_PROVIDER;
    return std::min(bps, kQXRP_POPL_LP_MAX_BPS);
}

/** Validator allocation as bps of total emission (complement of LP share). */
[[nodiscard]] inline std::uint32_t
poplValidatorParticipationBps(std::uint32_t providerCount) noexcept
{
    return kBPS_DENOM - poplLpParticipationBps(providerCount);
}

}  // namespace xrpl