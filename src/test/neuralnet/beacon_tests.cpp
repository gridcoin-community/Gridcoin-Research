#include "base58.h"
#include "neuralnet/beacon.h"

#include <boost/test/unit_test.hpp>
#include <vector>

namespace {
//!
//! \brief Provides various public and private key representations for tests.
//!
//! Keys match the shared contract message keys embedded in the application.
//!
struct TestKey
{
    //!
    //! \brief Create a valid private key for tests.
    //!
    static CKey Private()
    {
        std::vector<unsigned char> private_key = ParseHex(
            "308201130201010420fbd45ffb02ff05a3322c0d77e1e7aea264866c24e81e5ab6"
            "a8e150666b4dc6d8a081a53081a2020101302c06072a8648ce3d0101022100ffff"
            "fffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604"
            "010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959"
            "f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47"
            "d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03b"
            "bfd25e8cd0364141020101a144034200044b2938fbc38071f24bede21e838a0758"
            "a52a0085f2e034e7f971df445436a252467f692ec9c5ba7e5eaa898ab99cbd9949"
            "496f7e3cafbf56304b1cc2e5bdf06e");

        CKey key;
        key.SetPrivKey(CPrivKey(private_key.begin(), private_key.end()));

        return key;
    }

    //!
    //! \brief Create a valid public key for tests.
    //!
    static CPubKey Public()
    {
        return CPubKey(std::vector<unsigned char> {
            0x04, 0x4b, 0x29, 0x38, 0xfb, 0xc3, 0x80, 0x71, 0xf2, 0x4b, 0xed,
            0xe2, 0x1e, 0x83, 0x8a, 0x07, 0x58, 0xa5, 0x2a, 0x00, 0x85, 0xf2,
            0xe0, 0x34, 0xe7, 0xf9, 0x71, 0xdf, 0x44, 0x54, 0x36, 0xa2, 0x52,
            0x46, 0x7f, 0x69, 0x2e, 0xc9, 0xc5, 0xba, 0x7e, 0x5e, 0xaa, 0x89,
            0x8a, 0xb9, 0x9c, 0xbd, 0x99, 0x49, 0x49, 0x6f, 0x7e, 0x3c, 0xaf,
            0xbf, 0x56, 0x30, 0x4b, 0x1c, 0xc2, 0xe5, 0xbd, 0xf0, 0x6e
        });
    }

    //!
    //! \brief Create an address from the test public key.
    //!
    static CBitcoinAddress Address()
    {
        return CBitcoinAddress(CTxDestination(Public().GetID()));
    }
}; // TestKey
} // anonymous namespace

// -----------------------------------------------------------------------------
// Beacon
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Beacon)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_invalid_beacon)
{
    const NN::Beacon beacon;

    BOOST_CHECK(beacon.m_public_key.Raw().empty() == true);
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_a_public_key)
{
    const NN::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_parses_a_beacon_from_a_legacy_contract_value)
{
    const std::string legacy = EncodeBase64(
        "Unused CPID field;"
        "Unused random hex field;"
        "Unused rain address field;"
        + TestKey::Public().ToString());

    const NN::Beacon beacon = NN::Beacon::Parse(legacy);

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_the_beacon_is_well_formed)
{
    const NN::Beacon valid(TestKey::Public());
    BOOST_CHECK(valid.WellFormed() == true);

    const NN::Beacon invalid_empty;
    BOOST_CHECK(invalid_empty.WellFormed() == false);

    const NN::Beacon invalid_bad_key(CPubKey(ParseHex("12345")));
    BOOST_CHECK(invalid_bad_key.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_calculates_the_age_of_the_beacon)
{
    const int64_t now = 100;
    const NN::Beacon beacon(CPubKey(), 99);

    BOOST_CHECK_EQUAL(beacon.Age(now), 1);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_beacon_expired)
{
    const NN::Beacon valid(CPubKey(), NN::Beacon::MAX_AGE);
    BOOST_CHECK(valid.Expired(NN::Beacon::MAX_AGE) == false);

    const NN::Beacon almost(CPubKey(), 0);
    BOOST_CHECK(almost.Expired(NN::Beacon::MAX_AGE) == false);

    const NN::Beacon expired(CPubKey(), 0);
    BOOST_CHECK(expired.Expired(NN::Beacon::MAX_AGE + 1) == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_beacon_is_renewable)
{
    const NN::Beacon not_needed(CPubKey(), NN::Beacon::RENEWAL_AGE);
    BOOST_CHECK(not_needed.Renewable(NN::Beacon::RENEWAL_AGE) == false);

    const NN::Beacon almost(CPubKey(), 0);
    BOOST_CHECK(almost.Renewable(NN::Beacon::RENEWAL_AGE) == false);

    const NN::Beacon renewable(CPubKey(), 0);
    BOOST_CHECK(renewable.Renewable(NN::Beacon::RENEWAL_AGE + 1) == true);
}

BOOST_AUTO_TEST_CASE(it_produces_a_rain_address)
{
    const NN::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetAddress() == TestKey::Address());
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_legacy_string)
{
    const NN::Beacon beacon(TestKey::Public());

    const std::string expected = EncodeBase64(
        "0;0;"
        + TestKey::Address().ToString()
        + ";"
        + TestKey::Public().ToString());

    BOOST_CHECK_EQUAL(beacon.ToString(), expected);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const NN::Beacon beacon(TestKey::Public());

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << TestKey::Public();

    const CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << beacon;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << TestKey::Public();

    NN::Beacon beacon;
    stream >> beacon;

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// BeaconPayload
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(BeaconPayload)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_invalid_payload)
{
    const NN::BeaconPayload payload;

    BOOST_CHECK_EQUAL(payload.m_version, NN::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid.IsZero() == true);
    BOOST_CHECK(payload.m_beacon.m_public_key.Raw().empty() == true);
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_beacon_contract_data)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::BeaconPayload payload(cpid, NN::Beacon(TestKey::Public()));

    BOOST_CHECK_EQUAL(payload.m_version, NN::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_parses_a_payload_from_a_legacy_contract_key_and_value)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    const std::string key = cpid.ToString();
    const std::string value = EncodeBase64(
        "Unused CPID field;"
        "Unused random hex field;"
        "Unused rain address field;"
        + TestKey::Public().ToString());

    const NN::BeaconPayload payload = NN::BeaconPayload::Parse(key, value);

    BOOST_CHECK_EQUAL(payload.m_version, NN::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_behaves_like_a_contract_payload)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::BeaconPayload payload(cpid, NN::Beacon(TestKey::Public()));

    BOOST_CHECK(payload.ContractType() == NN::ContractType::BEACON);
    BOOST_CHECK(payload.WellFormed(NN::ContractAction::ADD) == true);
    BOOST_CHECK(payload.LegacyKeyString() == cpid.ToString());
    BOOST_CHECK(payload.LegacyValueString() == payload.m_beacon.ToString());
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed_for_add)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::BeaconPayload valid(cpid, NN::Beacon(TestKey::Public()));

    BOOST_CHECK(valid.WellFormed(NN::ContractAction::ADD) == true);

    const NN::BeaconPayload zero_cpid{NN::Cpid(), NN::Beacon(TestKey::Public())};

    // A zero CPID is technically valid...
    BOOST_CHECK(zero_cpid.WellFormed(NN::ContractAction::ADD) == true);

    const NN::BeaconPayload missing_key(cpid, NN::Beacon());
    BOOST_CHECK(missing_key.WellFormed(NN::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed_for_delete)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::BeaconPayload valid(cpid, NN::Beacon());

    BOOST_CHECK(valid.WellFormed(NN::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_add)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::Beacon beacon(TestKey::Public());
    const NN::BeaconPayload payload(cpid, beacon);

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::BeaconPayload::CURRENT_VERSION
        << cpid
        << beacon;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    payload.Serialize(stream, NN::ContractAction::ADD);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_add)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::Beacon beacon(TestKey::Public());

    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::BeaconPayload::CURRENT_VERSION
        << cpid
        << beacon;

    NN::BeaconPayload payload;
    payload.Unserialize(stream, NN::ContractAction::ADD);

    BOOST_CHECK_EQUAL(payload.m_version, NN::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);

    BOOST_CHECK(payload.WellFormed(NN::ContractAction::ADD) == true);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_delete)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::BeaconPayload payload(cpid, NN::Beacon());

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::BeaconPayload::CURRENT_VERSION
        << cpid;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    payload.Serialize(stream, NN::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_delete)
{
    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::BeaconPayload::CURRENT_VERSION
        << cpid;

    NN::BeaconPayload payload;
    payload.Unserialize(stream, NN::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(payload.m_version, NN::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key.Raw().empty() == true);
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);

    BOOST_CHECK(payload.WellFormed(NN::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_SUITE_END()
