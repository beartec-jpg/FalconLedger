#pragma once

#include <xrpl/basics/Slice.h>
#include <xrpl/beast/net/IPEndpoint.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/STExchange.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/json_get_or_throw.h>
#include <xrpl/protocol/tokens.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <vector>

namespace xrpl {

/** A public key.

    Public keys are used in the public-key cryptography
    system used to verify signatures attached to messages.

    The format of the public key is XRPL specific,
    information needed to determine the cryptosystem
    parameters used is stored inside the key.

    Supported key types:

        secp256k1   – 33 bytes (prefix 0x02 or 0x03)
        ed25519     – 33 bytes (prefix 0xED)
        falcon512   – 898 bytes (prefix 0xFB)
        falcon1024  – 1794 bytes (prefix 0xFC)

    Post-quantum Falcon keys are variable-length and stored
    dynamically.  The class uses a small inline buffer for
    classical 33-byte keys to avoid heap allocation in the
    common case.
*/
class PublicKey
{
protected:
    // Classical keys are 33 bytes; Falcon keys are much larger.
    static constexpr std::size_t kCLASSICAL_SIZE = 33;
    std::size_t size_ = 0;
    // Inline buffer for classical keys (avoids heap allocation).
    std::uint8_t inlineBuf_[kCLASSICAL_SIZE]{};
    // Heap buffer for post-quantum keys that exceed inline capacity.
    std::vector<std::uint8_t> heapBuf_;

    [[nodiscard]] std::uint8_t const*
    activeData() const noexcept
    {
        return heapBuf_.empty() ? inlineBuf_ : heapBuf_.data();
    }

public:
    using const_iterator = std::uint8_t const*;

    // Backward-compat constant (33 bytes for classical keys).
    static constexpr std::size_t kSIZE = kCLASSICAL_SIZE;

public:
    PublicKey() = delete;

    PublicKey(PublicKey const& other);
    PublicKey&
    operator=(PublicKey const& other);

    /** Create a public key.

        Accepts both classical (33-byte) and post-quantum Falcon keys.
        Preconditions:
            signingPubKeyType(slice) != std::nullopt
    */
    explicit PublicKey(Slice const& slice);

    [[nodiscard]] std::uint8_t const*
    data() const noexcept
    {
        return activeData();
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] const_iterator
    begin() const noexcept
    {
        return activeData();
    }

    [[nodiscard]] const_iterator
    cbegin() const noexcept
    {
        return activeData();
    }

    [[nodiscard]] const_iterator
    end() const noexcept
    {
        return activeData() + size_;
    }

    [[nodiscard]] const_iterator
    cend() const noexcept
    {
        return activeData() + size_;
    }

    [[nodiscard]] Slice
    slice() const noexcept
    {
        return {activeData(), size_};
    }

    operator Slice() const noexcept
    {
        return slice();
    }

    /** Returns true if this is a post-quantum (Falcon) key. */
    [[nodiscard]] bool
    isPQ() const noexcept
    {
        return size_ > kCLASSICAL_SIZE;
    }
};

/** Print the public key to a stream.
 */
std::ostream&
operator<<(std::ostream& os, PublicKey const& pk);

inline bool
operator==(PublicKey const& lhs, PublicKey const& rhs)
{
    return lhs.size() == rhs.size() &&
        std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool
operator<(PublicKey const& lhs, PublicKey const& rhs)
{
    return std::lexicographical_compare(
        lhs.data(), lhs.data() + lhs.size(), rhs.data(), rhs.data() + rhs.size());
}

template <class Hasher>
void
hash_append(Hasher& h, PublicKey const& pk)
{
    h(pk.data(), pk.size());
}

template <>
struct STExchange<STBlob, PublicKey>
{
    explicit STExchange() = default;

    using value_type = PublicKey;

    static void
    get(std::optional<value_type>& t, STBlob const& u)
    {
        t.emplace(Slice(u.data(), u.size()));
    }

    static std::unique_ptr<STBlob>
    set(SField const& f, PublicKey const& t)
    {
        return std::make_unique<STBlob>(f, t.data(), t.size());
    }
};

//------------------------------------------------------------------------------

inline std::string
toBase58(TokenType type, PublicKey const& pk)
{
    return encodeBase58Token(type, pk.data(), pk.size());
}

template <>
std::optional<PublicKey>
parseBase58(TokenType type, std::string const& s);

enum class ECDSACanonicality { Canonical, FullyCanonical };

/** Determines the canonicality of a signature.

    A canonical signature is in its most reduced form.
    For example the R and S components do not contain
    additional leading zeroes. However, even in
    canonical form, (R,S) and (R,G-S) are both
    valid signatures for message M.

    Therefore, to prevent malleability attacks we
    define a fully canonical signature as one where:

        R < G - S

    where G is the curve order.

    This routine returns std::nullopt if the format
    of the signature is invalid (for example, the
    points are encoded incorrectly).

    @return std::nullopt if the signature fails
            validity checks.

    @note Only the format of the signature is checked,
          no verification cryptography is performed.
*/
std::optional<ECDSACanonicality>
ecdsaCanonicality(Slice const& sig);

/** Returns the type of public key.

    @return std::nullopt If the public key does not
            represent a known type.
*/
/** @{ */
[[nodiscard]] std::optional<KeyType>
publicKeyType(Slice const& slice);

[[nodiscard]] inline std::optional<KeyType>
publicKeyType(PublicKey const& publicKey)
{
    return publicKeyType(publicKey.slice());
}
/** @} */

/** Verify a secp256k1 signature on the digest of a message. */
[[nodiscard]] bool
verifyDigest(
    PublicKey const& publicKey,
    uint256 const& digest,
    Slice const& sig,
    bool mustBeFullyCanonical = true) noexcept;

/** Verify a signature on a message.
    With secp256k1 signatures, the data is first hashed with
    SHA512-Half, and the resulting digest is signed.
*/
[[nodiscard]] bool
verify(PublicKey const& publicKey, Slice const& m, Slice const& sig) noexcept;

/** Returns the key type of a transaction signing public key blob.

    Unlike @c publicKeyType(), this also recognizes post-quantum Falcon
    keys (0xFB / 0xFC prefix), which are variable length and therefore
    cannot be represented by the fixed-size @c PublicKey class.

    @return std::nullopt if the slice is not a recognized classical or
            Falcon signing key.
*/
[[nodiscard]] std::optional<KeyType>
signingPubKeyType(Slice const& slice);

/** Verify a signature on a message using a raw signing-key blob.

    Accepts both classical (secp256k1/ed25519, 33-byte) signing keys and
    post-quantum Falcon (0xFB / 0xFC) signing keys, routing to the correct
    verifier based on the key prefix.  Returns false (never throws) for any
    unrecognized or malformed key, signature, or message.

    This is the entry point used by the transaction signature checks so that
    qXRP transactions may be signed with Falcon post-quantum keys.
*/
[[nodiscard]] bool
verify(Slice const& publicKey, Slice const& m, Slice const& sig) noexcept;

/** Derive the AccountID for a transaction signing public key blob.

    Computes RIPEMD160(SHA256(blob)) — the same transform used by
    @c calcAccountID(PublicKey) — but accepts a raw slice so the caller does
    not need to branch on classical vs. Falcon key length.  This makes the
    account-authorization rules (master key, regular key, signer list) work
    identically for classical and post-quantum signing keys.
*/
AccountID
calcAccountID(Slice const& signingPubKey);

/** Calculate the 160-bit node ID from a node public key. */
NodeID
calcNodeID(PublicKey const&);

// VFALCO This belongs in AccountID.h but
//        is here because of header issues
AccountID
calcAccountID(PublicKey const& pk);

/// Returns true if @a s encodes a valid node public key.
///
/// Accepts classical 33-byte keys (secp256k1, ed25519) and
/// post-quantum Falcon-512 / Falcon-1024 keys (0xFB / 0xFC prefix).
/// Use this instead of @c publicKeyType() when validating validator
/// node keys in qXRP transactions.
[[nodiscard]] bool
isValidNodeKey(Slice s) noexcept;

/// Derive the bond-entry AccountID for any valid node public key.
///
/// For both classical and post-quantum keys the result is
/// RIPEMD160(SHA256(key_blob)) — the same transform used by
/// @c calcAccountID(PublicKey).  This overload accepts a raw slice
/// so callers do not need to branch on key type.
///
/// Precondition: @c isValidNodeKey(s) must be true.
AccountID
calcValidatorBondID(Slice s);

inline std::string
getFingerprint(
    beast::IP::Endpoint const& address,
    std::optional<PublicKey> const& publicKey = std::nullopt,
    std::optional<std::string> const& id = std::nullopt)
{
    std::stringstream ss;
    ss << "IP Address: " << address;
    if (publicKey.has_value())
    {
        ss << ", Public Key: " << toBase58(TokenType::NodePublic, *publicKey);
    }
    if (id.has_value())
    {
        ss << ", Id: " << id.value();
    }
    return ss.str();
}
}  // namespace xrpl

//------------------------------------------------------------------------------

namespace json {
template <>
inline xrpl::PublicKey
getOrThrow(json::Value const& v, xrpl::SField const& field)
{
    using namespace xrpl;
    std::string const b58 = getOrThrow<std::string>(v, field);
    if (auto pubKeyBlob = strUnHex(b58);
        pubKeyBlob.has_value() && publicKeyType(makeSlice(*pubKeyBlob)))
    {
        return PublicKey{makeSlice(*pubKeyBlob)};
    }
    for (auto const tokenType : {TokenType::NodePublic, TokenType::AccountPublic})
    {
        if (auto const pk = parseBase58<PublicKey>(tokenType, b58))
            return *pk;
    }
    Throw<JsonTypeMismatchError>(field.getJsonName(), "PublicKey");
}
}  // namespace json
