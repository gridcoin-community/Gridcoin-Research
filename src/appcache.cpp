#include "appcache.h"

// TODO: Remove this include when mvApplicationCache has moved to this file.
#include "main.h"

#include <boost/algorithm/string.hpp>

// Predicate
AppCacheMatches::AppCacheMatches(const std::string& needle)
    : needle(needle)
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

