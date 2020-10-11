// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "gridcoin/beacon.h"

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
    //! \brief Create a key ID from the test public key.
    //!
    static CKeyID KeyId()
    {
        return Public().GetID();
    }

    //!
    //! \brief Create an address from the test public key.
    //!
    static CBitcoinAddress Address()
    {
        return CBitcoinAddress(CTxDestination(KeyId()));
    }

    //!
    //! \brief Create a beacon verification code from the test public key.
    //!
    static std::string VerificationCode()
    {
        const CKeyID key_id = KeyId();

        return EncodeBase58(key_id.begin(), key_id.end());
    }

    //!
    //! \brief Create a beacon payload signature signed by this private key.
    //!
    static std::vector<uint8_t> Signature()
    {
        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher
            << GRC::BeaconPayload::CURRENT_VERSION
            << GRC::Beacon(Public())
            << GRC::Cpid::Parse("00010203040506070809101112131415");

        std::vector<uint8_t> signature;
        CKey private_key = Private();

        private_key.Sign(hasher.GetHash(), signature);

        return signature;
    }
}; // TestKey
} // anonymous namespace

// -----------------------------------------------------------------------------
// Beacon
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Beacon)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_invalid_beacon)
{
    const GRC::Beacon beacon;

    BOOST_CHECK(beacon.m_public_key.Raw().empty() == true);
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_a_public_key)
{
    const GRC::Beacon beacon(TestKey::Public());

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

    const GRC::Beacon beacon = GRC::Beacon::Parse(legacy);

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_the_beacon_is_well_formed)
{
    const GRC::Beacon valid(TestKey::Public());
    BOOST_CHECK(valid.WellFormed() == true);

    const GRC::Beacon invalid_empty;
    BOOST_CHECK(invalid_empty.WellFormed() == false);

    const GRC::Beacon invalid_bad_key(CPubKey(ParseHex("12345")));
    BOOST_CHECK(invalid_bad_key.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_calculates_the_age_of_the_beacon)
{
    const int64_t now = 100;
    const GRC::Beacon beacon(CPubKey(), 99);

    BOOST_CHECK_EQUAL(beacon.Age(now), 1);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_beacon_expired)
{
    const GRC::Beacon valid(CPubKey(), GRC::Beacon::MAX_AGE);
    BOOST_CHECK(valid.Expired(GRC::Beacon::MAX_AGE) == false);

    const GRC::Beacon almost(CPubKey(), 1);
    BOOST_CHECK(almost.Expired(GRC::Beacon::MAX_AGE + 1) == false);

    const GRC::Beacon expired(CPubKey(), 1);
    BOOST_CHECK(expired.Expired(GRC::Beacon::MAX_AGE + 2) == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_beacon_is_renewable)
{
    const GRC::Beacon not_needed(CPubKey(), GRC::Beacon::RENEWAL_AGE);
    BOOST_CHECK(not_needed.Renewable(GRC::Beacon::RENEWAL_AGE) == false);

    const GRC::Beacon almost(CPubKey(), 1);
    BOOST_CHECK(almost.Renewable(GRC::Beacon::RENEWAL_AGE) == false);

    const GRC::Beacon renewable(CPubKey(), 1);
    BOOST_CHECK(renewable.Renewable(GRC::Beacon::RENEWAL_AGE + 2) == true);
}

BOOST_AUTO_TEST_CASE(it_produces_a_key_id)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetId() == TestKey::KeyId());
}

BOOST_AUTO_TEST_CASE(it_produces_a_rain_address)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetAddress() == TestKey::Address());
}

BOOST_AUTO_TEST_CASE(it_produces_a_verification_code)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetVerificationCode() == TestKey::VerificationCode());
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_legacy_string)
{
    const GRC::Beacon beacon(TestKey::Public());

    const std::string expected = EncodeBase64(
        "0;0;"
        + TestKey::Address().ToString()
        + ";"
        + TestKey::Public().ToString());

    BOOST_CHECK_EQUAL(beacon.ToString(), expected);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const GRC::Beacon beacon(TestKey::Public());

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

    GRC::Beacon beacon;
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
    const GRC::BeaconPayload payload;

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid.IsZero() == true);
    BOOST_CHECK(payload.m_beacon.m_public_key.Raw().empty() == true);
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_beacon_contract_data)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_parses_a_payload_from_a_legacy_contract_key_and_value)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    const std::string key = cpid.ToString();
    const std::string value = EncodeBase64(
        "Unused CPID field;"
        "Unused random hex field;"
        "Unused rain address field;"
        + TestKey::Public().ToString());

    const GRC::BeaconPayload payload = GRC::BeaconPayload::Parse(key, value);

    // Legacy beacon payloads always parse to version 1:
    BOOST_CHECK_EQUAL(payload.m_version, 1);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_behaves_like_a_contract_payload)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));
    payload.m_signature = TestKey::Signature();

    BOOST_CHECK(payload.ContractType() == GRC::ContractType::BEACON);
    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(payload.LegacyKeyString() == cpid.ToString());
    BOOST_CHECK(payload.LegacyValueString() == payload.m_beacon.ToString());
    BOOST_CHECK(payload.RequiredBurnAmount() > 0);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload valid(cpid, GRC::Beacon(TestKey::Public()));
    valid.m_signature = TestKey::Signature();

    BOOST_CHECK(valid.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(valid.WellFormed(GRC::ContractAction::REMOVE) == true);

    GRC::BeaconPayload zero_cpid{GRC::Cpid(), GRC::Beacon(TestKey::Public())};
    zero_cpid.m_signature = TestKey::Signature();

    // A zero CPID is technically valid...
    BOOST_CHECK(zero_cpid.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(zero_cpid.WellFormed(GRC::ContractAction::REMOVE) == true);

    GRC::BeaconPayload missing_key(cpid, GRC::Beacon());
    missing_key.m_signature = TestKey::Signature();

    BOOST_CHECK(missing_key.WellFormed(GRC::ContractAction::ADD) == false);
    BOOST_CHECK(missing_key.WellFormed(GRC::ContractAction::REMOVE) == false);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_a_legacy_v1_payload_is_well_formed)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());

    const GRC::BeaconPayload add = GRC::BeaconPayload(1, cpid, beacon);

    BOOST_CHECK(add.WellFormed(GRC::ContractAction::ADD) == true);
    // Legacy beacon deletion contracts ignore the value:
    BOOST_CHECK(add.WellFormed(GRC::ContractAction::REMOVE) == true);

    const GRC::BeaconPayload remove = GRC::BeaconPayload(1, cpid, GRC::Beacon());

    BOOST_CHECK(remove.WellFormed(GRC::ContractAction::ADD) == false);
    BOOST_CHECK(remove.WellFormed(GRC::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_CASE(it_signs_the_payload)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));

    CKey private_key = TestKey::Private();

    BOOST_CHECK(payload.Sign(private_key));

    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    payload.Serialize(hasher, GRC::ContractAction::UNKNOWN);

    CKey key;

    BOOST_CHECK(key.SetPubKey(TestKey::Public()));
    BOOST_CHECK(key.Verify(hasher.GetHash(), payload.m_signature));
}

BOOST_AUTO_TEST_CASE(it_verifies_the_payload_signature)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));

    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    payload.Serialize(hasher, GRC::ContractAction::UNKNOWN);

    CKey private_key = TestKey::Private();

    BOOST_CHECK(private_key.Sign(hasher.GetHash(), payload.m_signature));
    BOOST_CHECK(payload.VerifySignature());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());
    GRC::BeaconPayload payload(cpid, beacon);
    payload.m_signature = TestKey::Signature();

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::BeaconPayload::CURRENT_VERSION
        << cpid
        << beacon
        << payload.m_signature;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    payload.Serialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());
    const std::vector<uint8_t> signature = TestKey::Signature();

    CDataStream stream_add = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::BeaconPayload::CURRENT_VERSION
        << cpid
        << beacon
        << signature;

    CDataStream stream_remove = stream_add;

    GRC::BeaconPayload payload;
    payload.Unserialize(stream_add, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        payload.m_signature.begin(),
        payload.m_signature.end(),
        signature.begin(),
        signature.end());

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == true);

    payload = GRC::BeaconPayload();
    payload.Unserialize(stream_remove, GRC::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        payload.m_signature.begin(),
        payload.m_signature.end(),
        signature.begin(),
        signature.end());

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_SUITE_END()
