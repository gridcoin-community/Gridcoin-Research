// Copyright (c) 2024 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <util.h>
#include <gridcoin/sidestake.h>

BOOST_AUTO_TEST_SUITE(sidestake_tests)

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_trivial)
{
    GRC::Allocation allocation;

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), true);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), false);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_below_minimum)
{
    GRC::Allocation allocation((double) 0.0000499999);

    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), true);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), false);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_minimum)
{
    GRC::Allocation allocation((double) 0.0001);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double)
{
    GRC::Allocation allocation((double) 0.0005);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 2000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_one_percent)
{
    GRC::Allocation allocation((double) 0.01);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 100);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_just_below_unity)
{
    GRC::Allocation allocation((double) 0.9999);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 9999);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_double_maximum_before_multiplication)
{
    GRC::Allocation allocation((double) 1.0);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 1);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_Initialization_from_fraction)
{
    GRC::Allocation allocation(Fraction(2500, 10000));

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 2500);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), false);
    BOOST_CHECK_EQUAL(allocation.IsZero(), false);
    BOOST_CHECK_EQUAL(allocation.IsPositive(), true);
    BOOST_CHECK_EQUAL(allocation.IsNonNegative(), true);
    BOOST_CHECK_EQUAL(allocation.ToCAmount(), (CAmount) 0);

    allocation.Simplify();

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 4);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_ToPercent)
{
    GRC::Allocation allocation((double) 0.0005);

    BOOST_CHECK(std::abs(allocation.ToPercent() - (double) 0.05) < 1e-08);
}

BOOST_AUTO_TEST_CASE(sidestake_Allocation_multiplication_and_derivation_of_allocation)
{
    // Multiplication is a very common operation with Allocations, because
    // the general pattern is to multiply the allocation times a CAmount rewards
    // to determine the rewards in Halfords (CAmount) to put on the output.

    // Allocations that are initialized from doubles are rounded to the nearest 1/10000. This is the worst case
    // therefore, in terms of numerator and denominator.
    GRC::Allocation allocation(0.9999);

    BOOST_CHECK_EQUAL(allocation.GetNumerator(), 9999);
    BOOST_CHECK_EQUAL(allocation.GetDenominator(), 10000);
    BOOST_CHECK_EQUAL(allocation.IsSimplified(), true);

    CAmount max_accrual = 16384 * COIN;

    CAmount actual_output = static_cast<GRC::Allocation>(allocation * max_accrual).ToCAmount();
    BOOST_CHECK_EQUAL(actual_output, int64_t {1638236160000});
}

BOOST_AUTO_TEST_SUITE_END()
