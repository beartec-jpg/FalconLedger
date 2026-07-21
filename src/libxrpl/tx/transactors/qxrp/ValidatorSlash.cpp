// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/tx/transactors/qxrp/ValidatorSlash.h>

#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/STValidation.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/tx/ApplyContext.h>

#include <exception>
#include <optional>
#include <utility>

namespace xrpl {
namespace {

/** Parse a serialized STValidation; verify Falcon signature. */
std::optional<STValidation>
parseValidationEvidence(Slice blob)
{
    if (blob.empty())
        return std::nullopt;

    try
    {
        SerialIter sit{blob};
        // checkSignature=true verifies Falcon (classical rejects in isValid).
        STValidation val(
            sit,
            [](PublicKey const& pk) { return calcNodeID(pk); },
            true);
        if (!val.isValid())
            return std::nullopt;
        return val;
    }
    catch (std::exception const&)
    {
        return std::nullopt;
    }
}

/**
 * Cryptographically verify a DOUBLE_SIGN offense.
 *
 * Requirements (all must hold):
 *  1. Both evidence blobs deserialize as STValidation with valid Falcon sigs
 *  2. Both signed by the same public key
 *  3. That key matches the target bond's sfConsensusKey
 *  4. Bond keylet ID equals sfSlashTarget
 *  5. Same sfLedgerSequence (same consensus round)
 *  6. Different sfLedgerHash (actual double-sign / equivocation)
 */
bool
verifyDoubleSignEvidence(
    Slice evidence1,
    Slice evidence2,
    AccountID const& slashTarget,
    SLE const& sleBond)
{
    auto v1 = parseValidationEvidence(evidence1);
    auto v2 = parseValidationEvidence(evidence2);
    if (!v1 || !v2)
        return false;

    auto const pk1 = v1->getSignerPublic().slice();
    auto const pk2 = v2->getSignerPublic().slice();
    if (pk1.size() == 0 || pk1 != pk2)
        return false;

    // Bond consensus key must match the signing key on both validations.
    if (!sleBond.isFieldPresent(sfConsensusKey))
        return false;
    auto const ck = sleBond.getFieldVL(sfConsensusKey);
    if (makeSlice(ck) != pk1)
        return false;

    // Slash target is the bond keylet AccountID derived from the consensus key.
    if (calcValidatorBondID(pk1) != slashTarget)
        return false;

    // Same ledger sequence, different ledger hash → equivocation.
    if (v1->getFieldU32(sfLedgerSequence) != v2->getFieldU32(sfLedgerSequence))
        return false;
    if (v1->getLedgerHash() == v2->getLedgerHash())
        return false;

    return true;
}

}  // namespace

NotTEC
ValidatorSlash::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureProofOfParticipation))
        return temDISABLED;

    auto const offense = ctx.tx.getFieldU32(sfSlashOffense);
    if (offense < kSLASH_OFFENSE_DOUBLE_SIGN || offense > kSLASH_OFFENSE_INVALID_VOTE)
        return temMALFORMED;

    // Only DOUBLE_SIGN is enforced. ABSENCE / INVALID_VOTE stay disabled until
    // robust on-ledger detection exists (security audit C-01 / L-01).
    if (offense != kSLASH_OFFENSE_DOUBLE_SIGN)
        return temDISABLED;

    if (!ctx.tx.isFieldPresent(sfSlashEvidence1) ||
        !ctx.tx.isFieldPresent(sfSlashEvidence2))
        return temMALFORMED;

    // Cheap pre-check: blobs must differ. Full crypto verify is in preclaim.
    if (ctx.tx.getFieldVL(sfSlashEvidence1) == ctx.tx.getFieldVL(sfSlashEvidence2))
        return temMALFORMED;

    return tesSUCCESS;
}

TER
ValidatorSlash::preclaim(PreclaimContext const& ctx)
{
    auto const target = ctx.tx.getAccountID(sfSlashTarget);

    auto const sleBond = ctx.view.read(keylet::validatorBond(target));
    if (!sleBond)
        return tecNO_ENTRY;

    auto const bondStatus = sleBond->getFieldU32(sfBondStatus);
    if (bondStatus == kBOND_STATUS_UNBONDING)
        return tecDUPLICATE;
    if (bondStatus != kBOND_STATUS_BONDED && bondStatus != kBOND_STATUS_REGISTERED)
        return tecNO_PERMISSION;

    // Cryptographic evidence check (DOUBLE_SIGN only reaches here via preflight).
    auto const offense = ctx.tx.getFieldU32(sfSlashOffense);
    if (offense != kSLASH_OFFENSE_DOUBLE_SIGN)
        return temDISABLED;

    auto const e1 = ctx.tx.getFieldVL(sfSlashEvidence1);
    auto const e2 = ctx.tx.getFieldVL(sfSlashEvidence2);
    if (!verifyDoubleSignEvidence(makeSlice(e1), makeSlice(e2), target, *sleBond))
        return tecNO_PERMISSION;

    return tesSUCCESS;
}

TER
ValidatorSlash::doApply()
{
    auto const target = ctx_.tx.getAccountID(sfSlashTarget);
    auto const offense = ctx_.tx.getFieldU32(sfSlashOffense);

    auto sleBond = ctx_.view().peek(keylet::validatorBond(target));
    if (!sleBond)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // Defense in depth: re-verify evidence in doApply (preclaim already did).
    if (offense != kSLASH_OFFENSE_DOUBLE_SIGN)
        return temDISABLED;

    {
        auto const e1 = ctx_.tx.getFieldVL(sfSlashEvidence1);
        auto const e2 = ctx_.tx.getFieldVL(sfSlashEvidence2);
        if (!verifyDoubleSignEvidence(makeSlice(e1), makeSlice(e2), target, *sleBond))
            return tecNO_PERMISSION;
    }

    auto const bondOwner = sleBond->getAccountID(sfAccount);
    auto sleTarget = ctx_.view().peek(keylet::account(bondOwner));
    if (!sleTarget)
        return tefINTERNAL;  // LCOV_EXCL_LINE

    // DOUBLE_SIGN → full slash (100%).
    std::uint32_t const slashBps = kSLASH_DOUBLE_SIGN_BPS;

    auto const bondedAmount = sleBond->getFieldAmount(sfBondedAmount);
    auto const bondedDrops = bondedAmount.xrp().drops();

    auto const slashedDrops = static_cast<std::int64_t>(
        (static_cast<__int128>(bondedDrops) * slashBps) / kBPS_DENOM);
    auto const remainderDrops = bondedDrops - slashedDrops;

    sleBond->setFieldAmount(sfBondedAmount, STAmount{XRPAmount{remainderDrops}});

    // Forced unbonding — cannot claim rewards while unbonding.
    sleBond->setFieldU32(sfBondStatus, kBOND_STATUS_UNBONDING);
    sleBond->setFieldU32(sfUnbondingStartLedger, ctx_.view().seq());

    auto const prevMul = sleBond->getFieldU32(sfSlashMultiplier);
    auto const newMul = (prevMul >= 1'000) ? prevMul - 1'000 : 0;
    sleBond->setFieldU32(sfSlashMultiplier, newMul);
    sleBond->setFieldU32(sfSlashCount, sleBond->getFieldU32(sfSlashCount) + 1);

    sleBond->setFieldH256(sfPreviousTxnID, ctx_.tx.getTransactionID());
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, ctx_.view().seq());
    ctx_.view().update(sleBond);

    // Slashed portion is burned (not paid to submitter) — no grief-for-profit.
    if (slashedDrops > 0)
        ctx_.destroyXRP(XRPAmount{slashedDrops});

    if (remainderDrops > 0)
    {
        auto const prevBal = sleTarget->getFieldAmount(sfBalance);
        sleTarget->setFieldAmount(sfBalance, prevBal + STAmount{XRPAmount{remainderDrops}});
        ctx_.view().update(sleTarget);
    }

    return tesSUCCESS;
}

void
ValidatorSlash::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
ValidatorSlash::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
