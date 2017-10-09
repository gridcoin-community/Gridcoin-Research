#include "cpid.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cpid_tests);

#include <iostream>

BOOST_AUTO_TEST_CASE(cpid_VerifyComputeCPIDv2)
{
    uint256 blockhash = 1;
    BOOST_CHECK_EQUAL(
                ComputeCPIDv2("test@unittest.com", "abcdefghijklmno", blockhash),
                "4078bd252856710b95c4f31377bb1ba86bc3666ac7676a706e736ece7176d4756a7b76417ad36cd97a6976d78fc5d0d2");
}

BOOST_AUTO_TEST_SUITE_END()
