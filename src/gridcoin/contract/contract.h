// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/support/enumbytes.h"
#include "serialize.h"

#include <boost/optional.hpp>
#include <string>
#include <vector>

class CBlock;
class CBlockIndex;
class CTransaction;

namespace GRC {
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
public:
    //!
    //! \brief Version number of the current format for a serialized contract.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief The amount of coin set for a burn output in a transaction that
    //! broadcasts a contract in units of 1/100000000 GRC.
    //!
    static constexpr CAmount STANDARD_BURN_AMOUNT = 0.5 * COIN;

    //!
    //! \brief A contract type from a transaction message.
    //!
    struct Type : public EnumByte<ContractType>
    {
        //!
        //! \brief Initialize an instance for a \c ContractType value.
        //!
        //! \param type A value enumerated on \c ContractType.
        //!
        Type(ContractType type);

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
        //! \return The string as it would appear in a legacy transaction message.
        //!
        std::string ToString() const;
    }; // Contract::Type

    //!
    //! \brief A contract action from a transaction message.
    //!
    struct Action : public EnumByte<ContractAction>
    {
        //!
        //! \brief Initialize an instance for a \c ContractAction value.
        //!
        //! \param type A value enumerated on \c ContractAction.
        //!
        Action(ContractAction action);

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
        std::string ToString() const;
    }; // Contract::Action

    //!
    //! \brief Contains the type-specific payload body of a contract.
    //!
    struct Body
    {
        friend class Contract;

        //!
        //! \brief Initialize an empty body payload.
        //!
        Body();

        //!
        //! \brief Initialize a contract body from the supplied payload.
        //!
        //! \param payload Contract data specific to the contract type.
        //!
        Body(ContractPayload payload);

        //!
        //! \brief Determine whether the object contains a well-formed payload.
        //!
        //! The result of this method call does NOT guarantee that the payload
        //! is valid--some of the contract types require additional validation
        //! or context. A return value of \c true only indicates that payloads
        //! contain necessary data as a preliminary check.
        //!
        //! \param action The action declared for the contract that contains the
        //! payload. It may control how to validate the payload.
        //!
        //! \return \c true if the payload is complete.
        //!
        bool WellFormed(const ContractAction action) const;

        //!
        //! \brief Get the wrapped contract payload for code that knows that a
        //! contract contains a legacy string payload.
        //!
        //! Version 1 contracts always contain a legacy payload object. This
        //! method produces a payload for code that works directly with that
        //! legacy format. It becomes unnecessary after we introduce payload
        //! data types for each of the legacy contract types.
        //!
        //! \return The wrapped legacy payload body.
        //!
        ContractPayload AssumeLegacy() const;

        //!
        //! \brief Get a typed contract payload for code that knows that a
        //! contract contains a legacy string payload.
        //!
        //! Version 1 contracts always contain a legacy payload object. This
        //! method parses the legacy payload into an IContractPayload object
        //! that matches the contract type.
        //!
        //! \param type Determines the type to convert a legacy payload into.
        //!
        //! \return An IContractPayload implementation for the specified type.
        //!
        ContractPayload ConvertFromLegacy(const ContractType type) const;

        //!
        //! \brief Serialize the object to the provided stream.
        //!
        //! \param stream The output stream.
        //! \param action May control how to serialize the payload.
        //!
        template<typename Stream>
        void Serialize(Stream& stream, const ContractAction action) const
        {
            m_payload->Serialize(stream, action);
        }

        //!
        //! \brief Deserialize the object from the provided stream.
        //!
        //! \param stream The input stream.
        //! \param action May control how to deserialize the payload.
        //!
        template<typename Stream>
        void Unserialize(Stream& stream, const ContractAction action)
        {
            m_payload->Unserialize(stream, action);
        }

    private:
        ContractPayload m_payload; //!< Data specific to the contract type.

        //!
        //! \brief Reinitialize the contract body with an IContractHandler
        //! object for the specified contract type.
        //!
        //! \param type Indicates which IContractHandler type to construct.
        //!
        void ResetType(const ContractType type);
    }; // Contract::Body

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
    uint32_t m_version = CURRENT_VERSION;

    Type m_type;            //!< Determines how to handle the contract.
    Action m_action;        //!< Action to perform with the contract.
    Body m_body;            //!< Payload specific to the contract type.

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
    //! \param type    The type of contract to publish.
    //! \param action  The action of the contract to publish.
    //! \param body    The body of the contract.
    //!
    Contract(Type type, Action action, Body body);

    //!
    //! \brief Initialize a \c Contract object by supplying each of the fields
    //! from a contract message.
    //!
    //! \param version      Version of the serialized contract format.
    //! \param type         Contract type parsed from the transaction message.
    //! \param action       Contract action parsed from the transaction message.
    //! \param body         The body payload of the contract.
    //!
    Contract(uint32_t version, Type type, Action action, Body body);

    //!
    //! \brief Determine whether the supplied message might contain a contract.
    //!
    //! Call \c Contract::WellFormed() to check whether a contract is actually
    //! viable.
    //!
    //! \param message A message as it exists in a transaction.
    //!
    //! \return \c true if the message appears to contain a contract.
    //!
    static bool Detect(const std::string& message);

    //!
    //! \brief Create a contract instance by parsing the supplied message.
    //!
    //! \param message Extracted from the \c hashboinc field of a transaction.
    //!
    //! \return The message parsed into a \c Contract instance.
    //!
    static Contract Parse(const std::string& message);

    //!
    //! \brief Determine whether the contract shall sign the message or verify
    //! the signature using the administrative master keys.
    //!
    //! \return \c true for administrative contract type/action pairs.
    //!
    bool RequiresMasterKey() const;

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const;

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
    //! \brief Get the wrapped contract payload object.
    //!
    //! WARNING: this method returns a smart pointer that shares ownership of
    //! the payload. Code that uses the value must keep this object until the
    //! code finishes using the data to avoid a dangling reference. When code
    //! chains calls onto the returned object without initializing a variable
    //! for the pointer, the automatic variable may be destroyed before calls
    //! complete.
    //!
    //! \return An IContractPayload object specific to the contract type. For
    //! legacy version 1 contracts, this converts a legacy payload as needed.
    //!
    ContractPayload SharePayload() const;

    //!
    //! \brief Get the wrapped contract payload object in the context of the
    //! specified payload type.
    //!
    //! The returned object uses \c static_cast when dereferencing the wrapped
    //! payload object to avoid dynamic lookup. Use only for contract handlers
    //! that know the type of the contract payload at compile time.
    //!
    //! \tparam PayloadType Type to cast or convert the payload into.
    //!
    //! \return An IContractPayload object specific to the contract type. For
    //! legacy version 1 contracts, this converts a legacy payload as needed.
    //!
    template <typename PayloadType>
    ReadOnlyContractPayload<PayloadType> SharePayloadAs() const
    {
        return ReadOnlyContractPayload<PayloadType>(SharePayload());
    }

    //!
    //! \brief Copy the contract payload as the specified type.
    //!
    //! \tparam PayloadType Type to cast or convert the payload into.
    //!
    //! \return A new contract payload object as the specified type.
    //!
    template <typename PayloadType>
    PayloadType CopyPayloadAs() const
    {
        static_assert(
            std::is_base_of<IContractPayload, PayloadType>::value,
            "Contract::CopyPayloadAs<T>: T not derived from IContractPayload.");

        // We use static_cast here instead of dynamic_cast to avoid the lookup.
        // Since only handlers for a particular contract type should access the
        // the payload, the derived type is known at the casting site.
        //
        return static_cast<const PayloadType&>(*SharePayload());
    }

    //!
    //! \brief Move the contract payload as the specified type.
    //!
    //! \tparam PayloadType Type to cast or convert the payload into.
    //!
    //! \return The wrapped contract payload object as the specified type.
    //!
    template <typename PayloadType>
    PayloadType PullPayloadAs()
    {
        static_assert(
            std::is_base_of<IContractPayload, PayloadType>::value,
            "Contract::PullPayloadAs<T>: T not derived from IContractPayload.");

        // We use static_cast here instead of dynamic_cast to avoid the lookup.
        // Since only handlers for a particular contract type should access the
        // the payload, the derived type is known at the casting site.
        //
        return std::move(static_cast<PayloadType&>(*SharePayload()));;
    }

    //!
    //! \brief Write a message to the debug log with the contract data.
    //!
    //! \param prefix Message to prepend to the log entry before the contract
    //! data elements.
    //!
    void Log(const std::string& prefix) const;

    //!
    //! \brief Serialize the object to the provided stream.
    //!
    //! For CTransaction::nVersion >= 2.
    //!
    //! \param stream The output stream.
    //!
    template<typename Stream>
    void Serialize(Stream& s) const
    {
        s << m_version;
        s << m_type;
        s << m_action;

        m_body.Serialize(s, m_action.Value());
    }

    //!
    //! \brief Deserialize the object from the provided stream.
    //!
    //! For CTransaction::nVersion >= 2.
    //!
    //! \param stream The input stream.
    //!
    template<typename Stream>
    void Unserialize(Stream& s)
    {
        s >> m_version;
        s >> m_type;
        s >> m_action;

        m_body.ResetType(m_type.Value());
        m_body.Unserialize(s, m_action.Value());
    }
}; // Contract

//!
//! \brief Initialize a new contract.
//!
//! \tparam PayloadType A type that implements the IContractPayload interface.
//!
//! \param action The action of the contract to publish.
//! \param body   Arguments to pass to the constructor of the contract payload.
//!
//! \return A contract object for submission in a transaction.
//!
template <typename PayloadType, typename... Args>
Contract MakeContract(const ContractAction action, Args&&... args)
{
    static_assert(
        std::is_base_of<IContractPayload, PayloadType>::value,
        "Contract::PullPayloadAs<T>: T not derived from IContractPayload.");

    auto payload = ContractPayload::Make<PayloadType>(std::forward<Args>(args)...);
    const ContractType type = payload->ContractType();

    return Contract(type, action, std::move(payload));
}

//!
//! \brief Initialize a new legacy contract.
//!
//! \param type   The type of contract to publish.
//! \param action The action of the contract to publish.
//! \param key    Legacy representation of a contract key.
//! \param value  Legacy representation of a contract value.
//!
//! \return A contract object for submission in a transaction.
//!
Contract MakeLegacyContract(
    const ContractType type,
    const ContractAction action,
    std::string key,
    std::string value);

//!
//! \brief Replay historical contract messages, nominally six months.
//!
//! \param pindex Block index to start with.
//!
void ReplayContracts(CBlockIndex *pindex_end, CBlockIndex *pindex_start = nullptr);

//!
//! \brief Apply contracts from transactions in a block by passing them to the
//! appropriate contract handlers.
//!
//! \param block            Block to extract contracts from.
//! \param pindex           Block index context for the block.
//! \param beacon_db_height Height that db is updated to prior to call
//! \param out_found        Will update to \c true when a block contains a contract.
//!
void ApplyContracts(const CBlock& block,
    const CBlockIndex* const pindex, const int& beacon_db_height,
    bool& out_found_contract);

//!
//! \brief Apply contracts from transactions by passing them to the appropriate
//! contract handlers.
//!
//! \param tx               Transaction to extract contracts from.
//! \param pindex           Block index for the block that contains the transaction.
//! \param beacon_db_height Height that db is updated to prior to call
//!
void ApplyContracts(
    const CTransaction& tx,
    const CBlockIndex* const pindex, const int& beacon_db_height,
    bool& out_found_contract);

//!
//! \brief Perform contextual validation for the contracts in a transaction.
//!
//! \param tx Transaction to validate contracts for.
//!
//! \return \c false When a contract in the transaction fails validation.
//!
bool ValidateContracts(const CTransaction& tx);

//!
//! \brief Revert previously-applied contracts from a transaction by passing
//! them to the appropriate contract handlers.
//!
//! \param tx     Transaction that contains contracts to revert.
//! \param pindex Block index for the block that contains the transaction.
//!
void RevertContracts(const CTransaction& tx, const CBlockIndex* const pindex);
}
