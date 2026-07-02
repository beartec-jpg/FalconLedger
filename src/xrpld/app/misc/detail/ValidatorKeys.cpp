#include <xrpld/app/misc/ValidatorKeys.h>

#include <xrpld/core/Config.h>
#include <xrpld/core/ConfigSections.h>

#include <xrpl/basics/Log.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/falcon.h>

#include <utility>

namespace xrpl {
ValidatorKeys::ValidatorKeys(Config const& config, beast::Journal j)
{
    if (config.exists(SECTION_VALIDATOR_TOKEN))
    {
        configInvalid_ = true;
        JLOG(j.fatal()) << "Classical [" SECTION_VALIDATOR_TOKEN
                           "] is disabled on Falcon Ledger; use ["
                        << SECTION_VALIDATION_FALCON_SECRET << "]";
        return;
    }

    if (config.exists(SECTION_VALIDATION_SEED))
    {
        configInvalid_ = true;
        JLOG(j.fatal()) << "Classical [" SECTION_VALIDATION_SEED
                           "] is disabled on Falcon Ledger; use ["
                        << SECTION_VALIDATION_FALCON_SECRET << "]";
        return;
    }

    if (!config.exists(SECTION_VALIDATION_FALCON_SECRET))
        return;

    auto const fsecret = config.section(SECTION_VALIDATION_FALCON_SECRET).lines().front();
    auto decoded = decodeFalconSecret(fsecret);
    if (!decoded)
    {
        configInvalid_ = true;
        JLOG(j.fatal()) << "Invalid falcon secret in [" SECTION_VALIDATION_FALCON_SECRET
                           "]";
        return;
    }

    auto const& [pqPk, pqSk] = *decoded;
    PublicKey const pk(pqPk.slice());
    keys.emplace(pk, fsecret);
    nodeID = calcNodeID(pk);
    sequence = 0;
}
}  // namespace xrpl