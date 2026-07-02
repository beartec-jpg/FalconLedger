#pragma once

#include <xrpl/basics/Log.h>
#include <xrpl/beast/utility/instrumentation.h>
#include <xrpl/protocol/PQPublicKey.h>
#include <xrpl/protocol/PQSecretKey.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Units.h>
#include <xrpl/protocol/falcon.h>

#include <cstdint>
#include <optional>
#include <sstream>

namespace xrpl {

// Validation flags

// This is a full (as opposed to a partial) validation
constexpr std::uint32_t kVF_FULL_VALIDATION = 0x00000001;

// The signature is fully canonical
constexpr std::uint32_t kVF_FULLY_CANONICAL_SIG = 0x80000000;

class STValidation final : public STObject, public CountedObject<STValidation>
{
    bool trusted_ = false;

    // Determines the validity of the signature in this validation; unseated
    // optional if we haven't yet checked it, a boolean otherwise.
    mutable std::optional<bool> valid_;

    // The public key associated with the key used to sign this validation.
    // Variable-length to support both classical (33-byte) and Falcon keys.
    PublicKey const signingPubKey_;

    // The ID of the validator that issued this validation. For validators
    // that use manifests this will be derived from the master public key.
    NodeID const nodeID_;

    NetClock::time_point seenTime_;

public:
    /** Construct a STValidation from a peer from serialized data.

        @param sit Iterator over serialized data
        @param lookupNodeID Invocable with signature
                               NodeID(PublicKey const&)
                            used to find the Node ID based on the public key
                            that signed the validation. For manifest based
                            validators, this should be the NodeID of the master
                            public key.
        @param checkSignature Whether to verify the data was signed properly

        @note Throws if the object is not valid
    */
    template <class LookupNodeID>
    STValidation(SerialIter& sit, LookupNodeID&& lookupNodeID, bool checkSignature);

    /** Construct, sign and trust a new STValidation issued by this node.

        @param signTime When the validation is signed
        @param publicKey The current signing public key
        @param secretKey The current signing secret key
        @param nodeID ID corresponding to node's public master key
        @param f callback function to "fill" the validation with necessary data
    */
    template <typename F>
    STValidation(
        NetClock::time_point signTime,
        PublicKey const& pk,
        SecretKey const& sk,
        NodeID const& nodeID,
        F&& f);

    /** Construct, sign and trust a new STValidation with Falcon PQ keys. */
    template <typename F>
    STValidation(
        NetClock::time_point signTime,
        PQPublicKey const& pk,
        PQSecretKey const& sk,
        NodeID const& nodeID,
        F&& f);

    // Hash of the validated ledger
    uint256
    getLedgerHash() const;

    // Hash of consensus transaction set used to generate ledger
    uint256
    getConsensusHash() const;

    NetClock::time_point
    getSignTime() const;

    NetClock::time_point
    getSeenTime() const noexcept;

    PublicKey const&
    getSignerPublic() const noexcept;

    NodeID const&
    getNodeID() const noexcept;

    bool
    isValid() const noexcept;

    bool
    isFull() const noexcept;

    bool
    isTrusted() const noexcept;

    uint256
    getSigningHash() const;

    void
    setTrusted();

    void
    setUntrusted();

    void
    setSeen(NetClock::time_point s);

    Blob
    getSerialized() const;

    Blob
    getSignature() const;

    std::string
    render() const
    {
        std::stringstream ss;
        ss << "validation: " << " ledger_hash: " << getLedgerHash()
           << " consensus_hash: " << getConsensusHash()
           << " sign_time: " << to_string(getSignTime())
           << " seen_time: " << to_string(getSeenTime())
           << " signer_public_key: " << getSignerPublic() << " node_id: " << getNodeID()
           << " is_valid: " << isValid() << " is_full: " << isFull()
           << " is_trusted: " << isTrusted() << " signing_hash: " << getSigningHash()
           << " base58: " << toBase58(TokenType::NodePublic, getSignerPublic());
        return ss.str();
    }

private:
    static SOTemplate const&
    validationFormat();

    STBase*
    copy(std::size_t n, void* buf) const override;
    STBase*
    move(std::size_t n, void* buf) override;

    friend class detail::STVar;
};

template <class LookupNodeID>
STValidation::STValidation(SerialIter& sit, LookupNodeID&& lookupNodeID, bool checkSignature)
    : STObject(validationFormat(), sit, sfValidation)
    , signingPubKey_([this]() {
        auto const spk = getFieldVL(sfSigningPubKey);

        auto const keyType = signingPubKeyType(makeSlice(spk));
        if (!keyType ||
            (*keyType != KeyType::Falcon512 && *keyType != KeyType::Falcon1024))
            Throw<std::runtime_error>("Invalid public key in validation");

        return PublicKey{makeSlice(spk)};
    }())
    , nodeID_(lookupNodeID(signingPubKey_))
{
    if (checkSignature && !isValid())
    {
        JLOG(debugLog().error()) << "Invalid signature in validation: "
                                 << getJson(JsonOptions::Values::None);
        Throw<std::runtime_error>("Invalid signature in validation");
    }

    XRPL_ASSERT(nodeID_.isNonZero(), "xrpl::STValidation::STValidation(SerialIter) : nonzero node");
}

/** Construct, sign and trust a new STValidation issued by this node.

    @param signTime When the validation is signed
    @param publicKey The current signing public key
    @param secretKey The current signing secret key
    @param nodeID ID corresponding to node's public master key
    @param f callback function to "fill" the validation with necessary data
*/
template <typename F>
STValidation::STValidation(
    NetClock::time_point signTime,
    PublicKey const& pk,
    SecretKey const& sk,
    NodeID const& nodeID,
    F&& f)
    : STObject(validationFormat(), sfValidation)
    , signingPubKey_(pk)
    , nodeID_(nodeID)
    , seenTime_(signTime)
{
    XRPL_ASSERT(
        nodeID_.isNonZero(),
        "xrpl::STValidation::STValidation(PublicKey, SecretKey) : nonzero "
        "node");

    // Accept both classical (secp256k1/ed25519) and PQ (Falcon) keys.
    auto const keyType = signingPubKeyType(pk.slice());
    if (!keyType)
        logicError("STValidation: invalid signing key type");

    setFieldVL(sfSigningPubKey, pk.slice());
    setFieldU32(sfSigningTime, signTime.time_since_epoch().count());

    // Perform additional initialization
    f(*this);

    // Finally, sign the validation and mark it as trusted:
    setFlag(kVF_FULLY_CANONICAL_SIG);

    if (*keyType == KeyType::Falcon512 || *keyType == KeyType::Falcon1024)
    {
        // Falcon signs the raw hash bytes (not a digest-then-sign scheme).
        auto const hash = getSigningHash();
        auto const hashSlice = Slice(hash.data(), hash.size());
        // Reconstruct PQPublicKey / PQSecretKey from the stored key data.
        // For Falcon, the SecretKey parameter carries the PQ secret in the
        // node identity's pqSecretKey_ field, but here we receive a
        // classical SecretKey placeholder.  The actual Falcon signing uses
        // signFalcon which requires PQSecretKey — so we use the
        // Slice-based sign approach: the caller must ensure the SecretKey
        // is actually a PQ secret.  See the pqSign overload below.
        logicError(
            "STValidation: Use the PQPublicKey/PQSecretKey constructor "
            "overload for Falcon validation signing");
    }
    else
    {
        setFieldVL(sfSignature, signDigest(pk, sk, getSigningHash()));
    }
    setTrusted();

    // Check to ensure that all required fields are present.
    for (auto const& e : validationFormat())
    {
        if (e.style() == SoeRequired && !isFieldPresent(e.sField()))
            logicError("Required field '" + e.sField().getName() + "' missing from validation.");
    }

    // We just signed this, so it should be valid.
    valid_ = true;
}

/** Construct, sign and trust a new STValidation with Falcon PQ keys.

    This overload is used when the node has a Falcon key pair for
    validation signing.
*/
template <typename F>
STValidation::STValidation(
    NetClock::time_point signTime,
    PQPublicKey const& pk,
    PQSecretKey const& sk,
    NodeID const& nodeID,
    F&& f)
    : STObject(validationFormat(), sfValidation)
    , signingPubKey_(pk.slice())
    , nodeID_(nodeID)
    , seenTime_(signTime)
{
    XRPL_ASSERT(
        nodeID_.isNonZero(),
        "xrpl::STValidation::STValidation(PQPublicKey, PQSecretKey) : nonzero node");

    setFieldVL(sfSigningPubKey, pk.slice());
    setFieldU32(sfSigningTime, signTime.time_since_epoch().count());

    // Perform additional initialization
    f(*this);

    // Sign with Falcon.
    setFlag(kVF_FULLY_CANONICAL_SIG);
    auto const hash = getSigningHash();
    auto const hashSlice = Slice(hash.data(), hash.size());
    auto sig = signFalcon(sk, hashSlice);
    setFieldVL(sfSignature, makeSlice(sig));
    setTrusted();

    // Check to ensure that all required fields are present.
    for (auto const& e : validationFormat())
    {
        if (e.style() == SoeRequired && !isFieldPresent(e.sField()))
            logicError("Required field '" + e.sField().getName() + "' missing from validation.");
    }

    valid_ = true;
}

inline PublicKey const&
STValidation::getSignerPublic() const noexcept
{
    return signingPubKey_;
}

inline NodeID const&
STValidation::getNodeID() const noexcept
{
    return nodeID_;
}

inline bool
STValidation::isTrusted() const noexcept
{
    return trusted_;
}

inline void
STValidation::setTrusted()
{
    trusted_ = true;
}

inline void
STValidation::setUntrusted()
{
    trusted_ = false;
}

inline void
STValidation::setSeen(NetClock::time_point s)
{
    seenTime_ = s;
}

}  // namespace xrpl
