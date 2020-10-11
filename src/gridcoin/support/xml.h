// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

inline std::string ExtractXML(const std::string& xml, const std::string& key, const std::string& key_end)
{
    std::string::size_type loc = xml.find(key, 0);

    if (loc == std::string::npos) {
        return "";
    }

    std::string::size_type loc_end = xml.find(key_end, loc + 3);

    if (loc_end == std::string::npos) {
        return "";
    }

    return xml.substr(loc + (key.length()), loc_end - loc - (key.length()));
}
