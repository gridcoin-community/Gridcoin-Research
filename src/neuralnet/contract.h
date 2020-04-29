#pragma once

#include "base58.h"
#include "key.h"
#include "serialize.h"
#include "uint256.h"

#include <boost/optional.hpp>
#include <string>
#include <vector>

namespace NN {
//!
//! \brief Represents the type of a Gridcoin contract.
//!
//! CONSENSUS: Do not remove an item from this enum or change or repurpose the
//! byte value.
//!
enum class ContractType : uint8_t
{
    UNKNOWN    = 0x00, //!< An invalid, non-standard, or empty contract type.
    BEACON     = 0x01, //!< Beacon advertisement or deletion.
    POLL       = 0x02, //!< Submission of a new poll.
    PROJECT    = 0x03, //!< Project whitelist addition or removal.
    PROTOCOL   = 0x04, //!< Network control message or configuration directive.
    SCRAPER    = 0x05, //!< Scraper node authorization grants and revocations.
    SUPERBLOCK = 0x06, //!< The result of superblock quorum consensus.
    VOTE       = 0x07, //!< A vote cast by a wallet for a poll.
    MAX_VALUE  = 0x07, //!< Increment this when adding items to the enum.
};

//!
//! \brief The type of action that a contract declares.
//!
//! CONSENSUS: Do not remove an item from this enum or change or repurpose the
//! byte value.
//!
enum class ContractAction : uint8_t
{
    UNKNOWN   = 0x00, //!< An invalid, non-standard, or empty contract action.
    ADD       = 0x01, //!< Handle a new contract addition (A).
    REMOVE    = 0x02, //!< Remove an existing contract (D).
    MAX_VALUE = 0x02, //!< Increment this when adding items to the enum.
};

//!
//! \brief Represents a Gridcoin contract embedded in a transaction message.
//!
//! Gridcoin contracts are not "smart". These messages contain metadata that
//! control the operation of the research reward protocol and other Gridcoin
//! features.
//!
//! The legacy protocol defined these messages as an XML-like string format.
//! Block versions 11+ changed the contract protocol to a binary format. The
//! contract classes include a significant amount of code that converts that
//! legacy format into objects for compatibility with historical blocks.
//!
//! The application defines "contract handler" services that process each of
//! the different contract types when a node receives contract messages from
//! transactions. These classes share the IContractHandler interface.
//!
class Contract
{
private:
    //!
    //! \brief A wrapper around an enum type that contains a valid enum value
    //! or a string that represents a value not present in the enum.
    //!
    //! \tparam E The enum type wrapped by this class.
    //!
    template<typename E>
    struct EnumVariant
    {
        // Replace with E::underlying_type_t when moving to C++14:
        using EnumUnderlyingType = typename std::underlying_type<E>::type;

        //!
        //! \brief Compare a supplied enum value for equality.
        //!
        //! \param other An enum value to check equality for.
        //!
        //! \return \c true if the suppled value matches the wrapped enum value.
        //!
        bool operator==(const E& other) const
        {
            return m_value == other;
        }

        //!
        //! \brief Compare a supplied enum value for inequality.
        //!
        //! \param other An enum value to check inequality for.
        //!
        //! \return \c true if the suppled value does not match the wrapped
        //! enum value.
        //!
        bool operator!=(const E& other) const
        {
            return m_value != other;
        }

        //!
        //! \brief Get the wrapped enum value.
        //!
        //! \return A value enumerated on enum \c E.
        //!
        E Value() const
        {
            return m_value;
        }

        //!
        //! \brief Get the wrapped enum value as a value of the underlying type.
        //!
        //! \return For example, an unsigned char for an enum that represents
        //! an underlying byte value.
        //!
        EnumUnderlyingType Raw() const
        {
            return static_cast<EnumUnderlyingType>(m_value);
        }

        //!
        //! \brief Get the string representation of the wrapped enum value.
        //!
        //! \return The string as it would appear in a transaction message or
        //! the captured string if parsed from an unrecognized value.
        //!
        virtual std::string ToString() const = 0;

        //!
        //! \brief Get the size of the data to serialize/deserialize.
        //!
        //! \param nType    Target protocol type (network, disk, etc.).
        //! \param nVersion Protocol version.
        //!
        //! \return Size of the data in bytes.
        //!
        unsigned int GetSerializeSize(int nType, int nVersion) const
        {
            return ::GetSerializeSize(Raw(), nType, nVersion);
        }

        //!
        //! \brief Serialize the wrapped enum value to the provided stream.
        //!
        //! \param stream   The output stream.
        //! \param nType    Target protocol type (network, disk, etc.).
        //! \param nVersion Protocol version.
        //!
        template<typename Stream>
        void Serialize(Stream& stream) const
        {
            ::Serialize(stream, Raw());
        }

        //!
        //! \brief Deserialize an enum value from the provided stream.
        //!
        //! \param stream   The input stream.
        //! \param nType    Target protocol type (network, disk, etc.).
        //! \param nVersion Protocol version.
        //!
        template<typename Stream>
        void Unserialize(Stream& stream)
        {
            EnumUnderlyingType value;

            ::Unserialize(stream, value);

            if (value > static_cast<EnumUnderlyingType>(E::MAX_VALUE)) {
                m_value = E::UNKNOWN;
            } else {
                m_value = static_cast<E>(value);
            }
        }

    protected:
        //!
        //! \brief Contains the string representation of a non-standard or
        //! invalid contract type or contract action.
        //!
        //! \c Contract::Type or \c Contract::Action objects may parse string
        //! values that do not exist on the corresponding \c ContractType and
        //! \c ContractAction enums. These objects store that non-standard or
        //! invalid string value behind a construct of this type.
        //!
        typedef boost::optional<std::string> OptionalString;

        //!
        //! \brief Delegated constructor called by child types.
        //!
        //! \param value The enum value to wrap.
        //! \param other Unknown, non-standard, or empty enum value, if any.
        //!
        EnumVariant(E value, OptionalString other)
            : m_value(value), m_other(std::move(other))
        {
        }

        E m_value;              //!< The wrapped enum value.
        OptionalString m_other; //!< Holds invalid or non-standard types.
    }; // Contract::EnumVariant

public:
    //!
    //! \brief Version number of the current format for a serialized contract.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr int CURRENT_VERSION = 2; // TODO: int32_t

    //!
    //! \brief The amount of coin set for a burn output in a transaction that
    //! broadcasts a contract in units of 1/100000000 GRC.
    //!
    //! Currently, we burn the smallest amount possible (0.00000001).
    //!
    static constexpr int64_t BURN_AMOUNT = 1;

    //!
    //! \brief A contract type from a transaction message.
    //!
    //! \c Contract::Type objects directly represent values enumerated on the
    //! \c ContractType enum but provide some internal machinery to store the
    //! string values of invalid or non-standard types found in a message.
    //!
    struct Type : public EnumVariant<ContractType>
    {
        //!
        //! \brief Initialize an instance for a known \c ContractType value.
        //!
        //! \param type A value enumerated on \c ContractType.
        //!
        Type(ContractType type);

        //!
        //! \brief Initialize an instance for an unknown contract type.
        //!
        //! \param other An unknown, invalid, or non-standard contract type
        //! parsed from a transaction message.
        //!
        Type(std::string other);

        //!
        //! \brief Parse a \c ContractType value from its legacy string
        //! representation.
        //!
        //! \param input String representation of a contract type.
        //!
        //! \return A value enumerated on \c ContractType. Returns the value of
        //! \c ContractType::UNKNOWN for an unrecognized contract type.
        //!
        static Type Parse(std::string input);

        //!
        //! \brief Get the string representation of the wrapped contract type.
        //!
        //! \return The string as it would appear in a transaction message.
        //!
        std::string ToString() const override;

    private:
        //!
        //! \brief Initialize an instance for a contract type that has a legacy
        //! string representation.
        //!
        //! \param type   A valid, known contract type described by the string.
        //! \param legacy Described the provided contract type in the past.
        //!
        Type(ContractType type, std::string legacy);
    }; // Contract::Type

    //!
    //! \brief A contract action from a transaction message.
    //!
    //! \c Contract::Action objects directly represent values enumerated on the
    //! \c ContractAction enum but provide some internal machinery to store the
    //! string values of invalid or non-standard actions found in a message.
    //!
    struct Action : public EnumVariant<ContractAction>
    {
        //!
        //! \brief Initialize an instance for a known \c ContractAction value.
        //!
        //! \param type A value enumerated on \c ContractAction.
        //!
        Action(ContractAction action);

        //!
        //! \brief Initialize an instance for an unknown contract action.
        //!
        //! \param other An unknown, invalid, or non-standard contract action
        //! parsed from a transaction message.
        //!
        Action(std::string other);

        //!
        //! \brief Parse a \c ContractAction value from its legacy string
        //! representation.
        //!
        //! \param input String representation of a contract action.
        //!
        //! \return A value enumerated on \ContractAction. Returns the value of
        //! \c ContractAction::UNKNOWN for an unrecognized contract action.
        //!
        static Action Parse(std::string input);

        //!
        //! \brief Get the string representation of the wrapped contract action.
        //!
        //! \return The string as it would appear in a transaction message.
        //!
        std::string ToString() const override;
    }; // Contract::Action

    //!
    //! \brief Parses and stores a contract message signature in binary format.
    //!
    struct Signature
    {
        //!
        //! \brief Initialize an empty, invalid \c Signature object.
        //!
        Signature();

        //!
        //! \brief Initialize a \c Signature object from a series of bytes.
        //!
        //! \param bytes As DER-encoded ASN.1 ECDSA.
        //!
        Signature(std::vector<unsigned char> bytes);

        //!
        //! \brief Create a \c Signature object from its string representation.
        //!
        //! \param input Base64 encoding of the binary signature. Typically
        //! 96 characters.
        //!
        static Signature Parse(const std::string& input);

        //!
        //! \brief Determine whether the object contains a viable signature.
        //!
        //! This method does NOT verify the signature against a public key. Use
        //! only for early checks to determine whether to continue verification.
        //!
        //! \return \c true if resembles a DER-encoded ASN.1 ECDSA signature.
        //!
        bool Viable() const;

        //!
        //! \brief Get the bytes in the signature.
        //!
        //! \return Typically 70 to 72 bytes.
        //!
        const std::vector<unsigned char>& Raw() const;

        //!
        //! \brief Get the string representation of the signature.
        //!
        //! \return Base64 encoding of the binary signature.
        //!
        std::string ToString() const;

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(m_bytes);
        }

    private:
        std::vector<unsigned char> m_bytes; //!< As DER-encoded ASN.1 ECDSA.
    }; // Contract::Signature

    //!
    //! \brief Parses and stores the contract public key in binary format.
    //!
    struct PublicKey
    {
        //!
        //! \brief Initialize an empty, invalid \c PublicKey object.
        //!
        PublicKey();

        //!
        //! \brief Wrap a \c PublicKey object around a \c CPubKey instance.
        //!
        //! \param key The public key to wrap.
        //!
        PublicKey(CPubKey key);

        //!
        //! \brief Create a \c PublicKey object from its string representation.
        //!
        //! \param input Hex string representation of the bytes in the key.
        //!
        static PublicKey Parse(const std::string& input);

        //!
        //! \brief Compare a supplied \CPubKey object for equality.
        //!
        //! \param other A public key to check equality for.
        //!
        //! \return \c true if the supplied public key's bytes match.
        //!
        bool operator==(const CPubKey& other) const;

        //!
        //! \brief Compare a supplied \CPubKey object for inequality.
        //!
        //! \param other A public key to check inequality for.
        //!
        //! \return \c true if the supplied public key's bytes do not match.
        //!
        bool operator!=(const CPubKey& other) const;

        //!
        //! \brief Determine whether the object contains a viable public key.
        //!
        //! This method does NOT verify the key's structure. Use only for early
        //! checks to determine whether to continue verification.
        //!
        //! \return \true if resembles a full or compressed public key.
        //!
        bool Viable() const;

        //!
        //! \brief Get the wrapped \c CPubKey object.
        //!
        //! \return A reference to the wrapped key object.
        //!
        const CPubKey& Key() const;

        //!
        //! \brief Get the string representation of the public key.
        //!
        //! \return Hex string representation of the bytes in the key.
        //!
        std::string ToString() const;

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(m_key);
        }

    private:
        CPubKey m_key; //!< The wrapped public key.
    }; // Contract::PublicKey

    //!
    //! \brief Version number of the serialized contract format.
    //!
    //! Defaults to the most recent version for a new contract instance.
    //!
    //! Version 1: Legacy string XML-like contract messages parsed from the
    //! \c hashBoinc field of a transaction object. It contains a signature
    //! and public key.
    //!
    //! Version 2: Contract data serializable in binary format. Stored in a
    //! transaction's \c vContracts field. It excludes the legacy signature
    //! and public key from version 1.
    //!
    int m_version = CURRENT_VERSION;

    Type m_type;            //!< Determines how to handle the contract.
    Action m_action;        //!< Action to perform with the contract.
    std::string m_key;      //!< Uniquely identifies the contract subject.
    std::string m_value;    //!< Body containing any specialized data.
    Signature m_signature;  //!< Proves authenticity of the contract.
    PublicKey m_public_key; //!< Verifies the contract signature.
    int64_t m_tx_timestamp; //!< Timestamp of the contract's transaction.

    //!
    //! \brief Initialize an empty \c Contract object.
    //!
    //! The new contract is invalid until populated and signed.
    //!
    Contract();

    //!
    //! \brief Initialize a new, unsigned \c Contract object to publish in a
    //! transaction.
    //!
    //! \param type   The type of contract to publish.
    //! \param action The action of the contract to publish.
    //! \param key    Uniquely identifies the contract target.
    //! \param value  Data specialized for the contract type.
    //!
    Contract(Type type, Action action, std::string key, std::string value);

    //!
    //! \brief Initialize a \c Contract object by supplying each of the fields
    //! from a contract message.
    //!
    //! \param version      Version of the serialized contract format.
    //! \param type         Contract type parsed from the transaction message.
    //! \param action       Contract action parsed from the transaction message.
    //! \param key          The contract key as it exists in the transaction.
    //! \param value        The contract value as it exists in the transaction.
    //! \param signature    Proves authenticity of the contract message.
    //! \param public_key   Optional for some types. Verifies the signature.
    //! \param tx_timestamp Timestamp of the transaction containing the contract.
    //!
    Contract(
        int version,
        Type type,
        Action action,
        std::string key,
        std::string value,
        Signature signature,
        PublicKey public_key,
        int64_t tx_timestamp);

    //!
    //! \brief Get the master public key used to verify administrative contracts.
    //!
    //! \return A \c CPubKey object containing the master public key.
    //!
    static const CPubKey& MasterPublicKey();

    //!
    //! \brief Get the master private key provided by configuration or command-
    //! line option.
    //!
    //! \return An empty key (vector) when no master key configured.
    //!
    static const CPrivKey MasterPrivateKey();

    //!
    //! \brief Get the output address controlled by the master private key.
    //!
    //! \return Address as calculated from the master public key.
    //!
    static const CBitcoinAddress MasterAddress();

    //!
    //! \brief Get the message public key used to verify public contracts.
    //!
    //! \return A \c CPubKey object containing the message public key.
    //!
    static const CPubKey& MessagePublicKey();

    //!
    //! \brief Get the message private key used to sign public contracts.
    //!
    //! The private key is revealed by design, for public messages only.
    //!
    //! \return The message private key as a secure vector of bytes.
    //!
    static const CPrivKey& MessagePrivateKey();

    //!
    //! \brief Get the destination burn address for transactions that contain
    //! contract messages.
    //!
    //! \return Burn address for transactions that contain contract messages.
    //!
    static const std::string BurnAddress();

    //!
    //! \brief Determine whether the supplied message might contain a contract.
    //!
    //! Call \c Contract::WellFormed() or \c Contract::VerifySignature() to
    //! check whether a contract is actually viable.
    //!
    //! \param message A message as it exists in a transaction.
    //!
    //! \return \c true if the message appears to contain a contract.
    //!
    static bool Detect(const std::string& message);

    //!
    //! \brief Create a contract instance by parsing the supplied message.
    //!
    //! \param message   Extracted from the \c hashboinc field of a transaction.
    //! \param timestamp Timestamp of the transaction containing the contract.
    //!
    //! \return The message parsed into a \c Contract instance.
    //!
    static Contract Parse(const std::string& message, const int64_t timestamp);

    //!
    //! \brief Determine whether the contract shall sign the message or verify
    //! the signature using the administrative master keys.
    //!
    //! \return \c true for administrative contract type/action pairs.
    //!
    bool RequiresMasterKey() const;

    //!
    //! \brief Determine whether the contract shall sign the message or verify
    //! the signature using the embedded, shared message keys.
    //!
    //! \return \c true for certain public actions (add poll, vote, beacon...).
    //!
    bool RequiresMessageKey() const;

    //!
    //! \brief Determine whether the contract shall sign the message or verify
    //! the signature using a special (non-user-supplied) key.
    //!
    //! \return \c true when a contract requires the master or message keys.
    //!
    bool RequiresSpecialKey() const;

    //!
    //! \brief Get the public key used to verify the contract's signature.
    //!
    //! \return The appropriate public key for the contract type.
    //!
    const CPubKey& ResolvePublicKey() const;

    //!
    //! \brief Determine whether the instance represents a complete contract.
    //!
    //! The results of these method calls do NOT guarantee that a contract is
    //! valid for the specific Proof-of-Research component that handles it. A
    //! return value of \c true only indicates that the contract includes all
    //! of the pieces of data needed for a well-formed contract message.
    //!
    //! \return \c true if the contract contains each of the required elements.
    //!
    bool WellFormed() const;

    //!
    //! \brief Determine whether a received contract is completely valid.
    //!
    //! \return \c true if the contract is well-formed and contains a valid
    //! signature.
    //!
    bool Validate() const;

    //!
    //! \brief Sign the contract using the provided private key.
    //!
    //! \param private_key The key to sign the message with.
    //!
    //! \return \c true if the signature was successfully created.
    //!
    bool Sign(CKey& private_key);

    //!
    //! \brief Sign the contract using the shared message private key.
    //!
    //! \return \c true if the signature was successfully created.
    //!
    bool SignWithMessageKey();

    //!
    //! \brief Validate the integrity and authenticity of the contract message
    //! by verifying its digital signature.
    //!
    //! \return \c true if the signature validates the contract's claims.
    //!
    bool VerifySignature() const;

    //!
    //! \brief Generate a hash of the contract data as the input to create or
    //! verify the contract signature.
    //!
    //! \return Hash of the contract type, key, and value. Versions 2+ also
    //! include the action.
    //!
    uint256 GetHash() const;

    //!
    //! \brief Get the string representation of the contract.
    //!
    //! \return The contract string in an XML-like format as published in a
    //! transaction message.
    //!
    std::string ToString() const;

    //!
    //! \brief Write a message to the debug log with the contract data.
    //!
    //! \param prefix Message to prepend to the log entry before the contract
    //! data elements.
    //!
    void Log(const std::string& prefix) const;

    //
    // Serialize and deserialize the contract in binary format instead of
    // parsing and formatting the legacy XML-like string representation.
    //
    // For CTransaction::nVersion >= 2.
    //
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(m_version);
        }

        READWRITE(m_type);
        READWRITE(m_action);
        READWRITE(m_key);
        READWRITE(m_value);
    }
}; // Contract

//!
//! \brief Stores or processes neural network contract messages.
//!
//! Typically, contract handler implementations only mutate the data they store
//! when the application processes contract messages embedded in a transaction.
//! Consumers of the data will usually access it in a read-only fashion because
//! the data is based on immutable values stored in connected blocks.
//!
//! For this reason, contract handlers NEED NOT to implement the interface in a
//! thread-safe manner. The application applies contract messages from only one
//! thread. However, a contract handler MUST guarantee thread-safety for any of
//! its methods that provide the read-only access to the contract data imported
//! by this interface.
//!
struct IContractHandler
{
    //!
    //! \brief Destructor.
    //!
    virtual ~IContractHandler() {}

    //!
    //! \brief Handle an contract addition.
    //!
    //! \param contract A contract message that describes the addition.
    //!
    virtual void Add(const Contract& contract) = 0;

    //!
    //! \brief Handle a contract deletion.
    //!
    //! \param contract A contract message that describes the deletion.
    //!
    virtual void Delete(const Contract& contract) = 0;

    //!
    //! \brief Revert a contract found in a disconnected block.
    //!
    //! The application calls this method for each contract in the disconnected
    //! blocks during chain reorganization. The default implementation forwards
    //! the contract object to an appropriate \c Add() or \c Delete() method by
    //! reversing the action specified in the contract.
    //!
    //! \param contract A contract message that describes the action to revert.
    //!
    virtual void Revert(const Contract& contract);
};

//!
//! \brief Apply a contract from a transaction message by passing it to the
//! appropriate contract handler.
//!
//! \param contract Received in a transaction message.
//!
void ProcessContract(const Contract& contract);

//!
//! \brief Revert a previously-applied contract from a transaction message by
//! passing it to the appropriate contract handler.
//!
//! \param contract Received in a transaction message.
//!
void RevertContract(const Contract& contract);
}
