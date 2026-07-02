#include <xrpld/rpc/Context.h>

#include <xrpl/json/json_value.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/RPCErr.h>
#include <xrpl/protocol/falcon.h>
#include <xrpl/protocol/jss.h>

namespace xrpl {

// {
//   key_type: <string> // optional, defaults to "falcon512"
// }
//
// Falcon Ledger validators use post-quantum Falcon keys only.
json::Value
doValidationCreate(RPC::JsonContext& context)
{
    json::Value obj(json::ValueType::Object);

    KeyType keyType = KeyType::Falcon512;
    if (context.params.isMember(jss::key_type))
    {
        auto const kt = keyTypeFromString(context.params[jss::key_type].asString());
        if (!kt)
            return rpcError(RpcBadKeyType);
        keyType = *kt;
    }

    if (keyType != KeyType::Falcon512 && keyType != KeyType::Falcon1024)
        return RPC::makeError(
            RpcInvalidParams,
            "Classical validator keys are disabled on Falcon Ledger; use falcon512 or falcon1024");

    auto kp = generateFalconKeyPair(keyType);
    if (!kp)
        return rpcError(RpcInternal);

    auto& [pqPk, pqSk] = *kp;

    obj[jss::validation_public_key] = strHex(pqPk.slice());
    obj[jss::validation_public_key_hex] = strHex(pqPk.slice());
    obj[jss::falcon_secret] = encodeFalconSecret(pqPk, pqSk);
    obj[jss::key_type] = to_string(keyType);

    return obj;
}

}  // namespace xrpl