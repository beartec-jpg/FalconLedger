// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/protocol/QXRPConstants.h>

#include <algorithm>
#include <cstdint>

namespace xrpl {

/** Yearly-average CID emission target (bps of treasury per year) for a 1-based epoch. */
[[nodiscard]] inline std::uint32_t
cidYearlyAvgBps(std::uint32_t epochNum) noexcept
{
    if (epochNum == 0)
        return kQXRP_CID_YEARLY_START_BPS;

    auto const year = (epochNum - 1) / kQXRP_EPOCHS_PER_YEAR;
    auto const decline = kQXRP_CID_YEARLY_STEP_BPS * year;
    if (decline >= kQXRP_CID_YEARLY_START_BPS - kQXRP_CID_YEARLY_FLOOR_BPS)
        return kQXRP_CID_YEARLY_FLOOR_BPS;

    return kQXRP_CID_YEARLY_START_BPS - decline;
}

/** Per-epoch CID emission rate (bps of treasury) for a 1-based epoch.

    The yearly-average target is spread across kQXRP_EPOCHS_PER_YEAR epochs with
    linearly declining weights (52, 51, …, 1) so each epoch emits slightly less
    than the previous one while the calendar-year sum equals the yearly average. */
[[nodiscard]] inline std::uint32_t
cidEmissionBps(std::uint32_t epochNum) noexcept
{
    if (epochNum == 0)
        epochNum = 1;

    auto const slot = (epochNum - 1) % kQXRP_EPOCHS_PER_YEAR;
    auto const weight = kQXRP_EPOCHS_PER_YEAR - slot;
    auto const yearlyAvg = cidYearlyAvgBps(epochNum);

    // Round to nearest bps so the final epoch of each year is never zeroed by truncation.
    return (yearlyAvg * weight + kQXRP_CID_YEAR_WEIGHT_SUM / 2) /
        kQXRP_CID_YEAR_WEIGHT_SUM;
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