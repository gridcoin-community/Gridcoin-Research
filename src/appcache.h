#pragma once

#include <string>
#include <map>

//!
//! \brief Application cache type.
//!
typedef std::map<std::string, std::string> AppCache;

//!
//! \brief Application cache data key iterator.
//!
//! An iterator like class which can be used to iterate through the application
//! in ranged based for loops based on keys. For example, to iterate through
//! all the cached polls:
//!
//! \code
//! for(const auto& item : AppCacheIterator("poll")
//! {
//!    const std::string& poll_name = item.second;
//! }
//! \endcode
//!
class AppCacheIterator
{
public:
    //!
    //! \brief Constructor.
    //! \param key Key to search for. For legacy code compatibility reasons
    //! \p key is matched to the start of the cached key, so \a poll will
    //! iterate over items with the key \a polls as well.
    //!
    AppCacheIterator(const std::string& key);

    //!
    //! \brief Get iterator to first matching element.
    //! \return Iterator pointing to the first matching element, or end()
    //! if none is found.
    //!
    AppCache::iterator begin();

    //!
    //! \brief Get iterator to the element after the last element.
    //! \return End iterator element.
    //!
    AppCache::iterator end();

private:
    AppCache::iterator _begin;
    AppCache::iterator _end;
};
