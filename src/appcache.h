#pragma once

#include <string>
#include <map>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/range.hpp>

//!
//! \brief Application cache type.
//!
typedef std::map<std::string, std::string> AppCache;

// Predicate
struct AppCacheMatches
{
    AppCacheMatches(const std::string& needle);
    bool operator()(const AppCache::value_type& t);
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
boost::iterator_range<filter_iterator> AppCacheFilter(const std::string& needle);
