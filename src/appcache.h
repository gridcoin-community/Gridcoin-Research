#pragma once

#include <string>
#include <map>
#include <boost/algorithm/string.hpp>
#include <boost/iterator/filter_iterator.hpp>

//!
//! \brief Application cache type.
//!
typedef std::map<std::string, std::string> AppCache;

// Predicate
struct AppCacheMatches
{
    AppCacheMatches(const std::string& needle)
        : needle(needle)
    {}

    bool operator()(const AppCache::value_type& t)
    {
        return boost::algorithm::starts_with(t.first, needle);
    };

    std::string needle;
};

typedef boost::filter_iterator<AppCacheMatches, AppCache::iterator> filter_iterator;

//!
//! \brief Application cache data key iterator.
//!
//! An iterator like class which can be used to iterate through the application
//! in ranged based for loops based on keys. For example, to iterate through
//! all the cached polls:
//!
//! \code
//! for(const auto& item : AppCacheFilter("poll")
//! {
//!    const std::string& poll_name = item.second;
//! }
//! \endcode
//!
boost::iterator_range<filter_iterator> AppCacheFilter(const std::string& needle)
{
    auto pred = AppCacheMatches(needle);
    auto begin = boost::make_filter_iterator(pred, mvApplicationCache.begin(), mvApplicationCache.end());
    auto end = boost::make_filter_iterator(pred, mvApplicationCache.end(), mvApplicationCache.end());
    return boost::make_iterator_range(begin, end);
}

