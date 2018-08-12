#include "appcache.h"
#include "main.h"
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
        { "beacon", Section::BEACON },
        { "beaconalt", Section::BEACONALT },
        { "superblock", Section::SUPERBLOCK },
        { "global", Section::GLOBAL },
        { "protocol", Section::PROTOCOL },
        { "neuralsecurity", Section::NEURALSECURITY },
        { "currentneuralsecurity", Section::CURRENTNEURALSECURITY },
        { "trxid", Section::TRXID },
        { "poll", Section::POLL },
        { "vote", Section::VOTE },
        { "project", Section::PROJECT },
        { "projectmapping", Section::PROJECTMAPPING }
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

std::string GetListOf(Section section)
{
    return GetListOf(section, 0, 0);
}

std::string GetListOf(
        Section section,
        int64_t minTime,
        int64_t maxTime)
{
    std::string rows;
    for(const auto& item : GetSection(section))
    {
        const std::string& key = item.first;
        const AppCacheEntry& entry = item.second;

        // Compare age restrictions if specified.
        if((minTime && entry.timestamp <= minTime) ||
           (maxTime && entry.timestamp >= maxTime))
            continue;

        // Skip invalid beacons.
        if (section == Section::BEACON && Contains("INVESTOR", entry.value))
            continue;

        rows += key + "<COL>" + entry.value + "<ROW>";
    }

    return rows;
}

size_t GetCountOf(Section section)
{
    const std::string& data = GetListOf(section);
    size_t count = 0;
    size_t pos = 0;
    const std::string needle("<ROW>");
    const size_t width = needle.size();
    while((pos = data.find(needle, pos)) != std::string::npos)
    {
       ++count;
       pos += width;
    }

    return count;
}

Section StringToSection(const std::string &section)
{
    auto entry = section_name_map.find(section);
    if(entry == section_name_map.end())
        throw std::runtime_error("Invalid section " + section);
    
    return entry->second;
}
