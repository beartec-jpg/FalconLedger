#include <xrpld/rpc/Context.h>

#include <xrpl/json/json_value.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PQPublicKey.h>
#include <xrpl/protocol/PQSecretKey.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/RPCErr.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/falcon.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/tokens.h>

#include <optional>

namespace xrpl {

static std::optional<Seed>
validationSeed(json::Value const& params)
{
    if (!params.isMember(jss::secret))
        return randomSeed();

    return parseGenericSeed(params[jss::secret].asString());
}

// {
//   secret: <string>   // optional
//   key_type: <string> // optional, defaults to "falcon512"
// }
//
// This command requires Role::ADMIN access because it makes
// no sense to ask an untrusted server for this.
json::Value
doValidationCreate(RPC::JsonContext& context)
{
    json::Value obj(json::ValueType::Object);

    // Check for key_type parameter; default to Falcon-512.
    KeyType keyType = KeyType::Falcon512;
    if (context.params.isMember(jss::key_type))
    {
        auto const kt = keyTypeFromString(context.params[jss::key_type].asString());
        if (!kt)
            return rpcError(RpcBadKeyType);
        keyType = *kt;
    }

    if (keyType == KeyType::Falcon512 || keyType == KeyType::Falcon1024)
    {
        // Generate Falcon post-quantum validator key pair.
        auto kp = generateFalconKeyPair(keyType);
        if (!kp)
            return rpcError(RpcInternal);

        auto& [pqPk, pqSk] = *kp;

        // For Falcon validator key, use hex of the pubkey for UNL / identification (full Falcon mode)
        obj[jss::validation_public_key] = strHex(pqPk.slice());

        // The falcon_secret bundles both public and private key.
        obj[jss::falcon_secret] = encodeFalconSecret(pqPk, pqSk);

        // No seed-based fields for Falcon (keys are random, not seed-derived).
        obj[jss::key_type] = to_string(keyType);
    }
    else
    {
        // Classical key generation (secp256k1 / ed25519).
        auto seed = validationSeed(context.params);

        if (!seed)
            return rpcError(RpcBadSeed);

        auto const privateKey = generateSecretKey(keyType, *seed);

        auto const valPk = derivePublicKey(keyType, privateKey);
        obj[jss::validation_public_key] =
            toBase58(TokenType::NodePublic, valPk);
        // Raw classical key bytes for ValidatorRegister sfConsensusKey (must
        // match the UNL n9 key so epoch scoring can resolve the bond SLE).
        obj[jss::validation_public_key_hex] = strHex(valPk.slice());

        obj[jss::validation_private_key] = toBase58(TokenType::NodePrivate, privateKey);

        obj[jss::validation_seed] = toBase58(*seed);
        obj[jss::validation_key] = seedAs1751(*seed);
    }

    return obj;
}

}  // namespace xrpl
