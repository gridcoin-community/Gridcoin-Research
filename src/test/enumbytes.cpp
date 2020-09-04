#include "enumbytes.h"
#include "streams.h"

#include <boost/test/unit_test.hpp>

namespace {
//!
//! \brief An enumeration to use for testing.
//!
enum class TestEnum {
    A,
    B,
    OUT_OF_BOUND,
};
} // Anonymous namespace


// -----------------------------------------------------------------------------
// EnumByte
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(EnumByte_)

BOOST_AUTO_TEST_CASE(it_initalizes_to_zero)
{
    const EnumByte<TestEnum> a;

    BOOST_CHECK(a.Value() == TestEnum::A);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_an_enum_value)
{
    const EnumByte<TestEnum> b(TestEnum::B);

    BOOST_CHECK(b.Value() == TestEnum::B);
}

BOOST_AUTO_TEST_CASE(it_compares_another_enum_wrapper)
{
    const EnumByte<TestEnum> a1;
    const EnumByte<TestEnum> a2;
    const EnumByte<TestEnum> b(TestEnum::B);

    BOOST_CHECK(a1 == a2);
    BOOST_CHECK(a1 != b);

    BOOST_CHECK(a1 < b);
    BOOST_CHECK(!(b < a1));

    BOOST_CHECK(b > a1);
    BOOST_CHECK(!(a1 > b));

    BOOST_CHECK(a1 <= a2);
    BOOST_CHECK(a1 <= b);
    BOOST_CHECK(!(b <= a1));

    BOOST_CHECK(a1 >= a2);
    BOOST_CHECK(b >= a1);
    BOOST_CHECK(!(a1 >= b));
}

BOOST_AUTO_TEST_CASE(it_compares_an_enum_of_the_wrapped_type)
{
    const EnumByte<TestEnum> a;
    const EnumByte<TestEnum> b(TestEnum::B);

    BOOST_CHECK(a == TestEnum::A);
    BOOST_CHECK(a != TestEnum::B);

    BOOST_CHECK(a < TestEnum::B);
    BOOST_CHECK(!(b < TestEnum::A));

    BOOST_CHECK(b > TestEnum::A);
    BOOST_CHECK(!(a > TestEnum::B));

    BOOST_CHECK(a <= TestEnum::A);
    BOOST_CHECK(a <= TestEnum::B);
    BOOST_CHECK(!(b <= TestEnum::A));

    BOOST_CHECK(a >= TestEnum::A);
    BOOST_CHECK(b >= TestEnum::A);
    BOOST_CHECK(!(a >= TestEnum::B));
}

BOOST_AUTO_TEST_CASE(it_produces_an_unsigned_integer_for_the_enum_value)
{
    const EnumByte<TestEnum> b(TestEnum::B);

    BOOST_CHECK(b.Raw() == static_cast<uint8_t>(TestEnum::B));
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const EnumByte<TestEnum> b(TestEnum::B);

    const CDataStream expected = CDataStream(SER_NETWORK, 1)
        << static_cast<uint8_t>(TestEnum::B);

    const CDataStream stream = CDataStream(SER_NETWORK, 1)
        << b;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    CDataStream stream = CDataStream(SER_NETWORK, 1)
        << static_cast<uint8_t>(TestEnum::B);

    EnumByte<TestEnum> b;
    stream >> b;

    BOOST_CHECK(b == TestEnum::B);
}

BOOST_AUTO_TEST_CASE(it_refuses_to_deserialize_out_of_range_values)
{
    CDataStream stream = CDataStream(SER_NETWORK, 1)
        << static_cast<uint8_t>(TestEnum::OUT_OF_BOUND);

    EnumByte<TestEnum> out;

    BOOST_CHECK_THROW(stream >> out, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()
