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

/** PoPL LP allocation as a fraction of total epoch emission, in bps.
    Tapers linearly from 50 % at epoch 1 to 30 % from epoch 24 onward. */
[[nodiscard]] inline std::uint32_t
poplLpAllocationBps(std::uint32_t epochNum) noexcept
{
    if (epochNum <= 1)
        return kQXRP_POPL_LP_START_BPS;

    if (epochNum >= kQXRP_POPL_TAPER_EPOCHS)
        return kQXRP_POPL_LP_END_BPS;

    auto const taper =
        (kQXRP_POPL_LP_START_BPS - kQXRP_POPL_LP_END_BPS) * (epochNum - 1) /
        (kQXRP_POPL_TAPER_EPOCHS - 1);

    return kQXRP_POPL_LP_START_BPS - taper;
}

/** Validator allocation as bps of total emission (complement of LP share). */
[[nodiscard]] inline std::uint32_t
poplValidatorAllocationBps(std::uint32_t epochNum) noexcept
{
    return kBPS_DENOM - poplLpAllocationBps(epochNum);
}

}  // namespace xrpl