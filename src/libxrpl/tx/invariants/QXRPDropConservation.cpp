// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// QXRPDropConservation — verify that no transaction creates drops from thin air.
//
// The invariant mirrors XRPNotCreated but provides qXRP-specific context in the
// failure message and can be extended to cover qXRP-specific ledger objects
// (ltVALIDATOR_BOND, ltREWARD_EPOCH) as they are added.

#include <xrpl/tx/invariants/InvariantCheck.h>

#include <xrpl/basics/Log.h>
#include <xrpl/beast/utility/Journal.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/SystemParameters.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/XRPAmount.h>

namespace xrpl {

void
QXRPDropConservation::visitEntry(
    bool isDelete,
    std::shared_ptr<SLE const> const& before,
    std::shared_ptr<SLE const> const& after)
{
    // Track the net change in drops across the ledger object types that can
    // hold XRP.  The same accounting used by XRPNotCreated applies here.
    if (before)
    {
        switch (before->getType())
        {
            case ltACCOUNT_ROOT:
                drops_ -= (*before)[sfBalance].xrp().drops();
                break;
            case ltPAYCHAN:
                drops_ -=
                    ((*before)[sfAmount] - (*before)[sfBalance]).xrp().drops();
                break;
            case ltESCROW:
                if (isXRP((*before)[sfAmount]))
                    drops_ -= (*before)[sfAmount].xrp().drops();
                break;
            case ltVALIDATOR_BOND:
                // Track locked bond drops so ValidatorBond/ReleaseBond satisfy
                // the conservation check: account_delta + bond_delta == -fee.
                drops_ -= (*before)[sfBondedAmount].xrp().drops();
                break;
            default:
                break;
        }
    }

    if (after)
    {
        switch (after->getType())
        {
            case ltACCOUNT_ROOT:
                drops_ += (*after)[sfBalance].xrp().drops();
                break;
            case ltPAYCHAN:
                if (!isDelete)
                    drops_ +=
                        ((*after)[sfAmount] - (*after)[sfBalance]).xrp().drops();
                break;
            case ltESCROW:
                if (!isDelete && isXRP((*after)[sfAmount]))
                    drops_ += (*after)[sfAmount].xrp().drops();
                break;
            case ltVALIDATOR_BOND:
                if (!isDelete)
                    drops_ += (*after)[sfBondedAmount].xrp().drops();
                break;
            default:
                break;
        }
    }
}

bool
QXRPDropConservation::finalize(
    STTx const& tx,
    TER const,
    XRPAmount const fee,
    ReadView const&,
    beast::Journal const& j) const
{
    // The net change in drops must be non-positive: drops can only be
    // destroyed (burned as fee) or redistributed, never created from nothing.
    if (drops_ > 0)
    {
        JLOG(j.fatal())
            << "qXRP invariant failed: transaction created "
            << drops_ << " drops out of thin air"
            << " (max supply is " << kINITIAL_XRP.drops() << " drops)";
        return false;
    }

    // The magnitude of the net decrease must equal the fee actually charged.
    // If they differ the transaction consumed or produced drops without a
    // corresponding fee debit.
    //
    // Exception: ValidatorSlash intentionally burns BondedAmount (rawDestroyXRP)
    // beyond the tx fee. visitEntry sees bond decrease + fee debit, so
    // -drops_ == fee + slashed. Require at least the fee and never creation.
    if (tx.getTxnType() == ttVALIDATOR_SLASH)
    {
        if (-drops_ < fee.drops())
        {
            JLOG(j.fatal())
                << "qXRP invariant failed: ValidatorSlash net drop change "
                << drops_ << " is less than fee " << fee.drops();
            return false;
        }
        return true;
    }

    if (-drops_ != fee.drops())
    {
        JLOG(j.fatal())
            << "qXRP invariant failed: net drop change " << drops_
            << " does not match fee " << fee.drops()
            << " — possible treasury leak or double-debit";
        return false;
    }

    return true;
}

}  // namespace xrpl
