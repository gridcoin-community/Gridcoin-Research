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
    AppCacheMatches(const std::string& section);
    bool operator()(const AppCache::value_type& t);
    std::string needle;
};

typedef boost::filter_iterator<AppCacheMatches, AppCache::iterator> filter_iterator;

//!
//! \brief Application cache data key iterator.
//! \param section Section to iterate over.
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
boost::iterator_range<filter_iterator> AppCacheFilter(const std::string& section);

//!
//! \brief Write value into application cache.
//! \param section Cache section to write to.
//! \param key Entry key to write.
//! \param value Entry value to write.
//!
void WriteCache(
        const std::string& section,
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
std::string ReadCache(
        const std::string& section,
        const std::string& key);

//!
//! \brief Read values from appcache section.
//! \param section Cache section to read from.
//! \param key Entry key to read timestamp from.
//! \returns Timestamp for \p key in \p section if available, or an 0
//! if either the section or the key don't exist.
//!
int64_t ReadCacheTimestamp(
        const std::string& section,
        const std::string& key);

//!
//! \brief Clear all values in a cache section.
//! \param section Cache section to clear.
//! \note This only clears the values. It does not erase them.
//!
void ClearCache(const std::string& section);

//!
//! \brief Erase key from appcache section.
//! \param section Cache section to erase from.
//! \param key Entry key to erase.
//!
void DeleteCache(const std::string& section, const std::string& key);

//!
//! \brief Get a list of section values.
//! \param section Section to read.
//!
//! Reads \p section and concatenates the keys and values into a string:
//!
//! key<COL>value<ROW>
//!
//! \note If \p section is \a "beacon" then all non-valid CPID values are
//! discarded.
//!
//! \return Formatted section values string.
//! \todo Make this return std::vector<std::string> instead.
//!
std::string GetListOf(const std::string& section);

//!
//! \brief Get a list of section values with age restrictions.
//! \copydoc GetListOf
//! \param minTime Entry min timestamp. Set to 0 to disable limit.
//! \param maxTime Entry max timestamp. Set to 0 to disable limit.
//!
std::string GetListOf(
        const std::string& section,
        int64_t minTime,
        int64_t maxTime);

//!
//! \brief Count value entries in section.
//!
//! Performs a GetListOf() and counts the results.
//!
//! \param section Section to count.
//! \return Number of values in \p section.
//! \see GetListOf() for beacon restrictions.
//!
size_t GetCountOf(const std::string& section);
