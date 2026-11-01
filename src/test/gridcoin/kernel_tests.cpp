// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "gridcoin/staking/kernel.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(kernel_tests)

BOOST_AUTO_TEST_CASE(stake_value_is_the_exact_inverse_of_stake_weight)
{
    BOOST_CHECK_EQUAL(GRC::STAKE_WEIGHT_UNIT_V8, 1250000);

    // Weight to value and back is the identity.
    for (const int64_t weight : {int64_t{0}, int64_t{1}, int64_t{79}, int64_t{80}, int64_t{1000000},
                                 MAX_MONEY / GRC::STAKE_WEIGHT_UNIT_V8}) {
        BOOST_CHECK_EQUAL(GRC::CalculateStakeWeightV8(GRC::CalculateStakeValueV8(weight)), weight);
    }

    // Value to weight keeps whole weight units only, so the round trip gives the largest whole-unit value that does
    // not exceed the original.
    for (const CAmount value : {CAmount{0}, CAmount{1}, CAmount{1249999}, CAmount{1250000}, CAmount{1250001},
                                COIN, CAmount{123456789012}, MAX_MONEY}) {
        const CAmount round_trip = GRC::CalculateStakeValueV8(GRC::CalculateStakeWeightV8(value));
        BOOST_CHECK_MESSAGE(round_trip <= value && value - round_trip < GRC::STAKE_WEIGHT_UNIT_V8, "value " << value);
    }
}

BOOST_AUTO_TEST_SUITE_END()
