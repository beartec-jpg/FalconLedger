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
#include <string>
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

/// Encode a Falcon key pair into a single portable secret string.
///
/// Unlike classical keys, a Falcon public key cannot be re-derived from its
/// secret key alone (liboqs exposes no such operation).  A usable Falcon
/// wallet secret must therefore carry *both* halves.  The returned value is a
/// hex string of the on-wire public-key blob (prefix byte + raw key) directly
/// followed by the raw secret-key bytes.
///
/// This is the value emitted by `wallet_propose` as `falcon_secret` and the
/// value accepted by `decodeFalconSecret` for offline signing.
std::string
encodeFalconSecret(PQPublicKey const& pk, PQSecretKey const& sk);

/// Decode a `falcon_secret` string produced by `encodeFalconSecret`.
///
/// The parameter set is determined from the leading prefix byte (0xFB =
/// Falcon-512, 0xFC = Falcon-1024) and the total length is validated against
/// the fixed public/secret key sizes for that set.
///
/// @returns the reconstructed {public_key, secret_key} pair, or nullopt if the
///          input is not a well-formed Falcon secret bundle.
std::optional<std::pair<PQPublicKey, PQSecretKey>>
decodeFalconSecret(std::string const& hex);

}  // namespace xrpl
