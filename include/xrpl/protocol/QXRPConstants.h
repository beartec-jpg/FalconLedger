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

/// Genesis circulating account ("masterpassphrase") — holds kQXRP_GENESIS_ALLOCATION.
/// Classical secp256k1 signatures are permitted only for Payment txs from this
/// account so operators can bootstrap Falcon wallets on a Falcon-only network.
[[nodiscard]] AccountID const&
getGenesisCirculatingAccountID() noexcept;

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

/// First epoch that creates a non-zero claimable emission pool.
/// Epochs 1 .. (kQXRP_FIRST_EMISSION_EPOCH - 1) still close RewardEpoch state
/// (burn bps, LP stats) but schedule 0 treasury emission for bootstrap quiet period.
/// Mainnet launch spec: quiet through epoch 7; first unlock at epoch 8.
#ifdef QXRP_FIRST_EMISSION_EPOCH
constexpr std::uint32_t kQXRP_FIRST_EMISSION_EPOCH = QXRP_FIRST_EMISSION_EPOCH;
#else
constexpr std::uint32_t kQXRP_FIRST_EMISSION_EPOCH = 8;
#endif

// ─── CID (Continuous Inflationary Decline) emission ─────────────────────────

/// Reward epochs per calendar year (~52 × ~7 days).
constexpr std::uint32_t kQXRP_EPOCHS_PER_YEAR = 52;

/// Year-1 average treasury emission: 12 % (1 200 bps of treasury per year).
constexpr std::uint32_t kQXRP_CID_YEAR1_AVG_BPS = 1'200;

/// Year-5 average treasury emission: 4.5 % (450 bps of treasury per year).
constexpr std::uint32_t kQXRP_CID_YEAR5_AVG_BPS = 450;

/// Long-term yearly-average floor: 1.5 % (150 bps of treasury per year).
constexpr std::uint32_t kQXRP_CID_YEARLY_FLOOR_BPS = 150;

/// Per-epoch floor once the linear curve reaches the yearly floor (ceil(150 / 52)).
constexpr std::uint32_t kQXRP_CID_EPOCH_FLOOR_BPS = 3;

/// Linear per-epoch decline: numerator / denominator (bps per epoch).
/// Calibrated so year-1 sums to 1 200 bps and year-5 sums to 450 bps with no
/// intra-year reset — each epoch is ~0.069 bps lower than the previous one.
constexpr std::uint32_t kQXRP_CID_DECLINE_NUM = 750;
constexpr std::uint32_t kQXRP_CID_DECLINE_DEN = 10'816;

// ─── PoPL emission split (validator / vault LP / AMM LP) ─────────────────────
//
// Epoch emission is split three ways:
//   • Lend vault LPs  — +1% of emission per distinct vault share holder, cap 25%
//   • AMM / DEX LPs   — +1% per distinct AMM LP-token holder (XRP pairs), cap 25%
//   • Validators      — remainder (at least 50% when both LP baskets are full)
//
// Caps limit the *size of each basket*, not how many wallets may claim.
// Within each basket, payout is pro-rata by share balance (manual claim).

/// Vault-LP basket grows 1 % of total emission per active vault provider.
constexpr std::uint32_t kQXRP_POPL_LP_BPS_PER_PROVIDER = 100;

/// Provider count at which vault-LP basket stops growing (25 providers → 25 %).
constexpr std::uint32_t kQXRP_POPL_LP_MAX_PROVIDERS = 25;

/// Maximum vault-LP share of total emission (25 % = 2 500 bps).
constexpr std::uint32_t kQXRP_POPL_LP_MAX_BPS = 2'500;

/// AMM-LP basket grows 1 % of total emission per active AMM LP holder.
constexpr std::uint32_t kQXRP_POPL_AMM_LP_BPS_PER_PROVIDER = 100;

/// Provider count at which AMM-LP basket stops growing.
constexpr std::uint32_t kQXRP_POPL_AMM_LP_MAX_PROVIDERS = 25;

/// Maximum AMM-LP share of total emission (25 % = 2 500 bps).
constexpr std::uint32_t kQXRP_POPL_AMM_LP_MAX_BPS = 2'500;

// Legacy halving constants (retained for reference / tests only).
[[maybe_unused]] constexpr std::uint32_t kQXRP_EPOCHS_PER_HALVING = 208;
[[maybe_unused]] constexpr std::uint32_t kQXRP_INITIAL_EMISSION_BPS = 50;
[[maybe_unused]] constexpr std::uint32_t kQXRP_MIN_EMISSION_BPS = 1;

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

// ─── Account name service ────────────────────────────────────────────────────

/// Bond locked while holding a name (100 FALCON).
constexpr std::int64_t kNAME_BOND_DROPS = 100 * kDROPS_PER_XRP.drops();

/// Name length bounds (normalized form).
constexpr std::size_t kNAME_MIN_LEN = 3;
constexpr std::size_t kNAME_MAX_LEN = 32;

/// Cooldown after NameUnbond before NameRelease may return the bond.
/// One full network epoch (mainnet 172_800; fast-epoch rehearsal uses
/// kQXRP_LEDGERS_PER_EPOCH override).
constexpr std::uint32_t kNAME_UNBOND_LEDGERS = kQXRP_LEDGERS_PER_EPOCH;

// ─── Validator bond status codes ─────────────────────────────────────────────

constexpr std::uint8_t kBOND_STATUS_REGISTERED = 0;
constexpr std::uint8_t kBOND_STATUS_BONDED      = 1;
constexpr std::uint8_t kBOND_STATUS_UNBONDING   = 2;

// ─── Account name status codes ───────────────────────────────────────────────

constexpr std::uint8_t kNAME_STATUS_ACTIVE    = 0;
constexpr std::uint8_t kNAME_STATUS_RELEASING = 1;

// ─── Composite score weights (must sum to 100) ───────────────────────────────
//
// Measured factors (uptime / vote accuracy / latency / consistency) use the
// additive weights below and are divided by 100.  kSCORE_WEIGHT_SLASH_MULT is
// applied separately as a multiplicative slashMultiplier factor (bps / 10_000),
// not as an additive term in the raw average.
//
// Signals are independent and continuous (no fixed flat demerits such as
// "everyone who failed X loses 200"). Latency is relative to the earliest
// trusted signer; consistency uses max absence streak in the window.
// Composite is EMA-smoothed across scoring passes so recovery is incremental.

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

/// EMA blend for composite: weight of the *new* window sample in bps.
/// score = (raw * NEW + prev * (10000-NEW)) / 10000.  0 prev → raw only.
constexpr std::uint32_t kSCORE_EMA_NEW_BPS = 3'500;  // 35 % new / 65 % history

static_assert(kSCORE_EMA_NEW_BPS <= kBPS_DENOM, "EMA new weight must be <= 100%");

/// Relative latency: −1 bps per 10 ms behind earliest signer (≡ −100 bps/s),
/// continuous (not a single lump for "late"). Floor at 0.
constexpr std::uint32_t kLATENCY_PENALTY_BPS_PER_10MS = 1;

// ─── Active set (score-ranked proposers / reward-eligible cap) ───────────────

/// Top-K bonded validators by composite score remain reward-eligible
/// (sfCompositeScore kept). Others keep component metrics for transparency
/// but composite is cleared so they do not dilute the active set pool.
/// K must be >= the intended mainnet UNL size; raise only via protocol upgrade.
constexpr std::uint32_t kQXRP_ACTIVE_SET_K = 32;

// ─── Minimum composite score to claim rewards ────────────────────────────────

/// Validators with a composite score below this threshold cannot ClaimReward.
/// Value in bps (out of 10 000). Default: 500 bps = 5 %.
constexpr std::uint32_t kMIN_COMPOSITE_SCORE_BPS = 500;

// ─── Slash offense codes ──────────────────────────────────────────────────────

// NOTE (security C-01):
// Only DOUBLE_SIGN is enforced, and only with cryptographic STValidation
// evidence (same key, same ledger sequence, different ledger hashes).
// ABSENCE and INVALID_VOTE return temDISABLED until robust detection exists.

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
