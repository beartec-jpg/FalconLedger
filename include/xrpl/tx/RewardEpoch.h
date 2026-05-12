// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Epoch boundary processing for the ProofOfParticipation protocol feature.

#pragma once

#include <xrpl/beast/utility/Journal.h>
#include <xrpl/protocol/Protocol.h>

namespace xrpl {

class OpenView;
class Rules;

/// Process an epoch boundary in the given open view.
///
/// Called unconditionally once per ledger during ledger build, immediately
/// after all transactions in the set have been applied to @p view.
///
/// When the ProofOfParticipation amendment is active AND @p seq is an
/// epoch-boundary ledger (seq % kQXRP_LEDGERS_PER_EPOCH == 0), the function
/// creates (or replaces) the singleton ltREWARD_EPOCH object with the
/// parameters for the next epoch:
///   - emission rate (halved every kQXRP_EPOCHS_PER_HALVING epochs)
///   - epoch pool balance drawn from the treasury
///   - dynamic burn fraction based on treasury fill pressure
///
/// The function is a no-op when the amendment is inactive or when the ledger
/// is not on an epoch boundary.  It never fails — any error is logged and
/// silently ignored so ledger construction can continue.
void
applyRewardEpoch(
    OpenView& view,
    LedgerIndex seq,
    Rules const& rules,
    beast::Journal j);

}  // namespace xrpl
