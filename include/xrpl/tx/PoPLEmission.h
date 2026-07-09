// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/protocol/QXRPConstants.h>

#include <algorithm>
#include <cstdint>

namespace xrpl {

/** Continuous Inflationary Decline (CID) emission rate for a 1-based epoch. */
[[nodiscard]] inline std::uint32_t
cidEmissionBps(std::uint32_t epochNum) noexcept
{
    if (epochNum == 0)
        return kQXRP_CID_START_BPS;

    auto const decline = kQXRP_CID_STEP_BPS * (epochNum - 1);
    if (decline >= kQXRP_CID_START_BPS - kQXRP_CID_FLOOR_BPS)
        return kQXRP_CID_FLOOR_BPS;

    return kQXRP_CID_START_BPS - decline;
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