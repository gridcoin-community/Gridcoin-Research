#include "hash.h"
#include "neuralnet/contract/contract.h"
#include "neuralnet/project.h"
#include "wallet.h"

#include <boost/test/unit_test.hpp>
#include <vector>

namespace {
//!
//! \brief A contract payload for testing.
//!
class TestPayload : public NN::IContractPayload
{
public:
    std::string m_data;

    TestPayload(std::string data) : m_data(std::move(data))
    {
    }

    NN::ContractType ContractType() const override
    {
        return NN::ContractType::UNKNOWN;
    }

    bool WellFormed(const NN::ContractAction action) const override
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

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const NN::ContractAction contract_action)
    {
        READWRITE(m_data);
    }
}; // TestPayload

//!
//! \brief Provides various public and private key representations for tests.
//!
//! Keys match the shared message keys embedded in the application.
//!
struct TestKey
{
    //!
    //! \brief Create a valid private key for tests.
    //!
    //! \return This is actually the shared message private key.
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
    //! \return Complements the private key above.
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
    //! \brief Create a valid public key for tests.
    //!
    //! \return Hex-encoded uncompressed key that complements the private key
    //! above (and same key as the public key object above).
    //!
    static std::string PublicString()
    {
        return "044b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a25"
               "2467f692ec9c5ba7e5eaa898ab99cbd9949496f7e3cafbf56304b1cc2e5bdf06e";
    }

    //!
    //! \brief Create some invalid hex-encoded public key strings for tests.
    //!
    //! \return A set of various malformed public key strings that should fail
    //! validation after parsing.
    //!
    static std::vector<std::string> GarbagePublicStrings()
    {
        return std::vector<std::string> {
            // Too short: 32 bytes (not a real key):
            "044b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a25",
            // Too long: 66 bytes (not a real key):
            "044b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a252"
            "467f692ec9c5ba7e5eaa898ab99cbd9949496f7e3cafbf56304b1cc2e5bdf06e11",
            // Garbage: invalid hex characters
            "zz4b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a252",
        };
    }
}; // struct TestKey

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
    //! \brief Create a valid signature of a version 1 contract for tests.
    //!
    //! \return Signature as a vector of bytes. Same signature as the base64
    //! string version above.
    //!
    static std::vector<unsigned char> V1Bytes()
    {
        return std::vector<unsigned char> {
            0x30, 0x44, 0x02, 0x20, 0x70, 0xaf, 0xb6, 0xd6, 0x73, 0x22, 0xb0,
            0x6a, 0x16, 0xdc, 0xbd, 0x10, 0x3f, 0xdf, 0x9f, 0x9c, 0x8c, 0x4b,
            0x34, 0xf3, 0xb9, 0xc8, 0x78, 0x5d, 0xfe, 0x0a, 0xc6, 0x23, 0x49,
            0x8f, 0x0b, 0xcc, 0x02, 0x20, 0x29, 0xed, 0x0b, 0xc7, 0x1e, 0x17,
            0xf5, 0xfb, 0x1a, 0x84, 0x07, 0x9e, 0xff, 0x93, 0x4d, 0x23, 0x65,
            0x6e, 0x51, 0x20, 0x84, 0xda, 0xc4, 0x8f, 0x63, 0x5d, 0xa4, 0x79,
            0xd4, 0xb9, 0x9f, 0x9f
        };
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

    //!
    //! \brief Create an invalid signature for tests.
    //!
    //! \return Signature as a vector of bytes. Same signature as the valid
    //! version 1 above with the last byte changed.
    //!
    static std::vector<unsigned char> InvalidBytes()
    {
        return std::vector<unsigned char> {
            0x30, 0x44, 0x02, 0x20, 0x70, 0xaf, 0xb6, 0xd6, 0x73, 0x22, 0xb0,
            0x6a, 0x16, 0xdc, 0xbd, 0x10, 0x3f, 0xdf, 0x9f, 0x9c, 0x8c, 0x4b,
            0x34, 0xf3, 0xb9, 0xc8, 0x78, 0x5d, 0xfe, 0x0a, 0xc6, 0x23, 0x49,
            0x8f, 0x0b, 0xcc, 0x02, 0x20, 0x29, 0xed, 0x0b, 0xc7, 0x1e, 0x17,
            0xf5, 0xfb, 0x1a, 0x84, 0x07, 0x9e, 0xff, 0x93, 0x4d, 0x23, 0x65,
            0x6e, 0x51, 0x20, 0x84, 0xda, 0xc4, 0x8f, 0x63, 0x5d, 0xa4, 0x79,
            0xd4, 0xb9, 0x9f, 0x0f
        };
    }

    //!
    //! \brief Create some invalid base64-encoded signatures for tests.
    //!
    //! \return A set of various malformed signature strings that should fail
    //! validation after parsing.
    //!
    static std::vector<std::string> GarbageStrings()
    {
        return std::vector<std::string> {
            // Too short: 63 bytes, base64-encoded (not a real signature):
            "OGU0ZjQwNTE4ODA2NjEyMTIxMDJiYmJlMDMzOTM3ZTJkMTcyNDdjYmQzMDE5OTg5MzI3NTlhNjJkMjNlMGNl",
            // Too long: 74 bytes, base64-encoded (not a real signature):
            "OGU0ZjQwNTE4ODA2NjEyMTIxMDJiYmJlMDMzOTM3ZTJkMTcyNDdjYmQzMDE5OTg5MzI3NTlhNjJkMjNlMGNlMDk0MGU4Y2EzMjA=",
            // Garbage base64-encoded string (one padding removed):
            "MEQCIBQji0VFbMdiD5urHGgeq0UaiMB6IfI6+JKCC3Y9gMxCAiBornelAZ2bFusBPiD4DL+HS2SbiVU4j4pmMW4dQiiDIA=",
            // Garbage base64-encoded string (one character removed):
            "MEQCIBQji0VFbMdiD5urHGgeq0UaiMB6IfI6+JKCC3Y9gMxCAiBornelAZ2bFusBPiD4DL+HS2SbiVU4j4pmMW4dQiiDI==",
            // Garbage base64-encoded string (non-base64 character):
            "^EQCIBQji0VFbMdiD5urHGgeq0UaiMB6IfI6+JKCC3Y9gMxCAiBornelAZ2bFusBPiD4DL+HS2SbiVU4j4pmMW4dQiiDIA==",
        };
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
    static NN::Contract Current()
    {
        return NN::Contract(
            NN::Contract::CURRENT_VERSION,
            NN::ContractType::PROJECT,
            NN::ContractAction::ADD,
            NN::ContractPayload::Make<NN::Project>("test", "test", 123),
            NN::Contract::Signature(),
            NN::Contract::PublicKey(),
            123); // Tx timestamp is never part of a message
    }

    //!
    //! \brief Create a complete, signed, legacy, version 1 contract object.
    //!
    //! \return Contains the default content used to create the v1 signature
    //! above and includes that signature.
    //!
    static NN::Contract V1()
    {
        return NN::MakeLegacyContract(
            NN::ContractType::BEACON,
            NN::ContractAction::ADD,
            "test",
            "test")
            .ToLegacy();
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
            0x03,                   // Type: PROJECT
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
    NN::Contract::Type type(NN::ContractType::BEACON);

    BOOST_CHECK(type == NN::ContractType::BEACON);
}

BOOST_AUTO_TEST_CASE(it_parses_a_contract_type_from_a_string)
{
    NN::Contract::Type type = NN::Contract::Type::Parse("beacon");

    BOOST_CHECK(type == NN::ContractType::BEACON);
}

BOOST_AUTO_TEST_CASE(it_parses_unknown_contract_types_to_unknown)
{
    NN::Contract::Type type = NN::Contract::Type::Parse("something");

    BOOST_CHECK(type == NN::ContractType::UNKNOWN);
}

BOOST_AUTO_TEST_CASE(it_provides_the_wrapped_contract_type_enum_value)
{
    NN::Contract::Type type(NN::ContractType::BEACON);

    BOOST_CHECK(type.Value() == NN::ContractType::BEACON);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    NN::Contract::Type type(NN::ContractType::BEACON);

    BOOST_CHECK(type.ToString() == "beacon");
}

BOOST_AUTO_TEST_CASE(it_supports_equality_operators_with_contract_type_enums)
{
    NN::Contract::Type type(NN::ContractType::BEACON);

    BOOST_CHECK(type == NN::ContractType::BEACON);
    BOOST_CHECK(type != NN::ContractType::PROJECT);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    NN::Contract::Type type(NN::ContractType::BEACON); // BEACON == 0x01

    BOOST_CHECK(GetSerializeSize(type, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);
    stream << type;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == std::vector<unsigned char> { 0x01 });
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    NN::Contract::Type type(NN::ContractType::UNKNOWN);

    std::vector<unsigned char> bytes { 0x01 };
    CDataStream stream(bytes, SER_NETWORK, 1);

    stream >> type;

    BOOST_CHECK(type == NN::ContractType::BEACON); // BEACON == 0x01
}

BOOST_AUTO_TEST_CASE(it_deserializes_unknown_values_from_a_stream)
{
    // Start with a valid contract type:
    NN::Contract::Type type(NN::ContractType::BEACON);

    std::vector<unsigned char> bytes { 0xEE }; // Invalid
    CDataStream stream(bytes, SER_NETWORK, 1);

    stream >> type;

    BOOST_CHECK(type == NN::ContractType::UNKNOWN);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract::Action
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__Action)

BOOST_AUTO_TEST_CASE(it_initializes_to_a_provided_action)
{
    NN::Contract::Action action(NN::ContractAction::ADD);

    BOOST_CHECK(action == NN::ContractAction::ADD);
}

BOOST_AUTO_TEST_CASE(it_parses_a_contract_action_from_a_string)
{
    NN::Contract::Action action = NN::Contract::Action::Parse("A");

    BOOST_CHECK(action == NN::ContractAction::ADD);
}

BOOST_AUTO_TEST_CASE(it_parses_unknown_contract_actions_to_unknown)
{
    NN::Contract::Action action = NN::Contract::Action::Parse("something");

    BOOST_CHECK(action == NN::ContractAction::UNKNOWN);
}

BOOST_AUTO_TEST_CASE(it_provides_the_wrapped_contract_action_enum_value)
{
    NN::Contract::Action action(NN::ContractAction::REMOVE);

    BOOST_CHECK(action.Value() == NN::ContractAction::REMOVE);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    NN::Contract::Action action(NN::ContractAction::REMOVE);

    BOOST_CHECK(action.ToString() == "D");
}

BOOST_AUTO_TEST_CASE(it_supports_equality_operators_with_contract_action_enums)
{
    NN::Contract::Action action(NN::ContractAction::ADD);

    BOOST_CHECK(action == NN::ContractAction::ADD);
    BOOST_CHECK(action != NN::ContractAction::REMOVE);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    NN::Contract::Action action(NN::ContractAction::ADD); // ADD == 0x01

    BOOST_CHECK(GetSerializeSize(action, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);

    stream << action;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == std::vector<unsigned char> { 0x01 });
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    NN::Contract::Action action(NN::ContractAction::UNKNOWN);

    CDataStream stream(std::vector<unsigned char> { 0x02 }, SER_NETWORK, 1);

    stream >> action;

    BOOST_CHECK(action == NN::ContractAction::REMOVE); // REMOVE == 0x02
}

BOOST_AUTO_TEST_CASE(it_deserializes_unknown_values_from_a_stream)
{
    // Start with a valid contract action:
    NN::Contract::Action action(NN::ContractAction::ADD);

    std::vector<unsigned char> bytes { 0xEE }; // Invalid
    CDataStream stream(bytes, SER_NETWORK, 1);

    stream >> action;

    BOOST_CHECK(action == NN::ContractAction::UNKNOWN);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract::Body
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__Body)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_payload)
{
    const NN::Contract::Body body;

    BOOST_CHECK(body.WellFormed(NN::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_supplied_payload)
{
    const NN::Contract::Body body(NN::ContractPayload::Make<TestPayload>("test"));

    BOOST_CHECK(body.WellFormed(NN::ContractAction::ADD) == true);
}

BOOST_AUTO_TEST_CASE(it_provides_access_to_a_known_legacy_payload)
{
    // The Contract class intentionally abstracts the construction of the body
    // to encapsulate the details for management of the polymorphic object. We
    // need to instantiate a contract object to test the body here:
    //
    const NN::Contract contract = TestMessage::V1();
    const NN::ContractPayload payload = contract.m_body.AssumeLegacy();

    BOOST_CHECK(payload->ContractType() == NN::ContractType::UNKNOWN);
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
    const NN::Contract contract = NN::MakeLegacyContract(
        NN::ContractType::PROJECT,
        NN::ContractAction::ADD,
        "Project Name",
        "https://example.com/@");

    const NN::ContractPayload payload = contract.m_body.ConvertFromLegacy(
        NN::ContractType::PROJECT);

    BOOST_CHECK(payload->ContractType() == NN::ContractType::PROJECT);
    BOOST_CHECK(payload->WellFormed(contract.m_action.Value()) == true);
    BOOST_CHECK_EQUAL(payload->LegacyKeyString(), "Project Name");
    BOOST_CHECK_EQUAL(payload->LegacyValueString(), "https://example.com/@");

    const NN::Project& project = payload.As<NN::Project>();

    BOOST_CHECK_EQUAL(project.m_name, "Project Name");
    BOOST_CHECK_EQUAL(project.m_url, "https://example.com/@");
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const NN::Contract::Body body(NN::ContractPayload::Make<TestPayload>("test"));

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << std::string("test");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    body.Serialize(stream, NN::ContractAction::ADD);

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

    NN::Contract::Body body(NN::ContractPayload::Make<TestPayload>(""));
    body.Unserialize(stream, NN::ContractAction::ADD);

    const NN::ContractPayload contract_payload = body.AssumeLegacy();
    const TestPayload& payload = contract_payload.As<TestPayload>();

    BOOST_CHECK_EQUAL(payload.m_data, "test");
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract::Signature
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__Signature)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_invalid_signature_by_default)
{
    NN::Contract::Signature signature;

    BOOST_CHECK(signature.Viable() == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_bytes_in_a_signature)
{
    NN::Contract::Signature signature(std::vector<unsigned char> { 0x05 });

    BOOST_CHECK(signature.Raw() == std::vector<unsigned char> { 0x05 });
}

BOOST_AUTO_TEST_CASE(it_parses_a_signature_from_a_v1_base64_encoded_string)
{
    std::string input = TestSig::V1String();

    NN::Contract::Signature signature = NN::Contract::Signature::Parse(input);

    BOOST_CHECK(signature.ToString() == input);
    BOOST_CHECK(signature.Raw() == TestSig::V1Bytes());
}

BOOST_AUTO_TEST_CASE(it_gives_an_invalid_signature_when_parsing_an_empty_string)
{
    NN::Contract::Signature signature = NN::Contract::Signature::Parse("");

    BOOST_CHECK(signature.Viable() == false);
    BOOST_CHECK(signature.ToString() == "");
}

BOOST_AUTO_TEST_CASE(it_supports_a_basic_check_for_signature_viability)
{
    // OK: 70 bytes
    NN::Contract::Signature signature(TestSig::V1Bytes());

    BOOST_CHECK(signature.Viable() == true);

    // OK: 70 bytes (not a real signature)
    // Invalid signatures with correct length pass but won't verify against the
    // public key when checking the contract. This is just an early check.
    signature = NN::Contract::Signature(TestSig::InvalidBytes());

    BOOST_CHECK(signature.Viable() == true);

    // BAD: Check some invalid base64-encoded inputs:
    for (const auto& garbage : TestSig::GarbageStrings()) {
        signature = NN::Contract::Signature::Parse(garbage);

        BOOST_CHECK(signature.Viable() == false);
    }
}

BOOST_AUTO_TEST_CASE(it_provides_the_bytes_in_the_signature)
{
    NN::Contract::Signature signature(TestSig::V1Bytes());

    BOOST_CHECK(signature.Raw() == TestSig::V1Bytes());
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    std::vector<unsigned char> input = TestSig::V1Bytes();

    NN::Contract::Signature signature(input);

    BOOST_CHECK(signature.ToString() == TestSig::V1String());
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract::PublicKey
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract__PublicKey)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_invalid_key_by_default)
{
    NN::Contract::PublicKey key;

    BOOST_CHECK(key.Key() == CPubKey());
}

BOOST_AUTO_TEST_CASE(it_initializes_by_wrapping_a_provided_key_object)
{
    NN::Contract::PublicKey key(TestKey::Public());

    BOOST_CHECK(key.Key() == TestKey::Public());
}

BOOST_AUTO_TEST_CASE(it_parses_a_public_key_from_a_hex_encoded_string)
{
    std::string input = TestKey::PublicString();

    NN::Contract::PublicKey key = NN::Contract::PublicKey::Parse(input);

    BOOST_CHECK(key.Key() == TestKey::Public());
}

BOOST_AUTO_TEST_CASE(it_gives_an_invalid_key_when_parsing_an_empty_string)
{
    NN::Contract::PublicKey key = NN::Contract::PublicKey::Parse("");

    BOOST_CHECK(key.Viable() == false);
    BOOST_CHECK(key.ToString() == "");
}

BOOST_AUTO_TEST_CASE(it_supports_a_basic_check_for_key_viability)
{
    // OK: 65 bytes, uncompressed
    std::string full_length = TestKey::PublicString();
    NN::Contract::PublicKey key = NN::Contract::PublicKey::Parse(full_length);

    BOOST_CHECK(key.Viable() == true);

    // OK: 33 bytes, compressed (not a real key)
    key = NN::Contract::PublicKey::Parse(
        "044b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a252");

    BOOST_CHECK(key.Viable() == true);

    // BAD: Check some invalid hex-encoded inputs:
    for (const auto& garbage : TestKey::GarbagePublicStrings()) {
        key = NN::Contract::PublicKey::Parse(garbage);

        BOOST_CHECK(key.Viable() == false);
    }
}

BOOST_AUTO_TEST_CASE(it_provides_the_wrapped_key)
{
    NN::Contract::PublicKey key(TestKey::Public());

    BOOST_CHECK(key.Key() == TestKey::Public());
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    NN::Contract::PublicKey key(TestKey::Public());

    BOOST_CHECK(key.ToString() == TestKey::PublicString());
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Contract
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Contract)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_invalid_contract_by_default)
{
    NN::Contract contract;

    BOOST_CHECK(contract.m_version == NN::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == NN::ContractType::UNKNOWN);
    BOOST_CHECK(contract.m_action == NN::ContractAction::UNKNOWN);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == false);
    BOOST_CHECK(contract.m_signature.Raw().empty() == true);
    BOOST_CHECK(contract.m_public_key.Key() == CPubKey());
    BOOST_CHECK(contract.m_tx_timestamp == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_components_for_a_new_contract)
{
    NN::Contract contract(
        NN::ContractType::BEACON,
        NN::ContractAction::ADD,
        NN::ContractPayload::Make<TestPayload>("test data"));

    BOOST_CHECK(contract.m_version == NN::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == NN::ContractType::BEACON);
    BOOST_CHECK(contract.m_action == NN::ContractAction::ADD);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == true);
    BOOST_CHECK(contract.m_signature.Raw().empty() == true);
    BOOST_CHECK(contract.m_public_key.Key() == CPubKey());
    BOOST_CHECK(contract.m_tx_timestamp == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_components_from_a_contract_message)
{
    NN::Contract contract(
        NN::Contract::CURRENT_VERSION,
        NN::ContractType::BEACON,
        NN::ContractAction::ADD,
        NN::ContractPayload::Make<TestPayload>("test data"),
        NN::Contract::Signature(TestSig::V1Bytes()),
        NN::Contract::PublicKey(TestKey::Public()),
        789); // Tx timestamp

    BOOST_CHECK(contract.m_version == NN::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == NN::ContractType::BEACON);
    BOOST_CHECK(contract.m_action == NN::ContractAction::ADD);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == true);
    BOOST_CHECK(contract.m_signature.Raw() == TestSig::V1Bytes());
    BOOST_CHECK(contract.m_public_key.Key() == TestKey::Public());
    BOOST_CHECK(contract.m_tx_timestamp == 789);
}

BOOST_AUTO_TEST_CASE(it_provides_the_legacy_message_keys)
{
    BOOST_CHECK(NN::Contract::MessagePrivateKey().size() == 279);
    BOOST_CHECK(NN::Contract::MessagePublicKey().Raw().size() == 65);
}

BOOST_AUTO_TEST_CASE(it_detects_a_contract_in_a_transaction_message)
{
    BOOST_CHECK(NN::Contract::Detect(TestMessage::V1String()) == true);
    BOOST_CHECK(NN::Contract::Detect("<MESSAGE></MESSAGE>") == false);
    BOOST_CHECK(NN::Contract::Detect("") == false);
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

    BOOST_CHECK(NN::Contract::Detect(message) == false);
}

BOOST_AUTO_TEST_CASE(it_parses_a_legacy_v1_contract_from_a_transaction_message)
{
    NN::Contract contract = NN::Contract::Parse(TestMessage::V1String(), 123);
    NN::ContractPayload payload = contract.m_body.AssumeLegacy();

    BOOST_CHECK(contract.m_version == 1); // Legacy strings always parse to v1
    BOOST_CHECK(contract.m_type == NN::ContractType::BEACON);
    BOOST_CHECK(contract.m_action == NN::ContractAction::ADD);
    BOOST_CHECK(payload->LegacyKeyString() == "test");
    BOOST_CHECK(payload->LegacyValueString() == "test");
    BOOST_CHECK(contract.m_signature.Raw().size() == 70);
    BOOST_CHECK(contract.m_public_key.Key().Raw().size() == 0);
    BOOST_CHECK(contract.m_tx_timestamp == 123);
}

BOOST_AUTO_TEST_CASE(it_gives_an_invalid_contract_when_parsing_an_empty_message)
{
    NN::Contract contract = NN::Contract::Parse("", 123);

    BOOST_CHECK(contract.m_version == NN::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == NN::ContractType::UNKNOWN);
    BOOST_CHECK(contract.m_action == NN::ContractAction::UNKNOWN);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == false);
    BOOST_CHECK(contract.m_signature.Raw().size() == 0);
    BOOST_CHECK(contract.m_public_key.Key().Raw().size() == 0);
    BOOST_CHECK(contract.m_tx_timestamp == 0);
}

BOOST_AUTO_TEST_CASE(it_gives_an_invalid_contract_when_parsing_a_non_contract)
{
    NN::Contract contract = NN::Contract::Parse("<MESSAGE></MESSAGE>", 123);

    BOOST_CHECK(contract.m_version == 1); // Legacy strings always parse to v1
    BOOST_CHECK(contract.m_type == NN::ContractType::UNKNOWN);
    BOOST_CHECK(contract.m_action == NN::ContractAction::UNKNOWN);
    BOOST_CHECK(contract.m_body.WellFormed(contract.m_action.Value()) == false);
    BOOST_CHECK(contract.m_signature.Raw().size() == 0);
    BOOST_CHECK(contract.m_public_key.Key().Raw().size() == 0);
    BOOST_CHECK(contract.m_tx_timestamp == 123);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_contract_is_complete)
{
    NN::Contract contract = TestMessage::Current();
    BOOST_CHECK(contract.WellFormed() == true);

    // WellFormed() does NOT verify the signature:
    contract = TestMessage::Current();
    contract.m_signature = NN::Contract::Signature(TestSig::InvalidBytes());
    BOOST_CHECK(contract.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_legacy_v1_contract_is_complete)
{
    NN::Contract contract = NN::Contract::Parse(TestMessage::V1String(), 123);
    BOOST_CHECK(contract.WellFormed() == true);

    // WellFormed() does NOT verify the signature:
    contract = NN::Contract::Parse(TestMessage::InvalidV1String(), 123);
    BOOST_CHECK(contract.WellFormed() == true);

    contract = NN::Contract::Parse(TestMessage::PartialV1String(), 123);
    BOOST_CHECK(contract.WellFormed() == false);

    contract = NN::Contract::Parse("", 123);
    BOOST_CHECK(contract.WellFormed() == false);

    contract = NN::Contract::Parse("<MESSAGE></MESSAGE>", 123);
    BOOST_CHECK(contract.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_contract_is_valid)
{
    NN::Contract contract = TestMessage::Current();
    BOOST_CHECK(contract.Validate() == true);

    // Version 2+ contracts rely on the signatures in the transactions instead
    // of embedding another signature in the contract:
    contract = TestMessage::Current();
    contract.m_signature = NN::Contract::Signature(TestSig::InvalidBytes());
    BOOST_CHECK(contract.Validate() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_legacy_v1_contract_is_valid)
{
    NN::Contract contract = NN::Contract::Parse(TestMessage::V1String(), 123);
    BOOST_CHECK(contract.Validate() == true);

    // Valid() DOES verify the signature:
    contract = NN::Contract::Parse(TestMessage::InvalidV1String(), 123);
    BOOST_CHECK(contract.Validate() == false);

    contract = NN::Contract::Parse(TestMessage::PartialV1String(), 123);
    BOOST_CHECK(contract.Validate() == false);

    contract = NN::Contract::Parse("", 123);
    BOOST_CHECK(contract.Validate() == false);

    contract = NN::Contract::Parse("<MESSAGE></MESSAGE>", 123);
    BOOST_CHECK(contract.Validate() == false);
}

BOOST_AUTO_TEST_CASE(it_provides_access_to_the_contract_payload)
{
    const NN::Contract contract = TestMessage::Current();
    const NN::ContractPayload payload = contract.SharePayload();

    BOOST_CHECK(payload->ContractType() == NN::ContractType::PROJECT);
    BOOST_CHECK_EQUAL(payload->LegacyKeyString(), "test");
    BOOST_CHECK_EQUAL(payload->LegacyValueString(), "test");
}

BOOST_AUTO_TEST_CASE(it_casts_known_contract_payloads)
{
    const NN::Contract contract = TestMessage::Current();
    const auto payload = contract.SharePayloadAs<NN::Project>();

    BOOST_CHECK_EQUAL(payload->m_name, "test");
    BOOST_CHECK_EQUAL(payload->m_url, "test");
}

BOOST_AUTO_TEST_CASE(it_converts_known_legacy_contract_payloads)
{
    const NN::Contract contract = TestMessage::Current().ToLegacy();
    const auto payload = contract.SharePayloadAs<NN::Project>();

    BOOST_CHECK_EQUAL(payload->m_name, "test");
    BOOST_CHECK_EQUAL(payload->m_url, "test");
}

BOOST_AUTO_TEST_CASE(it_copies_a_cast_or_converted_payload)
{
    const NN::Contract contract = TestMessage::Current();
    const NN::Project project = contract.CopyPayloadAs<NN::Project>();

    BOOST_CHECK_EQUAL(project.m_name, "test");
    BOOST_CHECK_EQUAL(project.m_url, "test");

    // Copying the payload leaves the contract in a valid state:
    BOOST_CHECK(contract.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_moves_a_cast_or_converted_payload)
{
    NN::Contract contract = TestMessage::Current();
    const NN::Project project = contract.PullPayloadAs<NN::Project>();

    BOOST_CHECK_EQUAL(project.m_name, "test");
    BOOST_CHECK_EQUAL(project.m_url, "test");

    // Moving the payload invalidates the contract:
    BOOST_CHECK(contract.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_contract_needs_a_special_key)
{
    // Note: currently all contract types require either the master or message
    // public/private keys.

    NN::Contract contract;

    // The following tests are not exhaustive for every type/action pair:
    contract.m_type = NN::ContractType::BEACON;
    contract.m_action = NN::ContractAction::ADD;

    BOOST_CHECK(contract.RequiresSpecialKey() == true);
    BOOST_CHECK(contract.RequiresMasterKey() == false);
    BOOST_CHECK(contract.RequiresMessageKey() == true);

    contract.m_type = NN::ContractType::PROJECT;

    BOOST_CHECK(contract.RequiresSpecialKey() == true);
    BOOST_CHECK(contract.RequiresMasterKey() == true);
    BOOST_CHECK(contract.RequiresMessageKey() == false);
}

BOOST_AUTO_TEST_CASE(it_resolves_the_appropriate_public_key_for_a_contract)
{
    // Note: currently all contracts types require either the master or message
    // public/private keys.

    NN::Contract contract;

    contract.m_type = NN::ContractType::BEACON;
    contract.m_action = NN::ContractAction::ADD;

    BOOST_CHECK(contract.ResolvePublicKey() == NN::Contract::MessagePublicKey());

    contract.m_type = NN::ContractType::PROJECT;

    BOOST_CHECK(contract.ResolvePublicKey() == CWallet::MasterPublicKey());
}

BOOST_AUTO_TEST_CASE(it_signs_a_message_with_a_supplied_private_key)
{
    NN::Contract contract(
        NN::ContractType::UNKNOWN,
        NN::ContractAction::ADD,
        NN::ContractPayload::Make<TestPayload>("test"));

    CKey private_key = TestKey::Private();

    BOOST_CHECK(contract.Sign(private_key) == true);

    // Build the message body to hash to verify the new signature:
    std::vector<unsigned char> body {
        0x02, 0x00, 0x00, 0x00,       // Version
        0x00,                         // ContractType::UNKNOWN
        0x01,                         // ContractAction::ADD
        0x04, 0x74, 0x65, 0x73, 0x74, // "test" preceeded by length
    };

    uint256 hashed = Hash(body.begin(), body.end());

    BOOST_CHECK(contract.m_signature.Viable() == true);
    BOOST_CHECK(TestKey::Private().Verify(hashed, contract.m_signature.Raw()));
}

BOOST_AUTO_TEST_CASE(it_signs_a_legacy_v1_message_with_a_supplied_private_key)
{
    NN::Contract contract = TestMessage::V1();
    CKey private_key = TestKey::Private();

    BOOST_CHECK(contract.Sign(private_key) == true);

    // Build the message body to hash to verify the new signature:
    std::string body = "beacontesttest";
    uint256 hashed = Hash(body.begin(), body.end());

    BOOST_CHECK(TestKey::Private().Verify(hashed, contract.m_signature.Raw()));
}

BOOST_AUTO_TEST_CASE(it_signs_a_message_with_the_shared_message_private_key)
{
    NN::Contract contract(
        NN::ContractType::BEACON,
        NN::ContractAction::ADD,
        NN::ContractPayload::Make<TestPayload>("test"));

    BOOST_CHECK(contract.SignWithMessageKey() == true);

    // Build the message body to hash to verify the new signature:
    std::vector<unsigned char> body = {
        0x02, 0x00, 0x00, 0x00,       // Version
        0x01,                         // ContractType::BEACON
        0x01,                         // ContractAction::ADD
        0x04, 0x74, 0x65, 0x73, 0x74, // "test" preceeded by length
    };

    uint256 hashed = Hash(body.begin(), body.end());
    CKey key;
    key.SetPrivKey(NN::Contract::MessagePrivateKey());

    BOOST_CHECK(key.Verify(hashed, contract.m_signature.Raw()));
}

BOOST_AUTO_TEST_CASE(it_refuses_to_sign_a_message_with_an_invalid_private_key)
{
    NN::Contract contract(
        NN::ContractType::BEACON,
        NN::ContractAction::ADD,
        NN::ContractPayload::Make<TestPayload>("test"));

    CKey key; // Empty key

    BOOST_CHECK(contract.Sign(key) == false);
    BOOST_CHECK(contract.m_signature.Raw().size() == 0);
}

BOOST_AUTO_TEST_CASE(it_verifies_a_legacy_v1_contract_signature)
{
    // Test a message with a valid signature:
    NN::Contract contract = NN::Contract::Parse(TestMessage::V1String(), 123);
    BOOST_CHECK(contract.VerifySignature() == true);

    // Change the previously-signed content:
    contract.m_type = NN::ContractType::PROJECT;
    BOOST_CHECK(contract.VerifySignature() == false);

    // Test a message with an invalid signature:
    contract = NN::Contract::Parse(TestMessage::InvalidV1String(), 123);
    BOOST_CHECK(contract.VerifySignature() == false);
}

BOOST_AUTO_TEST_CASE(it_generates_a_hash_of_a_contract_body)
{
    NN::Contract contract = TestMessage::Current();

    CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

    hasher << contract.m_version;
    hasher << contract.m_type;
    hasher << contract.m_action;
    contract.m_body.Serialize(hasher, contract.m_action.Value());

    BOOST_CHECK(contract.GetHash() == hasher.GetHash());
}

BOOST_AUTO_TEST_CASE(it_generates_a_hash_of_a_legacy_v1_contract_body)
{
    NN::Contract contract = TestMessage::V1();

    BOOST_CHECK(contract.GetHash() == uint256S(
        "484e6c63845cd102b86b75d1c0cb36dd15ae41f8ad00690cdddbdade666b41b6"));
}

BOOST_AUTO_TEST_CASE(it_converts_itself_into_a_new_legacy_contract)
{
    const NN::Contract contract = TestMessage::Current();
    const NN::Contract legacy = contract.ToLegacy();

    const auto payload = contract.SharePayloadAs<NN::Project>();
    const NN::ContractPayload legacy_payload = legacy.SharePayload();

    BOOST_CHECK_EQUAL(legacy.m_version, 1);

    BOOST_CHECK(legacy.m_type == contract.m_type.Value());
    BOOST_CHECK(legacy.m_action == contract.m_action.Value());

    BOOST_CHECK(legacy_payload->LegacyKeyString() == payload->m_name);
    BOOST_CHECK(legacy_payload->LegacyValueString() == payload->m_url);

    BOOST_CHECK(legacy.m_signature.Raw() == contract.m_signature.Raw());
    BOOST_CHECK(legacy.m_public_key.Key() == contract.m_public_key.Key());

    BOOST_CHECK(legacy.m_tx_timestamp == contract.m_tx_timestamp);
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_legacy_string)
{
    NN::Contract contract = TestMessage::V1();
    contract.m_signature = TestSig::V1Bytes();

    BOOST_CHECK(contract.ToString() == TestMessage::V1String());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    NN::Contract contract = TestMessage::Current();

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
    NN::Contract contract;

    CDataStream stream(TestMessage::V2Serialized(), SER_NETWORK, 1);

    stream >> contract;
    contract.m_tx_timestamp = 123; // So that contract.Validate() passes

    NN::ContractPayload payload = contract.SharePayload();

    BOOST_CHECK(contract.Validate() == true); // Verifies signature
    BOOST_CHECK(contract.m_version == NN::Contract::CURRENT_VERSION);
    BOOST_CHECK(contract.m_type == NN::ContractType::PROJECT);
    BOOST_CHECK(contract.m_action == NN::ContractAction::ADD);
    BOOST_CHECK(payload->LegacyKeyString() == "test");
    // Version 2+ contracts rely on the signatures in the transactions instead
    // of embedding another signature in the contract:
    BOOST_CHECK(contract.m_public_key == CPubKey());
    BOOST_CHECK(contract.m_signature.Raw().empty() == true);
}

BOOST_AUTO_TEST_SUITE_END()
