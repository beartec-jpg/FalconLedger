#include <xrpl/protocol/PublicKey.h>

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/contract.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PQPublicKey.h>
#include <xrpl/protocol/falcon.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/detail/secp256k1.h>
#include <xrpl/protocol/digest.h>
#include <xrpl/protocol/tokens.h>

#include <boost/multiprecision/number.hpp>

#include <ed25519.h>
#include <secp256k1.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <string>

namespace xrpl {

std::ostream&
operator<<(std::ostream& os, PublicKey const& pk)
{
    os << strHex(pk);
    return os;
}

template <>
std::optional<PublicKey>
parseBase58(TokenType type, std::string const& s)
{
    auto const result = decodeBase58Token(s, type);
    auto const pks = makeSlice(result);
    // Accept both classical and Falcon (PQ) keys.
    if (!signingPubKeyType(pks))
        return std::nullopt;
    return PublicKey(pks);
}

//------------------------------------------------------------------------------

// Parse a length-prefixed number
//  Format: 0x02 <length-byte> <number>
static std::optional<Slice>
sigPart(Slice& buf)
{
    if (buf.size() < 3 || buf[0] != 0x02)
        return std::nullopt;
    auto const len = buf[1];
    buf += 2;
    if (len > buf.size() || len < 1 || len > 33)
        return std::nullopt;
    // Can't be negative
    if ((buf[0] & 0x80) != 0)
        return std::nullopt;
    if (buf[0] == 0)
    {
        // Can't be zero
        if (len == 1)
            return std::nullopt;
        // Can't be padded
        if ((buf[1] & 0x80) == 0)
            return std::nullopt;
    }
    std::optional<Slice> number = Slice(buf.data(), len);
    buf += len;
    return number;
}

static std::string
sliceToHex(Slice const& slice)
{
    std::string s;
    if ((slice[0] & 0x80) != 0)
    {
        s.reserve(2 * (slice.size() + 2));
        s = "0x00";
    }
    else
    {
        s.reserve(2 * (slice.size() + 1));
        s = "0x";
    }
    for (int i = 0; i < slice.size(); ++i)
    {
        constexpr char kHEX[] = "0123456789ABCDEF";
        s += kHEX[((slice[i] & 0xf0) >> 4)];
        s += kHEX[((slice[i] & 0x0f) >> 0)];
    }
    return s;
}

/** Determine whether a signature is canonical.
    Canonical signatures are important to protect against signature morphing
    attacks.
    @param vSig the signature data
    @param sigLen the length of the signature
    @param strict_param whether to enforce strictly canonical semantics

    @note For more details please see:
    https://xrpl.org/transaction-malleability.html
    https://bitcointalk.org/index.php?topic=8392.msg127623#msg127623
    https://github.com/sipa/bitcoin/commit/58bc86e37fda1aec270bccb3df6c20fbd2a6591c
*/
std::optional<ECDSACanonicality>
ecdsaCanonicality(Slice const& sig)
{
    using uint264 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
        264,
        264,
        boost::multiprecision::signed_magnitude,
        boost::multiprecision::unchecked,
        void>>;

    static uint264 const kG(
        "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");  // NOLINT(readability-identifier-naming)

    // The format of a signature should be:
    // <30> <len> [ <02> <lenR> <R> ] [ <02> <lenS> <S> ]
    if ((sig.size() < 8) || (sig.size() > 72))
        return std::nullopt;
    if ((sig[0] != 0x30) || (sig[1] != (sig.size() - 2)))
        return std::nullopt;
    Slice p = sig + 2;
    auto r = sigPart(p);
    auto s = sigPart(p);
    if (!r || !s || !p.empty())
        return std::nullopt;

    uint264 const rNum(sliceToHex(*r));
    if (rNum >= kG)
        return std::nullopt;

    uint264 const sNum(sliceToHex(*s));
    if (sNum >= kG)
        return std::nullopt;

    // (R,S) and (R,G-S) are canonical,
    // but is fully canonical when S <= G-S
    auto const Sp = kG - sNum;  // NOLINT(readability-identifier-naming)
    if (sNum > Sp)
        return ECDSACanonicality::Canonical;
    return ECDSACanonicality::FullyCanonical;
}

static bool
ed25519Canonical(Slice const& sig)
{
    if (sig.size() != 64)
        return false;
    // Big-endian Order, the Ed25519 subgroup order
    // NOLINTNEXTLINE(readability-identifier-naming)
    std::uint8_t const Order[] = {
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0xDE, 0xF9, 0xDE, 0xA2, 0xF7,
        0x9C, 0xD6, 0x58, 0x12, 0x63, 0x1A, 0x5C, 0xF5, 0xD3, 0xED,
    };
    // Take the second half of signature
    // and byte-reverse it to big-endian.
    auto const le = sig.data() + 32;
    std::uint8_t S[32];  // NOLINT(readability-identifier-naming)
    std::reverse_copy(le, le + 32, S);
    // Must be less than Order
    return std::lexicographical_compare(S, S + 32, Order, Order + 32);
}

//------------------------------------------------------------------------------

PublicKey::PublicKey(Slice const& slice)
    : size_(slice.size())
{
    if (size_ == 0)
    {
        logicError(
            "PublicKey::PublicKey - Input slice cannot be empty");
    }

    // Accept both classical (33-byte) and PQ (Falcon) keys.
    if (!signingPubKeyType(slice))
        logicError("PublicKey::PublicKey invalid type");

    if (size_ <= kCLASSICAL_SIZE)
    {
        std::memcpy(inlineBuf_, slice.data(), size_);
    }
    else
    {
        heapBuf_.assign(slice.data(), slice.data() + size_);
    }
}

PublicKey::PublicKey(PublicKey const& other)
    : size_(other.size_)
    , heapBuf_(other.heapBuf_)
{
    if (heapBuf_.empty())
        std::memcpy(inlineBuf_, other.inlineBuf_, size_);
}

PublicKey&
PublicKey::operator=(PublicKey const& other)
{
    if (this != &other)
    {
        size_ = other.size_;
        heapBuf_ = other.heapBuf_;
        if (heapBuf_.empty())
            std::memcpy(inlineBuf_, other.inlineBuf_, size_);
    }
    return *this;
}

//------------------------------------------------------------------------------

std::optional<KeyType>
publicKeyType(Slice const& slice)
{
    if (slice.size() == 33)
    {
        if (slice[0] == 0xED)
            return KeyType::Ed25519;

        if (slice[0] == 0x02 || slice[0] == 0x03)
            return KeyType::Secp256k1;
    }

    // Post-quantum Falcon keys (variable length).
    return pqPublicKeyType(slice);
}

bool
verifyDigest(
    PublicKey const& publicKey,
    uint256 const& digest,
    Slice const& sig,
    bool mustBeFullyCanonical) noexcept
{
    if (publicKeyType(publicKey) != KeyType::Secp256k1)
        return false;
    auto const canonicality = ecdsaCanonicality(sig);
    if (!canonicality)
        return false;
    if (mustBeFullyCanonical && (*canonicality != ECDSACanonicality::FullyCanonical))
        return false;

    secp256k1_pubkey pubkeyImp;
    if (secp256k1_ec_pubkey_parse(
            secp256k1Context(),
            &pubkeyImp,
            reinterpret_cast<unsigned char const*>(publicKey.data()),
            publicKey.size()) != 1)
        return false;

    secp256k1_ecdsa_signature sigImp;
    if (secp256k1_ecdsa_signature_parse_der(
            secp256k1Context(),
            &sigImp,
            reinterpret_cast<unsigned char const*>(sig.data()),
            sig.size()) != 1)
        return false;
    if (*canonicality != ECDSACanonicality::FullyCanonical)
    {
        secp256k1_ecdsa_signature sigNorm;
        if (secp256k1_ecdsa_signature_normalize(secp256k1Context(), &sigNorm, &sigImp) != 1)
            return false;
        return secp256k1_ecdsa_verify(
                   secp256k1Context(),
                   &sigNorm,
                   reinterpret_cast<unsigned char const*>(digest.data()),
                   &pubkeyImp) == 1;
    }
    return secp256k1_ecdsa_verify(
               secp256k1Context(),
               &sigImp,
               reinterpret_cast<unsigned char const*>(digest.data()),
               &pubkeyImp) == 1;
}

bool
verify(PublicKey const& publicKey, Slice const& m, Slice const& sig) noexcept
{
    if (auto const type = publicKeyType(publicKey))
    {
        if (*type == KeyType::Secp256k1)
        {
            return verifyDigest(publicKey, sha512Half(m), sig);
        }
        if (*type == KeyType::Ed25519)
        {
            if (!ed25519Canonical(sig))
                return false;

            // We internally prefix Ed25519 keys with a 0xED
            // byte to distinguish them from secp256k1 keys
            // so when verifying the signature, we need to
            // first strip that prefix.
            return ed25519_sign_open(m.data(), m.size(), publicKey.data() + 1, sig.data()) == 0;
        }
        if (*type == KeyType::Falcon512 || *type == KeyType::Falcon1024)
        {
            try
            {
                return verifyFalcon(PQPublicKey(publicKey.slice()), m, sig);
            }
            catch (std::exception const&)
            {
                return false;
            }
        }
    }
    return false;
}

std::optional<KeyType>
signingPubKeyType(Slice const& slice)
{
    // Classical secp256k1/ed25519 (fixed 33-byte) keys first.
    if (auto const classical = publicKeyType(slice))
        return classical;

    // Post-quantum Falcon-512 / Falcon-1024 keys (variable length).
    return pqPublicKeyType(slice);
}

bool
verify(Slice const& publicKey, Slice const& m, Slice const& sig) noexcept
{
    try
    {
        // Post-quantum Falcon keys are detected by their prefix byte.  These
        // are variable length and cannot be held by the classical PublicKey
        // type, so they are routed directly to the Falcon verifier.
        if (pqPublicKeyType(publicKey))
        {
            return verifyFalcon(PQPublicKey(publicKey), m, sig);
        }

        // Classical secp256k1 / ed25519.
        if (publicKeyType(publicKey))
        {
            return verify(PublicKey(publicKey), m, sig);
        }
    }
    catch (std::exception const&)
    {
        // Any malformed key/signature is treated as a failed verification
        // rather than propagating an exception into consensus-critical code.
        return false;
    }

    return false;
}

NodeID
calcNodeID(PublicKey const& pk)
{
    static_assert(NodeID::kBYTES == sizeof(RipeshaHasher::result_type));

    RipeshaHasher h;
    h(pk.data(), pk.size());
    return NodeID{static_cast<RipeshaHasher::result_type>(h)};
}

bool
isValidNodeKey(Slice s) noexcept
{
    return pqPublicKeyType(s).has_value();
}

bool
isFalconSigningKey(Slice s) noexcept
{
    if (auto const t = signingPubKeyType(s))
        return *t == KeyType::Falcon512 || *t == KeyType::Falcon1024;
    return false;
}

std::optional<PublicKey>
parseValidatorPublicKey(std::string const& token)
{
    if (token.empty())
        return std::nullopt;

    if (auto const hex = strUnHex(token))
    {
        if (isValidNodeKey(makeSlice(*hex)))
            return PublicKey(makeSlice(*hex));
    }

    return std::nullopt;
}

AccountID
calcValidatorBondID(Slice s)
{
    // RIPEMD160(SHA256(key_blob)) — identical transform to calcAccountID()
    // but accepts any key length (classical 33-byte or Falcon variable-length).
    RipeshaHasher rsh;
    rsh(s.data(), s.size());
    return AccountID{static_cast<RipeshaHasher::result_type>(rsh)};
}

}  // namespace xrpl
