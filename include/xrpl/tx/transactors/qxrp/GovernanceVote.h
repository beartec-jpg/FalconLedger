// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/tx/Transactor.h>

namespace xrpl {

/** Phase 8 – GovernanceVote
 *
 *  A bonded validator casts a weighted vote on an open governance proposal.
 *  The voter's sfCompositeScore is added to sfVotedFor (sfVoteWeight == 1)
 *  or sfVotedAgainst (sfVoteWeight == 0).  Each account may only vote once
 *  per proposal; a second attempt returns tecDUPLICATE.
 *
 *  Vote tallying and parameter application happen exclusively at the epoch
 *  boundary via applyGovernanceTally() (called from BuildLedger).
 */
class GovernanceVote : public Transactor
{
public:
    static constexpr ConsequencesFactoryType ConsequencesFactory{Blocker};

    explicit GovernanceVote(ApplyContext& ctx) : Transactor(ctx) {}

    static NotTEC
    preflight(PreflightContext const& ctx);

    static TER
    preclaim(PreclaimContext const& ctx);

    TER
    doApply() override;
};

}  // namespace xrpl
