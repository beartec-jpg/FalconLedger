// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/protocol/PQSecretKey.h>

#include <xrpl/basics/contract.h>
#include <xrpl/crypto/secure_erase.h>

#include <stdexcept>

namespace xrpl {

PQSecretKey::PQSecretKey(KeyType type, std::vector<std::uint8_t> keyBytes)
    : type_(type)
    , blob_(std::move(keyBytes))
{
    std::size_t expected{};
    if (type == KeyType::Falcon512)
        expected = kFALCON512_SECKEY_BYTES;
    else if (type == KeyType::Falcon1024)
        expected = kFALCON1024_SECKEY_BYTES;
    else
        Throw<std::invalid_argument>("PQSecretKey: not a Falcon key type");

    if (blob_.size() != expected)
        Throw<std::invalid_argument>("PQSecretKey: wrong key length");
}

PQSecretKey::~PQSecretKey()
{
    // Use OPENSSL_cleanse via secureErase for guaranteed key material erasure.
    if (!blob_.empty())
        secureErase(blob_.data(), blob_.size());
}

}  // namespace xrpl
