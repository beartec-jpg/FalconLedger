// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Phase 7 – Validator Reputation Scoring
//
// applyValidatorScoring() runs every flag interval (seq % kFLAG_LEDGER_INTERVAL == 0).
// It measures a 256-ledger window from RCLValidations, then for each *bonded*
// validator (ltVALIDATOR_BOND + sfConsensusKey), not only the UNL:
//   1. Maps bond ConsensusKey → NodeID
//   2. Counts full validations (trusted or untrusted) for uptime / accuracy
//   3. Computes *independent* continuous signals:
//        uptime (presence), vote accuracy (correct/cast), latency (vs earliest),
//        consistency (max absence streak)
//   4. rawScore × slashMultiplier, then EMA-blend with previous composite
//   5. Writes scoring fields to ltVALIDATOR_BOND (no ActiveSet rank cut)
//   6. Sets sfAggregateCompositeScore = sum of all composites (pay ∝ score)
// UNL remains consensus trust only; open/rotating UNL is a future amendment.

#pragma once

#include <xrpl/beast/utility/Journal.h>
#include <xrpl/ledger/Ledger.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/Rules.h>

#include <memory>

namespace xrpl {

class Application;

/** Apply validator reputation scoring at an epoch boundary.

    Must be called during ledger build *after* applyRewardEpoch() so that the
    ltREWARD_EPOCH object already exists for the sfAggregateCompositeScore field.

    @param view   The open accumulation view, already positioned on the new ledger.
    @param parent The parent (previous) closed ledger.  Used to retrieve the
                  skip-list for the 256-ancestor hashing window.
    @param seq    Sequence number of the ledger being built.
    @param rules  Active amendment rules.
    @param app    Application context (provides getValidations() / getValidators()).
    @param j      Journal for diagnostic logging.
*/
void
applyValidatorScoring(
    OpenView& view,
    std::shared_ptr<Ledger const> const& parent,
    LedgerIndex seq,
    Rules const& rules,
    Application& app,
    beast::Journal j);

}  // namespace xrpl
