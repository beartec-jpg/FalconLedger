// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Post-quantum secret key for Falcon-512 and Falcon-1024.
//
// Memory containing the key material is securely zeroed on destruction,
// matching the guarantee provided by the classical `SecretKey`.

#pragma once

#include <xrpl/protocol/KeyType.h>

#include <cstdint>
#include <vector>

namespace xrpl {

/// Maximum raw byte sizes for Falcon secret keys.
inline constexpr std::size_t kFALCON512_SECKEY_BYTES  = 1281;
inline constexpr std::size_t kFALCON1024_SECKEY_BYTES = 2305;

class PQSecretKey
{
public:
    PQSecretKey() = delete;

    explicit PQSecretKey(KeyType type, std::vector<std::uint8_t> keyBytes);

    /// Destructor zeroes the key material before freeing.
    ~PQSecretKey();

    // Non-copyable to prevent accidental duplication of key material.
    PQSecretKey(PQSecretKey const&) = delete;
    PQSecretKey& operator=(PQSecretKey const&) = delete;

    // Moveable.
    PQSecretKey(PQSecretKey&&) noexcept = default;
    PQSecretKey& operator=(PQSecretKey&&) noexcept = default;

    [[nodiscard]] KeyType
    keyType() const noexcept
    {
        return type_;
    }

    [[nodiscard]] std::uint8_t const*
    data() const noexcept
    {
        return blob_.data();
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return blob_.size();
    }

private:
    KeyType type_;
    std::vector<std::uint8_t> blob_;
};

}  // namespace xrpl
