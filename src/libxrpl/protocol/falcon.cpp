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

#include <xrpl/basics/contract.h>
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

}  // namespace xrpl
