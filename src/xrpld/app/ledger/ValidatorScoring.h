// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Phase 7 – Validator Reputation Scoring
//
// applyValidatorScoring() runs once per epoch boundary (seq % kQXRP_LEDGERS_PER_EPOCH == 0).
// It builds a 256-ledger validation-count table from RCLValidations, then for each trusted
// UNL key it:
//   1. Derives AccountID = calcAccountID(pubKey)
//   2. Looks up ltVALIDATOR_BOND for that account
//   3. Computes signal BPS values (uptime, vote accuracy, latency, consistency)
//   4. Multiplies the raw weighted score by sfSlashMultiplier / kBPS_DENOM
//   5. Writes updated scoring fields back to ltVALIDATOR_BOND
//   6. Accumulates sfAggregateCompositeScore into ltREWARD_EPOCH

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
