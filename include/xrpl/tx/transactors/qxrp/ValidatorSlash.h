// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/tx/Transactor.h>

namespace xrpl {

class ValidatorSlash : public Transactor
{
public:
    static constexpr ConsequencesFactoryType ConsequencesFactory{Blocker};

    explicit ValidatorSlash(ApplyContext& ctx) : Transactor(ctx) {}

    static NotTEC
    preflight(PreflightContext const& ctx);

    static TER
    preclaim(PreclaimContext const& ctx);

    TER
    doApply() override;
};

}  // namespace xrpl
