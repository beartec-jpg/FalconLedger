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
#include <xrpl/protocol/tokens.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace xrpl {

namespace {

std::shared_ptr<SLE const>
findValidatorBond(ReadView const& view, PublicKey const& pubKey)
{
    // Bonds are keyed by calcValidatorBondID(sfConsensusKey) which must be
    // the same bytes as the UNL validation public key (not the wallet master).
    if (auto sle = view.read(keylet::validatorBond(calcValidatorBondID(pubKey.slice()))))
        return sle;
    if (auto sle = view.read(keylet::validatorBond(calcAccountID(pubKey))))
        return sle;
    return nullptr;
}

struct ScoringTarget
{
    std::shared_ptr<SLE> bond;
    NodeID nodeID{};
};

/** Per-ledger latency bps: 0s behind the earliest signer → 10_000;
 *  each full second of lag costs kLATENCY_PENALTY_BPS_PER_SEC (floor 0). */
constexpr std::uint32_t kLATENCY_PENALTY_BPS_PER_SEC = 100;
/** When a validator published no validations in the window, use this latency. */
constexpr std::uint32_t kLATENCY_NO_SAMPLE_BPS = 0;

std::uint32_t
latencyBpsFromDelay(std::uint64_t delaySeconds) noexcept
{
    auto const penalty = delaySeconds * kLATENCY_PENALTY_BPS_PER_SEC;
    if (penalty >= kBPS_DENOM)
        return 0;
    return static_cast<std::uint32_t>(kBPS_DENOM - penalty);
}

void
scoreBond(
    ScoringTarget& target,
    hash_map<NodeID, std::uint32_t> const& scoreTable,
    hash_map<NodeID, std::uint64_t> const& latencySumBps,
    hash_map<NodeID, std::uint32_t> const& latencySamples,
    LedgerIndex seq,
    OpenView& view,
    std::uint32_t& aggregateScore,
    beast::Journal j)
{
    auto sleBond = target.bond;
    auto const validationCount = [&]() -> std::uint32_t {
        if (auto it = scoreTable.find(target.nodeID); it != scoreTable.end())
            return it->second;
        return 0;
    }();

    auto const uptimeBps = std::min(
        static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(validationCount) * kBPS_DENOM /
            kFLAG_LEDGER_INTERVAL),
        kBPS_DENOM);

    // Trusted validations for the canonical ledger hash count as correct votes.
    auto const voteAccBps = uptimeBps;

    // Relative latency: average of per-ledger scores vs earliest signer.
    std::uint32_t latencyBps = kLATENCY_NO_SAMPLE_BPS;
    if (auto const sit = latencySamples.find(target.nodeID);
        sit != latencySamples.end() && sit->second > 0)
    {
        auto const sumIt = latencySumBps.find(target.nodeID);
        auto const sum =
            (sumIt != latencySumBps.end()) ? sumIt->second : std::uint64_t{0};
        latencyBps = static_cast<std::uint32_t>(sum / sit->second);
        latencyBps = std::min(latencyBps, kBPS_DENOM);
    }

    auto const consistencyBps = uptimeBps;

    // Weights for the four measured factors sum to 95; slash multiplier (5)
    // is applied as a post-factor (not an additive weight).
    auto const rawScore = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(uptimeBps)      * kSCORE_WEIGHT_UPTIME +
         static_cast<std::uint64_t>(voteAccBps)     * kSCORE_WEIGHT_VOTE_ACC +
         static_cast<std::uint64_t>(latencyBps)     * kSCORE_WEIGHT_LATENCY +
         static_cast<std::uint64_t>(consistencyBps) * kSCORE_WEIGHT_CONSISTENCY) /
        100u);

    auto const slashMult = sleBond->getFieldU32(sfSlashMultiplier);

    auto const compositeScore = static_cast<std::uint32_t>(
        (static_cast<__int128>(rawScore) * slashMult) / kBPS_DENOM);

    auto setScore = [&](SField const& f, std::uint32_t v) {
        if (v == 0)
            sleBond->makeFieldAbsent(f);
        else
            sleBond->setFieldU32(f, v);
    };
    setScore(sfUptimeBps,       uptimeBps);
    setScore(sfVoteAccuracyBps, voteAccBps);
    setScore(sfLatencyScoreBps, latencyBps);
    setScore(sfConsistencyBps,  consistencyBps);
    setScore(sfCompositeScore,  compositeScore);
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, seq);
    view.rawReplace(sleBond);

    JLOG(j.info()) << "qXRP ValidatorScoring: account=" << sleBond->getAccountID(sfAccount)
                   << " validations=" << validationCount
                   << " uptimeBps=" << uptimeBps
                   << " latencyBps=" << latencyBps
                   << " rawScore=" << rawScore
                   << " slashMult=" << slashMult
                   << " compositeScore=" << compositeScore;

    if (compositeScore <= std::numeric_limits<std::uint32_t>::max() - aggregateScore)
        aggregateScore += compositeScore;
    else
        aggregateScore = std::numeric_limits<std::uint32_t>::max();
}

}  // namespace

void
applyValidatorScoring(
    OpenView& view,
    std::shared_ptr<Ledger const> const& parent,
    LedgerIndex seq,
    Rules const& rules,
    Application& app,
    beast::Journal j)
{
    if (!rules.enabled(featureProofOfParticipation))
        return;
    if (seq == 0 || seq % kQXRP_LEDGERS_PER_EPOCH != 0)
        return;

    JLOG(j.debug()) << "qXRP ValidatorScoring: epoch boundary at ledger " << seq;

    auto& validations = app.getValidations();

    auto const windowLow =
        (seq > kFLAG_LEDGER_INTERVAL) ? (seq - kFLAG_LEDGER_INTERVAL) : LedgerIndex{1};
    validations.setSeqToKeep(windowLow, seq + kFLAG_LEDGER_INTERVAL);

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

    auto const unlKeys = app.getValidators().getTrustedMasterKeys();

    hash_map<NodeID, std::uint32_t> scoreTable;
    hash_map<NodeID, std::uint64_t> latencySumBps;
    hash_map<NodeID, std::uint32_t> latencySamples;
    scoreTable.reserve(unlKeys.size());
    latencySumBps.reserve(unlKeys.size());
    latencySamples.reserve(unlKeys.size());
    for (auto const& k : unlKeys)
    {
        auto const nid = calcNodeID(k);
        scoreTable.emplace(nid, std::uint32_t{0});
        latencySumBps.emplace(nid, std::uint64_t{0});
        latencySamples.emplace(nid, std::uint32_t{0});
    }

    std::uint32_t totalValsFound = 0;
    for (std::uint32_t i = 0; i < kFLAG_LEDGER_INTERVAL; ++i)
    {
        auto const ancestorHash = ledgerAncestors[numAncestors - 1 - i];
        auto const ancestorSeq  = static_cast<LedgerIndex>(seq - 2 - i);
        auto const vals = validations.getTrustedForLedger(ancestorHash, ancestorSeq);
        totalValsFound += vals.size();

        // Earliest sign time among trusted UNL validators for this ledger.
        std::optional<NetClock::time_point> earliest;
        for (auto const& v : vals)
        {
            auto const nid = v->getNodeID();
            if (!scoreTable.count(nid))
                continue;
            auto const t = v->getSignTime();
            if (!earliest || t < *earliest)
                earliest = t;
        }

        for (auto const& v : vals)
        {
            auto const nid = v->getNodeID();
            auto it = scoreTable.find(nid);
            if (it == scoreTable.end())
                continue;
            ++it->second;

            // Relative latency vs the fastest trusted signer of this ledger.
            if (earliest)
            {
                auto const t = v->getSignTime();
                auto const delay = (t > *earliest)
                    ? std::chrono::duration_cast<std::chrono::seconds>(t - *earliest)
                          .count()
                    : std::int64_t{0};
                auto const delaySec = static_cast<std::uint64_t>(
                    delay < 0 ? 0 : delay);
                latencySumBps[nid] += latencyBpsFromDelay(delaySec);
                ++latencySamples[nid];
            }
        }
        if (i < 3)
        {
            JLOG(j.info()) << "qXRP ValidatorScoring diag: i=" << i
                           << " ancestorSeq=" << ancestorSeq
                           << " hash=" << ancestorHash
                           << " valsFound=" << vals.size();
        }
    }
    JLOG(j.info()) << "qXRP ValidatorScoring diag: seq=" << seq
                   << " scoreTableSize=" << scoreTable.size()
                   << " totalValsFoundAcross256=" << totalValsFound;

    // ── 2. Map bonded validators → UNL NodeID and score ──

    hash_set<uint256> scoredBondKeys;
    std::vector<ScoringTarget> targets;
    targets.reserve(unlKeys.size());

    std::vector<std::shared_ptr<SLE const>> legacyBonds;
    std::vector<PublicKey> legacyUnl;

    for (auto const& pubKey : unlKeys)
    {
        auto sleConst = findValidatorBond(view, pubKey);
        if (!sleConst)
        {
            JLOG(j.info()) << "qXRP ValidatorScoring diag: no bond for UNL key "
                           << toBase58(TokenType::NodePublic, pubKey);
            legacyUnl.push_back(pubKey);
            continue;
        }

        if (sleConst->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
        {
            JLOG(j.info()) << "qXRP ValidatorScoring diag: bond "
                           << sleConst->getAccountID(sfAccount)
                           << " status=" << sleConst->getFieldU32(sfBondStatus)
                           << " (not bonded)";
            continue;
        }

        auto const bondKey = sleConst->key();
        if (scoredBondKeys.count(bondKey))
            continue;

        scoredBondKeys.insert(bondKey);
        targets.push_back({
            std::const_pointer_cast<SLE>(sleConst),
            calcNodeID(pubKey),
        });
    }

    // Legacy: bonds keyed by wallet master (sfAccount) while UNL uses n9.
    for (auto const& sleConst : view.sles)
    {
        if (sleConst->getType() != ltVALIDATOR_BOND)
            continue;
        if (sleConst->getFieldU32(sfBondStatus) != kBOND_STATUS_BONDED)
            continue;
        if (scoredBondKeys.count(sleConst->key()))
            continue;
        legacyBonds.push_back(sleConst);
    }

    // Pair legacy wallet-keyed bonds with unmatched UNL keys (sorted stable).
    if (!legacyBonds.empty() && !legacyUnl.empty())
    {
        std::sort(
            legacyBonds.begin(),
            legacyBonds.end(),
            [](auto const& a, auto const& b) {
                return a->getAccountID(sfAccount) < b->getAccountID(sfAccount);
            });
        std::sort(
            legacyUnl.begin(),
            legacyUnl.end(),
            [](PublicKey const& a, PublicKey const& b) {
                return toBase58(TokenType::NodePublic, a) <
                    toBase58(TokenType::NodePublic, b);
            });

        auto const n = std::min(legacyBonds.size(), legacyUnl.size());
        JLOG(j.warn()) << "qXRP ValidatorScoring: legacy bond/UNL pairing for "
                       << n << " validators (re-bond with validation_public_key_hex "
                       << "as ConsensusKey to remove this fallback)";

        for (std::size_t i = 0; i < n; ++i)
        {
            auto const& sleConst = legacyBonds[i];
            if (scoredBondKeys.count(sleConst->key()))
                continue;
            scoredBondKeys.insert(sleConst->key());
            targets.push_back({
                std::const_pointer_cast<SLE>(sleConst),
                calcNodeID(legacyUnl[i]),
            });
        }
    }

    std::uint32_t aggregateScore = 0;
    for (auto& target : targets)
        scoreBond(
            target,
            scoreTable,
            latencySumBps,
            latencySamples,
            seq,
            view,
            aggregateScore,
            j);

    if (auto sleEpoch = std::const_pointer_cast<SLE>(view.read(keylet::rewardEpoch())))
    {
        if (aggregateScore == 0)
            sleEpoch->makeFieldAbsent(sfAggregateCompositeScore);
        else
            sleEpoch->setFieldU32(sfAggregateCompositeScore, aggregateScore);
        view.rawReplace(sleEpoch);
    }
    else
    {
        JLOG(j.warn()) << "qXRP ValidatorScoring: ltREWARD_EPOCH not found at seq=" << seq
                       << " – aggregate score not recorded.";
    }

    JLOG(j.debug()) << "qXRP ValidatorScoring: seq=" << seq
                    << " aggregateCompositeScore=" << aggregateScore
                    << " scored " << targets.size() << " bonded validators.";
}

}  // namespace xrpl