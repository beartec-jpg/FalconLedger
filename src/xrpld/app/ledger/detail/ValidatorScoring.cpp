// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Phase 7 – Validator Reputation Scoring implementation.

#include <xrpld/app/ledger/ValidatorScoring.h>

#include <xrpld/app/consensus/RCLValidations.h>
#include <xrpld/app/main/Application.h>
#include <xrpld/app/misc/ValidatorList.h>

#include <xrpl/basics/Log.h>
#include <xrpl/basics/UnorderedContainers.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/Rules.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

namespace xrpl {

void
applyValidatorScoring(
    OpenView& view,
    std::shared_ptr<Ledger const> const& parent,
    LedgerIndex seq,
    Rules const& rules,
    Application& app,
    beast::Journal j)
{
    // Gate: amendment + epoch boundary only
    if (!rules.enabled(featureProofOfParticipation))
        return;
    if (seq == 0 || seq % kQXRP_LEDGERS_PER_EPOCH != 0)
        return;

    JLOG(j.debug()) << "qXRP ValidatorScoring: epoch boundary at ledger " << seq;

    // ── 1. Build a NodeID → validation-count table for the last 256 ledgers ──
    //
    // Mirrors NegativeUNLVote::buildScoreTable(). The parent ledger's skip list
    // contains the most recent kFLAG_LEDGER_INTERVAL (256) ancestor hashes.

    auto& validations = app.getValidations();

    // Tell the validation cache to retain history for the upcoming 256 ledgers so
    // this data is available for any other consumer during this interval.
    validations.setSeqToKeep(seq - 1, seq + kFLAG_LEDGER_INTERVAL);

    auto const hashIndex = parent->read(keylet::skip());
    if (!hashIndex || !hashIndex->isFieldPresent(sfHashes))
    {
        JLOG(j.debug()) << "qXRP ValidatorScoring: ledger " << seq
                        << " skip list unavailable – skipping scoring.";
        return;
    }

    auto const ledgerAncestors = hashIndex->getFieldV256(sfHashes).value();
    auto const numAncestors = ledgerAncestors.size();
    if (numAncestors < kFLAG_LEDGER_INTERVAL)
    {
        JLOG(j.debug()) << "qXRP ValidatorScoring: ledger " << seq
                        << " only " << numAncestors
                        << " ancestors in skip list (need " << kFLAG_LEDGER_INTERVAL
                        << ") – skipping scoring.";
        return;
    }

    // Build score table: NodeID → number of trusted validations seen in window.
    // We pre-populate with all current UNL NodeIDs so absent validators get 0.
    auto const unlKeys = app.getValidators().getTrustedMasterKeys();

    hash_map<NodeID, std::uint32_t> scoreTable;
    scoreTable.reserve(unlKeys.size());
    for (auto const& k : unlKeys)
        scoreTable.emplace(calcNodeID(k), std::uint32_t{0});

    // Sequence offset: parent has seq = (this ledger's seq) - 1.
    // The skip list's most recent entry is the grandparent (seq - 2), so each
    // ancestor i (0 = most recent) corresponds to ledger sequence seq - 2 - i.
    for (std::uint32_t i = 0; i < kFLAG_LEDGER_INTERVAL; ++i)
    {
        auto const ancestorHash = ledgerAncestors[numAncestors - 1 - i];
        auto const ancestorSeq  = static_cast<LedgerIndex>(seq - 2 - i);
        for (auto const& v : validations.getTrustedForLedger(ancestorHash, ancestorSeq))
        {
            auto const nid = v->getNodeID();
            if (auto it = scoreTable.find(nid); it != scoreTable.end())
                ++it->second;
        }
    }

    // ── 2. Score each bonded validator and update ltVALIDATOR_BOND ──

    std::uint32_t aggregateScore = 0;
    for (auto const& pubKey : unlKeys)
    {
        // Map UNL key → bond account.  In qXRP validators bond from the account
        // whose AccountID is derived from their validation public key.
        auto const accountID = calcAccountID(pubKey);
        auto sleBond = view.peek(keylet::validatorBond(accountID));
        if (!sleBond)
            continue;

        // Only score actively bonded validators; skip registered-only / unbonding.
        if (sleBond->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
            continue;

        auto const nodeID = calcNodeID(pubKey);
        auto const validationCount = [&]() -> std::uint32_t {
            if (auto it = scoreTable.find(nodeID); it != scoreTable.end())
                return it->second;
            return 0;
        }();

        // ── signal computation ──
        //
        // uptimeBps : fraction of the 256-ledger window where a trusted
        //             validation was observed. Clamped to [0, kBPS_DENOM].
        auto const uptimeBps = std::min(
            static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(validationCount) * kBPS_DENOM /
                kFLAG_LEDGER_INTERVAL),
            kBPS_DENOM);

        // voteAccuracyBps: trusted validations are already pre-filtered by the
        // RCL validation layer, so every counted validation is "accurate".  MVP:
        // equate with uptime.
        auto const voteAccBps = uptimeBps;

        // latencyBps: per-message latency data is not tracked in the ledger
        // state.  Use a neutral mid-point (50%) for the MVP.
        constexpr std::uint32_t kLATENCY_NEUTRAL_BPS = 5'000;

        // consistencyBps: participation streak consistency.  MVP: equate with
        // uptime (a validator that is up consistently is consistent).
        auto const consistencyBps = uptimeBps;

        // ── raw weighted composite score ──
        //
        // rawScore = (uptime*40 + voteAcc*30 + latency*15 + consistency*10) / 100
        // The slash-multiplier weight (5%) is applied separately below so that
        // slashing always reduces the final score even when all other signals
        // are at 100%.
        auto const rawScore = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(uptimeBps)      * kSCORE_WEIGHT_UPTIME +
             static_cast<std::uint64_t>(voteAccBps)     * kSCORE_WEIGHT_VOTE_ACC +
             static_cast<std::uint64_t>(kLATENCY_NEUTRAL_BPS) * kSCORE_WEIGHT_LATENCY +
             static_cast<std::uint64_t>(consistencyBps) * kSCORE_WEIGHT_CONSISTENCY) /
            100u);

        // sfSlashMultiplier starts at kBPS_DENOM (10 000 = 100%) and is
        // decremented by each ValidatorSlash transaction.
        auto const slashMult = sleBond->getFieldU32(sfSlashMultiplier);

        // compositeScore = rawScore * slashMult / kBPS_DENOM
        // Uses __int128 to avoid overflow on 32-bit × 32-bit before division.
        auto const compositeScore = static_cast<std::uint32_t>(
            (static_cast<__int128>(rawScore) * slashMult) / kBPS_DENOM);

        // ── write scoring signals onto the bond object ──
        sleBond->setFieldU32(sfUptimeBps,        uptimeBps);
        sleBond->setFieldU32(sfVoteAccuracyBps,  voteAccBps);
        sleBond->setFieldU32(sfLatencyScoreBps,  kLATENCY_NEUTRAL_BPS);
        sleBond->setFieldU32(sfConsistencyBps,   consistencyBps);
        sleBond->setFieldU32(sfCompositeScore,   compositeScore);
        sleBond->setFieldU32(sfPreviousTxnLgrSeq, seq);
        view.update(sleBond);

        JLOG(j.trace()) << "qXRP ValidatorScoring: accountID=" << accountID
                        << " validations=" << validationCount
                        << " uptimeBps=" << uptimeBps
                        << " compositeScore=" << compositeScore;

        // Saturating add to prevent overflow on the aggregate.
        if (compositeScore <= std::numeric_limits<std::uint32_t>::max() - aggregateScore)
            aggregateScore += compositeScore;
        else
            aggregateScore = std::numeric_limits<std::uint32_t>::max();
    }

    // ── 3. Write aggregate composite score back to ltREWARD_EPOCH ──
    //
    // applyRewardEpoch() already created / updated this singleton earlier in
    // the same OpenView accumulation pass.
    if (auto sleEpoch = view.peek(keylet::rewardEpoch()))
    {
        sleEpoch->setFieldU32(sfAggregateCompositeScore, aggregateScore);
        view.update(sleEpoch);
    }
    else
    {
        JLOG(j.warn()) << "qXRP ValidatorScoring: ltREWARD_EPOCH not found at seq=" << seq
                       << " – aggregate score not recorded.";
    }

    JLOG(j.debug()) << "qXRP ValidatorScoring: seq=" << seq
                    << " aggregateCompositeScore=" << aggregateScore
                    << " scored " << unlKeys.size() << " UNL keys.";
}

}  // namespace xrpl
