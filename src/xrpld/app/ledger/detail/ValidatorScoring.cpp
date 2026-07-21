// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Phase 7 – Validator Reputation Scoring implementation.
//
// Fluid / incremental design:
//   - Independent signals: uptime ≠ vote accuracy ≠ consistency ≠ latency
//   - Continuous latency (bps vs earliest signer, 10 ms steps)
//   - Consistency from max absence streak (outages hurt more than scatter)
//   - EMA of composite so recovery is gradual, not a fixed flat demerit
//   - ActiveSet(K): top-K by composite keep reward weight; others clear composite

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

/** Per-ledger latency bps: 0 ms behind earliest signer → 10_000;
 *  each 10 ms of lag costs kLATENCY_PENALTY_BPS_PER_10MS (floor 0). */
std::uint32_t
latencyBpsFromDelayMs(std::uint64_t delayMs) noexcept
{
    auto const units = delayMs / 10u;
    auto const penalty = units * kLATENCY_PENALTY_BPS_PER_10MS;
    if (penalty >= kBPS_DENOM)
        return 0;
    return static_cast<std::uint32_t>(kBPS_DENOM - penalty);
}

/** EMA: blend new raw composite with previous on-bond composite. */
std::uint32_t
emaComposite(std::uint32_t raw, std::uint32_t previous) noexcept
{
    if (previous == 0)
        return raw;
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(raw) * kSCORE_EMA_NEW_BPS +
         static_cast<std::uint64_t>(previous) * (kBPS_DENOM - kSCORE_EMA_NEW_BPS)) /
        kBPS_DENOM);
}

/** Consistency from max consecutive absence in the window (continuous). */
std::uint32_t
consistencyFromPresence(std::vector<std::uint8_t> const& present) noexcept
{
    if (present.empty())
        return 0;

    std::uint32_t maxGap = 0;
    std::uint32_t gap = 0;
    for (auto const bit : present)
    {
        if (bit == 0)
        {
            ++gap;
            maxGap = std::max(maxGap, gap);
        }
        else
        {
            gap = 0;
        }
    }

    // Linear continuous penalty: full-window absence → 0; no gap → 10_000.
    auto const pen = (static_cast<std::uint64_t>(maxGap) * kBPS_DENOM) / present.size();
    if (pen >= kBPS_DENOM)
        return 0;
    return static_cast<std::uint32_t>(kBPS_DENOM - pen);
}

struct WindowStats
{
    std::uint32_t presenceCount = 0;   // any trusted full val for the seq
    std::uint32_t correctCount = 0;    // signed the canonical ledger hash
    std::uint64_t latencySumBps = 0;
    std::uint32_t latencySamples = 0;
    std::vector<std::uint8_t> present;  // per-ledger in window, 0/1
};

void
scoreBond(
    ScoringTarget& target,
    hash_map<NodeID, WindowStats> const& stats,
    LedgerIndex seq,
    OpenView& view,
    beast::Journal j)
{
    auto sleBond = target.bond;

    WindowStats empty;
    auto const* st = &empty;
    if (auto it = stats.find(target.nodeID); it != stats.end())
        st = &it->second;

    auto const window = static_cast<std::uint32_t>(
        st->present.empty() ? kFLAG_LEDGER_INTERVAL : st->present.size());

    auto const uptimeBps = std::min(
        static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(st->presenceCount) * kBPS_DENOM / window),
        kBPS_DENOM);

    // Vote accuracy independent of uptime: correct / votes cast (not / window).
    // Offline → 0; always-wrong while online → 0 with high uptime.
    std::uint32_t voteAccBps = 0;
    if (st->presenceCount > 0)
    {
        voteAccBps = std::min(
            static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(st->correctCount) * kBPS_DENOM /
                st->presenceCount),
            kBPS_DENOM);
    }

    std::uint32_t latencyBps = 0;
    if (st->latencySamples > 0)
    {
        latencyBps = static_cast<std::uint32_t>(st->latencySumBps / st->latencySamples);
        latencyBps = std::min(latencyBps, kBPS_DENOM);
    }

    auto const consistencyBps = consistencyFromPresence(st->present);

    auto const rawScore = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(uptimeBps) * kSCORE_WEIGHT_UPTIME +
         static_cast<std::uint64_t>(voteAccBps) * kSCORE_WEIGHT_VOTE_ACC +
         static_cast<std::uint64_t>(latencyBps) * kSCORE_WEIGHT_LATENCY +
         static_cast<std::uint64_t>(consistencyBps) * kSCORE_WEIGHT_CONSISTENCY) /
        100u);

    auto const slashMult = sleBond->getFieldU32(sfSlashMultiplier);
    auto const rawSlashed = static_cast<std::uint32_t>(
        (static_cast<__int128>(rawScore) * slashMult) / kBPS_DENOM);

    // Incremental recovery / decay: blend with previous composite.
    auto const previous =
        sleBond->isFieldPresent(sfCompositeScore) ? sleBond->getFieldU32(sfCompositeScore)
                                                  : 0u;
    auto const compositeScore = emaComposite(rawSlashed, previous);

    auto setScore = [&](SField const& f, std::uint32_t v) {
        if (v == 0)
            sleBond->makeFieldAbsent(f);
        else
            sleBond->setFieldU32(f, v);
    };
    setScore(sfUptimeBps, uptimeBps);
    setScore(sfVoteAccuracyBps, voteAccBps);
    setScore(sfLatencyScoreBps, latencyBps);
    setScore(sfConsistencyBps, consistencyBps);
    setScore(sfCompositeScore, compositeScore);
    sleBond->setFieldU32(sfPreviousTxnLgrSeq, seq);
    view.rawReplace(sleBond);

    JLOG(j.info()) << "qXRP ValidatorScoring: account=" << sleBond->getAccountID(sfAccount)
                   << " presence=" << st->presenceCount << "/" << window
                   << " correct=" << st->correctCount
                   << " uptimeBps=" << uptimeBps << " voteAccBps=" << voteAccBps
                   << " latencyBps=" << latencyBps << " consistencyBps=" << consistencyBps
                   << " rawScore=" << rawScore << " rawSlashed=" << rawSlashed
                   << " prevComposite=" << previous << " compositeScore=" << compositeScore
                   << " slashMult=" << slashMult;
}

/** Keep only top-K composites for the active set; clear the rest. */
void
applyActiveSet(
    std::vector<ScoringTarget>& targets,
    OpenView& view,
    std::uint32_t& aggregateScore,
    beast::Journal j)
{
    aggregateScore = 0;
    if (targets.empty())
        return;

    // Sort by composite desc, then account for deterministic ties (no shared flat rank).
    std::sort(targets.begin(), targets.end(), [](ScoringTarget const& a, ScoringTarget const& b) {
        auto const ca = a.bond->isFieldPresent(sfCompositeScore)
            ? a.bond->getFieldU32(sfCompositeScore)
            : 0u;
        auto const cb = b.bond->isFieldPresent(sfCompositeScore)
            ? b.bond->getFieldU32(sfCompositeScore)
            : 0u;
        if (ca != cb)
            return ca > cb;
        return a.bond->getAccountID(sfAccount) < b.bond->getAccountID(sfAccount);
    });

    auto const keep = std::min<std::size_t>(kQXRP_ACTIVE_SET_K, targets.size());
    for (std::size_t i = 0; i < targets.size(); ++i)
    {
        auto& sleBond = targets[i].bond;
        if (i < keep)
        {
            auto const c = sleBond->isFieldPresent(sfCompositeScore)
                ? sleBond->getFieldU32(sfCompositeScore)
                : 0u;
            if (c <= std::numeric_limits<std::uint32_t>::max() - aggregateScore)
                aggregateScore += c;
            else
                aggregateScore = std::numeric_limits<std::uint32_t>::max();
            continue;
        }

        // Outside ActiveSet(K): keep diagnostic component scores, drop composite.
        if (sleBond->isFieldPresent(sfCompositeScore))
        {
            JLOG(j.info()) << "qXRP ActiveSet: demote account="
                           << sleBond->getAccountID(sfAccount)
                           << " rank=" << (i + 1) << " (K=" << kQXRP_ACTIVE_SET_K << ")";
            sleBond->makeFieldAbsent(sfCompositeScore);
            view.rawReplace(sleBond);
        }
    }

    JLOG(j.info()) << "qXRP ActiveSet: kept=" << keep << " of " << targets.size()
                   << " aggregateCompositeScore=" << aggregateScore;
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
    // Fluid cadence: re-score every flag interval (256 ledgers), not only epoch.
    if (seq == 0 || seq % kFLAG_LEDGER_INTERVAL != 0)
        return;

    JLOG(j.debug()) << "qXRP ValidatorScoring: window boundary at ledger " << seq;

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

    hash_map<NodeID, WindowStats> stats;
    stats.reserve(unlKeys.size());
    for (auto const& k : unlKeys)
    {
        auto& s = stats[calcNodeID(k)];
        s.present.assign(kFLAG_LEDGER_INTERVAL, 0);
    }

    std::uint32_t totalValsFound = 0;
    for (std::uint32_t i = 0; i < kFLAG_LEDGER_INTERVAL; ++i)
    {
        auto const ancestorHash = ledgerAncestors[numAncestors - 1 - i];
        auto const ancestorSeq = static_cast<LedgerIndex>(seq - 2 - i);

        // Any trusted full validation at this sequence (uptime / vote cast).
        auto const anyVals = validations.getTrustedForSequence(ancestorSeq);
        // Canonical-hash validations (correct vote + latency baseline).
        auto const correctVals =
            validations.getTrustedForLedger(ancestorHash, ancestorSeq);
        totalValsFound += correctVals.size();

        std::optional<NetClock::time_point> earliest;
        for (auto const& v : correctVals)
        {
            auto const nid = v->getNodeID();
            if (!stats.count(nid))
                continue;
            auto const t = v->getSignTime();
            if (!earliest || t < *earliest)
                earliest = t;
        }

        for (auto const& v : anyVals)
        {
            auto const nid = v->getNodeID();
            auto it = stats.find(nid);
            if (it == stats.end())
                continue;
            if (it->second.present[i] == 0)
            {
                it->second.present[i] = 1;
                ++it->second.presenceCount;
            }
        }

        for (auto const& v : correctVals)
        {
            auto const nid = v->getNodeID();
            auto it = stats.find(nid);
            if (it == stats.end())
                continue;
            ++it->second.correctCount;

            if (earliest)
            {
                auto const t = v->getSignTime();
                auto const delay = (t > *earliest)
                    ? std::chrono::duration_cast<std::chrono::milliseconds>(t - *earliest)
                          .count()
                    : std::int64_t{0};
                auto const delayMs = static_cast<std::uint64_t>(delay < 0 ? 0 : delay);
                it->second.latencySumBps += latencyBpsFromDelayMs(delayMs);
                ++it->second.latencySamples;
            }
        }

        if (i < 3)
        {
            JLOG(j.info()) << "qXRP ValidatorScoring diag: i=" << i
                           << " ancestorSeq=" << ancestorSeq
                           << " hash=" << ancestorHash
                           << " anyVals=" << anyVals.size()
                           << " correctVals=" << correctVals.size();
        }
    }
    JLOG(j.info()) << "qXRP ValidatorScoring diag: seq=" << seq
                   << " statsSize=" << stats.size()
                   << " totalCorrectAcross256=" << totalValsFound;

    // ── Map bonded validators → UNL NodeID and score ──

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

    for (auto& target : targets)
        scoreBond(target, stats, seq, view, j);

    std::uint32_t aggregateScore = 0;
    applyActiveSet(targets, view, aggregateScore, j);

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
