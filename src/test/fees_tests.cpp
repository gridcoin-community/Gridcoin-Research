// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "policy/fees.h"

#include <boost/test/unit_test.hpp>

#include <limits>

BOOST_AUTO_TEST_SUITE(fees_tests)

// FeeRate compares fee / size as the exact ratio, in integers.

BOOST_AUTO_TEST_CASE(equal_ratios_are_equal_rates)
{
    BOOST_CHECK((FeeRate{1, 2} == FeeRate{2, 4}));
    BOOST_CHECK((!(FeeRate{1, 2} < FeeRate{2, 4})));
    BOOST_CHECK((!(FeeRate{2, 4} < FeeRate{1, 2})));
    BOOST_CHECK((FeeRate{0, 1} == FeeRate{0, 250}));
    BOOST_CHECK((FeeRate{1, 3} != FeeRate{1, 2}));
}

BOOST_AUTO_TEST_CASE(a_rate_exactly_at_a_floor_is_not_below_it)
{
    // 137751 satoshi over 340 bytes is exactly 405150 satoshi per 1000 bytes.
    // Worked out as 137751 / (340 / 1000.0) in a 64-bit double (x86_64, ARM),
    // it comes out just under 405150.
    BOOST_CHECK((FeeRate{137751, 340} == FeeRate{405150, 1000}));
    BOOST_CHECK((FeeRate{137750, 340} < FeeRate{405150, 1000}));
    BOOST_CHECK((FeeRate{137752, 340} > FeeRate{405150, 1000}));
}

BOOST_AUTO_TEST_CASE(it_separates_rates_a_double_cannot)
{
    // 2e17 and 2e17 + 1 satoshi over 1000 bytes differ by 0.001 satoshi per
    // byte. A 64-bit double near 2e14 resolves only 1/32.
    const CAmount fee = 200000000000000000;

    BOOST_CHECK((FeeRate{fee, 1000} < FeeRate{fee + 1, 1000}));
    BOOST_CHECK((FeeRate{fee + 1, 1000} > FeeRate{fee, 1000}));

    // The same fee over one byte more is a lower rate.
    BOOST_CHECK((FeeRate{fee, 1001} < FeeRate{fee, 1000}));
}

BOOST_AUTO_TEST_CASE(negative_fees_order_below_zero)
{
    BOOST_CHECK((FeeRate{-1, 2} < FeeRate{-1, 3})); // -0.5 < -0.333...
    BOOST_CHECK((FeeRate{-3, 2} < FeeRate{-1, 1})); // -1.5 < -1
    BOOST_CHECK((FeeRate{-1, 1} < FeeRate{0, 5}));
    BOOST_CHECK((FeeRate{-2, 4} == FeeRate{-1, 2}));
}

BOOST_AUTO_TEST_CASE(extreme_fees_and_sizes_do_not_overflow)
{
    const CAmount max = std::numeric_limits<CAmount>::max();
    const CAmount min = std::numeric_limits<CAmount>::min();
    const unsigned int big = std::numeric_limits<unsigned int>::max();

    BOOST_CHECK((FeeRate{max - 1, big} < FeeRate{max, big}));
    BOOST_CHECK((FeeRate{min, big} < FeeRate{min + 1, big}));
    BOOST_CHECK((FeeRate{min, 1} < FeeRate{max, 1}));

    // Cross-multiplying the fees by the sizes would overflow here and get
    // both of these wrong.
    BOOST_CHECK((FeeRate{max, 4} < FeeRate{max, 3}));
    BOOST_CHECK((FeeRate{min, 3} < FeeRate{min, 4}));

    // The same fee over fewer bytes: a higher rate when the fee is positive,
    // a lower one when it is negative.
    BOOST_CHECK((FeeRate{max, big} < FeeRate{max, big - 1}));
    BOOST_CHECK((FeeRate{min, big - 1} < FeeRate{min, big}));
}

BOOST_AUTO_TEST_CASE(it_is_a_strict_weak_order)
{
    const FeeRate third{1, 3};
    const FeeRate also_third{2, 6};
    const FeeRate half{1, 2};

    BOOST_CHECK(!(third < third));
    BOOST_CHECK(third < half);
    BOOST_CHECK(!(half < third));

    // Equal rates order the same way against a third rate.
    BOOST_CHECK(also_third < half);
    BOOST_CHECK(!(half < also_third));
}

BOOST_AUTO_TEST_SUITE_END()
