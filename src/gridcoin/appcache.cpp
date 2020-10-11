// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/appcache.h"
#include "util.h"

#include <boost/algorithm/string.hpp>
#include <array>
#include <type_traits>

namespace
{
    typedef typename std::underlying_type<Section>::type Section_t;
    std::array<AppCacheSection, static_cast<Section_t>(Section::NUM_CACHES)> caches;

    // Section name to ID map. Used by MemorizeMessage and needs to be kept
    // up to date with the sections.
    const std::unordered_map<std::string, Section> section_name_map =
    {
        { "protocol", Section::PROTOCOL },
        { "scraper", Section::SCRAPER }
    };

    //static_assert(section_name_map.size() == NumCaches, "Section name table size mismatch");

    AppCacheSection& GetSection(Section section)
    {
        if(section == Section::NUM_CACHES)
            throw std::runtime_error("Invalid cache");

        auto idx = static_cast<Section_t>(section);
        return caches[idx];
    }
}

void WriteCache(
        Section section,
        const std::string& key,
        const std::string& value,
        int64_t locktime)
{
    if(key.empty())
        return;

    AppCacheSection& cache = GetSection(section);
    cache[key] = AppCacheEntry{ value, locktime };
}

AppCacheEntry ReadCache(
        Section section,
        const std::string& key)
{
    if (key.empty())
        return AppCacheEntry{ std::string(), 0 };

    const auto& cache = GetSection(section);
    auto entry = cache.find(key);
    return entry != cache.end()
                   ? entry->second
                   : AppCacheEntry{std::string(), 0};
}

AppCacheSection& ReadCacheSection(Section section)
{
    return GetSection(section);
}

SortedAppCacheSection ReadSortedCacheSection(Section section)
{
    const auto& cache = ReadCacheSection(section);
    return SortedAppCacheSection(cache.begin(), cache.end());
}

void ClearCache(Section section)
{
    GetSection(section).clear();
}

void DeleteCache(Section section, const std::string &key)
{
    GetSection(section).erase(key);
}

Section StringToSection(const std::string &section)
{
    auto entry = section_name_map.find(section);
    if(entry == section_name_map.end())
        throw std::runtime_error("Invalid section " + section);

    return entry->second;
}
