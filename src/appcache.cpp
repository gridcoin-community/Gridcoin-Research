#include "appcache.h"

// TODO: Remove this include when mvApplicationCache has moved to this file.
#include "main.h"

#include <boost/algorithm/string.hpp>

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
