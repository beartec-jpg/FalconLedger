// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ValidatorUnbond.h>

#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/tx/ApplyContext.h>

namespace xrpl {

NotTEC
ValidatorUnbond::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    // sfConsensusKey must be a classical secp256k1 or ed25519 node key.
    auto const ckBlob = ctx.tx.getFieldVL(sfConsensusKey);
    if (!publicKeyType(makeSlice(ckBlob)))
        return temINVALID_FLAG;

    return tesSUCCESS;
}

TER
ValidatorUnbond::preclaim(PreclaimContext const& ctx)
{
    auto const ckBlob = ctx.tx.getFieldVL(sfConsensusKey);

    auto sleBond = ctx.view.read(keylet::validatorBond(calcValidatorBondID(makeSlice(ckBlob))));
    if (!sleBond)
        return tecNO_ENTRY;

    if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
        return tecNO_PERMISSION;  // not currently bonded

    return tesSUCCESS;
}

TER
ValidatorUnbond::doApply()
{
    auto const ckBlob = ctx_.tx.getFieldVL(sfConsensusKey);

    auto sleBond = ctx_.view().peek(keylet::validatorBond(calcValidatorBondID(makeSlice(ckBlob))));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    sleBond->setFieldU32(sfBondStatus, kBOND_STATUS_UNBONDING);
    sleBond->setFieldU32(sfUnbondingStartLedger, view().seq());
    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, view().seq());
    ctx_.view().update(sleBond);

    return tesSUCCESS;
}

void
ValidatorUnbond::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ValidatorUnbond::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
