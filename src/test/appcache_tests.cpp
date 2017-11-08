#include "appcache.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(appcache_tests)

BOOST_AUTO_TEST_CASE(appcache_WrittenCacheShouldBeReadable)
{
    WriteCache("section", "key", "hello", 123456789);
    BOOST_CHECK(ReadCache("section", "key").value == "hello");
}

BOOST_AUTO_TEST_CASE(appcache_ClearCacheShouldClearEntireSection)
{
    WriteCache("section", "key1", "hello", 123456789);
    WriteCache("section", "key2", "hello", 123456789);
    ClearCache("section");
    BOOST_CHECK(ReadCache("section", "key1").value.empty() == true);
    BOOST_CHECK(ReadCache("section", "key2").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(appcache_KeyShouldBeEmptyAfterDeleteCache)
{
    WriteCache("section", "key", "hello", 123456789);
    DeleteCache("section", "key");
    BOOST_CHECK(ReadCache("section", "key").value.empty() == true);
}


BOOST_AUTO_TEST_SUITE_END()
