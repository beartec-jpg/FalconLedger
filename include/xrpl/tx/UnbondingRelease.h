// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Unbonding lock expiry processing for the ProofOfParticipation protocol.

#pragma once

#include <xrpl/beast/utility/Journal.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Protocol.h>

namespace xrpl {

class OpenView;
class Rules;

/// Release fully-elapsed unbonding locks at ledger close.
///
/// Called once per ledger during ledger build (after transactions are
/// applied).  When ProofOfParticipation is active, iterates all
/// ltVALIDATOR_BOND objects in kBOND_STATUS_UNBONDING state whose lock
/// period has elapsed:
///
///   currentLedger - sfUnbondingStartLedger >= kUNBONDING_LOCK_LEDGERS
///
/// For each such bond the function:
///   1. Returns the remaining bonded amount to the validator's account.
///   2. Erases the ltVALIDATOR_BOND object (removes from owner directory).
///
/// The function is a no-op when the amendment is inactive.  Errors are
/// logged and silently swallowed so ledger construction is never aborted.
void
applyUnbondingRelease(
    OpenView& view,
    LedgerIndex seq,
    Rules const& rules,
    beast::Journal j);

}  // namespace xrpl
