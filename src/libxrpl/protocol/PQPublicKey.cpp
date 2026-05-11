// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/protocol/PQPublicKey.h>

#include <xrpl/basics/strHex.h>
#include <xrpl/basics/contract.h>

#include <stdexcept>

namespace xrpl {

PQPublicKey::PQPublicKey(Slice s)
{
    if (s.empty())
        Throw<std::invalid_argument>("PQPublicKey: empty slice");

    std::uint8_t const prefix = s[0];
    if (prefix == kFALCON512_PREFIX)
    {
        type_ = KeyType::Falcon512;
        if (s.size() != kFALCON512_PUBKEY_BYTES + 1)
            Throw<std::invalid_argument>("PQPublicKey: bad Falcon-512 length");
    }
    else if (prefix == kFALCON1024_PREFIX)
    {
        type_ = KeyType::Falcon1024;
        if (s.size() != kFALCON1024_PUBKEY_BYTES + 1)
            Throw<std::invalid_argument>("PQPublicKey: bad Falcon-1024 length");
    }
    else
    {
        Throw<std::invalid_argument>("PQPublicKey: unknown prefix byte");
    }

    blob_.assign(s.data(), s.data() + s.size());
}

PQPublicKey::PQPublicKey(KeyType type, Slice rawKey)
    : type_(type)
{
    std::uint8_t prefix{};
    std::size_t expectedSize{};

    if (type == KeyType::Falcon512)
    {
        prefix = kFALCON512_PREFIX;
        expectedSize = kFALCON512_PUBKEY_BYTES;
    }
    else if (type == KeyType::Falcon1024)
    {
        prefix = kFALCON1024_PREFIX;
        expectedSize = kFALCON1024_PUBKEY_BYTES;
    }
    else
    {
        Throw<std::invalid_argument>("PQPublicKey: not a Falcon key type");
    }

    if (rawKey.size() != expectedSize)
        Throw<std::invalid_argument>("PQPublicKey: raw key has wrong length");

    blob_.reserve(1 + rawKey.size());
    blob_.push_back(prefix);
    blob_.insert(blob_.end(), rawKey.data(), rawKey.data() + rawKey.size());
}

std::string
PQPublicKey::toDebugString() const
{
    // Show type tag + first 8 bytes as hex
    std::string out(to_string(type_));
    out += ':';
    auto const limit = std::min(blob_.size(), std::size_t{9});
    out += strHex(Slice(blob_.data(), limit));
    out += "...";
    return out;
}

std::optional<KeyType>
pqPublicKeyType(Slice s) noexcept
{
    if (s.empty())
        return std::nullopt;
    if (s[0] == PQPublicKey::kFALCON512_PREFIX)
        return KeyType::Falcon512;
    if (s[0] == PQPublicKey::kFALCON1024_PREFIX)
        return KeyType::Falcon1024;
    return std::nullopt;
}

}  // namespace xrpl
