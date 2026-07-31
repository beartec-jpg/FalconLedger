// Copyright (c) 2026 Falcon Ledger / qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only

#include <xrpl/protocol/BTCTx.h>
#include <xrpl/protocol/BTCHeader.h>
#include <xrpl/protocol/digest.h>

#include <cstring>

namespace xrpl {
namespace {

bool
readVarInt(Slice const& data, std::size_t& off, std::uint64_t& out)
{
    if (off >= data.size())
        return false;
    auto const first = data.data()[off++];
    if (first < 0xfd)
    {
        out = first;
        return true;
    }
    if (first == 0xfd)
    {
        if (off + 2 > data.size())
            return false;
        out = static_cast<std::uint64_t>(data.data()[off]) |
            (static_cast<std::uint64_t>(data.data()[off + 1]) << 8);
        off += 2;
        return true;
    }
    if (first == 0xfe)
    {
        if (off + 4 > data.size())
            return false;
        out = static_cast<std::uint64_t>(data.data()[off]) |
            (static_cast<std::uint64_t>(data.data()[off + 1]) << 8) |
            (static_cast<std::uint64_t>(data.data()[off + 2]) << 16) |
            (static_cast<std::uint64_t>(data.data()[off + 3]) << 24);
        off += 4;
        return true;
    }
    // 0xff
    if (off + 8 > data.size())
        return false;
    out = 0;
    for (int i = 0; i < 8; ++i)
        out |= static_cast<std::uint64_t>(data.data()[off + i]) << (8 * i);
    off += 8;
    return true;
}

bool
skipBytes(Slice const& data, std::size_t& off, std::size_t n)
{
    if (off + n > data.size())
        return false;
    off += n;
    return true;
}

// Convert raw double-sha256 digest bytes (Bitcoin internal order) to Falcon uint256.
uint256
digestToUint256(std::uint8_t const d[32])
{
    uint256 out;
    for (int i = 0; i < 32; ++i)
        out.data()[i] = d[31 - i];
    return out;
}

uint256
doubleSha256Raw(void const* data, std::size_t len)
{
    sha256_hasher h1;
    h1(data, len);
    auto const d1 = static_cast<sha256_hasher::result_type>(h1);
    sha256_hasher h2;
    h2(d1.data(), d1.size());
    auto const d2 = static_cast<sha256_hasher::result_type>(h2);
    return digestToUint256(d2.data());
}

}  // namespace

uint256
btcScriptHash(Slice scriptPubKey)
{
    // Single SHA256(scriptPubKey) as UINT256 (design + activate on 1001).
    // Do NOT byte-reverse like Bitcoin block/txid hashes — watch identity is a
    // raw digest. Reversing here made live deposits fail temMALFORMED even when
    // OP_RETURN and merkle proofs were valid (activate stored unreversed SHA256).
    sha256_hasher h;
    h(scriptPubKey.data(), scriptPubKey.size());
    auto const d = static_cast<sha256_hasher::result_type>(h);
    uint256 out;
    std::memcpy(out.data(), d.data(), d.size());
    return out;
}

std::optional<BTCParsedTx>
btcParseTx(Slice rawTx)
{
    if (rawTx.empty() || rawTx.size() > kBTC_MAX_TX_BLOB)
        return std::nullopt;

    BTCParsedTx tx;
    std::size_t off = 0;

    // version
    if (!skipBytes(rawTx, off, 4))
        return std::nullopt;

    // marker/flag for segwit?
    bool segwit = false;
    if (off + 2 <= rawTx.size() && rawTx.data()[off] == 0x00 && rawTx.data()[off + 1] == 0x01)
    {
        segwit = true;
        off += 2;
    }
    tx.isSegwit = segwit;

    std::uint64_t nin = 0;
    if (!readVarInt(rawTx, off, nin) || nin > 10000)
        return std::nullopt;

    for (std::uint64_t i = 0; i < nin; ++i)
    {
        // prevout 32 + 4
        if (!skipBytes(rawTx, off, 36))
            return std::nullopt;
        std::uint64_t scriptLen = 0;
        if (!readVarInt(rawTx, off, scriptLen) || scriptLen > 10000)
            return std::nullopt;
        if (!skipBytes(rawTx, off, static_cast<std::size_t>(scriptLen)))
            return std::nullopt;
        // sequence
        if (!skipBytes(rawTx, off, 4))
            return std::nullopt;
    }

    std::uint64_t nout = 0;
    if (!readVarInt(rawTx, off, nout) || nout > 10000)
        return std::nullopt;

    tx.outputs.reserve(static_cast<std::size_t>(nout));
    for (std::uint64_t i = 0; i < nout; ++i)
    {
        if (off + 8 > rawTx.size())
            return std::nullopt;
        std::uint64_t value = 0;
        for (int b = 0; b < 8; ++b)
            value |= static_cast<std::uint64_t>(rawTx.data()[off + b]) << (8 * b);
        off += 8;
        std::uint64_t scriptLen = 0;
        if (!readVarInt(rawTx, off, scriptLen) || scriptLen > 10000)
            return std::nullopt;
        if (off + scriptLen > rawTx.size())
            return std::nullopt;
        BTCTxOutput o;
        o.valueSats = value;
        o.scriptPubKey.assign(rawTx.data() + off, rawTx.data() + off + scriptLen);
        off += static_cast<std::size_t>(scriptLen);
        tx.outputs.push_back(std::move(o));
    }

    if (segwit)
    {
        // skip witness for each input
        for (std::uint64_t i = 0; i < nin; ++i)
        {
            std::uint64_t nstack = 0;
            if (!readVarInt(rawTx, off, nstack) || nstack > 10000)
                return std::nullopt;
            for (std::uint64_t s = 0; s < nstack; ++s)
            {
                std::uint64_t itemLen = 0;
                if (!readVarInt(rawTx, off, itemLen) || itemLen > 10000)
                    return std::nullopt;
                if (!skipBytes(rawTx, off, static_cast<std::size_t>(itemLen)))
                    return std::nullopt;
            }
        }
    }

    // locktime
    if (!skipBytes(rawTx, off, 4))
        return std::nullopt;

    // txid: for segwit, hash non-witness serialization; for legacy, full tx
    if (!segwit)
    {
        tx.txid = doubleSha256Raw(rawTx.data(), rawTx.size());
    }
    else
    {
        // Rebuild legacy serialization: version | inputs | outputs | locktime
        // Simpler approach: re-serialize by stripping marker/flag/witness.
        // Parse again into a buffer.
        Blob stripped;
        stripped.reserve(rawTx.size());
        // version
        stripped.insert(stripped.end(), rawTx.data(), rawTx.data() + 4);
        // skip marker/flag already known at offset 4
        std::size_t p = 6;  // after version+marker+flag
        // copy inputs
        std::size_t save = p;
        std::uint64_t nin2 = 0;
        if (!readVarInt(rawTx, p, nin2))
            return std::nullopt;
        // write vin count from original at save
        stripped.insert(stripped.end(), rawTx.data() + save, rawTx.data() + p);
        for (std::uint64_t i = 0; i < nin2; ++i)
        {
            std::size_t start = p;
            if (!skipBytes(rawTx, p, 36))
                return std::nullopt;
            std::uint64_t sl = 0;
            if (!readVarInt(rawTx, p, sl))
                return std::nullopt;
            if (!skipBytes(rawTx, p, static_cast<std::size_t>(sl) + 4))
                return std::nullopt;
            stripped.insert(stripped.end(), rawTx.data() + start, rawTx.data() + p);
        }
        std::size_t voutStart = p;
        std::uint64_t nout2 = 0;
        if (!readVarInt(rawTx, p, nout2))
            return std::nullopt;
        for (std::uint64_t i = 0; i < nout2; ++i)
        {
            if (!skipBytes(rawTx, p, 8))
                return std::nullopt;
            std::uint64_t sl = 0;
            if (!readVarInt(rawTx, p, sl))
                return std::nullopt;
            if (!skipBytes(rawTx, p, static_cast<std::size_t>(sl)))
                return std::nullopt;
        }
        stripped.insert(stripped.end(), rawTx.data() + voutStart, rawTx.data() + p);
        // skip witnesses
        for (std::uint64_t i = 0; i < nin2; ++i)
        {
            std::uint64_t nstack = 0;
            if (!readVarInt(rawTx, p, nstack))
                return std::nullopt;
            for (std::uint64_t s = 0; s < nstack; ++s)
            {
                std::uint64_t itemLen = 0;
                if (!readVarInt(rawTx, p, itemLen))
                    return std::nullopt;
                if (!skipBytes(rawTx, p, static_cast<std::size_t>(itemLen)))
                    return std::nullopt;
            }
        }
        // locktime
        if (p + 4 > rawTx.size())
            return std::nullopt;
        stripped.insert(stripped.end(), rawTx.data() + p, rawTx.data() + p + 4);
        tx.txid = doubleSha256Raw(stripped.data(), stripped.size());
    }

    return tx;
}

bool
btcIsP2WSH(Slice scriptPubKey)
{
    return scriptPubKey.size() == 34 && scriptPubKey[0] == 0x00 &&
        scriptPubKey[1] == 0x20;
}

bool
btcParseBitvmVaultScript(Slice witnessScript, uint256& commitOut)
{
    // Must match scripts/btc-spv/bitvm/vault.py build_vault_script (CSV=6).
    // OP_IF OP_SHA256 <32 zero> OP_EQUALVERIFY OP_TRUE
    // OP_ELSE OP_6 OP_CSV OP_DROP OP_SHA256 <commit32> OP_EQUALVERIFY
    // <33 pubkey> OP_CHECKSIG OP_ENDIF
    auto const* p = witnessScript.data();
    auto const n = witnessScript.size();
    if (n < 1 + 1 + 1 + 32 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 32 + 1 + 1 + 33 + 1 + 1)
        return false;
    std::size_t i = 0;
    auto need = [&](std::size_t k) -> bool { return i + k <= n; };
    auto take = [&](std::uint8_t b) -> bool {
        if (!need(1) || p[i] != b)
            return false;
        ++i;
        return true;
    };
    if (!take(0x63))  // OP_IF
        return false;
    if (!take(0xa8))  // OP_SHA256
        return false;
    if (!take(0x20) || !need(32))
        return false;
    for (std::size_t k = 0; k < 32; ++k)
    {
        if (p[i + k] != 0)
            return false;
    }
    i += 32;
    if (!take(0x88))  // OP_EQUALVERIFY
        return false;
    if (!take(0x51))  // OP_TRUE
        return false;
    if (!take(0x67))  // OP_ELSE
        return false;
    // CSV blocks = kBTC_VAULT_CSV_BLOCKS (6) → OP_6
    if (kBTC_VAULT_CSV_BLOCKS >= 1 && kBTC_VAULT_CSV_BLOCKS <= 16)
    {
        if (!take(static_cast<std::uint8_t>(0x50 + kBTC_VAULT_CSV_BLOCKS)))
            return false;
    }
    else
        return false;
    if (!take(0xb2))  // OP_CHECKSEQUENCEVERIFY
        return false;
    if (!take(0x75))  // OP_DROP
        return false;
    if (!take(0xa8))  // OP_SHA256
        return false;
    if (!take(0x20) || !need(32))
        return false;
    std::memcpy(commitOut.data(), p + i, 32);
    i += 32;
    if (!take(0x88))  // OP_EQUALVERIFY
        return false;
    if (!need(1))
        return false;
    auto const pkLen = p[i++];
    if (pkLen != 33 && pkLen != 65)
        return false;
    if (!need(pkLen))
        return false;
    i += pkLen;
    if (!take(0xac))  // OP_CHECKSIG
        return false;
    if (!take(0x68))  // OP_ENDIF
        return false;
    return i == n;
}

std::optional<BTCDepositExtract>
btcExtractDeposit(
    BTCParsedTx const& tx,
    uint256 const& watchScriptHash,
    std::uint32_t preferredVout,
    Slice vaultWitnessScript)
{
    std::optional<AccountID> dest;
    int opReturnCount = 0;

    for (auto const& o : tx.outputs)
    {
        auto const& spk = o.scriptPubKey;
        // OP_RETURN: 0x6a <len> <payload>
        if (spk.size() >= 2 && spk[0] == 0x6a)
        {
            std::size_t len = spk[1];
            std::size_t payloadOff = 2;
            if (len == 0x4c && spk.size() >= 3)
            {
                len = spk[2];
                payloadOff = 3;
            }
            if (payloadOff + len > spk.size())
                continue;
            if (len == kBTC_OP_RETURN_PAYLOAD_LEN)
            {
                bool magic = true;
                for (std::size_t i = 0; i < 4; ++i)
                {
                    if (spk[payloadOff + i] != kBTC_OP_RETURN_MAGIC[i])
                    {
                        magic = false;
                        break;
                    }
                }
                if (magic)
                {
                    ++opReturnCount;
                    AccountID id;
                    std::memcpy(id.data(), spk.data() + payloadOff + 4, 20);
                    dest = id;
                }
            }
        }
    }

    if (opReturnCount != 1 || !dest)
        return std::nullopt;

    uint256 vaultCommit{};
    bool const haveVault =
        !vaultWitnessScript.empty() && btcParseBitvmVaultScript(vaultWitnessScript, vaultCommit);

    // Prefer preferredVout if it matches watch or vault
    auto tryVout = [&](std::uint32_t v) -> std::optional<BTCDepositExtract> {
        if (v >= tx.outputs.size())
            return std::nullopt;
        auto const& o = tx.outputs[v];
        if (o.valueSats == 0)
            return std::nullopt;
        auto const spk = makeSlice(o.scriptPubKey);

        // (1) Legacy fixed watch (P2PKH era)
        if (btcScriptHash(spk) == watchScriptHash)
        {
            BTCDepositExtract e;
            e.watchVout = v;
            e.valueSats = o.valueSats;
            e.destination = *dest;
            e.isVault = false;
            return e;
        }

        // (2) BitVM vault P2WSH: scriptPubKey commits to witness script
        if (haveVault && btcIsP2WSH(spk))
        {
            sha256_hasher h;
            h(vaultWitnessScript.data(), vaultWitnessScript.size());
            auto const d = static_cast<sha256_hasher::result_type>(h);
            bool match = true;
            for (std::size_t k = 0; k < 32; ++k)
            {
                if (spk[2 + k] != d[k])
                {
                    match = false;
                    break;
                }
            }
            if (!match)
                return std::nullopt;
            BTCDepositExtract e;
            e.watchVout = v;
            e.valueSats = o.valueSats;
            e.destination = *dest;
            e.isVault = true;
            e.vaultCommit = vaultCommit;
            return e;
        }
        return std::nullopt;
    };

    if (auto e = tryVout(preferredVout))
        return e;

    for (std::uint32_t i = 0; i < tx.outputs.size(); ++i)
    {
        if (auto e = tryVout(i))
            return e;
    }
    return std::nullopt;
}

}  // namespace xrpl
