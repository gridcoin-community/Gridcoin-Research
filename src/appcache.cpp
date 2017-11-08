#include "appcache.h"
#include "util.h"
#include "main.h"

#include <boost/algorithm/string.hpp>

namespace
{
    std::map<std::string, std::string> mvApplicationCache;
    std::map<std::string, int64_t> mvApplicationCacheTimestamp;
}

// Predicate
AppCacheMatches::AppCacheMatches(const std::string& section)
    : needle(section + ";")
{}

bool AppCacheMatches::operator()(const AppCache::value_type& t)
{
    return boost::algorithm::starts_with(t.first, needle);
}

// Filter
boost::iterator_range<filter_iterator> AppCacheFilter(const std::string& needle)
{
    auto pred = AppCacheMatches(needle);
    auto begin = boost::make_filter_iterator(pred, mvApplicationCache.begin(), mvApplicationCache.end());
    auto end = boost::make_filter_iterator(pred, mvApplicationCache.end(), mvApplicationCache.end());
    return boost::make_iterator_range(begin, end);
}

void WriteCache(
        const std::string& section,
        const std::string& key,
        const std::string& value,
        int64_t locktime)
{
    if (section.empty() || key.empty())
        return;

    const std::string entry = section + ";" + key;
    mvApplicationCache[entry] = value;
    mvApplicationCacheTimestamp[entry] = locktime;
}

std::string ReadCache(
        const std::string& section,
        const std::string& key)
{
    if (section.empty() || key.empty())
        return "";

    auto item = mvApplicationCache.find(section + ";" + key);
    return item != mvApplicationCache.end()
                   ? item->second
                   : "";
}

int64_t ReadCacheTimestamp(
        const std::string& section,
        const std::string& key)
{
    if (section.empty() || key.empty())
        return 0;

    auto item = mvApplicationCacheTimestamp.find(section + ";" + key);
    return item != mvApplicationCacheTimestamp.end()
                   ? item->second
                   : 0;
}

void ClearCache(const std::string& section)
{
    for(const auto& item : AppCacheFilter(section))
    {
        mvApplicationCache[item.first].clear();
        mvApplicationCacheTimestamp[item.first] = 1;
    }
}

void DeleteCache(const std::string& section, const std::string& key)
{
    const std::string entry = section + ";" + key;
    mvApplicationCache.erase(entry);
    mvApplicationCacheTimestamp.erase(entry);
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
    for(const auto& item : AppCacheFilter(section))
    {
        const std::string& key = item.first;
        const std::string& value = item.second;
        const std::string& subkey = key.substr(section.length()+1,key.length()-section.length()-1);

        // Skip invalid beacons.
        if (section == "beacon" && Contains(value, "INVESTOR"))
            continue;

        // Compare age restrictions if specified.
        if(minTime || maxTime)
        {
            int64_t timestamp = ReadCacheTimestamp(section, subkey);
            if((minTime && timestamp <= minTime) ||
               (maxTime && timestamp >= maxTime))
                continue;
        }

        std::string row = subkey + "<COL>" + value;
        if (!row.empty())
            rows += row + "<ROW>";
    }

    return rows;
}

size_t GetCountOf(const std::string& section)
{
    std::string data = GetListOf(section);
    std::vector<std::string> vScratchPad = split(data.c_str(),"<ROW>");
    return vScratchPad.size()+1;
}
