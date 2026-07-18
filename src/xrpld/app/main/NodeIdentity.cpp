#include <xrpld/app/main/NodeIdentity.h>

#include <xrpld/app/main/Application.h>
#include <xrpld/core/Config.h>
#include <xrpld/core/ConfigSections.h>

#include <xrpl/basics/Log.h>
#include <xrpl/basics/contract.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PQPublicKey.h>
#include <xrpl/protocol/PQSecretKey.h>
#include <xrpl/protocol/SecretKey.h>
#include <xrpl/protocol/Seed.h>
#include <xrpl/protocol/falcon.h>
#include <xrpl/server/Wallet.h>

#include <boost/program_options/variables_map.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace xrpl {

std::pair<PublicKey, SecretKey>
getNodeIdentity(Application& app, boost::program_options::variables_map const& cmdline)
{
    // Falcon Ledger: classical [node_seed] / --nodeid seeds are forbidden.
    // Node identity is Falcon-only (auto DB or [node_falcon_secret]).
    if (cmdline.contains("nodeid"))
    {
        Throw<std::runtime_error>(
            "Classical --nodeid is disabled on Falcon Ledger; omit it to use "
            "auto Falcon node identity, or set [" SECTION_NODE_FALCON_SECRET "]");
    }

    if (app.config().exists(SECTION_NODE_SEED))
    {
        Throw<std::runtime_error>(
            "Classical [" SECTION_NODE_SEED
            "] is disabled on Falcon Ledger; remove it. Node identity is "
            "Falcon-only (auto-generated, or [" SECTION_NODE_FALCON_SECRET
            "] / [" SECTION_VALIDATION_FALCON_SECRET "])");
    }

    // Explicit Falcon node secret (same falcon_secret hex format as validation).
    if (app.config().exists(SECTION_NODE_FALCON_SECRET))
    {
        auto const line = app.config().section(SECTION_NODE_FALCON_SECRET).lines().front();
        auto decoded = decodeFalconSecret(line);
        if (!decoded)
            Throw<std::runtime_error>("Invalid [" SECTION_NODE_FALCON_SECRET "]");

        auto& [pqPk, pqSk] = *decoded;
        PublicKey pubKey(pqPk.slice());
        // Placeholder classical SecretKey slot — PQ material loaded via Application.
        auto dummySk = randomSecretKey();
        return {pubKey, dummySk};
    }

    // Prefer reusing the validation Falcon key for P2P identity when configured
    // (one Falcon key for the node; no classical keypair anywhere).
    if (app.config().exists(SECTION_VALIDATION_FALCON_SECRET))
    {
        auto const line = app.config().section(SECTION_VALIDATION_FALCON_SECRET).lines().front();
        auto decoded = decodeFalconSecret(line);
        if (!decoded)
            Throw<std::runtime_error>("Invalid [" SECTION_VALIDATION_FALCON_SECRET "] for node identity");

        auto& [pqPk, pqSk] = *decoded;
        PublicKey pubKey(pqPk.slice());
        auto dummySk = randomSecretKey();
        return {pubKey, dummySk};
    }

    auto db = app.getWalletDB().checkoutDb();

    if (cmdline.contains("newnodeid"))
        clearNodeIdentity(*db);

    return getNodeIdentity(*db);
}

}  // namespace xrpl
