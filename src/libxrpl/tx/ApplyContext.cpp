#include <xrpl/tx/ApplyContext.h>

#include <xrpl/basics/Log.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/beast/utility/Journal.h>
#include <xrpl/beast/utility/instrumentation.h>
#include <xrpl/core/ServiceRegistry.h>
#include <xrpl/json/to_string.h>
#include <xrpl/ledger/ApplyView.h>
#include <xrpl/ledger/OpenView.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxMeta.h>
#include <xrpl/protocol/XRPAmount.h>
#include <xrpl/tx/invariants/InvariantCheck.h>

#include <xrpl/protocol/STLedgerEntry.h>

#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace xrpl {

ApplyContext::ApplyContext(
    ServiceRegistry& registry,
    OpenView& base,
    std::optional<uint256 const> const& parentBatchId,
    STTx const& tx,
    TER preclaimResult,
    XRPAmount baseFee,
    ApplyFlags flags,
    beast::Journal journal)
    : registry(registry)
    , tx(tx)
    , preclaimResult(preclaimResult)
    , baseFee(baseFee)
    , journal(journal)
    , base_(base)
    , flags_(flags)
    , parentBatchId_(parentBatchId)
{
    XRPL_ASSERT(
        parentBatchId.has_value() == ((flags_ & TapBatch) == TapBatch),
        "Parent Batch ID should be set if batch apply flag is set");
    view_.emplace(&base_, flags_);
}

void
ApplyContext::discard()
{
    view_.emplace(&base_, flags_);
}

std::optional<TxMeta>
ApplyContext::apply(TER ter)
{
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) view_ emplaced in constructor
    return view_->apply(base_, tx, ter, parentBatchId_, (flags_ & TapDryRun) != 0u, journal);
}

std::size_t
ApplyContext::size()
{
    return view_->size();  // NOLINT(bugprone-unchecked-optional-access)
}

void
ApplyContext::visit(
    std::function<void(
        uint256 const&,
        bool,
        std::shared_ptr<SLE const> const&,
        std::shared_ptr<SLE const> const&)> const& func)
{
    view_->visit(base_, func);  // NOLINT(bugprone-unchecked-optional-access)
}

TER
ApplyContext::failInvariantCheck(TER const result)
{
    // If we already failed invariant checks before and we are now attempting to
    // only charge a fee, and even that fails the invariant checks something is
    // very wrong. We switch to tefINVARIANT_FAILED, which does NOT get included
    // in a ledger.

    return (result == tecINVARIANT_FAILED || result == tefINVARIANT_FAILED)
        ? TER{tefINVARIANT_FAILED}
        : TER{tecINVARIANT_FAILED};
}

template <std::size_t... Is>
TER
ApplyContext::checkInvariantsHelper(
    TER const result,
    XRPAmount const fee,
    std::index_sequence<Is...>)
{
    try
    {
        auto checkers = getInvariantChecks();

        // call each check's per-entry method
        visit([&checkers](
                  uint256 const& index,
                  bool isDelete,
                  std::shared_ptr<SLE const> const& before,
                  std::shared_ptr<SLE const> const& after) {
            (..., std::get<Is>(checkers).visitEntry(isDelete, before, after));
        });

        // Note: do not replace this logic with a `...&&` fold expression.
        // The fold expression will only run until the first check fails (it
        // short-circuits). While the logic is still correct, the log
        // message won't be. Every failed invariant should write to the log,
        // not just the first one.
        std::array<bool, sizeof...(Is)> const finalizers{{std::get<Is>(checkers).finalize(
            tx, result, fee, *view_, journal)...}};  // NOLINT(bugprone-unchecked-optional-access)

        // call each check's finalizer to see that it passes
        if (!std::all_of(finalizers.cbegin(), finalizers.cend(), [](auto const& b) { return b; }))
        {
            JLOG(journal.fatal()) << "Transaction has failed one or more global invariants: "
                                  << to_string(tx.getJson(JsonOptions::Values::None));

            return failInvariantCheck(result);
        }
    }
    catch (std::exception const& ex)
    {
        JLOG(journal.fatal()) << "Transaction caused an exception in a global invariant"
                              << ", ex: " << ex.what()
                              << ", tx: " << to_string(tx.getJson(JsonOptions::Values::None));

        return failInvariantCheck(result);
    }

    return result;
}

TER
ApplyContext::checkInvariants(TER const result, XRPAmount const fee)
{
    XRPL_ASSERT(
        isTesSuccess(result) || isTecClaim(result),
        "xrpl::ApplyContext::checkInvariants : is tesSUCCESS or tecCLAIM");

    return checkInvariantsHelper(
        result, fee, std::make_index_sequence<std::tuple_size_v<InvariantChecks>>{});
}

void
ApplyContext::destroyXRP(XRPAmount const& fee)
{
    if (fee <= beast::kZERO)
        return;

    // ── Determine the burn fraction for this ledger ──────────────────────
    //
    // Read sfCurrentBurnBps from the singleton ltREWARD_EPOCH object.  If the
    // epoch object does not exist yet (genesis, or ProofOfParticipation not
    // yet active) fall back to kFEE_BURN_DEFAULT_BPS (55 %).
    //
    // Two pressure signals adjust the burn fraction each epoch close, but the
    // per-tx path here only reads – it never writes to the epoch object.
    //
    // All arithmetic is integer; no floating point.

    std::uint32_t burnBps = kFEE_BURN_DEFAULT_BPS;  // 5 500 = 55 %

    if (auto sleEpoch =
            view_->read(keylet::rewardEpoch()))  // NOLINT(bugprone-unchecked-optional-access)
    {
        auto const stored = sleEpoch->getFieldU32(sfCurrentBurnBps);
        if (stored >= kFEE_BURN_MIN_BPS && stored <= kFEE_BURN_MAX_BPS)
            burnBps = stored;
    }

    // burnDrops  = fee * burnBps / kBPS_DENOM
    // treasury   = fee - burnDrops
    auto const feeDrops     = fee.drops();
    auto const burnDrops    = static_cast<std::int64_t>(
        (static_cast<__int128>(feeDrops) * burnBps) / kBPS_DENOM);
    auto const treasuryDrops = feeDrops - burnDrops;

    // ── Burn the burn fraction (decrements ledger total supply) ──────────
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    view_->rawDestroyXRP(XRPAmount{burnDrops});

    // ── Credit the treasury fraction ──────────────────────────────────────
    // The treasury account is identified by the well-known deterministic seed.
    // If the account doesn't exist (shouldn't happen after genesis) we fall
    // through and the treasury portion is burned as well.
    if (treasuryDrops > 0)
    {
        static auto const kTreasuryID = calcAccountID(
            generateKeyPair(KeyType::Secp256k1, generateSeed(kQXRP_TREASURY_SEED)).first);

        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        if (auto sleTreasury = view_->peek(keylet::account(kTreasuryID)))
        {
            auto const prev = sleTreasury->getFieldAmount(sfBalance);
            sleTreasury->setFieldAmount(
                sfBalance, prev + STAmount{XRPAmount{treasuryDrops}});
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            view_->rawReplace(sleTreasury);
        }
        else
        {
            // Treasury account missing — burn the remaining drops too so the
            // ledger invariant (total drops conserved) is not violated.
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            view_->rawDestroyXRP(XRPAmount{treasuryDrops});
        }
    }
}

}  // namespace xrpl
