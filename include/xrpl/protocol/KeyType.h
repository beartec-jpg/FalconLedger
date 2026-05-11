#pragma once

#include <optional>
#include <string>

namespace xrpl {

enum class KeyType {
    Secp256k1 = 0,
    Ed25519 = 1,
    // qXRP post-quantum key types
    Falcon512  = 2,
    Falcon1024 = 3,
};

inline std::optional<KeyType>
keyTypeFromString(std::string const& s)
{
    if (s == "secp256k1")
        return KeyType::Secp256k1;

    if (s == "ed25519")
        return KeyType::Ed25519;

    if (s == "falcon512")
        return KeyType::Falcon512;

    if (s == "falcon1024")
        return KeyType::Falcon1024;

    return {};
}

inline char const*
to_string(KeyType type)
{
    if (type == KeyType::Secp256k1)
        return "secp256k1";

    if (type == KeyType::Ed25519)
        return "ed25519";

    if (type == KeyType::Falcon512)
        return "falcon512";

    if (type == KeyType::Falcon1024)
        return "falcon1024";

    return "INVALID";
}

template <class Stream>
inline Stream&
operator<<(Stream& s, KeyType type)
{
    return s << to_string(type);
}

}  // namespace xrpl
