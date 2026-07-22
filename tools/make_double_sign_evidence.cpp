// Produce ValidatorSlash DOUBLE_SIGN evidence (two Falcon STValidations).
// Usage: make_double_sign_evidence <falcon_secret> <ledger_seq> <hash1_hex> <hash2_hex>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/QXRPConstants.h>
#include <xrpl/protocol/STValidation.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/falcon.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace xrpl;

static Blob hexToBlob(std::string_view hex)
{
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
        hex.remove_prefix(2);
    if (hex.size() % 2)
        throw std::runtime_error("odd hex length");
    auto nyb = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("bad hex");
    };
    Blob out(hex.size() / 2);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<std::uint8_t>((nyb(hex[2 * i]) << 4) | nyb(hex[2 * i + 1]));
    return out;
}

static std::string toHex(Slice s)
{
    static char const* d = "0123456789ABCDEF";
    std::string o(s.size() * 2, '0');
    for (size_t i = 0; i < s.size(); ++i)
    {
        o[2 * i] = d[(s[i] >> 4) & 0xf];
        o[2 * i + 1] = d[s[i] & 0xf];
    }
    return o;
}

static uint256 parseHash(std::string_view hex)
{
    auto b = hexToBlob(hex);
    if (b.size() != 32)
        throw std::runtime_error("hash must be 32 bytes hex");
    uint256 h;
    std::memcpy(h.data(), b.data(), 32);
    return h;
}

static Blob makeVal(PQPublicKey const& pk, PQSecretKey const& sk, std::uint32_t seq, uint256 const& h)
{
    STValidation val(
        NetClock::time_point{NetClock::duration{1}},
        pk,
        sk,
        calcNodeID(PublicKey{pk.slice()}),
        [&](STObject& obj) {
            obj.setFieldH256(sfLedgerHash, h);
            obj.setFieldU32(sfLedgerSequence, seq);
            obj.setFlag(kVF_FULL_VALIDATION | kVF_FULLY_CANONICAL_SIG);
        });
    return val.getSerialized();
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5)
        {
            std::cerr << "usage: " << argv[0]
                      << " <falcon_secret_hex> <ledger_seq> <hash1_hex> <hash2_hex>\n";
            return 2;
        }
        if (!falconAvailable(KeyType::Falcon512))
        {
            std::cerr << "Falcon-512 unavailable\n";
            return 1;
        }
        auto kp = decodeFalconSecret(argv[1]);
        if (!kp)
        {
            std::cerr << "decodeFalconSecret failed\n";
            return 1;
        }
        auto& pk = kp->first;
        auto& sk = kp->second;
        auto seq = static_cast<std::uint32_t>(std::stoul(argv[2]));
        auto h1 = parseHash(argv[3]);
        auto h2 = parseHash(argv[4]);
        if (h1 == h2)
        {
            std::cerr << "hashes must differ\n";
            return 2;
        }

        auto e1 = makeVal(pk, sk, seq, h1);
        auto e2 = makeVal(pk, sk, seq, h2);

        auto okParse = [](Blob const& b) {
            try
            {
                SerialIter sit{makeSlice(b)};
                STValidation v(sit, [](PublicKey const& p) { return calcNodeID(p); }, true);
                return v.isValid();
            }
            catch (...)
            {
                return false;
            }
        };
        if (!okParse(e1) || !okParse(e2))
        {
            std::cerr << "invalid validation after sign\n";
            return 1;
        }

        auto bond = calcValidatorBondID(pk.slice());
        std::cout << "{\n"
                  << "  \"evidence1_hex\": \"" << toHex(makeSlice(e1)) << "\",\n"
                  << "  \"evidence2_hex\": \"" << toHex(makeSlice(e2)) << "\",\n"
                  << "  \"bond_id\": \"" << toBase58(bond) << "\",\n"
                  << "  \"bond_id_hex\": \"" << toHex(Slice(bond.data(), bond.size())) << "\",\n"
                  << "  \"pubkey_hex\": \"" << toHex(pk.slice()) << "\",\n"
                  << "  \"ledger_seq\": " << seq << ",\n"
                  << "  \"e1_len\": " << e1.size() << ",\n"
                  << "  \"e2_len\": " << e2.size() << "\n"
                  << "}\n";
        return 0;
    }
    catch (std::exception const& e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
