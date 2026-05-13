// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <xrpl/tx/Transactor.h>

namespace xrpl {

/** Phase 8 – GovernanceProposal
 *
 *  A bonded validator submits a proposal to change one of the mutable
 *  on-chain governance parameters (currently only sfCurrentBurnBps).
 *  The proposal lives as an ltGOVERNANCE_PROPOSAL ledger object for
 *  kGOVERNANCE_VOTING_LEDGERS and is tallied at epoch close.
 */
class GovernanceProposal : public Transactor
{
public:
    static constexpr auto kCONSEQUENCES_FACTORY = ConsequencesFactoryType::Blocker;

    explicit GovernanceProposal(ApplyContext& ctx) : Transactor(ctx) {}

    static NotTEC
    preflight(PreflightContext const& ctx);

    static TER
    preclaim(PreclaimContext const& ctx);

    TER
    doApply() override;

    void
    visitInvariantEntry(
        bool isDelete,
        std::shared_ptr<SLE const> const& before,
        std::shared_ptr<SLE const> const& after) override;

    [[nodiscard]] bool
    finalizeInvariants(
        STTx const& tx,
        TER result,
        XRPAmount fee,
        ReadView const& view,
        beast::Journal const& j) override;
};

}  // namespace xrpl
