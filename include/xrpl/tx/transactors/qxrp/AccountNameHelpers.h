// Copyright (c) 2026 qXRP Team.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/QXRPConstants.h>

#include <cctype>
#include <optional>
#include <string>

namespace xrpl {
namespace account_names {

/// Normalize and validate a name. Returns lowercase form, or nullopt if invalid.
/// Rules: 3–32 chars, [a-z0-9.], no leading/trailing/consecutive dots.
inline std::optional<std::string>
normalizeName(Slice raw)
{
    if (raw.empty() || raw.size() < kNAME_MIN_LEN || raw.size() > kNAME_MAX_LEN)
        return std::nullopt;

    std::string out;
    out.reserve(raw.size());
    bool prevDot = false;
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        unsigned char const c = static_cast<unsigned char>(raw[i]);
        char lower = static_cast<char>(std::tolower(c));
        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9'))
        {
            out.push_back(lower);
            prevDot = false;
        }
        else if (lower == '.')
        {
            if (i == 0 || i + 1 == raw.size() || prevDot)
                return std::nullopt;
            out.push_back('.');
            prevDot = true;
        }
        else
        {
            return std::nullopt;
        }
    }

    if (out.size() < kNAME_MIN_LEN || out.size() > kNAME_MAX_LEN)
        return std::nullopt;

    return out;
}

inline bool
isValidNormalizedName(std::string const& name)
{
    return normalizeName(makeSlice(name)).has_value();
}

}  // namespace account_names
}  // namespace xrpl
