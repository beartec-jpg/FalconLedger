// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ValidatorRegister.h>

#include <xrpl/ledger/helpers/AccountRootHelpers.h>
#include <xrpl/ledger/helpers/DirectoryHelpers.h>
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
ValidatorRegister::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    // sfPublicKey must be a Falcon-512 or Falcon-1024 post-quantum key.
    auto const pkBlob = ctx.tx.getFieldVL(sfPublicKey);
    if (!isValidNodeKey(makeSlice(pkBlob)))
        return temINVALID_FLAG;

    // sfConsensusKey must be a classical secp256k1 or ed25519 node key.
    auto const ckBlob = ctx.tx.getFieldVL(sfConsensusKey);
    if (!publicKeyType(makeSlice(ckBlob)))
        return temINVALID_FLAG;

    return tesSUCCESS;
}

TER
ValidatorRegister::preclaim(PreclaimContext const& ctx)
{
    auto const ckBlob = ctx.tx.getFieldVL(sfConsensusKey);
    auto const bondKeylet = keylet::validatorBond(calcValidatorBondID(makeSlice(ckBlob)));

    // Reject duplicate registration.
    if (ctx.view.read(bondKeylet))
        return tecDUPLICATE;

    return tesSUCCESS;
}

TER
ValidatorRegister::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const pkBlob = ctx_.tx.getFieldVL(sfPublicKey);
    auto const ckBlob = ctx_.tx.getFieldVL(sfConsensusKey);

    // Bond SLE keyed by classical consensus key so ValidatorScoring can find it
    // by mapping trusted UNL keys → calcValidatorBondID(sfConsensusKey).
    auto sleBond = std::make_shared<SLE>(keylet::validatorBond(calcValidatorBondID(makeSlice(ckBlob))));
    sleBond->setAccountID(sfAccount, account);
    sleBond->setFieldVL(sfPublicKey, pkBlob);     // Falcon key (on-chain identity)
    sleBond->setFieldVL(sfConsensusKey, ckBlob);  // classical key (scoring lookup)
    sleBond->setFieldAmount(sfBondedAmount, STAmount{XRPAmount{0}});
    sleBond->setFieldU32(sfBondStatus, kBOND_STATUS_REGISTERED);
    sleBond->setFieldU32(sfSlashMultiplier, kBPS_DENOM);  // start at 10 000 = clean
    // SoeDefault fields (sfUptimeBps, sfVoteAccuracyBps, sfLatencyScoreBps,
    // sfConsistencyBps, sfCompositeScore, sfSlashCount, sfLastClaimedEpoch,
    // sfUnbondingStartLedger) are intentionally left unset — they default to 0.
    // Explicitly setting them to 0 would be rejected by applyTemplate.
    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, view().seq());

    // Add to owner directory for reserve counting.
    auto const page = ctx_.view().dirInsert(
        keylet::ownerDir(account),
        sleBond->key(),
        describeOwnerDir(account));
    if (!page)
        return tecDIR_FULL;

    sleBond->setFieldU64(sfOwnerNode, *page);
    ctx_.view().insert(sleBond);

    // Bump owner count — the bond object counts against reserve.
    auto sleAccount = ctx_.view().peek(keylet::account(account));
    if (!sleAccount)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    auto const ownerCount = sleAccount->getFieldU32(sfOwnerCount);
    sleAccount->setFieldU32(sfOwnerCount, ownerCount + 1);
    ctx_.view().update(sleAccount);

    return tesSUCCESS;
}

void
ValidatorRegister::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ValidatorRegister::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
