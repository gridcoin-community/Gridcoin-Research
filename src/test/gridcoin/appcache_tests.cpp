// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/appcache.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(appcache_tests)

BOOST_AUTO_TEST_CASE(appcache_WrittenCacheShouldBeReadable)
{
    ClearCache(Section::PROTOCOL);
    WriteCache(Section::PROTOCOL, "key", "hello", 123456789);
    BOOST_CHECK(ReadCache(Section::PROTOCOL, "key").value == "hello");
}

BOOST_AUTO_TEST_CASE(appcache_ClearCacheShouldClearEntireSection)
{
    ClearCache(Section::PROTOCOL);
    WriteCache(Section::PROTOCOL, "key1", "hello", 123456789);
    WriteCache(Section::PROTOCOL, "key2", "hello", 123456789);
    ClearCache(Section::PROTOCOL);
    BOOST_CHECK(ReadCache(Section::PROTOCOL, "key1").value.empty() == true);
    BOOST_CHECK(ReadCache(Section::PROTOCOL, "key2").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(appcache_KeyShouldBeEmptyAfterDeleteCache)
{
    ClearCache(Section::PROTOCOL);
    WriteCache(Section::PROTOCOL, "key", "hello", 123456789);
    DeleteCache(Section::PROTOCOL, "key");
    BOOST_CHECK(ReadCache(Section::PROTOCOL, "key").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(appcache_SortedSectionsShouldBeSorted)
{
    ClearCache(Section::PROTOCOL);
    WriteCache(Section::PROTOCOL, "b", "321", 0);
    WriteCache(Section::PROTOCOL, "a", "123", 0);

    const SortedAppCacheSection& section = ReadSortedCacheSection(Section::PROTOCOL);
    auto it = section.begin();
    BOOST_CHECK(it->first == "a");
    BOOST_CHECK(it->second.value == "123");

    ++it;
    BOOST_CHECK(it->first == "b");
    BOOST_CHECK(it->second.value == "321");
}


BOOST_AUTO_TEST_SUITE_END()
