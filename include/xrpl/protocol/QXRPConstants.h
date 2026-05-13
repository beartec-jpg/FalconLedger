// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// qXRP-specific protocol constants. All values use integer arithmetic; no
// floating-point anywhere in the reward or fee-split math.
//
// Terminology:
//   drop       — 1e-6 qXRP (same granularity as upstream XRP)
//   epoch      — one reward period, measured in ledgers
//   bps        — basis points (1 bps = 0.01 %)
//   BPS_DENOM  — denominator for bps arithmetic (10 000)

#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/SystemParameters.h>

#include <cstdint>

namespace xrpl {

// ─── Treasury AccountID ─────────────────────────────────────────────────────

/// Return the well-known deterministic treasury AccountID (singleton).
///
/// Prefer this over re-deriving the ID inline to ensure there is a single
/// source of truth and to avoid repeating the key-derivation on every call.
/// The result is computed once and cached on first call.
[[nodiscard]] AccountID const&
getTreasuryAccountID() noexcept;

// ─── Supply split ───────────────────────────────────────────────────────────

/// 2 % of 200 B = 4 B qXRP — initial circulating supply at genesis.
constexpr XRPAmount kQXRP_GENESIS_ALLOCATION{4'000'000'000 * kDROPS_PER_XRP};

/// 98 % of 200 B = 196 B qXRP — held by the on-chain treasury at genesis.
constexpr XRPAmount kQXRP_TREASURY_ALLOCATION{196'000'000'000 * kDROPS_PER_XRP};

static_assert(
    kQXRP_GENESIS_ALLOCATION.drops() + kQXRP_TREASURY_ALLOCATION.drops() ==
        kINITIAL_XRP.drops(),
    "Genesis + Treasury must equal the total supply");

// Required for the overflow-free fillBps calculation in RewardEpoch.cpp:
//   fillBps = treasuryDrops / (kQXRP_TREASURY_ALLOCATION / kBPS_DENOM)
// This assert confirms the division is exact so we lose no precision.
static_assert(
    kQXRP_TREASURY_ALLOCATION.drops() % 10'000 == 0,
    "kQXRP_TREASURY_ALLOCATION must be exactly divisible by kBPS_DENOM (10000)");

// ─── Treasury account ───────────────────────────────────────────────────────

/// Well-known seed from which the deterministic treasury account is derived.
/// This seed is public by design — the account is controlled exclusively by
/// the ProofOfParticipation amendment logic, not by any private key.
constexpr char const* kQXRP_TREASURY_SEED = "qXRPTreasuryReservedSeedV1000000";

/// Ledger type tag used to identify the treasury in genesis helper code.
/// The actual AccountID is computed at runtime via calcAccountID(seed).

// ─── Epoch / emission schedule ──────────────────────────────────────────────

/// Number of ledgers per reward epoch (~7 days at 3.5 s/ledger).
///
/// Can be overridden at compile time with -DQXRP_EPOCH_LEDGERS=<N> for regtest
/// or development builds.  Pass -Dqxrp_epoch_override=<N> to CMake and the
/// build system will set the preprocessor define automatically.
///
/// Example (build a regtest binary with 10-ledger epochs):
///   cmake .. -Dqxrp_epoch_override=10
#ifdef QXRP_EPOCH_LEDGERS
constexpr std::uint32_t kQXRP_LEDGERS_PER_EPOCH = QXRP_EPOCH_LEDGERS;
#else
constexpr std::uint32_t kQXRP_LEDGERS_PER_EPOCH = 172'800;
#endif

/// Number of epochs per halving period (roughly 4 years → 208 epochs).
constexpr std::uint32_t kQXRP_EPOCHS_PER_HALVING = 208;

/// Initial epoch emission as a fraction of the treasury balance, in bps.
/// 50 bps of 196 B = 980 M qXRP emitted in epoch 0 (before any halvings).
constexpr std::uint32_t kQXRP_INITIAL_EMISSION_BPS = 50;

/// Minimum emission rate — never drops below this even after many halvings.
constexpr std::uint32_t kQXRP_MIN_EMISSION_BPS = 1;

// ─── Fee split ──────────────────────────────────────────────────────────────

/// Basis-point denominator used throughout fee-split and scoring arithmetic.
constexpr std::uint32_t kBPS_DENOM = 10'000;

/// Hard floor on the burn fraction (40 % = 4 000 bps).
constexpr std::uint32_t kFEE_BURN_MIN_BPS = 4'000;

/// Hard ceiling on the burn fraction (70 % = 7 000 bps).
constexpr std::uint32_t kFEE_BURN_MAX_BPS = 7'000;

/// Default midpoint: 55 % burn / 45 % treasury (used when no epoch data
/// exists yet, e.g. the very first ledger after genesis).
constexpr std::uint32_t kFEE_BURN_DEFAULT_BPS = 5'500;

/// Sensitivity of the burn fraction to treasury fill pressure (bps/bps).
/// Higher value → burn fraction moves faster as treasury fills.
constexpr std::uint32_t kFEE_TREASURY_SENSITIVITY_BPS = 1'000;

/// Sensitivity of the burn fraction to fee-volume pressure (bps/bps).
constexpr std::uint32_t kFEE_USAGE_SENSITIVITY_BPS = 500;

// ─── Bonding ────────────────────────────────────────────────────────────────

/// Minimum bond amount in drops (1 000 qXRP).
constexpr std::int64_t kQXRP_MIN_BOND_DROPS = 1'000 * kDROPS_PER_XRP.drops();

/// Number of ledgers a validator must wait after unbonding before funds are
/// released (~30 days at 3.5 s/ledger).
constexpr std::uint32_t kUNBONDING_LOCK_LEDGERS = 262'800;

// ─── Validator bond status codes ─────────────────────────────────────────────

constexpr std::uint8_t kBOND_STATUS_REGISTERED = 0;
constexpr std::uint8_t kBOND_STATUS_BONDED      = 1;
constexpr std::uint8_t kBOND_STATUS_UNBONDING   = 2;

// ─── Composite score weights (must sum to 100) ───────────────────────────────

constexpr std::uint32_t kSCORE_WEIGHT_UPTIME      = 40;
constexpr std::uint32_t kSCORE_WEIGHT_VOTE_ACC    = 30;
constexpr std::uint32_t kSCORE_WEIGHT_LATENCY     = 15;
constexpr std::uint32_t kSCORE_WEIGHT_CONSISTENCY = 10;
constexpr std::uint32_t kSCORE_WEIGHT_SLASH_MULT  =  5;

static_assert(
    kSCORE_WEIGHT_UPTIME + kSCORE_WEIGHT_VOTE_ACC + kSCORE_WEIGHT_LATENCY +
            kSCORE_WEIGHT_CONSISTENCY + kSCORE_WEIGHT_SLASH_MULT ==
        100,
    "Composite score weights must sum to 100");

// ─── Minimum composite score to claim rewards ────────────────────────────────

/// Validators with a composite score below this threshold cannot ClaimReward.
/// Value in bps (out of 10 000). Default: 500 bps = 5 %.
constexpr std::uint32_t kMIN_COMPOSITE_SCORE_BPS = 500;

// ─── Slash offense codes ──────────────────────────────────────────────────────

constexpr std::uint32_t kSLASH_OFFENSE_DOUBLE_SIGN   = 1;  ///< Two diverging validations
constexpr std::uint32_t kSLASH_OFFENSE_ABSENCE        = 2;  ///< Sustained absence (3+ epochs)
constexpr std::uint32_t kSLASH_OFFENSE_INVALID_VOTE   = 3;  ///< Proven invalid vote

/// Fraction of bond slashed for double-sign (bps of current bonded amount).
constexpr std::uint32_t kSLASH_DOUBLE_SIGN_BPS  = kBPS_DENOM;  // 100 % — full slash
/// Fraction of bond slashed for sustained absence (bps).
constexpr std::uint32_t kSLASH_ABSENCE_BPS      = 2'500;       // 25 %
/// Fraction of bond slashed for invalid vote (bps).
constexpr std::uint32_t kSLASH_INVALID_VOTE_BPS = 5'000;       // 50 %

// ─── On-chain governance ──────────────────────────────────────────────────────

/// Governance proposal types (sfProposalType values)
constexpr std::uint32_t kPROPOSAL_TYPE_BURN_BPS = 1;  ///< Change sfCurrentBurnBps

/// Voting threshold: 67 % of sfAggregateCompositeScore must vote YES.
constexpr std::uint32_t kGOVERNANCE_SUPERMAJORITY_BPS = 6'700;

/// Governance voting window in ledgers (~7 days).
constexpr std::uint32_t kGOVERNANCE_VOTING_LEDGERS = 172'800;

}  // namespace xrpl
