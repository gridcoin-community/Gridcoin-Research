#include <boost/test/unit_test.hpp>

#include "uint256.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"

#include <cstdint>

BOOST_AUTO_TEST_SUITE(uint256_tests)

BOOST_AUTO_TEST_CASE(uint256_equality)
{
    uint256 num1 = 10;
    uint256 num2 = 11;
    BOOST_CHECK(num1+1 == num2);

    uint64_t num3 = 10;
    BOOST_CHECK(num1 == num3);
    BOOST_CHECK(num1+num2 == num3+num2);
}

BOOST_AUTO_TEST_CASE(uint256_serialize)
{
    const std::string hex_in("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    uint256 num;
    num.SetHex(hex_in);
    CDataStream ds(SER_NETWORK,1);
    ds << num;
    BOOST_CHECK(32 == ds.size());
    std::string hex_out = HexStr(ds.begin(), ds.end());
    BOOST_CHECK_EQUAL( hex_out,
    "1f1e1d1c1b1a191817161514131211100f0e0d0c0b0a09080706050403020100"
    );
}

BOOST_AUTO_TEST_SUITE_END()
