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
    // Single SHA256 of script (convention for watch script identity)
    sha256_hasher h;
    h(scriptPubKey.data(), scriptPubKey.size());
    auto const d = static_cast<sha256_hasher::result_type>(h);
    return digestToUint256(d.data());
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

std::optional<BTCDepositExtract>
btcExtractDeposit(
    BTCParsedTx const& tx,
    uint256 const& watchScriptHash,
    std::uint32_t preferredVout)
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

    // Prefer preferredVout if it matches watch
    auto tryVout = [&](std::uint32_t v) -> std::optional<BTCDepositExtract> {
        if (v >= tx.outputs.size())
            return std::nullopt;
        auto const& o = tx.outputs[v];
        if (btcScriptHash(makeSlice(o.scriptPubKey)) != watchScriptHash)
            return std::nullopt;
        if (o.valueSats == 0)
            return std::nullopt;
        BTCDepositExtract e;
        e.watchVout = v;
        e.valueSats = o.valueSats;
        e.destination = *dest;
        return e;
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
