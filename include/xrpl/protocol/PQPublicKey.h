// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Post-quantum public key class for Falcon-512 and Falcon-1024.
//
// This class is intentionally separate from the classical `PublicKey` (which
// is a fixed 33-byte type).  Code that must handle both classical and
// post-quantum keys uses the `CryptoKey` variant defined below.

#pragma once

#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/KeyType.h>

#include <cstdint>
#include <string>
#include <vector>

namespace xrpl {

/// Maximum raw byte sizes for supported Falcon variants.
inline constexpr std::size_t kFALCON512_PUBKEY_BYTES  = 897;
inline constexpr std::size_t kFALCON1024_PUBKEY_BYTES = 1793;

/// Variable-length post-quantum public key.
///
/// The first byte of the on-wire encoding is a discriminator that identifies
/// the Falcon parameter set:
///   0xFB  Falcon-512
///   0xFC  Falcon-1024
///
/// This prefix byte was chosen to be distinct from the prefixes used by the
/// existing classical key types (0x02/0x03 for secp256k1, 0xED for ed25519).
class PQPublicKey
{
public:
    static constexpr std::uint8_t kFALCON512_PREFIX  = 0xFB;
    static constexpr std::uint8_t kFALCON1024_PREFIX = 0xFC;

    PQPublicKey() = delete;

    /// Construct from a raw byte blob that already includes the prefix byte.
    explicit PQPublicKey(Slice s);

    /// Construct from a raw key blob + explicit type tag.
    explicit PQPublicKey(KeyType type, Slice rawKey);

    [[nodiscard]] KeyType
    keyType() const noexcept
    {
        return type_;
    }

    /// Full on-wire encoding: prefix byte + raw key bytes.
    [[nodiscard]] Slice
    slice() const noexcept
    {
        return Slice(blob_.data(), blob_.size());
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return blob_.size();
    }

    bool
    operator==(PQPublicKey const& rhs) const noexcept
    {
        return blob_ == rhs.blob_;
    }

    bool
    operator!=(PQPublicKey const& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool
    operator<(PQPublicKey const& rhs) const noexcept
    {
        return blob_ < rhs.blob_;
    }

    /// Returns a string form suitable for logging (hex-encoded, truncated).
    [[nodiscard]] std::string
    toDebugString() const;

private:
    KeyType type_;
    std::vector<std::uint8_t> blob_;  // prefix byte + raw key bytes
};

/// Detect a PQPublicKey from its leading prefix byte.
/// Returns nullopt when the byte does not match a known Falcon prefix.
std::optional<KeyType>
pqPublicKeyType(Slice s) noexcept;

}  // namespace xrpl
