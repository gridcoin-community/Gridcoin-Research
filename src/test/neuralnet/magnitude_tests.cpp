#include "neuralnet/magnitude.h"
#include "streams.h"

#include <iostream>
#include <boost/test/unit_test.hpp>

// -----------------------------------------------------------------------------
// Magnitude
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Magnitude)

BOOST_AUTO_TEST_CASE(it_initializes_to_zero_magnitude)
{
    const NN::Magnitude mag = NN::Magnitude::Zero();

    BOOST_CHECK_EQUAL(mag.Scaled(), 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_zero_when_rounding_invalid_magnitudes)
{
    // Negative
    const NN::Magnitude negative = NN::Magnitude::RoundFrom(-1.234);
    BOOST_CHECK_EQUAL(negative.Scaled(), 0);

    // Rounds-down to zero
    const NN::Magnitude effectivly_zero = NN::Magnitude::RoundFrom(0.005);
    BOOST_CHECK_EQUAL(effectivly_zero.Scaled(), 0);

    // Exceeded maximum
    const NN::Magnitude overflow = NN::Magnitude::RoundFrom(32767.123);
    BOOST_CHECK_EQUAL(overflow.Scaled(), 0);
}

BOOST_AUTO_TEST_CASE(it_rounds_a_small_magnitude_to_two_places)
{
    const NN::Magnitude min = NN::Magnitude::RoundFrom(0.0051);
    BOOST_CHECK_EQUAL(min.Scaled(), 1);

    const NN::Magnitude max = NN::Magnitude::RoundFrom(0.994);
    BOOST_CHECK_EQUAL(max.Scaled(), 99);
}

BOOST_AUTO_TEST_CASE(it_rounds_a_medium_magnitude_to_one_place)
{
    const NN::Magnitude min = NN::Magnitude::RoundFrom(0.995);
    BOOST_CHECK_EQUAL(min.Scaled(), 100);

    const NN::Magnitude max = NN::Magnitude::RoundFrom(9.94);
    BOOST_CHECK_EQUAL(max.Scaled(), 990);
}

BOOST_AUTO_TEST_CASE(it_rounds_a_large_magnitude_to_a_whole)
{
    const NN::Magnitude min = NN::Magnitude::RoundFrom(9.95);
    BOOST_CHECK_EQUAL(min.Scaled(), 1000);

    const NN::Magnitude max = NN::Magnitude::RoundFrom(32767.0);
    BOOST_CHECK_EQUAL(max.Scaled(), 3276700);
}

BOOST_AUTO_TEST_CASE(it_compares_another_magnitude_for_equality)
{
    const NN::Magnitude magnitude1 = NN::Magnitude::RoundFrom(1.23);
    const NN::Magnitude magnitude2 = NN::Magnitude::RoundFrom(1.23);

    BOOST_CHECK(magnitude1 == magnitude2);
    BOOST_CHECK(magnitude1 != NN::Magnitude::Zero());
}

BOOST_AUTO_TEST_CASE(it_compares_an_integer_for_equality)
{
    const NN::Magnitude magnitude = NN::Magnitude::RoundFrom(123);

    BOOST_CHECK(magnitude == 123);
    BOOST_CHECK(magnitude != 999);
}

BOOST_AUTO_TEST_CASE(it_compares_a_floating_point_number_for_equality)
{
    const NN::Magnitude magnitude = NN::Magnitude::RoundFrom(1.23);

    // Floating-point comparisons round the other value according to the size
    // for the precision tiers before checking equality:
    BOOST_CHECK(magnitude == 1.23);
    BOOST_CHECK(magnitude == 1.2);
    BOOST_CHECK(magnitude == 1.25);
    BOOST_CHECK(magnitude != 1.26);
}

BOOST_AUTO_TEST_CASE(it_reports_its_size_category)
{
    using Kind = NN::Magnitude::Kind;

    const NN::Magnitude zero = NN::Magnitude::Zero();
    BOOST_CHECK(zero.Which() == Kind::ZERO);

    const NN::Magnitude effectively_zero = NN::Magnitude::RoundFrom(0.005);
    BOOST_CHECK(effectively_zero.Which() == Kind::ZERO);

    const NN::Magnitude min_small = NN::Magnitude::RoundFrom(0.0051);
    BOOST_CHECK(min_small.Which() == Kind::SMALL);

    const NN::Magnitude max_small = NN::Magnitude::RoundFrom(0.994);
    BOOST_CHECK(max_small.Which() == Kind::SMALL);

    const NN::Magnitude min_medium = NN::Magnitude::RoundFrom(0.995);
    BOOST_CHECK(min_medium.Which() == Kind::MEDIUM);

    const NN::Magnitude max_medium = NN::Magnitude::RoundFrom(9.94);
    BOOST_CHECK(max_medium.Which() == Kind::MEDIUM);

    const NN::Magnitude min_large = NN::Magnitude::RoundFrom(9.95);
    BOOST_CHECK(min_large.Which() == Kind::LARGE);

    const NN::Magnitude max_large = NN::Magnitude::RoundFrom(32767.0);
    BOOST_CHECK(max_large.Which() == Kind::LARGE);
}

BOOST_AUTO_TEST_CASE(it_presents_the_scaled_magnitude_representation)
{
    const NN::Magnitude small = NN::Magnitude::RoundFrom(0.11);
    BOOST_CHECK_EQUAL(small.Scaled(), 11);

    const NN::Magnitude medium = NN::Magnitude::RoundFrom(1.1);
    BOOST_CHECK_EQUAL(medium.Scaled(), 110);

    const NN::Magnitude large = NN::Magnitude::RoundFrom(11.0);
    BOOST_CHECK_EQUAL(large.Scaled(), 1100);
}

BOOST_AUTO_TEST_CASE(it_presents_the_compact_magnitude_representation)
{
    const NN::Magnitude small = NN::Magnitude::RoundFrom(0.11);
    BOOST_CHECK_EQUAL(small.Compact(), 11);

    const NN::Magnitude medium = NN::Magnitude::RoundFrom(1.1);
    BOOST_CHECK_EQUAL(medium.Compact(), 11);

    const NN::Magnitude large = NN::Magnitude::RoundFrom(11.0);
    BOOST_CHECK_EQUAL(large.Compact(), 11);
}

BOOST_AUTO_TEST_CASE(it_presents_the_floatng_point_magnitude_representation)
{
    const NN::Magnitude small = NN::Magnitude::RoundFrom(0.11);
    BOOST_CHECK_EQUAL(small.Floating(), 0.11);

    const NN::Magnitude medium = NN::Magnitude::RoundFrom(1.1);
    BOOST_CHECK_EQUAL(medium.Floating(), 1.1);

    const NN::Magnitude large = NN::Magnitude::RoundFrom(11.0);
    BOOST_CHECK_EQUAL(large.Floating(), 11.0);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    const NN::Magnitude zero = NN::Magnitude::Zero();
    BOOST_CHECK_EQUAL(zero.ToString(), "0");

    const NN::Magnitude small = NN::Magnitude::RoundFrom(0.11);
    BOOST_CHECK_EQUAL(small.ToString(), "0.11");

    const NN::Magnitude medium = NN::Magnitude::RoundFrom(1.1);
    BOOST_CHECK_EQUAL(medium.ToString(), "1.1");

    const NN::Magnitude large = NN::Magnitude::RoundFrom(11.0);
    BOOST_CHECK_EQUAL(large.ToString(), "11");

    const NN::Magnitude max = NN::Magnitude::RoundFrom(32767.0);
    BOOST_CHECK_EQUAL(max.ToString(), "32767");
}

BOOST_AUTO_TEST_SUITE_END()
