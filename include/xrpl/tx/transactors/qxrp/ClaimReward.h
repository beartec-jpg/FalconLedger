// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <xrpl/tx/Transactor.h>

namespace xrpl {

/// Claim accumulated reward drops from the treasury for the current epoch.
/// Requires: bonded status + composite score above kMIN_COMPOSITE_SCORE_BPS.
class ClaimReward : public Transactor
{
public:
    static constexpr auto kCONSEQUENCES_FACTORY = ConsequencesFactoryType::Normal;

    explicit ClaimReward(ApplyContext& ctx) : Transactor(ctx)
    {
    }

    static NotTEC
    preflight(PreflightContext const& ctx);

    static TER
    preclaim(PreclaimContext const& ctx);

    TER
    doApply() override;
};

}  // namespace xrpl
