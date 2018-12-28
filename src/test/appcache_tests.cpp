#include "appcache.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(appcache_tests)

BOOST_AUTO_TEST_CASE(appcache_WrittenCacheShouldBeReadable)
{
    ClearCache(Section::GLOBAL);
    WriteCache(Section::GLOBAL, "key", "hello", 123456789);
    BOOST_CHECK(ReadCache(Section::GLOBAL, "key").value == "hello");
}

BOOST_AUTO_TEST_CASE(appcache_ClearCacheShouldClearEntireSection)
{
    ClearCache(Section::GLOBAL);
    WriteCache(Section::GLOBAL, "key1", "hello", 123456789);
    WriteCache(Section::GLOBAL, "key2", "hello", 123456789);
    ClearCache(Section::GLOBAL);
    BOOST_CHECK(ReadCache(Section::GLOBAL, "key1").value.empty() == true);
    BOOST_CHECK(ReadCache(Section::GLOBAL, "key2").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(appcache_KeyShouldBeEmptyAfterDeleteCache)
{
    ClearCache(Section::GLOBAL);
    WriteCache(Section::GLOBAL, "key", "hello", 123456789);
    DeleteCache(Section::GLOBAL, "key");
    BOOST_CHECK(ReadCache(Section::GLOBAL, "key").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(appcache_SortedSectionsShouldBeSorted)
{
    ClearCache(Section::BEACON);
    WriteCache(Section::BEACON, "b", "321", 0);
    WriteCache(Section::BEACON, "a", "123", 0);
    
    const SortedAppCacheSection& section = ReadSortedCacheSection(Section::BEACON);
    auto it = section.begin();
    BOOST_CHECK(it->first == "a");
    BOOST_CHECK(it->second.value == "123");
    
    ++it;
    BOOST_CHECK(it->first == "b");
    BOOST_CHECK(it->second.value == "321");
}


BOOST_AUTO_TEST_SUITE_END()
