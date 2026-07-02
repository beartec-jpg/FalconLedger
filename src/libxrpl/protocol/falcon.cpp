// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Falcon signature wrapper backed by liboqs (Open Quantum Safe).
//
// liboqs is a required dependency for qXRP: Falcon post-quantum signatures
// are not optional.  The amendment guard on ProofOfParticipation ensures
// on-chain Falcon keys are only accepted after validator-majority upgrade,
// but the crypto primitives are always compiled in.

#include <xrpl/protocol/falcon.h>

#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/contract.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/crypto/secure_erase.h>
#include <xrpl/protocol/KeyType.h>

#include <oqs/oqs.h>

#include <cstring>
#include <stdexcept>

namespace xrpl {

namespace {

char const*
oqsAlgName(KeyType type)
{
    if (type == KeyType::Falcon512)
        return OQS_SIG_alg_falcon_512;
    if (type == KeyType::Falcon1024)
        return OQS_SIG_alg_falcon_1024;
    Throw<std::invalid_argument>("oqsAlgName: not a Falcon type");
}

}  // anonymous namespace

bool
falconAvailable(KeyType type) noexcept
{
    if (type != KeyType::Falcon512 && type != KeyType::Falcon1024)
        return false;
    return OQS_SIG_alg_is_enabled(oqsAlgName(type));
}

std::optional<std::pair<PQPublicKey, PQSecretKey>>
generateFalconKeyPair(KeyType type)
{
    if (!falconAvailable(type))
        return std::nullopt;

    OQS_SIG* sig = OQS_SIG_new(oqsAlgName(type));
    if (!sig)
        return std::nullopt;

    std::vector<std::uint8_t> pubBuf(sig->length_public_key);
    std::vector<std::uint8_t> secBuf(sig->length_secret_key);

    OQS_STATUS rc = OQS_SIG_keypair(sig, pubBuf.data(), secBuf.data());
    OQS_SIG_free(sig);

    if (rc != OQS_SUCCESS)
        return std::nullopt;

    return std::make_pair(
        PQPublicKey(type, Slice(pubBuf.data(), pubBuf.size())),
        PQSecretKey(type, std::move(secBuf)));
}

std::vector<std::uint8_t>
signFalcon(PQSecretKey const& sk, Slice message)
{
    OQS_SIG* sig = OQS_SIG_new(oqsAlgName(sk.keyType()));
    if (!sig)
        Throw<std::runtime_error>("signFalcon: OQS_SIG_new failed");

    std::vector<std::uint8_t> sigBuf(sig->length_signature);
    std::size_t sigLen = sig->length_signature;

    OQS_STATUS rc = OQS_SIG_sign(
        sig,
        sigBuf.data(),
        &sigLen,
        message.data(),
        message.size(),
        sk.data());

    OQS_SIG_free(sig);

    if (rc != OQS_SUCCESS)
        Throw<std::runtime_error>("signFalcon: OQS_SIG_sign failed");

    sigBuf.resize(sigLen);
    return sigBuf;
}

bool
verifyFalcon(PQPublicKey const& pk, Slice message, Slice signature)
{
    if (!falconAvailable(pk.keyType()))
        return false;

    OQS_SIG* sig = OQS_SIG_new(oqsAlgName(pk.keyType()));
    if (!sig)
        return false;

    // The PQPublicKey blob starts with a 1-byte prefix, skip it.
    Slice rawPub = pk.slice();
    rawPub += 1;  // skip prefix byte

    OQS_STATUS rc = OQS_SIG_verify(
        sig,
        message.data(),
        message.size(),
        signature.data(),
        signature.size(),
        rawPub.data());

    OQS_SIG_free(sig);
    return rc == OQS_SUCCESS;
}

std::string
encodeFalconSecret(PQPublicKey const& pk, PQSecretKey const& sk)
{
    // Layout: [public-key on-wire blob (prefix + raw)] || [raw secret-key bytes]
    auto const ps = pk.slice();
    std::vector<std::uint8_t> buf;
    buf.reserve(ps.size() + sk.size());
    buf.insert(buf.end(), ps.data(), ps.data() + ps.size());
    buf.insert(buf.end(), sk.data(), sk.data() + sk.size());
    auto const encoded = strHex(buf);
    secureErase(buf.data(), buf.size());
    return encoded;
}

std::optional<std::pair<PQPublicKey, PQSecretKey>>
decodeFalconSecret(std::string const& hex)
{
    auto const bytes = strUnHex(hex);
    if (!bytes || bytes->empty())
        return std::nullopt;

    auto const& b = *bytes;
    std::uint8_t const prefix = static_cast<std::uint8_t>(b[0]);

    KeyType type{};
    std::size_t pubTotal{};
    std::size_t secLen{};

    if (prefix == PQPublicKey::kFALCON512_PREFIX)
    {
        type = KeyType::Falcon512;
        pubTotal = kFALCON512_PUBKEY_BYTES + 1;
        secLen = kFALCON512_SECKEY_BYTES;
    }
    else if (prefix == PQPublicKey::kFALCON1024_PREFIX)
    {
        type = KeyType::Falcon1024;
        pubTotal = kFALCON1024_PUBKEY_BYTES + 1;
        secLen = kFALCON1024_SECKEY_BYTES;
    }
    else
    {
        return std::nullopt;
    }

    if (b.size() != pubTotal + secLen)
        return std::nullopt;

    try
    {
        auto const* base = reinterpret_cast<std::uint8_t const*>(b.data());
        PQPublicKey pk(Slice(base, pubTotal));
        std::vector<std::uint8_t> secBytes(base + pubTotal, base + pubTotal + secLen);
        PQSecretKey sk(type, std::move(secBytes));
        secureErase(secBytes.data(), secBytes.size());
        return std::make_pair(std::move(pk), std::move(sk));
    }
    catch (std::exception const&)
    {
        return std::nullopt;
    }
}

}  // namespace xrpl
