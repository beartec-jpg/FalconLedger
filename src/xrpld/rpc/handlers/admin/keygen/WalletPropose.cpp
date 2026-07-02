#include <xrpld/rpc/handlers/admin/keygen/WalletPropose.h>

#include <xrpld/rpc/Context.h>
#include <xrpld/rpc/detail/RPCHelpers.h>

#include <xrpl/basics/strHex.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AccountID.h>
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

#include <cmath>
#include <map>
#include <optional>
#include <string>

namespace xrpl {

double
estimateEntropy(std::string const& input)
{
    // First, we calculate the Shannon entropy. This gives
    // the average number of bits per symbol that we would
    // need to encode the input.
    std::map<int, double> freq;

    for (auto const& c : input)
        freq[c]++;

    double se = 0.0;

    for (auto const& [_, f] : freq)
    {
        (void)_;
        auto x = f / input.length();
        se += (x)*log2(x);
    }

    // We multiply it by the length, to get an estimate of
    // the number of bits in the input. We floor because it
    // is better to be conservative.
    return std::floor(-se * input.length());
}

// {
//  passphrase: <string>
// }
json::Value
doWalletPropose(RPC::JsonContext& context)
{
    return walletPropose(context.params);
}

json::Value
walletPropose(json::Value const& params)
{
    std::optional<KeyType> keyType = KeyType::Falcon512;

    if (params.isMember(jss::key_type))
    {
        if (!params[jss::key_type].isString())
        {
            return RPC::expectedFieldError(jss::key_type, "string");
        }

        keyType = keyTypeFromString(params[jss::key_type].asString());

        if (!keyType)
            return rpcError(RpcInvalidParams);
    }

    // ── Post-quantum Falcon wallets ─────────────────────────────────────────
    // Falcon keys are not derived from a compact 16-byte seed: liboqs has no
    // seed-expansion API and the public key cannot be re-derived from the
    // secret key alone.  We therefore generate a fresh random key pair and
    // hand back the full signing material in `falcon_secret`.
    if (keyType == KeyType::Falcon512 || keyType == KeyType::Falcon1024)
    {
        if (params.isMember(jss::passphrase) || params.isMember(jss::seed) ||
            params.isMember(jss::seed_hex))
        {
            return RPC::makeError(
                RpcBadSeed,
                "Falcon keys are randomly generated; a seed or passphrase "
                "cannot be supplied.");
        }

        auto keyPair = generateFalconKeyPair(*keyType);
        if (!keyPair)
        {
            return RPC::makeError(
                RpcInternal,
                "Falcon key generation is unavailable in this build "
                "(liboqs missing the requested parameter set).");
        }

        auto const& [pub, sec] = *keyPair;
        auto const pubSlice = pub.slice();

        json::Value obj(json::ValueType::Object);
        obj[jss::account_id] = toBase58(calcAccountID(pubSlice));
        obj[jss::key_type] = to_string(*keyType);
        obj[jss::public_key] =
            encodeBase58Token(TokenType::AccountPublic, pubSlice.data(), pubSlice.size());
        obj[jss::public_key_hex] = strHex(pubSlice);
        // The full signing secret (public + private). It cannot be regenerated
        // from a seed, so the holder must store it securely.
        obj[jss::falcon_secret] = encodeFalconSecret(pub, sec);
        obj[jss::warning] =
            "This is a post-quantum Falcon wallet. The falcon_secret field is "
            "the complete signing secret and cannot be recovered from a seed. "
            "Store it securely and never share it.";
        return obj;
    }

    return RPC::makeError(
        RpcInvalidParams,
        "Classical wallets are disabled on Falcon Ledger; use key_type falcon512 or falcon1024");
}

}  // namespace xrpl
