// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/fraction.h>

#include <amount.h>
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
int msb(const int64_t& n)
{
    // Can't take the log of 0.
    if (n == 0) {
        return 0;
    }

    // Log2 is O(1) both time and space-wise.
    return (static_cast<int>(floor(log2(std::abs(n)))) + 1);
}

int msb2(const int64_t& n_in)
{
    int64_t n = std::abs(n_in);

    int index = 0;

    if (n == 0) {
        return 0;
    }

    for (int i = 0; i <= 63; ++i) {
        if (n % 2 == 1) {
            index = i;
        }

        n /= 2;
    }

    return index + 1;
}

// This is the one currently used in the Fraction class
int msb3(const int64_t& n_in)
{
    int64_t n = std::abs(n_in);

    int index = 0;

    for (; index <= 63; ++index) {
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
    for (unsigned int i = 0; i <= 63; ++i) {
        int64_t n = 0;

        if (i > 0) {
            n = (int64_t {1} << (i - 1));
        }

        BOOST_CHECK_EQUAL(msb(n), i);
    }

    int bias_for_msb_result_63 = 0;

    // msb ugly, ugly, ugly. Log2 looses resolution near the top of the range...
    for (int i = 0; i < 16; ++i) {
        bias_for_msb_result_63 = (int64_t {1} << i);

        int msb_result = msb(std::numeric_limits<int64_t>::max() - bias_for_msb_result_63);

        if (msb_result == 63) {
            LogPrintf("INFO: %s: bias_for_msb_result_63 = %i, msb_result = %i", __func__, bias_for_msb_result_63, msb_result);
            break;
        } else {
        }
    }

    // bias_for_msb_result_63 is currently 32768! It should be zero! This disqualifies the log2 based approach based on
    // a correctness check.
    BOOST_CHECK_EQUAL(msb(std::numeric_limits<int64_t>::max() - bias_for_msb_result_63), 63);

    BOOST_CHECK_EQUAL(msb2(std::numeric_limits<int64_t>::max()), 63);
    BOOST_CHECK_EQUAL(msb3(std::numeric_limits<int64_t>::max()), 63);

    std::vector<std::pair<int64_t, int>> msb_results, msb2_results, msb3_results;

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
            error("%s: iteration %u: mismatch: %" PRId64 ", msb = %i, %" PRId64 " msb2 = %i, %" PRId64 " msb3 = %i",
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

BOOST_AUTO_TEST_CASE(Fraction_msb_performance_test)
{
    // This is a test to bracket the three different algorithms above in anonymous namespace for doing msb calcs. The first is O(1),
    // the second and third are O(log n), but the O(1) straight from the C++ library is pretty heavyweight and highly dependent on CPU
    // architecture.

    FastRandomContext rand(uint256 {0});

    unsigned int iterations = 10000000;

    g_timer.InitTimer("msb_test", true);

    for (unsigned int i = 0; i < iterations; ++i) {
        msb((int64_t)rand.rand64());
    }

    int64_t msb_test_time = g_timer.GetTimes(strprintf("msb %u iterations", iterations), "msb_test").time_since_last_check;

    FastRandomContext rand2(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        msb2((int64_t)rand2.rand64());
    }

    int64_t msb2_test_time = g_timer.GetTimes(strprintf("msb2 %u iterations", iterations), "msb_test").time_since_last_check;

    FastRandomContext rand3(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        msb3((int64_t)rand3.rand64());
    }

    int64_t msb3_test_time = g_timer.GetTimes(strprintf("msb3 %u iterations", iterations), "msb_test").time_since_last_check;

    // The execution time of the above on a 13900K is

    // INFO: GetTimes: timer msb_test: msb 10000000 iterations: elapsed time: 86 ms, time since last check: 86 ms.
    // INFO: GetTimes: timer msb_test: msb2 10000000 iterations: elapsed time: 166 ms, time since last check: 80 ms.
    // INFO: GetTimes: timer msb_test: msb3 10000000 iterations: elapsed time: 246 ms, time since last check: 80 ms.

    // Which is almost identical. msb appears to be much slower on 32 bit architectures.

    // One can easily have T1 = k1 * O(1) and T2 = k2 * O(n) = k2 * n * O(1) where T1 > T2 for n < q if k1 > q * k2, so the O(1)
    // algorithm is by no means the best choice.

    // This test makes sure that the three algorithms are within 20x of the one with the minimum execution time. If not, it will
    // fail to prompt us to look at this again.

    double minimum_time = std::min<double>(std::min<double>(msb_test_time, msb2_test_time), msb3_test_time);

    BOOST_CHECK((double) msb_test_time / minimum_time < 20.0);
    BOOST_CHECK((double) msb2_test_time / minimum_time < 20.0);
    BOOST_CHECK((double) msb3_test_time / minimum_time < 20.0);
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
    Fraction quotient = lhs / rhs;
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
        Fraction quotient = lhs / rhs;
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string{"denominator specified is zero"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_1)
{
    Fraction lhs((int64_t) 1 << 30, 1);
    Fraction rhs((int64_t) 1 << 31, 1);

    LogPrintf("INFO: %s: msb((int64_t) 1 << 30) = %i", __func__, msb3((int64_t) 1 << 30));
    LogPrintf("INFO: %s: msb((int64_t) 1 << 31) = %i", __func__, msb3((int64_t) 1 << 31));

    std::string err;

    try {
        Fraction product = lhs * rhs;
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
        Fraction product = lhs * rhs;
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
        Fraction product = lhs * rhs;
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
        Fraction addition = lhs + rhs;
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
        Fraction addition = lhs + rhs;
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
        Fraction addition = lhs + rhs;
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
        Fraction addition = lhs + rhs;
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
        Fraction fraction = Fraction::FromString("100/567/300");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);

    try {
        Fraction fraction = Fraction::FromString("100 / 567");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);

    try {
        Fraction fraction = Fraction::FromString("100.1");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);

    try {
        Fraction fraction = Fraction::FromString("");
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, valid_err_message);
}

BOOST_AUTO_TEST_SUITE_END()
