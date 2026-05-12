// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/tx/Transactor.h>

namespace xrpl {

/// Release the bond of a validator whose unbonding lock has expired.
///
/// Analogous to EscrowFinish: the validator (or anyone) submits this
/// transaction after `kUNBONDING_LOCK_LEDGERS` ledgers have elapsed since
/// unbonding began.  On success:
///   - The remaining bonded drops are returned to the validator's account.
///   - The ltVALIDATOR_BOND object is removed from the ledger.
///   - The validator's ownerCount is decremented.
class ReleaseBond : public Transactor
{
public:
    static constexpr ConsequencesFactoryType ConsequencesFactory{Normal};

    explicit ReleaseBond(ApplyContext& ctx) : Transactor(ctx) {}

    static NotTEC
    preflight(PreflightContext const& ctx);

    static TER
    preclaim(PreclaimContext const& ctx);

    TER
    doApply() override;
};

}  // namespace xrpl
