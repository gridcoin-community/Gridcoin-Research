#include "appcache.h"
#include "main.h"

#include <boost/algorithm/string.hpp>
#include <algorithm>

AppCacheIterator::AppCacheIterator(const std::string& key)
    : _begin(mvApplicationCache.begin())
    , _end(mvApplicationCache.end())
{
    auto predicate = [key](typename AppCache::const_reference t)
    {
        return boost::algorithm::starts_with(t.first, key);
    };

    // Find the first occurrence of 'key'
    _begin = std::find_if(_begin, _end, predicate);
}

AppCache::iterator AppCacheIterator::begin()
{
    return _begin;
}

AppCache::iterator AppCacheIterator::end()
{
    return _end;
}
