#include "appcache.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(appcache_tests)

BOOST_AUTO_TEST_CASE(appcache_WrittenCacheShouldBeReadable)
{
    ClearCache("section");
    WriteCache("section", "key", "hello", 123456789);
    BOOST_CHECK(ReadCache("section", "key").value == "hello");
}

BOOST_AUTO_TEST_CASE(appcache_ClearCacheShouldClearEntireSection)
{
    ClearCache("section");
    WriteCache("section", "key1", "hello", 123456789);
    WriteCache("section", "key2", "hello", 123456789);
    ClearCache("section");
    BOOST_CHECK(ReadCache("section", "key1").value.empty() == true);
    BOOST_CHECK(ReadCache("section", "key2").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(appcache_KeyShouldBeEmptyAfterDeleteCache)
{
    ClearCache("section");
    WriteCache("section", "key", "hello", 123456789);
    DeleteCache("section", "key");
    BOOST_CHECK(ReadCache("section", "key").value.empty() == true);
    BOOST_CHECK_EQUAL(GetCountOf("section"),0);
}

BOOST_AUTO_TEST_CASE(appcache_GetCountOfShouldCountEntries)
{
    ClearCache("section");
    WriteCache("section", "key1", "hello", 123456789);
    WriteCache("section", "key2", "hello", 123456789);
    WriteCache("section", "key3", "hello", 123456789);
    BOOST_CHECK_EQUAL(GetCountOf("section"), 3);
}

BOOST_AUTO_TEST_CASE(appcache_GetListOfBeaconShouldIgnoreInvestors)
{
    ClearCache("beacon");
    WriteCache("beacon", "CPID1", "INVESTOR", 12345);
    BOOST_CHECK(GetListOf("beacon").empty() == true);

    WriteCache("beacon", "CPID2", "abc123", 12345);
    BOOST_CHECK(GetListOf("beacon").empty() == false);
    BOOST_CHECK(GetListOf("beacon").find("abc123") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(appcache_SortedSectionsShouldBeSorted)
{
    ClearCache("beacon");
    WriteCache("beacon", "b", "321", 0);
    WriteCache("beacon", "a", "123", 0);
    
    const SortedAppCacheSection& section = ReadSortedCacheSection("beacon");
    auto it = section.begin();
    BOOST_CHECK(it->first == "a");
    BOOST_CHECK(it->second.value == "123");
    
    ++it;
    BOOST_CHECK(it->first == "b");
    BOOST_CHECK(it->second.value == "321");
}


BOOST_AUTO_TEST_SUITE_END()
