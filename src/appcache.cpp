#include "appcache.h"
#include "util.h"
#include "main.h"

#include <boost/algorithm/string.hpp>

namespace
{
    AppCache mvApplicationCache;
}

void WriteCache(
        const std::string& section,
        const std::string& key,
        const std::string& value,
        int64_t locktime)
{
    if (section.empty() || key.empty())
        return;

    mvApplicationCache[section][key] = AppCacheEntry{ value, locktime };
}

AppCacheEntry ReadCache(
        const std::string& section,
        const std::string& key)
{
    if (section.empty() || key.empty())
        return AppCacheEntry{ std::string(), 0 };

    const auto& cache = ReadCacheSection(section);
    auto entry = cache.find(key);
    return entry != cache.end()
                   ? entry->second
                   : AppCacheEntry{std::string(), 0};
}

AppCacheSection ReadCacheSection(const std::string& section)
{
    const auto& cache = mvApplicationCache.find(section);
    return cache != mvApplicationCache.end()
        ? cache->second
        : AppCacheSection();
}

void ClearCache(const std::string& section)
{
    mvApplicationCache.erase(section);
}

void DeleteCache(const std::string& section, const std::string& key)
{
    auto cache = mvApplicationCache.find(section);
    if(cache == mvApplicationCache.end())
       return;

    cache->second.erase(key); 
}

std::string GetListOf(const std::string& section)
{
    return GetListOf(section, 0, 0);
}

std::string GetListOf(
        const std::string& section,
        int64_t minTime,
        int64_t maxTime)
{
    std::string rows;
    for(const auto& item : ReadCacheSection(section))
    {
        const std::string& key = item.first;
        const AppCacheEntry& entry = item.second;

        // Compare age restrictions if specified.
        if((minTime && entry.timestamp <= minTime) ||
           (maxTime && entry.timestamp >= maxTime))
            continue;

        // Skip invalid beacons.
        if (section == "beacon" && Contains("INVESTOR", entry.value))
            continue;

        rows += key + "<COL>" + entry.value + "<ROW>";
    }

    return rows;
}

size_t GetCountOf(const std::string& section)
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
