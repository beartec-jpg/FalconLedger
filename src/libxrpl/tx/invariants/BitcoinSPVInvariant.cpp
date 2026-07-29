// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/invariants/BitcoinSPVInvariant.h>

#include <xrpl/basics/Log.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/TxFormats.h>

namespace xrpl {

void
ValidBitcoinSPV::visitEntry(
    bool isDelete,
    std::shared_ptr<SLE const> const& before,
    std::shared_ptr<SLE const> const& after)
{
    if (!before && !after)
        return;

    auto const type = after ? after->getType() : before->getType();

    if (type == ltBTC_DEPOSIT)
    {
        if (!before && after && !isDelete)
        {
            createdDeposit_ = true;
            depositAmount_ = after->getFieldU64(sfBtcAmount);
        }
        if (before && (isDelete || !after))
            deletedDeposit_ = true;
    }
    else if (type == ltBTC_BRIDGE_STATE)
    {
        auto const beforeTotal = before ? before->getFieldU64(sfBtcTotalMinted) : 0ull;
        auto const afterTotal =
            (after && !isDelete) ? after->getFieldU64(sfBtcTotalMinted) : 0ull;
        totalMintedDelta_ +=
            static_cast<std::int64_t>(afterTotal) - static_cast<std::int64_t>(beforeTotal);
    }
    else if (type == ltBTC_WITHDRAWAL)
    {
        if (!before && after && !isDelete)
            createdWithdraw_ = true;
    }
}

bool
ValidBitcoinSPV::finalize(
    STTx const& tx,
    TER const result,
    XRPAmount const,
    ReadView const&,
    beast::Journal const& j) const
{
    if (deletedDeposit_)
    {
        JLOG(j.fatal()) << "Invariant failed: ltBTC_DEPOSIT deleted";
        return false;
    }

    if (!isTesSuccess(result))
    {
        if (createdDeposit_ || totalMintedDelta_ != 0)
        {
            JLOG(j.fatal()) << "Invariant failed: BTC SPV state changed on failed tx";
            return false;
        }
        return true;
    }

    if (tx.getTxnType() == ttBTC_DEPOSIT_CLAIM)
    {
        if (!createdDeposit_)
        {
            JLOG(j.fatal()) << "Invariant failed: claim succeeded without deposit SLE";
            return false;
        }
        if (totalMintedDelta_ != static_cast<std::int64_t>(depositAmount_))
        {
            JLOG(j.fatal()) << "Invariant failed: sfBtcTotalMinted delta != deposit amount";
            return false;
        }
    }
    else if (tx.getTxnType() == ttBTC_BRIDGE_BURN)
    {
        if (!createdWithdraw_)
        {
            JLOG(j.fatal()) << "Invariant failed: burn succeeded without withdraw SLE";
            return false;
        }
        // total minted must decrease (negative delta)
        if (totalMintedDelta_ >= 0)
        {
            JLOG(j.fatal()) << "Invariant failed: burn did not decrease sfBtcTotalMinted";
            return false;
        }
    }
    else if (createdDeposit_)
    {
        JLOG(j.fatal()) << "Invariant failed: deposit SLE created outside claim";
        return false;
    }
    else if (createdWithdraw_ && tx.getTxnType() != ttBTC_BRIDGE_BURN)
    {
        JLOG(j.fatal()) << "Invariant failed: withdraw SLE outside burn";
        return false;
    }
    else if (
        totalMintedDelta_ != 0 && tx.getTxnType() != ttBTC_BRIDGE_ACTIVATE &&
        tx.getTxnType() != ttBTC_BRIDGE_BURN)
    {
        JLOG(j.fatal()) << "Invariant failed: unexpected sfBtcTotalMinted change";
        return false;
    }

    return true;
}

}  // namespace xrpl
