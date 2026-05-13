// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Thin C++ wrapper for Falcon PQC signatures via liboqs.
//
// Key generation, signing, and verification are exposed as free functions
// that mirror the interface already used for classical keys in
// include/xrpl/protocol/Sign.h.

#pragma once

#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/PQPublicKey.h>
#include <xrpl/protocol/PQSecretKey.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace xrpl {

/// Generate a Falcon key pair using secure random entropy.
///
/// @param type  Must be KeyType::Falcon512 or KeyType::Falcon1024.
/// @returns     {public_key, secret_key} pair, or nullopt if liboqs is
///              unavailable for the requested parameter set.
std::optional<std::pair<PQPublicKey, PQSecretKey>>
generateFalconKeyPair(KeyType type);

/// Sign `message` with `sk`.
///
/// Returns the detached signature as a byte vector.
/// The signature length is at most:
///   Falcon-512:  809 bytes
///   Falcon-1024: 1577 bytes
std::vector<std::uint8_t>
signFalcon(PQSecretKey const& sk, Slice message);

/// Verify a detached Falcon signature.
///
/// @param pk        Public key matching the signer's secret key.
/// @param message   The original signed message.
/// @param signature The detached signature produced by signFalcon().
/// @returns         true iff the signature is valid.
bool
verifyFalcon(PQPublicKey const& pk, Slice message, Slice signature);

/// Returns true when the current build includes liboqs with support for
/// the requested Falcon parameter set.
bool
falconAvailable(KeyType type) noexcept;

}  // namespace xrpl
