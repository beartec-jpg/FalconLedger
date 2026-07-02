#include <xrpld/app/consensus/RCLCxPeerPos.h>

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/beast/utility/instrumentation.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/KeyType.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/tokens.h>

#include <cstdint>

namespace xrpl {

// Used to construct received proposals
RCLCxPeerPos::RCLCxPeerPos(
    PublicKey const& publicKey,
    Slice const& signature,
    uint256 const& suppression,
    Proposal const& proposal)  // trivially copyable
    : publicKey_(publicKey), suppression_(suppression), proposal_(proposal)
{
    XRPL_ASSERT(
        signature.empty() || signature.size() <= signature_.capacity(),
        "xrpl::RCLCxPeerPos::RCLCxPeerPos : valid signature size");

    if (!signature.empty() && signature.size() <= signature_.capacity())
        signature_.assign(signature.begin(), signature.end());
}

bool
RCLCxPeerPos::checkSign() const
{
    if (signature().empty())
        return false;

    auto const keyType = signingPubKeyType(publicKey().slice());
    if (!keyType)
        return false;

    if (*keyType == KeyType::Falcon512 || *keyType == KeyType::Falcon1024)
    {
        auto const hash = proposal_.signingHash();
        return verify(
            publicKey().slice(),
            Slice(hash.data(), hash.size()),
            signature());
    }

    return false;
}

json::Value
RCLCxPeerPos::getJson() const
{
    auto ret = proposal().getJson();

    if (publicKey().size() != 0u)
        ret[jss::peer_id] = toBase58(TokenType::NodePublic, publicKey());

    return ret;
}

uint256
proposalUniqueId(
    uint256 const& proposeHash,
    uint256 const& previousLedger,
    std::uint32_t proposeSeq,
    NetClock::time_point closeTime,
    Slice const& publicKey,
    Slice const& signature)
{
    Serializer s(512);
    s.addBitString(proposeHash);
    s.addBitString(previousLedger);
    s.add32(proposeSeq);
    s.add32(closeTime.time_since_epoch().count());
    s.addVL(publicKey);
    s.addVL(signature);

    return s.getSHA512Half();
}

}  // namespace xrpl
