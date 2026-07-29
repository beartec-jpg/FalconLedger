// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/beast/utility/Journal.h>
#include <xrpl/ledger/ReadView.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/XRPAmount.h>

#include <cstdint>
#include <memory>

namespace xrpl {

/** Cross-check FBTC mint path: deposits created only on success; total minted
    moves only with deposit create. Complements ValidMPTPayment. */
class ValidBitcoinSPV
{
    bool createdDeposit_ = false;
    bool deletedDeposit_ = false;
    bool createdWithdraw_ = false;
    std::int64_t totalMintedDelta_ = 0;
    std::uint64_t depositAmount_ = 0;

public:
    void
    visitEntry(
        bool isDelete,
        std::shared_ptr<SLE const> const& before,
        std::shared_ptr<SLE const> const& after);

    bool
    finalize(
        STTx const& tx,
        TER const result,
        XRPAmount const fee,
        ReadView const& view,
        beast::Journal const& j) const;
};

}  // namespace xrpl
