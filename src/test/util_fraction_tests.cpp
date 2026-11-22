// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/fraction.h>

#include <amount.h>
#include <chainparams.h>
#include <gridcoin/protocol.h>
#include <gridcoin/sidestake.h>
#include <logging.h>
#include <random.h>
#include <streams.h>
#include <test/state_guard.h>
#include <util.h>
#include <util/time.h>
#include <version.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
// This version, which is recommended by some resources on the web, is actually slower, and has several issues. See
// the unit tests below.
unsigned int msb(const int64_t& n)
{
    // Can't take the log of 0.
    if (n == 0) {
        return 0U;
    }

    // Log2 is O(1) both time and space-wise. n is not zero here, so |n| >= 1 and
    // floor(log2(|n|)) >= 0: the cast to unsigned keeps the value.
    return (static_cast<unsigned int>(floor(log2(std::abs(n)))) + 1U);
}

unsigned int msb2(const int64_t& n_in)
{
    int64_t n = std::abs(n_in);

    unsigned int index = 0;

    if (n == 0) {
        return 0U;
    }

    // sign-compare checked: i is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    for (unsigned int i = 0; i <= 63U; ++i) {
        if (n % 2 == 1) {
            index = i;
        }

        n /= 2;
    }

    return index + 1;
}

// The shift loop the Fraction overflow guard used until Fraction::BitWidth replaced it.
// The tests below compare the two.
unsigned int msb3(const int64_t& n_in)
{
    int64_t n = std::abs(n_in);

    unsigned int index = 0;

    // sign-compare checked: index is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    for (; index <= 63U; ++index) {
        if (n >> index == 0) {
            break;
        }
    }

    return index;
}
} //anonymous namespace

BOOST_AUTO_TEST_SUITE(util_fraction_tests, *boost::unit_test::fixture<grc_test::StateGuard>())

BOOST_AUTO_TEST_CASE(Fraction_msb_algorithm_equivalence)
{
    // sign-compare checked: i is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    for (unsigned int i = 0; i <= 63U; ++i) {
        int64_t n = 0;

        // sign-compare checked: i is unsigned int; 0U is a non-negative literal, so both sides are unsigned.
        if (i > 0U) {
            n = (int64_t {1} << (i - 1));
        }

        BOOST_CHECK_EQUAL(msb(n), i);
    }

    int bias_for_msb_result_63 = 0;

    // msb ugly, ugly, ugly. Log2 looses resolution near the top of the range...
    for (int i = 0; i < 16; ++i) {
        bias_for_msb_result_63 = (int64_t {1} << i);

        unsigned int msb_result = msb(std::numeric_limits<int64_t>::max() - bias_for_msb_result_63);

        // sign-compare checked: msb_result is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
        if (msb_result == 63U) {
            LogPrintf("INFO: %s: bias_for_msb_result_63 = %i, msb_result = %u", __func__, bias_for_msb_result_63, msb_result);
            break;
        } else {
        }
    }

    // bias_for_msb_result_63 is currently 32768! It should be zero! This disqualifies the log2 based approach based on
    // a correctness check.
    // sign-compare checked: msb() is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(msb(std::numeric_limits<int64_t>::max() - bias_for_msb_result_63), 63U);

    // sign-compare checked: msb2() is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(msb2(std::numeric_limits<int64_t>::max()), 63U);
    // sign-compare checked: msb3() is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(msb3(std::numeric_limits<int64_t>::max()), 63U);

    std::vector<std::pair<int64_t, unsigned int>> msb_results, msb2_results, msb3_results;

    unsigned int iterations = 1000;

    FastRandomContext rand(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        int64_t n = rand.rand32();

        msb_results.push_back(std::make_pair(n, msb(n)));
    }

    FastRandomContext rand2(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        int64_t n = rand2.rand32();

        msb2_results.push_back(std::make_pair(n, msb2(n)));
    }

    FastRandomContext rand3(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        int64_t n = rand3.rand32();

        msb3_results.push_back(std::make_pair(n, msb3(n)));
    }

    bool success = true;

    for (unsigned int i = 0; i < iterations; ++i) {
        if (msb_results[i] != msb2_results[i] || msb_results[i] != msb3_results[i]) {
            success = false;
            error("%s: iteration %u: mismatch: %" PRId64 ", msb = %u, %" PRId64 " msb2 = %u, %" PRId64 " msb3 = %u",
                  __func__,
                  i,
                  msb_results[i].first,
                  msb_results[i].second,
                  msb2_results[i].first,
                  msb2_results[i].second,
                  msb3_results[i].first,
                  msb3_results[i].second
                  );
        }
    }

    BOOST_CHECK(success);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_trivial)
{
    Fraction fraction;

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), true);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_already_simplified)
{
    Fraction fraction(2, 3);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}


BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_not_simplified)
{
    Fraction fraction(4, 6);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 4);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 6);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), false);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification)
{
    Fraction fraction(4, 6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification_neg_pos)
{
    Fraction fraction(-4, 6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification_pos_neg)
{
    Fraction fraction(4, -6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification_neg_neg)
{
    Fraction fraction(-4, -6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Copy_Constructor)
{
    Fraction fraction(4, 6);

    Fraction fraction2(fraction);

    BOOST_CHECK_EQUAL(fraction2.GetNumerator(), 4);
    BOOST_CHECK_EQUAL(fraction2.GetDenominator(), 6);
    BOOST_CHECK_EQUAL(fraction2.IsSimplified(), false);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_int64_t)
{
    Fraction fraction((int64_t) -2);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_string)
{
    Fraction fraction = Fraction::FromString("100/-567");

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -100);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 567);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Simplify)
{
    Fraction fraction(-4, -6);

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), false);

    fraction.Simplify();

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_ToDouble)
{
    Fraction fraction (1, 4);

    BOOST_CHECK_EQUAL(fraction.ToDouble(), 0.25);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition)
{
    Fraction lhs(2, 3);
    Fraction rhs(3, 4);

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 17);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 12);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_with_internal_simplification_common_denominator)
{
    Fraction lhs(3, 10);
    Fraction rhs(2, 10);

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_with_internal_simplification)
{
    Fraction lhs(3, 10);
    Fraction rhs(1, 5);

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_with_internal_gcd_simplification)
{
    Fraction lhs(1, 6);
    Fraction rhs(2, 15);

    // gcd(6, 15) = 3, so this really is
    //
    // 1 * (15/3) + 2 * (6/3)   1 * 5 + 2 * 2    3
    // ---------------------- = ------------- = --
    //   3 * (6/3) * (15/3)       3 * 2 * 5     10

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 3);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 10);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_subtraction)
{
    Fraction lhs(2, 3);
    Fraction rhs(3, 4);

    Fraction difference = lhs - rhs;

    BOOST_CHECK_EQUAL(difference.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(difference.GetDenominator(), 12);
    BOOST_CHECK_EQUAL(difference.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_subtraction_with_internal_simplification)
{
    Fraction lhs(2, 10);
    Fraction rhs(7, 10);

    Fraction difference = lhs - rhs;

    BOOST_CHECK_EQUAL(difference.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(difference.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(difference.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_with_internal_simplification)
{
    Fraction lhs(-2, 3);
    Fraction rhs(3, 4);

    Fraction product = lhs * rhs;

    BOOST_CHECK_EQUAL(product.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(product.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(product.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_with_cross_simplification_overflow_resistance)
{

    Fraction lhs(std::numeric_limits<int64_t>::max() - 3, std::numeric_limits<int64_t>::max() - 1, false);
    Fraction rhs((std::numeric_limits<int64_t>::max() - 1) / (int64_t) 2, (std::numeric_limits<int64_t>::max() - 3) / (int64_t) 2);

    Fraction product;

    // This should NOT overflow
    bool overflow = false;
    try {
        product = lhs * rhs;
    } catch (std::overflow_error& e) {
        overflow = true;
    }

    BOOST_CHECK_EQUAL(overflow, false);

    if (!overflow) {
        BOOST_CHECK(product == Fraction(1));
    }
}

BOOST_AUTO_TEST_CASE(util_Fraction_division_with_internal_simplification)
{
    Fraction lhs(-2, 3);
    Fraction rhs(4, 3);

    Fraction quotient = lhs / rhs;

    BOOST_CHECK_EQUAL(quotient.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(quotient.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(quotient.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_addition_with_internal_simplification)
{
    Fraction fraction(3, 10);

    fraction += Fraction(2, 10);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_subtraction_with_internal_simplification)
{
    Fraction fraction(7, 10);

    fraction -= Fraction(2, 10);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_multiplication_with_internal_simplification)
{
    Fraction fraction(-2, 3);

    fraction *= Fraction(3, 4);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_division_with_internal_simplification)
{
    Fraction fraction(-2, 3);

    fraction /= Fraction(4, 3);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_by_zero_Fraction)
{
    Fraction lhs(-2, 3);
    Fraction rhs(0);

    Fraction product = lhs * rhs;

    BOOST_CHECK_EQUAL(product.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(product.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(product.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_division_by_zero_Fraction)
{
    Fraction lhs(-2, 3);
    Fraction rhs(0);

    std::string err;

    try {
        [[maybe_unused]] Fraction quotient = lhs / rhs;
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, "denominator specified is zero");
}

BOOST_AUTO_TEST_CASE(util_Fraction_division_by_zero_int64_t)
{
    Fraction lhs(-2, 3);
    int64_t rhs = 0;

    std::string err;

    try {
        [[maybe_unused]] Fraction quotient = lhs / rhs;
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string{"denominator specified is zero"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_1)
{
    Fraction lhs((int64_t) 1 << 30, 1);
    Fraction rhs((int64_t) 1 << 31, 1);

    LogPrintf("INFO: %s: msb((int64_t) 1 << 30) = %u", __func__, msb3((int64_t) 1 << 30));
    LogPrintf("INFO: %s: msb((int64_t) 1 << 31) = %u", __func__, msb3((int64_t) 1 << 31));

    std::string err;

    try {
        [[maybe_unused]] Fraction product = lhs * rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_2)
{
    Fraction lhs(((int64_t) 1 << 31) - 1, 1);
    Fraction rhs(((int64_t) 1 << 31) - 1, 1);

    std::string err;

    try {
        [[maybe_unused]] Fraction product = lhs * rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {""});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_3)
{
    Fraction lhs((int64_t) 1 << 31, 1);
    Fraction rhs((int64_t) 1 << 31, 1);

    std::string err;

    try {
        [[maybe_unused]] Fraction product = lhs * rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {"fraction multiplication results in an overflow"});
}


BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_1)
{
    Fraction lhs(std::numeric_limits<int64_t>::max() / 2, 1);
    Fraction rhs(std::numeric_limits<int64_t>::max() / 2 + 1, 1);

    std::string err;

    try {
        [[maybe_unused]] Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {});
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_2)
{
    Fraction lhs(std::numeric_limits<int64_t>::max() / 2 + 1, 1);
    Fraction rhs(std::numeric_limits<int64_t>::max() / 2 + 1, 1);

    std::string err;

    try {
        [[maybe_unused]] Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {"fraction addition of a + b where a > 0 and b > 0 results in an overflow"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_3)
{
    Fraction lhs(std::numeric_limits<int64_t>::min() / 2 + 1, 1);
    Fraction rhs(std::numeric_limits<int64_t>::min() / 2, 1);

    std::string err;

    try {
        [[maybe_unused]] Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {});
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_4)
{
    Fraction lhs(std::numeric_limits<int64_t>::min() / 2, 1);
    Fraction rhs(std::numeric_limits<int64_t>::min() / 2, 1);

    std::string err;

    try {
        [[maybe_unused]] Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {"fraction addition of a + b where a < 0 and b < 0 results in an overflow"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_equal)
{
    BOOST_CHECK_EQUAL(Fraction(1, 2) == Fraction(2, 4), true);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) == Fraction(1, -2), true);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) == Fraction(1, 2), false);
    // Same numerator, different denominator — must compare as not equal
    BOOST_CHECK_EQUAL(Fraction(1, 2) == Fraction(1, 3), false);
    BOOST_CHECK_EQUAL(Fraction(3, 5) == Fraction(3, 7), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_not_equal)
{
    BOOST_CHECK_EQUAL(Fraction(1, 2) != Fraction(2, 4), false);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) != Fraction(1, -2), false);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) != Fraction(1, 2), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_less_than_or_equal)
{
    BOOST_CHECK_EQUAL(Fraction(3, 4) <= Fraction(4, 5), true);
    BOOST_CHECK_EQUAL(Fraction(3, 4) <= Fraction(6, 8), true);
    BOOST_CHECK_EQUAL(Fraction(3, 4) <= Fraction(2, 3), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_greater_than_or_equal)
{
    BOOST_CHECK_EQUAL(Fraction(4, 5) >= Fraction(3, 4), true);
    BOOST_CHECK_EQUAL(Fraction(6, 8) >= Fraction(3, 4), true);
    BOOST_CHECK_EQUAL(Fraction(2, 3) >= Fraction(3, 4), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_less_than)
{
    BOOST_CHECK_EQUAL(Fraction(3, 4) < Fraction(4, 5), true);
    BOOST_CHECK_EQUAL(Fraction(3, 4) < Fraction(6, 8), false);
    BOOST_CHECK_EQUAL(Fraction(3, 4) < Fraction(2, 3), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_greater_than)
{
    BOOST_CHECK_EQUAL(Fraction(4, 5) > Fraction(3, 4), true);
    BOOST_CHECK_EQUAL(Fraction(6, 8) > Fraction(3, 4), false);
    BOOST_CHECK_EQUAL(Fraction(2, 3) > Fraction(3, 4), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_logic_negation)
{
    BOOST_CHECK_EQUAL(!Fraction(1, 2), false);
    BOOST_CHECK_EQUAL(!Fraction(-1, 2), false);
    BOOST_CHECK_EQUAL(!Fraction(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_ToString)
{
    Fraction fraction(123, 10000);

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.ToString(), "123/10000");
}

BOOST_AUTO_TEST_CASE(util_Fraction_FromString)
{
    Fraction fraction = Fraction::FromString("100/567");

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK(fraction == Fraction(100, 567, true));

    fraction = Fraction::FromString("-100/567");

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK(fraction == Fraction(-100, 567, true));

    fraction = Fraction::FromString("100/-567");

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK(fraction == Fraction(-100, 567, true));

    fraction = Fraction::FromString("5");

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK(fraction == Fraction(5, 1, true));

    std::string err;
    std::string valid_err_message {"fraction input string cannot be parsed to fraction"};

    try {
        [[maybe_unused]] Fraction fraction = Fraction::FromString("100/567/300");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);

    try {
        [[maybe_unused]] Fraction fraction = Fraction::FromString("100 / 567");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);

    try {
        [[maybe_unused]] Fraction fraction = Fraction::FromString("100.1");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);

    try {
        [[maybe_unused]] Fraction fraction = Fraction::FromString("");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);
}

BOOST_AUTO_TEST_CASE(Fraction_msb_performance_test)
{
    // Fraction::BitWidth, a leading-zero count, replaced the msb3 shift loop
    // in the overflow guard. This brackets the two so that a regression to a
    // slow implementation is noticed: the count must not run slower than the
    // loop it replaced, by any margin a machine could introduce.
    const unsigned int iterations = 10000000;
    const int64_t min = std::numeric_limits<int64_t>::min();

    g_timer.InitTimer("msb_test", true);

    int64_t sink = 0;

    FastRandomContext rand_loop(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        const int64_t n = static_cast<int64_t>(rand_loop.rand64());

        // msb3 takes std::abs, which is undefined on INT64_MIN.
        if (n != min) {
            sink += msb3(n);
        }
    }

    const int64_t loop_time = g_timer.GetTimes(strprintf("msb3 %u iterations", iterations), "msb_test").time_since_last_check;

    FastRandomContext rand_width(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        sink += Fraction::BitWidth(static_cast<int64_t>(rand_width.rand64()));
    }

    const int64_t width_time = g_timer.GetTimes(strprintf("BitWidth %u iterations", iterations), "msb_test").time_since_last_check;

    LogPrintf("INFO: %s: msb3 %" PRId64 " ms, Fraction::BitWidth %" PRId64 " ms", __func__, loop_time, width_time);

    BOOST_CHECK(sink != 0);
    BOOST_CHECK(width_time <= 20 * std::max<int64_t>(loop_time, 1));
}

BOOST_AUTO_TEST_CASE(bit_width_matches_the_former_shift_loop)
{
    // The guard's throw set must not move with the implementation, so the
    // count must agree with the loop on every value the loop could take.
    const int64_t max = std::numeric_limits<int64_t>::max();
    const int64_t min = std::numeric_limits<int64_t>::min();

    // sign-compare checked: BitWidth() is unsigned int; 0U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(0), 0U);
    // sign-compare checked: BitWidth() is unsigned int; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(1), 1U);
    // sign-compare checked: BitWidth() is unsigned int; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(-1), 1U);
    // sign-compare checked: BitWidth() is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(max), 63U);
    // sign-compare checked: BitWidth() is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(-max), 63U);
    // |INT64_MIN| = 2^63 takes 64 bits. The loop could not be given this value
    // (std::abs is undefined on it); the guard special-cased it to 64.
    // sign-compare checked: BitWidth() is unsigned int; 64U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(min), 64U);

    // sign-compare checked: i is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    for (unsigned int i = 0; i < 63U; ++i) {
        const int64_t power = int64_t {1} << i;

        BOOST_CHECK_EQUAL(Fraction::BitWidth(power), i + 1);
        BOOST_CHECK_EQUAL(Fraction::BitWidth(-power), i + 1);
        BOOST_CHECK_EQUAL(Fraction::BitWidth(power - 1), i);
        BOOST_CHECK_EQUAL(Fraction::BitWidth(power), msb3(power));
        BOOST_CHECK_EQUAL(Fraction::BitWidth(power - 1), msb3(power - 1));
    }

    FastRandomContext rand(uint256 {0});

    for (int i = 0; i < 10000; ++i) {
        const int64_t n = static_cast<int64_t>(rand.rand64());

        if (n != min) {
            BOOST_CHECK_EQUAL(Fraction::BitWidth(n), msb3(n));
        }
    }
}

BOOST_AUTO_TEST_CASE(bit_width_on_both_edges_of_every_width)
{
    // A width of k bits covers the magnitudes 2^(k-1) through 2^k - 1. Each width is
    // checked at both ends of that range and at random magnitudes inside it, on both
    // signs, against the literal width rather than another implementation.
    // sign-compare checked: BitWidth() is unsigned int; 0U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(0), 0U);
    // sign-compare checked: BitWidth() is unsigned int; 64U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(Fraction::BitWidth(std::numeric_limits<int64_t>::min()), 64U);

    FastRandomContext rand(uint256 {2});

    // sign-compare checked: k is unsigned int; 63U is a non-negative literal, so both sides are unsigned.
    for (unsigned int k = 1; k <= 63U; ++k) {
        const int64_t low = int64_t {1} << (k - 1);
        // 2^k - 1, written so that k = 63 gives INT64_MAX without overflowing.
        const int64_t high = (low - 1) + low;

        BOOST_CHECK_EQUAL(Fraction::BitWidth(low), k);
        BOOST_CHECK_EQUAL(Fraction::BitWidth(-low), k);
        BOOST_CHECK_EQUAL(Fraction::BitWidth(high), k);
        BOOST_CHECK_EQUAL(Fraction::BitWidth(-high), k);

        for (int i = 0; i < 16; ++i) {
            // low + [0, low) stays in [2^(k-1), 2^k - 1].
            const int64_t n = low + static_cast<int64_t>(rand.randrange(static_cast<uint64_t>(low)));

            BOOST_CHECK_EQUAL(Fraction::BitWidth(n), k);
            BOOST_CHECK_EQUAL(Fraction::BitWidth(-n), k);
        }
    }
}

BOOST_AUTO_TEST_CASE(multiplication_throws_exactly_when_the_widths_pass_63)
{
    // overflow_mult(), the only caller of BitWidth(), refuses a product when the factors'
    // widths sum to more than 63. Fraction(a, 1) * Fraction(b, 1) reaches it as
    // overflow_mult(a, b), since both cross gcds are gcd(x, 1) = 1. Every pair of widths
    // is checked with factors at both ends of each width and of both signs, so the set
    // of products that throw is pinned wherever the bound can fall.
    const auto factors = [](int width) {
        const int64_t low = int64_t {1} << (width - 1);
        const int64_t high = (low - 1) + low;

        return std::vector<int64_t> {low, high, -low, -high};
    };

    for (int width_a = 1; width_a <= 63; ++width_a) {
        for (int width_b = 1; width_b <= 63; ++width_b) {
            const bool must_throw = width_a + width_b > 63;

            for (const int64_t a : factors(width_a)) {
                for (const int64_t b : factors(width_b)) {
                    bool threw = false;
                    Fraction product;

                    try {
                        product = Fraction(a, 1) * Fraction(b, 1);
                    } catch (const std::overflow_error&) {
                        threw = true;
                    }

                    BOOST_CHECK_EQUAL(threw, must_throw);

                    // a * b is only formed where the widths prove it fits.
                    if (!threw && !must_throw) {
                        BOOST_CHECK_EQUAL(product.GetNumerator(), a * b);
                        BOOST_CHECK_EQUAL(product.GetDenominator(), 1);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(copy_keeps_the_pair_and_the_flag)
{
    // Copy construction and assignment are both member-wise: neither reduces,
    // neither recomputes.
    const Fraction unreduced(2, 4);

    const Fraction copied(unreduced);
    BOOST_CHECK_EQUAL(copied.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(copied.GetDenominator(), 4);
    BOOST_CHECK_EQUAL(copied.IsSimplified(), false);

    Fraction assigned;
    assigned = unreduced;
    BOOST_CHECK_EQUAL(assigned.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(assigned.GetDenominator(), 4);
    BOOST_CHECK_EQUAL(assigned.IsSimplified(), false);

    const Fraction reduced(unreduced, true);
    BOOST_CHECK_EQUAL(reduced.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(reduced.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(reduced.IsSimplified(), true);
    BOOST_CHECK(reduced == unreduced);
}

BOOST_AUTO_TEST_CASE(sign_predicates_read_unreduced_pairs)
{
    BOOST_CHECK(Fraction(1, -2).IsNegative());
    BOOST_CHECK(!Fraction(1, -2).IsPositive());
    BOOST_CHECK(Fraction(-1, -2).IsPositive());
    BOOST_CHECK(!Fraction(-1, -2).IsNegative());
    BOOST_CHECK(Fraction(0, -2).IsZero());
    BOOST_CHECK(Fraction(0, -2).IsNonNegative());
    BOOST_CHECK(!Fraction(0, -2).IsNegative());
    BOOST_CHECK(!Fraction(0, -2));
}

BOOST_AUTO_TEST_CASE(integer_multiply_cancels_against_the_denominator_first)
{
    const int64_t big = int64_t {1} << 40;

    // (2^40 + 1) / 2^40 * 2^40: the product 2^40 (2^40 + 1) does not fit, the
    // result 2^40 + 1 does. Cancelling 2^40 first never forms the product.
    const Fraction scaled = Fraction(big + 1, big) * big;
    BOOST_CHECK_EQUAL(scaled.GetNumerator(), big + 1);
    BOOST_CHECK_EQUAL(scaled.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(scaled.IsSimplified(), true);

    // The sign rides on the numerator.
    BOOST_CHECK(Fraction(-3, 4) * 8 == Fraction(-6));
    BOOST_CHECK(Fraction(3, 4) * -8 == Fraction(-6));
    BOOST_CHECK(Fraction(-3, 4) * -8 == Fraction(6));

    // Zero either way is 0/1.
    const Fraction zero_lhs = Fraction(0, 7) * 5;
    BOOST_CHECK_EQUAL(zero_lhs.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(zero_lhs.GetDenominator(), 1);

    const Fraction zero_rhs = Fraction(3, 4) * 0;
    BOOST_CHECK_EQUAL(zero_rhs.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(zero_rhs.GetDenominator(), 1);

    // A result that leaves int64_t still throws. 3/2 * (INT64_MAX - 1) cancels
    // the 2 and then needs 3 (INT64_MAX - 1) / 2, which is above INT64_MAX.
    BOOST_CHECK_EXCEPTION(Fraction(3, 2) * (std::numeric_limits<int64_t>::max() - 1),
                          std::overflow_error,
                          [](const std::overflow_error& e) {
                              return std::string(e.what()) == "fraction multiplication results in an overflow";
                          });

    // INT64_MIN as the integer is refused outright: the integer becomes
    // Fraction(INT64_MIN), which the constructor rejects.
    BOOST_CHECK_THROW(Fraction(0) * std::numeric_limits<int64_t>::min(), std::out_of_range);
    BOOST_CHECK_THROW(Fraction(1) * std::numeric_limits<int64_t>::min(), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(integer_divide_cancels_against_the_numerator_first)
{
    const int64_t big = int64_t {1} << 40;

    // 2^40 / (2^40 + 1) divided by 2^40: the denominator product does not fit,
    // the result 1 / (2^40 + 1) does.
    const Fraction quotient = Fraction(big, big + 1) / big;
    BOOST_CHECK_EQUAL(quotient.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(quotient.GetDenominator(), big + 1);

    BOOST_CHECK(Fraction(3, 4) / -2 == Fraction(-3, 8));
    BOOST_CHECK(Fraction(0, 5) / 7 == Fraction(0));

    BOOST_CHECK_EXCEPTION(Fraction(3, 4) / 0,
                          std::out_of_range,
                          [](const std::out_of_range& e) {
                              return std::string(e.what()) == "denominator specified is zero";
                          });
    BOOST_CHECK_THROW(Fraction(0) / 0, std::out_of_range);
    BOOST_CHECK_THROW(Fraction(1) / std::numeric_limits<int64_t>::min(), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(int64_min_is_refused_at_every_entry)
{
    const int64_t min = std::numeric_limits<int64_t>::min();

    BOOST_CHECK_THROW(Fraction{min}, std::out_of_range); // braces: Fraction(min) would declare a variable
    BOOST_CHECK_THROW(Fraction(min, 1), std::out_of_range);
    BOOST_CHECK_THROW(Fraction(1, min), std::out_of_range);
    BOOST_CHECK_THROW(Fraction(min, 1, true), std::out_of_range);

    // As an integer operand: every integer overload builds Fraction(min)
    // first, so the constructor refuses it before any arithmetic.
    BOOST_CHECK_THROW(Fraction(1) + min, std::out_of_range);
    BOOST_CHECK_THROW(Fraction(1) - min, std::out_of_range);
    BOOST_CHECK_THROW(Fraction(1) * min, std::out_of_range);
    BOOST_CHECK_THROW(Fraction(1) / min, std::out_of_range);
    BOOST_CHECK_THROW(static_cast<void>(Fraction(1) == min), std::out_of_range);
}

namespace {
//! The exact result of x op y for operands small enough that the int64_t
//! cross products fit, computed the schoolbook way.
Fraction Reference(int64_t a, int64_t b, int64_t c, int64_t d, char op)
{
    switch (op) {
    case '+': return Fraction(a * d + c * b, b * d, true);
    case '-': return Fraction(a * d - c * b, b * d, true);
    case '*': return Fraction(a * c, b * d, true);
    default:  return Fraction(a * d, b * c, true);
    }
}
} // anonymous namespace

BOOST_AUTO_TEST_CASE(arithmetic_matches_an_exact_reference)
{
    FastRandomContext rand(uint256 {1});

    for (int i = 0; i < 5000; ++i) {
        // |numerators| < 2^30, denominators in [1, 2^30]: every cross product
        // below stays within 2^60 and every sum within 2^61, inside the guard
        // for the reference and for the class alike.
        const int64_t a = static_cast<int64_t>(rand.rand32() >> 1) - (int64_t {1} << 30);
        const int64_t b = static_cast<int64_t>(rand.rand32() >> 2) + 1;
        const int64_t c = static_cast<int64_t>(rand.rand32() >> 1) - (int64_t {1} << 30);
        const int64_t d = static_cast<int64_t>(rand.rand32() >> 2) + 1;

        const Fraction x(a, b);
        const Fraction y(c, d);

        const Fraction sum = x + y;
        const Fraction difference = x - y;
        const Fraction product = x * y;

        BOOST_CHECK(sum == Reference(a, b, c, d, '+'));
        BOOST_CHECK(difference == Reference(a, b, c, d, '-'));
        BOOST_CHECK(product == Reference(a, b, c, d, '*'));

        if (c != 0) {
            BOOST_CHECK(x / y == Reference(a, b, c, d, '/'));
        }

        // Every result comes back in canonical form.
        for (const Fraction* result : {&sum, &difference, &product}) {
            BOOST_CHECK(result->IsSimplified());
            BOOST_CHECK(result->GetDenominator() > 0);
            BOOST_CHECK_EQUAL(std::gcd(result->GetNumerator(), result->GetDenominator()), 1);
        }

        // Ordering agrees with the sign of a d - c b.
        const int64_t cross = a * d - c * b;

        BOOST_CHECK_EQUAL(x < y, cross < 0);
        BOOST_CHECK_EQUAL(x > y, cross > 0);
        BOOST_CHECK_EQUAL(x == y, cross == 0);
        BOOST_CHECK_EQUAL(x <= y, cross <= 0);
        BOOST_CHECK_EQUAL(x >= y, cross >= 0);
    }
}

BOOST_AUTO_TEST_CASE(deserialization_keeps_the_wire_flag_and_round_trips_bytes)
{
    // The flag byte is carried as received and written back as held, because
    // transaction hashes are computed from re-serialized members. A byte that
    // disagrees with its pair changes IsSimplified() and nothing else: every
    // operator rebuilds its operands from the pair.
    const auto round_trip = [](int64_t num, int64_t den, bool flag) {
        CDataStream in(SER_NETWORK, PROTOCOL_VERSION);
        in << num << den << flag;
        const std::vector<std::byte> wire(in.begin(), in.end());

        Fraction f;
        in >> f;

        CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
        out << f;

        BOOST_CHECK_EQUAL(f.GetNumerator(), num);
        BOOST_CHECK_EQUAL(f.GetDenominator(), den);
        BOOST_CHECK_EQUAL(f.IsSimplified(), flag);
        BOOST_CHECK_EQUAL(wire.size(), 17U);
        BOOST_CHECK(std::vector<std::byte>(out.begin(), out.end()) == wire);

        return f;
    };

    // Honest encodings, as every released build writes them.
    round_trip(1, 2, true);
    round_trip(2, 4, false);
    round_trip(6, -8, false);

    // Lying encodings: preserved on the wire, ignored by the arithmetic.
    const Fraction claims_reduced = round_trip(2, 4, true);
    BOOST_CHECK(claims_reduced == Fraction(1, 2));
    BOOST_CHECK(claims_reduced + Fraction(0) == Fraction(1, 2));
    BOOST_CHECK(claims_reduced * 2 == Fraction(1));
    BOOST_CHECK(Fraction(claims_reduced, true).GetDenominator() == 2);

    const Fraction claims_unreduced = round_trip(1, 2, false);
    BOOST_CHECK(claims_unreduced == Fraction(2, 4));
    BOOST_CHECK(claims_unreduced - Fraction(1, 2) == Fraction(0));

    // Rejected on read, as before.
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << int64_t {1} << int64_t {0} << true;

        Fraction f;
        BOOST_CHECK_THROW(ss >> f, std::ios_base::failure);
    }
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << std::numeric_limits<int64_t>::min() << int64_t {1} << true;

        Fraction f;
        BOOST_CHECK_THROW(ss >> f, std::ios_base::failure);
    }
}

BOOST_AUTO_TEST_CASE(from_string_edges)
{
    const std::string parse_error = "fraction input string cannot be parsed to fraction";

    const auto parse_fails = [&](const std::string& s) {
        try {
            Fraction::FromString(s);
        } catch (const std::out_of_range& e) {
            return std::string(e.what()) == parse_error;
        }

        return false;
    };

    BOOST_CHECK(parse_fails(""));
    BOOST_CHECK(parse_fails("/"));
    BOOST_CHECK(parse_fails("1/"));
    BOOST_CHECK(parse_fails("/2"));
    BOOST_CHECK(parse_fails("1/2/3"));
    BOOST_CHECK(parse_fails(" 1/2"));
    BOOST_CHECK(parse_fails("1/ 2"));
    BOOST_CHECK(parse_fails("0.25"));
    BOOST_CHECK(parse_fails("1e3"));
    BOOST_CHECK(parse_fails("9223372036854775808"));

    BOOST_CHECK_EXCEPTION(Fraction::FromString("1/0"),
                          std::out_of_range,
                          [](const std::out_of_range& e) {
                              return std::string(e.what()) == "denominator specified is zero";
                          });
    BOOST_CHECK_THROW(Fraction::FromString("-9223372036854775808"), std::out_of_range);
    BOOST_CHECK_THROW(Fraction::FromString("1/-9223372036854775808"), std::out_of_range);

    // Parsed values come back in canonical form.
    const Fraction negative_denominator = Fraction::FromString("3/-6");
    BOOST_CHECK_EQUAL(negative_denominator.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(negative_denominator.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(negative_denominator.IsSimplified(), true);

    const Fraction in_force = Fraction::FromString("103050/100000");
    BOOST_CHECK_EQUAL(in_force.GetNumerator(), 2061);
    BOOST_CHECK_EQUAL(in_force.GetDenominator(), 2000);
}

BOOST_AUTO_TEST_CASE(magnitude_unit_times_the_max_reward_constants)
{
    // SnapshotAccrualComputer::MaxReward() computes
    //
    //   (magnitude_unit * 32768 * 2 * COIN).ToCAmount()
    //
    // with the unit read from the "magnitudeunit" protocol entry. These pin
    // that chain for the default unit, for the entry in force when this was
    // written, for the clamp, and at the edge of what a decimal entry can
    // carry. See the overflow analysis at that function.
    const auto max_reward = [](const GRC::Allocation& magnitude_unit) {
        return (magnitude_unit * 32768 * 2 * COIN).ToCAmount();
    };

    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(1, 4)), CAmount {1638400000000});                          // 16384 GRC
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(Fraction::FromString("103050/100000"))), CAmount {6753484800000}); // 67534.848 GRC
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(5, 1)), CAmount {32768000000000});                         // 327680 GRC

    // Eight decimal places just under the clamp. Before the integer operand was
    // cancelled first this threw: the chain had formed 499999999 * 2^8 by the
    // last step and then multiplied it by COIN with nothing left to cancel.
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(499999999, 100000000)), CAmount {32767999934464});

    // Fifteen places still fit: the reduced result is p * 2^9 / 5^7.
    const GRC::Allocation fifteen_places(4999999999999999LL, 1000000000000000LL);
    BOOST_CHECK_NO_THROW(max_reward(fifteen_places));
    BOOST_CHECK(fifteen_places * 32768 * 2 * COIN == Fraction(4999999999999999LL * 512, 78125));

    // Sixteen places do not: the reduced numerator p * 2^8 is above INT64_MAX,
    // so the result itself has no int64_t representation.
    BOOST_CHECK_THROW(max_reward(GRC::Allocation(49999999999999999LL, 10000000000000000LL)), std::overflow_error);
}

BOOST_AUTO_TEST_CASE(magnitude_entry_bound_carries_the_consumer_arithmetic)
{
    // From block version 15 a "magnitudeunit" or "magnitudeweightfactor"
    // entry has a reduced numerator and denominator of at most
    // GRC::MAX_MAGNITUDE_ENTRY_TERM (10^6). These run the Fraction expressions
    // the entries feed at that bound: the unit's clamp (SnapshotCalculator::
    // GetMagnitudeUnit), MaxReward's chain, and the weight factor's clamp
    // (Poll::ResolveMagnitudeWeightFactor). The two integer products are
    // checked by static_asserts at the consumers. See the analysis at
    // MAX_MAGNITUDE_ENTRY_TERM.
    const Consensus::Params& consensus = Params().GetConsensus();

    const auto clamp_unit = [&](const Fraction& unit) {
        return unit > consensus.MaxMagnitudeUnit ? consensus.MaxMagnitudeUnit : unit;
    };
    const auto clamp_weight = [&](const Fraction& factor) {
        return std::clamp<Fraction>(factor, consensus.MinMagnitudeWeightFactor, consensus.MaxMagnitudeWeightFactor);
    };
    const auto max_reward = [](const GRC::Allocation& magnitude_unit) {
        return (magnitude_unit * 32768 * 2 * COIN).ToCAmount();
    };

    // The extremes. 999983 is prime, so 10^6 over it cancels nothing.
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(clamp_unit(Fraction(1000000, 999983, true)))), CAmount {6553711413094});
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(clamp_unit(Fraction(1, 1000000, true)))), CAmount {6553600});
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(clamp_unit(Fraction(999999, 1000000, true)))), CAmount {6553593446400});
    BOOST_CHECK_EQUAL(max_reward(GRC::Allocation(clamp_unit(Fraction(1000000, 1, true)))), CAmount {32768000000000});

    // The edge the bound keeps clear of: with nothing to cancel, MaxReward's
    // last step multiplies p * 2^16 by COIN (27 bits), so the guard admits p
    // below 2^20 = 1,048,576 and refuses p at it.
    BOOST_CHECK_NO_THROW(max_reward(GRC::Allocation(Fraction(1048575, 999983, true))));
    BOOST_CHECK_THROW(max_reward(GRC::Allocation(Fraction(1048576, 999983, true))), std::overflow_error);

    // A sweep over the accepted range. Each pair is reduced first, as the
    // parser does; none may throw in any of the three expressions.
    int64_t checked = 0;
    std::string first_failure;

    const auto check = [&](const int64_t num, const int64_t den) {
        ++checked;
        try {
            const Fraction entry(num, den, true);
            (void)max_reward(GRC::Allocation(clamp_unit(entry)));
            (void)clamp_weight(entry);
        } catch (const std::exception& e) {
            if (first_failure.empty()) first_failure = strprintf("%d/%d: %s", num, den, e.what());
        }
    };

    for (const int64_t den : {1, 2, 3, 7, 997, 99991, 390625, 524288, 999983, 999997, 999999, 1000000}) {
        for (int64_t num = 1; num <= GRC::MAX_MAGNITUDE_ENTRY_TERM; num += 997) check(num, den);
        check(GRC::MAX_MAGNITUDE_ENTRY_TERM, den);
    }

    for (int64_t den = 1; den <= GRC::MAX_MAGNITUDE_ENTRY_TERM; den += 997) {
        check(999999, den);
        check(GRC::MAX_MAGNITUDE_ENTRY_TERM, den);
    }

    BOOST_CHECK_MESSAGE(first_failure.empty(), first_failure);
    BOOST_CHECK_GT(checked, 14000);
}

BOOST_AUTO_TEST_CASE(allocation_to_camount_truncates_toward_zero)
{
    // Golden vectors. A change here changes reward amounts on chain.
    BOOST_CHECK_EQUAL((GRC::Allocation(1, 3) * 100).ToCAmount(), CAmount {33});
    BOOST_CHECK_EQUAL((GRC::Allocation(2, 3) * 100).ToCAmount(), CAmount {66});
    BOOST_CHECK_EQUAL((GRC::Allocation(-1, 3) * 100).ToCAmount(), CAmount {-33});
    BOOST_CHECK_EQUAL((GRC::Allocation(0.10) * 1234567).ToCAmount(), CAmount {123456});
    BOOST_CHECK_EQUAL((GRC::Allocation(0.25) * 1000000001).ToCAmount(), CAmount {250000000});
    BOOST_CHECK_CLOSE(GRC::Allocation(0.35).ToPercent(), 35.0, 1e-9);

    // The double constructor holds basis points: 0.10 is 1000/10000, reduced.
    BOOST_CHECK_EQUAL(GRC::Allocation(0.10).GetNumerator(), 1);
    BOOST_CHECK_EQUAL(GRC::Allocation(0.10).GetDenominator(), 10);

    // The truncated remainders of a split stay with the complement.
    const CAmount reward = 1000000001;
    const CAmount ten = (GRC::Allocation(0.10) * reward).ToCAmount();
    const CAmount fifteen = (GRC::Allocation(0.15) * reward).ToCAmount();

    BOOST_CHECK_EQUAL(ten + fifteen, CAmount {250000000});
    BOOST_CHECK_EQUAL(reward - ten - fifteen, CAmount {750000001});

    // An allocation built from a fraction keeps the pair it was given.
    const GRC::Allocation from_fraction(Fraction(2, 4));
    BOOST_CHECK_EQUAL(from_fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(from_fraction.GetDenominator(), 4);
}

BOOST_AUTO_TEST_SUITE_END()
