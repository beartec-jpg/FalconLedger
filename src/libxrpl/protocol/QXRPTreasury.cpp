// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
//
// Single implementation point for getTreasuryAccountID().
//
// All other translation units that need the treasury AccountID should call
// this function rather than re-deriving it inline, to ensure there is one
// canonical source of truth.

#include <xrpl/protocol/QXRPConstants.h>

#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>

namespace xrpl {

AccountID const&
getTreasuryAccountID() noexcept
{
    // Computed once from the well-known public seed.  The account is
    // controlled exclusively by the ProofOfParticipation amendment logic;
    // no private key is exposed to any operator.
    static AccountID const kID = calcAccountID(
        generateKeyPair(KeyType::Secp256k1, generateSeed(kQXRP_TREASURY_SEED))
            .first);
    return kID;
}

AccountID const&
getGenesisCirculatingAccountID() noexcept
{
    static AccountID const kID = calcAccountID(
        generateKeyPair(KeyType::Secp256k1, generateSeed("masterpassphrase")).first);
    return kID;
}

}  // namespace xrpl
