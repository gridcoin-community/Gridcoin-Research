// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/cpid.h"
#include "streams.h"

#include <boost/test/unit_test.hpp>
#include <iostream>
#include <vector>

// -----------------------------------------------------------------------------
// Cpid
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Cpid)

BOOST_AUTO_TEST_CASE(it_initializes_to_a_zero_cpid)
{
    GRC::Cpid cpid;

    std::array<unsigned char, 16> zeros { };

    BOOST_CHECK(cpid.Raw() == zeros);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_supplied_bytes)
{
    std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    GRC::Cpid cpid(expected);

    std::vector<unsigned char> bytes(cpid.Raw().begin(), cpid.Raw().end());

    BOOST_CHECK(bytes == expected);
}

BOOST_AUTO_TEST_CASE(it_parses_a_cpid_from_its_string_representation)
{
    GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    BOOST_CHECK(cpid.Raw() == (std::array<unsigned char, 16> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    }));
}

BOOST_AUTO_TEST_CASE(it_parses_an_invalid_cpid_string_to_zeros)
{
    // Empty:
    GRC::Cpid cpid = GRC::Cpid::Parse("");
    BOOST_CHECK(cpid.IsZero() == true);

    // Too short: 31 characters
    cpid = GRC::Cpid::Parse("0001020304050607080910111213141");
    BOOST_CHECK(cpid.IsZero() == true);

    // Too long: 33 characters
    cpid = GRC::Cpid::Parse("000102030405060708091011121314155");
    BOOST_CHECK(cpid.IsZero() == true);

    // Non-hex character at the end:
    cpid = GRC::Cpid::Parse("0001020304050607080910111213141Z");
    BOOST_CHECK(cpid.IsZero() == true);
}

BOOST_AUTO_TEST_CASE(it_hashes_an_internal_cpid_and_email_to_make_a_public_cpid)
{
    std::vector<unsigned char> expected {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    };

    GRC::Cpid cpid = GRC::Cpid::Hash(
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", // internal CPID
        "test@example.com");

    std::vector<unsigned char> bytes(cpid.Raw().begin(), cpid.Raw().end());

    BOOST_CHECK(bytes == expected);
}

BOOST_AUTO_TEST_CASE(it_refuses_to_hash_an_invalid_internal_cpid_or_email)
{
    GRC::Cpid cpid = GRC::Cpid::Hash("", "test@example.com");

    BOOST_CHECK(cpid.IsZero() == true);

    cpid = GRC::Cpid::Hash("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "");

    BOOST_CHECK(cpid.IsZero() == true);

    cpid = GRC::Cpid::Hash("", "");

    BOOST_CHECK(cpid.IsZero() == true);
}

BOOST_AUTO_TEST_CASE(it_compares_another_cpid_for_equality)
{
    GRC::Cpid cpid1(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    });

    GRC::Cpid cpid2(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    });

    BOOST_CHECK(cpid1 == cpid2);
    BOOST_CHECK(cpid1 != GRC::Cpid());
}

BOOST_AUTO_TEST_CASE(it_compares_another_cpid_by_relative_greatness)
{
    GRC::Cpid cpid1(std::vector<unsigned char> {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    });

    GRC::Cpid cpid2(std::vector<unsigned char> {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    });

    BOOST_CHECK(cpid1 < cpid2);
    BOOST_CHECK(cpid2 > cpid1);

    GRC::Cpid cpid3(std::vector<unsigned char> {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    });

    BOOST_CHECK(cpid1 < cpid3);
    BOOST_CHECK(cpid3 > cpid1);
    BOOST_CHECK(GRC::Cpid() < cpid3);
    BOOST_CHECK(cpid3 > GRC::Cpid());

    GRC::Cpid cpid4(std::vector<unsigned char> {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    });

    GRC::Cpid cpid5(std::vector<unsigned char> {
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    });

    BOOST_CHECK(cpid4 < cpid5);
    BOOST_CHECK(cpid5 > cpid4);

    GRC::Cpid cpid6(std::vector<unsigned char> {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
    });

    BOOST_CHECK(cpid6 < cpid5);
    BOOST_CHECK(cpid5 > cpid6);

    BOOST_CHECK((cpid1 < cpid1) == false);
    BOOST_CHECK((cpid1 > cpid1) == false);
    BOOST_CHECK((GRC::Cpid() < GRC::Cpid()) == false);
    BOOST_CHECK((GRC::Cpid() > GRC::Cpid()) == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_cpid_is_all_zeros)
{
    GRC::Cpid cpid;

    BOOST_CHECK(cpid.IsZero() == true);

    cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    BOOST_CHECK(cpid.IsZero() == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_cpid_matches_an_internal_cpid)
{
    GRC::Cpid cpid(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    });

    std::string internal_cpid = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    std::string email = "test@example.com";

    BOOST_CHECK(cpid.Matches(internal_cpid, email) == true);
    BOOST_CHECK(cpid.Matches(internal_cpid, "invalid") == false);
    BOOST_CHECK(cpid.Matches("invalid", email) == false);

    // An invalid CPID should not match:
    BOOST_CHECK(GRC::Cpid().Matches(internal_cpid, email) == false);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    GRC::Cpid cpid(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    BOOST_CHECK(cpid.ToString() == "00010203040506070809101112131415");
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    GRC::Cpid cpid(expected);

    BOOST_CHECK(GetSerializeSize(cpid, SER_NETWORK, 1) == 16);

    CDataStream stream(SER_NETWORK, 1);
    stream << cpid;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    GRC::Cpid cpid;

    std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    CDataStream stream(expected, SER_NETWORK, 1);
    stream >> cpid;
    std::vector<unsigned char> bytes(cpid.Raw().begin(), cpid.Raw().end());

    BOOST_CHECK(bytes == expected);
}

BOOST_AUTO_TEST_CASE(it_is_hashable_to_key_a_lookup_map)
{
    GRC::Cpid cpid(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    std::hash<GRC::Cpid> hasher;

    // CPID halves, little endian
    const size_t expected = 0x0706050403020100ull + 0x1514131211100908ull;

    BOOST_CHECK_EQUAL(hasher(cpid), expected);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// MiningId
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(MiningId)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_invalid_mining_id)
{
    GRC::MiningId mining_id;

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_an_investor)
{
    GRC::MiningId mining_id = GRC::MiningId::ForInvestor();

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVESTOR);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_provided_cpid)
{
    GRC::Cpid expected(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    GRC::MiningId mining_id(expected);

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::CPID);

    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        BOOST_CHECK(*cpid == expected);
    } else {
        BOOST_FAIL("MiningId variant does not contain the CPID.");
    }
}

BOOST_AUTO_TEST_CASE(it_parses_an_investor_mining_id)
{
    GRC::MiningId mining_id = GRC::MiningId::Parse("INVESTOR");

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVESTOR);

    mining_id = GRC::MiningId::Parse("investor");

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVESTOR);
}

BOOST_AUTO_TEST_CASE(it_parses_a_cpid_mining_id)
{
    std::string hex = "00010203040506070809101112131415";

    GRC::MiningId mining_id = GRC::MiningId::Parse(hex);

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::CPID);

    GRC::Cpid expected = GRC::Cpid::Parse(hex);

    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        BOOST_CHECK(*cpid == expected);

        BOOST_CHECK((*cpid).Raw() == (std::array<unsigned char, 16> {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        }));
    } else {
        BOOST_FAIL("MiningId variant does not contain the CPID.");
    }
}

BOOST_AUTO_TEST_CASE(it_refuses_to_parse_an_invalid_cpid)
{
    // Empty:
    GRC::MiningId mining_id = GRC::MiningId::Parse("");
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);

    // Bad investor:
    mining_id = GRC::MiningId::Parse("INVESTOR ");
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);

    // Too short: 31 characters
    mining_id = GRC::MiningId::Parse("0001020304050607080910111213141");
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);

    // Too long: 33 characters
    mining_id = GRC::MiningId::Parse("000102030405060708091011121314155");
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);

    // Non-hex character at the end:
    mining_id = GRC::MiningId::Parse("0001020304050607080910111213141Z");
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);
}

BOOST_AUTO_TEST_CASE(it_compares_another_mining_id_for_equality)
{
    GRC::MiningId mining_id1(GRC::Cpid(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    }));

    GRC::MiningId mining_id2(GRC::Cpid(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    }));

    BOOST_CHECK(GRC::MiningId() == GRC::MiningId());
    BOOST_CHECK(GRC::MiningId::ForInvestor() == GRC::MiningId::ForInvestor());

    BOOST_CHECK(mining_id1 == mining_id2);
    BOOST_CHECK(mining_id1 != GRC::MiningId());
    BOOST_CHECK(mining_id1 != GRC::MiningId::ForInvestor());
    BOOST_CHECK(mining_id1 != GRC::MiningId(GRC::Cpid()));
}

BOOST_AUTO_TEST_CASE(it_compares_another_cpid_for_equality)
{
    GRC::MiningId mining_id(GRC::Cpid(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    }));

    GRC::Cpid cpid(std::vector<unsigned char> {
        0x56, 0xd4, 0x2d, 0x93, 0xc5, 0x0c, 0xfa, 0xd6,
        0x5e, 0x13, 0x0d, 0xea, 0x34, 0x2a, 0xa0, 0xf9,
    });

    BOOST_CHECK(mining_id == cpid);
    BOOST_CHECK(mining_id != GRC::Cpid());
}

BOOST_AUTO_TEST_CASE(it_determines_which_mining_id_variant_it_exhibits)
{
    GRC::MiningId mining_id;
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);

    mining_id = GRC::MiningId::ForInvestor();
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVESTOR);

    mining_id = GRC::MiningId(GRC::Cpid());
    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::CPID);
}

BOOST_AUTO_TEST_CASE(it_provides_guarded_access_to_stored_cpid_values)
{
    GRC::Cpid expected(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    GRC::MiningId mining_id(expected);

    // To prevent mistakes, client code must check that a MiningId variant
    // actually contains a CPID before using it:
    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        BOOST_CHECK(*cpid == expected);
    } else {
        BOOST_FAIL("MiningId variant does not contain the CPID.");
    }

    mining_id = GRC::MiningId();
    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        BOOST_FAIL("MiningId variant should not contain the CPID.");
    }

    mining_id = GRC::MiningId::ForInvestor();
    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        BOOST_FAIL("MiningId variant should not contain the CPID.");
    }
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    BOOST_CHECK(GRC::MiningId().ToString().empty() == true);
    BOOST_CHECK(GRC::MiningId::ForInvestor().ToString() == "INVESTOR");

    GRC::MiningId mining_id(GRC::Cpid(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    }));

    BOOST_CHECK(mining_id.ToString() == "00010203040506070809101112131415");
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_invalid)
{
    GRC::MiningId mining_id;

    BOOST_CHECK(GetSerializeSize(mining_id, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);
    stream << mining_id;

    BOOST_CHECK(stream.size() == 1);
    BOOST_CHECK(stream[0] == 0x00); // MiningId::Kind::INVALID
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_investor)
{
    GRC::MiningId mining_id = GRC::MiningId::ForInvestor();

    BOOST_CHECK(GetSerializeSize(mining_id, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);
    stream << mining_id;

    BOOST_CHECK(stream.size() == 1);
    BOOST_CHECK(stream[0] == 0x01); // MiningId::Kind::INVESTOR
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_cpid)
{
    std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    GRC::MiningId mining_id{GRC::Cpid(expected)};

    BOOST_CHECK(GetSerializeSize(mining_id, SER_NETWORK, 1) == 17);

    CDataStream stream(SER_NETWORK, 1);
    stream << mining_id;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output[0] == 0x02); // MiningId::Kind::CPID

    BOOST_CHECK_EQUAL_COLLECTIONS(
        ++output.begin(), // we already checked the first byte
        output.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_invalid)
{
    // Initialize mining_id with a valid value to test invalid:
    GRC::MiningId mining_id = GRC::MiningId::ForInvestor();

    CDataStream stream(SER_NETWORK, 1);
    stream << (unsigned char)0x00; // MiningId::Kind::INVALID
    stream >> mining_id;

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVALID);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_investor)
{
    GRC::MiningId mining_id;

    CDataStream stream(SER_NETWORK, 1);
    stream << (unsigned char)0x01; // MiningId::Kind::INVESTOR
    stream >> mining_id;

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::INVESTOR);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_cpid)
{
    GRC::MiningId mining_id;

    GRC::Cpid expected(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    CDataStream stream(SER_NETWORK, 1);
    stream << (unsigned char)0x02; // MiningId::Kind::CPID
    stream << expected;
    stream >> mining_id;

    BOOST_CHECK(mining_id.Which() == GRC::MiningId::Kind::CPID);

    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        BOOST_CHECK(*cpid == expected);
    } else {
        BOOST_FAIL("MiningId variant does not contain the CPID.");
    }
}

BOOST_AUTO_TEST_SUITE_END()
