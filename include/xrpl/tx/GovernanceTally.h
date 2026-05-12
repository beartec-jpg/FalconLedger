// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Phase 8 – Governance Tally
//
// applyGovernanceTally() iterates all open governance proposals that have
// expired (sfProposalExpiry <= seq).  For each, it computes whether the YES
// weight achieves a 67 % supermajority of sfAggregateCompositeScore.  If so,
// the proposal is applied to ltGOVERNANCE_PARAMS.

#pragma once

#include <xrpl/beast/utility/Journal.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/Rules.h>

#include <vector>

namespace xrpl {

/** Tally expired governance proposals and apply passed ones to ltGOVERNANCE_PARAMS.
 *
 *  Called from buildLedgerImpl after applyValidatorScoring() so that
 *  sfAggregateCompositeScore already reflects this epoch's scoring.
 *
 *  @param view    Open accumulation view.
 *  @param seq     Sequence of the ledger being built.
 *  @param rules   Active amendment rules.
 *  @param j       Journal.
 *  @param openProposalKeys  Keys of open ltGOVERNANCE_PROPOSAL objects that
 *                           may have expired.  Populated by callers that track
 *                           proposals on-chain.  If empty this function is a no-op
 *                           (scanning the whole state map is too expensive here).
 */
void
applyGovernanceTally(
    OpenView& view,
    LedgerIndex seq,
    Rules const& rules,
    beast::Journal j,
    std::vector<uint256> const& openProposalKeys);

}  // namespace xrpl
