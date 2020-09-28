// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <map>
#include <unordered_map>

enum class Section
{
    PROTOCOL,
    SCRAPER,

    // Enum counting entry. Using it will throw.
    NUM_CACHES
};

//!
//! \brief An entry in the application cache.
//!
struct AppCacheEntry
{
    std::string value; //!< Value of entry.
    int64_t timestamp; //!< Timestamp of entry.
};

//!
//! \brief Application cache section type.
//!
typedef std::unordered_map<std::string, AppCacheEntry> AppCacheSection;

//!
//! \brief Application cache section sorted by key.
//!
typedef std::map<std::string, AppCacheEntry> SortedAppCacheSection;

//!
//! \brief Application cache type.
//!
typedef std::unordered_map<std::string, AppCacheSection> AppCache;

//!
//! \brief Write value into application cache.
//! \param section Cache section to write to.
//! \param key Entry key to write.
//! \param value Entry value to write.
//!
void WriteCache(
        Section section,
        const std::string& key,
        const std::string& value,
        int64_t locktime);

//!
//! \brief Read values from appcache section.
//! \param section Cache section to read from.
//! \param key Entry key to read.
//! \returns Value for \p key in \p section if available, or an empty string
//! if either the section or the key don't exist.
//!
AppCacheEntry ReadCache(
        Section section,
        const std::string& key);

//!
//! \brief Read section from cache.
//! \param section Section to read.
//! \returns The data for \p section if available.
//!
AppCacheSection& ReadCacheSection(Section section);

//!
//! \brief Reads a section from cache and sorts it.
//! \param section Section to read.
//! \returns The data for \p section if available.
//!
//! Reads a cache section and transfers it to a sorted map. This can be an
//! expensive operation and should not be used unless there is a need
//! for sorted traversal.
//!
//! \see ReadCacheSection
//!
SortedAppCacheSection ReadSortedCacheSection(Section section);

//!
//! \brief Clear all values in a cache section.
//! \param section Cache section to clear.
//! \note This only clears the values. It does not erase them.
//!
void ClearCache(Section section);

//!
//! \brief Erase key from appcache section.
//! \param section Cache section to erase from.
//! \param key Entry key to erase.
//!
void DeleteCache(Section section, const std::string& key);

Section StringToSection(const std::string& section);
