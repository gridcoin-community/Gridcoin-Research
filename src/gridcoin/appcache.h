// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_APPCACHE_H
#define GRIDCOIN_APPCACHE_H

#include <string>
#include <map>
#include <unordered_map>

//!
//! \brief An entry in the application cache. This is provided as a legacy shim only and will be replaced by
//! native calls.
//!
struct AppCacheEntry
{
    std::string value; //!< Value of entry.
    int64_t timestamp; //!< Timestamp of entry.
};

//!
//! \brief Application cache section type. This is provided as a legacy shim only and will be replaced by
//! native calls.
//!
typedef std::unordered_map<std::string, AppCacheEntry> AppCacheSection;

//!
//! \brief Application cache section sorted by key. This is provided as a legacy shim only and will be replaced by
//! native calls.
//!
typedef std::map<std::string, AppCacheEntry> SortedAppCacheSection;

//!
//! \brief Extended AppCache structure similar to those in AppCache.h, except a deleted flag is provided. This
//! is provided as a legacy shim only and will be replaced by native calls.
//!
struct AppCacheEntryExt
{
    std::string value; // Value of entry.
    int64_t timestamp; // Timestamp of entry/deletion
    bool deleted; // Deleted flag.
};

//!
//! \brief Extended AppCache map typedef similar to those in AppCache.h, except a deleted flag is provided. This
//! is provided as a legacy shim only and will be replaced by native calls.
//!
typedef std::unordered_map<std::string, AppCacheEntryExt> AppCacheSectionExt;

#endif // GRIDCOIN_APPCACHE_H
