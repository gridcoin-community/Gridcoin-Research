// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>
#include <vector>

namespace {
//!
//! \brief A contract payload for testing.
//!
class TestPayload : public GRC::IContractPayload
{
public:
    std::string m_data;

    TestPayload(std::string data) : m_data(std::move(data))
    {
    }

    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::UNKNOWN;
    }

    bool WellFormed(const GRC::ContractAction action) const override
    {
        return !m_data.empty();
    }

    std::string LegacyKeyString() const override
    {
        return m_data;
    }

    std::string LegacyValueString() const override
    {
        return m_data;
    }

    int64_t RequiredBurnAmount() const override
    {
        return GRC::Contract::STANDARD_BURN_AMOUNT;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const GRC::ContractAction contract_action)
    {
        READWRITE(m_data);
    }
}; // TestPayload

//!
//! \brief Provides various signature representations for tests.
//!
//! Valid signatures created by signing the contracts below with the keys above.
//!
struct TestSig
{
    //!
    //! \brief Create a valid signature of a version 1 contract for tests.
    //!
    //! \return Base64-encoded signature. Created by signing the v1 contract
    //! message body in the class below with the private key above.
    //!
    static std::string V1String()
    {
        return "MEQCIHCvttZzIrBqFty9ED/fn5yMSzTzuch4Xf4KxiNJjwvM"
               "AiAp7QvHHhf1+xqEB57/k00jZW5RIITaxI9jXaR51Lmfnw==";
    }

    //!
    //! \brief Create an invalid signature for tests.
    //!
    //! \return Base64-encoded signature. Same invalid signature as the bytes
    //! below.
    //!
    static std::string InvalidString()
    {
        return "MEQCIHCvttZzIrBqFty9ED/fn5yMSzTzuch4Xf4KxiNJjwvM"
               "AiAp7QvHHhf1+xqEB57/k00jZW5RIITaxI9jXaR51LmfDw==";
    }
}; // struct TestSig

//!
//! \brief Provides various contract message representations for tests.
//!
struct TestMessage
{
    //!
    //! \brief Create a complete, signed contract object for the latest contract
    //! version.
    //!
    //! \return Contains the default content used to create the v2 signature
    //! above and includes that signature.
    //!
    static GRC::Contract Current()
    {
        return GRC::Contract(
            GRC::Contract::CURRENT_VERSION,
            GRC::ContractType::PROJECT,
            GRC::ContractAction::ADD,
            GRC::ContractPayload::Make<GRC::Project>("test", "test", 123));
    }

    //!
    //! \brief Create a complete, signed, legacy, version 1 contract object.
    //!
    //! \return Contains the default content used to create the v1 signature
    //! above and includes that signature.
    //!
    static GRC::Contract V1()
    {
        GRC::Contract contract = GRC::MakeLegacyContract(
            GRC::ContractType::PROJECT,
            GRC::ContractAction::ADD,
            "test",
            "test");

        contract.m_version = 1;

        return contract;
    }

    //!
    //! \brief Create a legacy version 1 contract message string for tests.
    //!
    //! \return Signed by the private key above.
    //!
    static std::string V1String()
    {
        // This message was signed by the message private key and contains a valid
        // signature:
        return std::string(
            "<MT>beacon</MT>"
            "<MK>test</MK>"
            "<MV>test</MV>"
            "<MA>A</MA>"
            "<MPK></MPK>"
            "<MS>" + TestSig::V1String() + "</MS>");
    }

    //!
    //! \brief Get a serialized representation of a valid version 2 contract
    //! message.
    //!
    //! \return As bytes. Matches the contract message string above (without
    //! tags) and includes version 2 components.
    //!
    static std::vector<unsigned char> V2Serialized()
    {
        std::vector<unsigned char> serialized {
            0x02, 0x00, 0x00, 0x00, // Version: 32-bit int (little-endian)
            0x05,                   // Type: PROJECT
            0x01,                   // Action: ADD
            0x01, 0x00, 0x00, 0x00, // Project contract version
            0x04,                   // Length of the project name
            0x74, 0x65, 0x73, 0x74, // "test" as bytes
            0x04,                   // Length of the project URL
            0x74, 0x65, 0x73, 0x74, // "test" as bytes
        };

        return serialized;
    }

    //!
    //! \brief Create a legacy, version 1 contract message string for tests.
    //!
    //! \return A contract message with an invalid signature.
    //!
    static std::string InvalidV1String()
    {
        return std::string(
            "<MT>beacon</MT>"
            "<MK>test</MK>"
            "<MV>test</MV>"
            "<MA>A</MA>"
            "<MPK></MPK>"
            "<MS>" + TestSig::InvalidString() + "</MS>");
    }

    //!
    //! \brief Create an invalid, partial, legacy, version 1 contract message
    //! string for tests.
    //!
    //! \return Contains a message type for detection, but missing items that
    //! must fail to validate.
    //!
    static std::string PartialV1String()
    {
        return "<MT>beacon</MT><MK>test</MK><MA>A</MA><MPK></MPK>";
    }
}; // struct TestMessage
} // anonymous namespace

// -----------------------------------------------------------------------------
// Contract::Type
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__Type)

BOOST_AUTO_TEST_CASE(it_initializes_to_a_provided_type)
{
    GRC::Contract::Type type(GRC::ContractType::BEACON);

    BOOST_CHECK(type == GRC::ContractType::BEACON);
}

BOOST_AUTO_TEST_CASE(it_parses_a_contract_type_from_a_string)
{
    GRC::Contract::Type type = GRC::Contract::Type::Parse("beacon");

    BOOST_CHECK(type == GRC::ContractType::BEACON);
}

BOOST_AUTO_TEST_CASE(it_parses_unknown_contract_types_to_unknown)
{
    GRC::Contract::Type type = GRC::Contract::Type::Parse("something");

    BOOST_CHECK(type == GRC::ContractType::UNKNOWN);
}

BOOST_AUTO_TEST_CASE(it_provides_the_wrapped_contract_type_enum_value)
{
    GRC::Contract::Type type(GRC::ContractType::BEACON);

    BOOST_CHECK(type.Value() == GRC::ContractType::BEACON);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    GRC::Contract::Type type(GRC::ContractType::BEACON);

    BOOST_CHECK(type.ToString() == "beacon");
}

BOOST_AUTO_TEST_CASE(it_supports_equality_operators_with_contract_type_enums)
{
    GRC::Contract::Type type(GRC::ContractType::BEACON);

    BOOST_CHECK(type == GRC::ContractType::BEACON);
    BOOST_CHECK(type != GRC::ContractType::PROJECT);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    GRC::Contract::Type type(GRC::ContractType::BEACON); // BEACON == 0x01

    BOOST_CHECK(GetSerializeSize(type, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);
    stream << type;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == std::vector<unsigned char> { 0x01 });
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    GRC::Contract::Type type(GRC::ContractType::UNKNOWN);

    std::vector<unsigned char> bytes { 0x01 };
    CDataStream stream(bytes, SER_NETWORK, 1);

    stream >> type;

    BOOST_CHECK(type == GRC::ContractType::BEACON); // BEACON == 0x01
}

BOOST_AUTO_TEST_CASE(it_refuses_to_deserialize_unknown_types)
{
    GRC::Contract::Type type(GRC::ContractType::UNKNOWN);

    std::vector<unsigned char> bytes { 0xFF }; // out-of-range
    CDataStream stream(bytes, SER_NETWORK, 1);

    BOOST_CHECK_THROW(stream >> type, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract::Action
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__Action)

BOOST_AUTO_TEST_CASE(it_initializes_to_a_provided_action)
{
    GRC::Contract::Action action(GRC::ContractAction::ADD);

    BOOST_CHECK(action == GRC::ContractAction::ADD);
}

BOOST_AUTO_TEST_CASE(it_parses_a_contract_action_from_a_string)
{
    GRC::Contract::Action action = GRC::Contract::Action::Parse("A");

    BOOST_CHECK(action == GRC::ContractAction::ADD);
}

BOOST_AUTO_TEST_CASE(it_parses_unknown_contract_actions_to_unknown)
{
    GRC::Contract::Action action = GRC::Contract::Action::Parse("something");

    BOOST_CHECK(action == GRC::ContractAction::UNKNOWN);
}

BOOST_AUTO_TEST_CASE(it_provides_the_wrapped_contract_action_enum_value)
{
    GRC::Contract::Action action(GRC::ContractAction::REMOVE);

    BOOST_CHECK(action.Value() == GRC::ContractAction::REMOVE);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    GRC::Contract::Action action(GRC::ContractAction::REMOVE);

    BOOST_CHECK(action.ToString() == "D");
}

BOOST_AUTO_TEST_CASE(it_supports_equality_operators_with_contract_action_enums)
{
    GRC::Contract::Action action(GRC::ContractAction::ADD);

    BOOST_CHECK(action == GRC::ContractAction::ADD);
    BOOST_CHECK(action != GRC::ContractAction::REMOVE);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    GRC::Contract::Action action(GRC::ContractAction::ADD); // ADD == 0x01

    BOOST_CHECK(GetSerializeSize(action, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);

    stream << action;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == std::vector<unsigned char> { 0x01 });
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    GRC::Contract::Action action(GRC::ContractAction::UNKNOWN);

    CDataStream stream(std::vector<unsigned char> { 0x02 }, SER_NETWORK, 1);

    stream >> action;

    BOOST_CHECK(action == GRC::ContractAction::REMOVE); // REMOVE == 0x02
}

BOOST_AUTO_TEST_CASE(it_refuses_to_deserialize_unknown_actions)
{
    GRC::Contract::Action action(GRC::ContractAction::UNKNOWN);

    std::vector<unsigned char> bytes { 0xFF }; // out-of-range
    CDataStream stream(bytes, SER_NETWORK, 1);

    BOOST_CHECK_THROW(stream >> action, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract::Body
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__Body)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_payload)
{
    const GRC::Contract::Body body;

    BOOST_CHECK(body.WellFormed(GRC::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_supplied_payload)
{
    const GRC::Contract::Body body(GRC::ContractPayload::Make<TestPayload>("test"));

    BOOST_CHECK(body.WellFormed(GRC::ContractAction::ADD) == true);
}

BOOST_AUTO_TEST_CASE(it_provides_access_to_a_known_legacy_payload)
{
    // The Contract class intentionally abstracts the construction of the body
    // to encapsulate the details for management of the polymorphic object. We
    // need to instantiate a contract object to test the body here:
    //
    const GRC::Contract contract = TestMessage::V1();
    const GRC::ContractPayload payload = contract.m_body.AssumeLegacy();

    BOOST_CHECK(payload->ContractType() == GRC::ContractType::UNKNOWN);
    BOOST_CHECK(payload->WellFormed(contract.m_action.Value()) == true);
    BOOST_CHECK_EQUAL(payload->LegacyKeyString(), "test");
    BOOST_CHECK_EQUAL(payload->LegacyValueString(), "test");
}

BOOST_AUTO_TEST_CASE(it_converts_a_legacy_payload_into_a_specific_contract_type)
{
    // The Contract class intentionally abstracts the construction of the body
    // to encapsulate the details for management of the polymorphic object. We
    // need to instantiate a contract object to test the body here:
    //
    const GRC::Contract contract = GRC::MakeLegacyContract(
        GRC::ContractType::PROJECT,
        GRC::ContractAction::ADD,
        "Project Name",
        "https://example.com/@");

    const GRC::ContractPayload payload = contract.m_body.ConvertFromLegacy(
        GRC::ContractType::PROJECT);

    BOOST_CHECK(payload->ContractType() == GRC::ContractType::PROJECT);
    BOOST_CHECK(payload->WellFormed(contract.m_action.Value()) == true);
    BOOST_CHECK_EQUAL(payload->LegacyKeyString(), "Project Name");
    BOOST_CHECK_EQUAL(payload->LegacyValueString(), "https://example.com/@");

    const GRC::Project& project = payload.As<GRC::Project>();

    BOOST_CHECK_EQUAL(project.m_name, "Project Name");
    BOOST_CHECK_EQUAL(project.m_url, "https://example.com/@");
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const GRC::Contract::Body body(GRC::ContractPayload::Make<TestPayload>("test"));

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << std::string("test");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    body.Serialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << std::string("test");

    GRC::Contract::Body body(GRC::ContractPayload::Make<TestPayload>(""));
    body.Unserialize(stream, GRC::ContractAction::ADD);

    const GRC::ContractPayload contract_payload = body.AssumeLegacy();
    const TestPayload& payload = contract_payload.As<TestPayload>();

    BOOST_CHECK_EQUAL(payload.m_data, "test");
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_invalid_contract_by_default)
{
    GRC::Contract contract;

    BOOST_CHECK(contract.m_version == GRC::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == GRC::ContractType::UNKNOWN);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::UNKNOWN);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_components_for_a_new_contract)
{
    GRC::Contract contract(
        GRC::ContractType::BEACON,
        GRC::ContractAction::ADD,
        GRC::ContractPayload::Make<TestPayload>("test data"));

    BOOST_CHECK(contract.m_version == GRC::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == GRC::ContractType::BEACON);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::ADD);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == true);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_components_from_a_contract_message)
{
    GRC::Contract contract(
        GRC::Contract::CURRENT_VERSION,
        GRC::ContractType::BEACON,
        GRC::ContractAction::ADD,
        GRC::ContractPayload::Make<TestPayload>("test data"));

    BOOST_CHECK(contract.m_version == GRC::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == GRC::ContractType::BEACON);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::ADD);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == true);
}

BOOST_AUTO_TEST_CASE(it_detects_a_contract_in_a_transaction_message)
{
    BOOST_CHECK(GRC::Contract::Detect(TestMessage::V1String()) == true);
    BOOST_CHECK(GRC::Contract::Detect("<MESSAGE></MESSAGE>") == false);
    BOOST_CHECK(GRC::Contract::Detect("") == false);
}

BOOST_AUTO_TEST_CASE(it_ignores_superblocks_during_legacy_v1_contract_detection)
{
    std::string message(
        "<MT>superblock</MT>"
        "<MK>test</MK>"
        "<MV>test</MV>"
        "<MA>A</MA>"
        "<MPK></MPK>"
        "<MS>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</MS>");

    BOOST_CHECK(GRC::Contract::Detect(message) == false);
}

BOOST_AUTO_TEST_CASE(it_parses_a_legacy_v1_contract_from_a_transaction_message)
{
    GRC::Contract contract = GRC::Contract::Parse(TestMessage::V1String());
    GRC::ContractPayload payload = contract.m_body.AssumeLegacy();

    BOOST_CHECK(contract.m_version == 1); // Legacy strings always parse to v1
    BOOST_CHECK(contract.m_type == GRC::ContractType::BEACON);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::ADD);
    BOOST_CHECK(payload->LegacyKeyString() == "test");
    BOOST_CHECK(payload->LegacyValueString() == "test");
}

BOOST_AUTO_TEST_CASE(it_gives_an_invalid_contract_when_parsing_an_empty_message)
{
    GRC::Contract contract = GRC::Contract::Parse("");

    BOOST_CHECK(contract.m_version == GRC::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == GRC::ContractType::UNKNOWN);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::UNKNOWN);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == false);
}

BOOST_AUTO_TEST_CASE(it_gives_an_invalid_contract_when_parsing_a_non_contract)
{
    GRC::Contract contract = GRC::Contract::Parse("<MESSAGE></MESSAGE>");

    BOOST_CHECK(contract.m_version == 1); // Legacy strings always parse to v1
    BOOST_CHECK(contract.m_type == GRC::ContractType::UNKNOWN);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::UNKNOWN);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_contract_is_complete)
{
    GRC::Contract contract = TestMessage::Current();
    BOOST_CHECK(contract.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_legacy_v1_contract_is_complete)
{
    GRC::Contract contract = GRC::Contract::Parse(TestMessage::V1String());
    BOOST_CHECK(contract.WellFormed() == true);

    // WellFormed() does NOT verify the signature:
    contract = GRC::Contract::Parse(TestMessage::InvalidV1String());
    BOOST_CHECK(contract.WellFormed() == true);

    contract = GRC::Contract::Parse(TestMessage::PartialV1String());
    BOOST_CHECK(contract.WellFormed() == false);

    contract = GRC::Contract::Parse("");
    BOOST_CHECK(contract.WellFormed() == false);

    contract = GRC::Contract::Parse("<MESSAGE></MESSAGE>");
    BOOST_CHECK(contract.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_determines_the_requred_burn_fee)
{
    const GRC::Contract contract = TestMessage::Current();

    BOOST_CHECK(contract.RequiredBurnAmount() > 0);
}

BOOST_AUTO_TEST_CASE(it_provides_access_to_the_contract_payload)
{
    const GRC::Contract contract = TestMessage::Current();
    const GRC::ContractPayload payload = contract.SharePayload();

    BOOST_CHECK(payload->ContractType() == GRC::ContractType::PROJECT);
    BOOST_CHECK_EQUAL(payload->LegacyKeyString(), "test");
    BOOST_CHECK_EQUAL(payload->LegacyValueString(), "test");
}

BOOST_AUTO_TEST_CASE(it_casts_known_contract_payloads)
{
    const GRC::Contract contract = TestMessage::Current();
    const auto payload = contract.SharePayloadAs<GRC::Project>();

    BOOST_CHECK_EQUAL(payload->m_name, "test");
    BOOST_CHECK_EQUAL(payload->m_url, "test");
}

BOOST_AUTO_TEST_CASE(it_converts_known_legacy_contract_payloads)
{
    const GRC::Contract contract = TestMessage::V1();
    const auto payload = contract.SharePayloadAs<GRC::Project>();

    BOOST_CHECK_EQUAL(payload->m_name, "test");
    BOOST_CHECK_EQUAL(payload->m_url, "test");
}

BOOST_AUTO_TEST_CASE(it_copies_a_cast_or_converted_payload)
{
    const GRC::Contract contract = TestMessage::Current();
    const GRC::Project project = contract.CopyPayloadAs<GRC::Project>();

    BOOST_CHECK_EQUAL(project.m_name, "test");
    BOOST_CHECK_EQUAL(project.m_url, "test");

    // Copying the payload leaves the contract in a valid state:
    BOOST_CHECK(contract.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_moves_a_cast_or_converted_payload)
{
    GRC::Contract contract = TestMessage::Current();
    const GRC::Project project = contract.PullPayloadAs<GRC::Project>();

    BOOST_CHECK_EQUAL(project.m_name, "test");
    BOOST_CHECK_EQUAL(project.m_url, "test");

    // Moving the payload invalidates the contract:
    BOOST_CHECK(contract.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    GRC::Contract contract = TestMessage::Current();

    // 20 bytes = 4 bytes for the serialization protocol version
    //  + 1 byte each for the type and action
    //  + 14 bytes for the project payload
    //  + 1 byte for the empty public key size
    //
    BOOST_CHECK(GetSerializeSize(contract, SER_NETWORK, 1) == 20);

    CDataStream stream(SER_NETWORK, 1);

    stream << contract;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == TestMessage::V2Serialized());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    GRC::Contract contract;

    CDataStream stream(TestMessage::V2Serialized(), SER_NETWORK, 1);

    stream >> contract;

    GRC::ContractPayload payload = contract.SharePayload();

    BOOST_CHECK(contract.WellFormed() == true);
    BOOST_CHECK(contract.m_version == GRC::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == GRC::ContractType::PROJECT);
    BOOST_CHECK(contract.m_action == GRC::ContractAction::ADD);
    BOOST_CHECK(payload->LegacyKeyString() == "test");
}

BOOST_AUTO_TEST_SUITE_END()
