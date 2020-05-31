#pragma once

#include "streams.h"

#include <memory>
#include <string>

class CHashWriter;
class CSizeComputer;

//!
//! \brief Add the default serialization methods for a contract payload.
//!
//! Because the IContractPayload interface declares these methods, we cannot
//! use the general-purpose ADD_SERIALIZE_METHODS macro from serialize.h.
//!
#define ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS                                       \
    void Serialize(CAutoFile& s, const NN::ContractAction action) const override     \
    {                                                                                \
        NCONST_PTR(this)->SerializationOp(s, CSerActionSerialize(), action);         \
    }                                                                                \
    void Unserialize(CAutoFile& s, const NN::ContractAction action) override         \
    {                                                                                \
        SerializationOp(s, CSerActionUnserialize(), action);                         \
    }                                                                                \
    void Serialize(CDataStream& s, const NN::ContractAction action) const override   \
    {                                                                                \
        NCONST_PTR(this)->SerializationOp(s, CSerActionSerialize(), action);         \
    }                                                                                \
    void Unserialize(CDataStream& s, const NN::ContractAction action) override       \
    {                                                                                \
        SerializationOp(s, CSerActionUnserialize(), action);                         \
    }                                                                                \
    void Serialize(CSizeComputer& s, const NN::ContractAction action) const override \
    {                                                                                \
        NCONST_PTR(this)->SerializationOp(s, CSerActionSerialize(), action);         \
    }                                                                                \
    void Serialize(CHashWriter& s, const NN::ContractAction action) const override   \
    {                                                                                \
        NCONST_PTR(this)->SerializationOp(s, CSerActionSerialize(), action);         \
    }

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
    VOTE       = 0x06, //!< A vote cast by a wallet for a poll.
    MAX_VALUE  = 0x06, //!< Increment this when adding items to the enum.
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
//! \brief Represents an object that contains the payload body of a contract.
//!
class IContractPayload
{
public:
    //!
    //! \brief Destructor.
    //!
    virtual ~IContractPayload() { }

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    virtual NN::ContractType ContractType() const = 0;

    //!
    //! \brief Determine whether the object contains a well-formed payload.
    //!
    //! The result of this method call does NOT guarantee that the payload
    //! is valid--some of the contract types require additional validation
    //! or context. A return value of \c true only indicates that payloads
    //! contain necessary data as a preliminary check.
    //!
    //! \param action The action declared for the contract that contains the
    //! payload. It may determine how to validate the payload.
    //!
    //! \return \c true if the payload is complete.
    //!
    virtual bool WellFormed(const ContractAction action) const = 0;

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    virtual std::string LegacyKeyString() const = 0;

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    virtual std::string LegacyValueString() const = 0;

    //!
    //! \brief Get the burn fee amount required to send a particular contract.
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    virtual int64_t RequiredBurnAmount() const = 0;

    //!
    //! \brief Serialize the contract to the provided file.
    //!
    virtual void Serialize(CAutoFile& s, const ContractAction action) const = 0;

    //!
    //! \brief Deserialize a contract from the provided file.
    //!
    virtual void Unserialize(CAutoFile& s, const ContractAction action) = 0;

    //!
    //! \brief Serialize the contract to the provided stream.
    //!
    virtual void Serialize(CDataStream& s, const ContractAction action) const = 0;

    //!
    //! \brief Deserialize a contract from the provided stream.
    //!
    virtual void Unserialize(CDataStream& s, const ContractAction action) = 0;

    //!
    //! \brief Write the contract data to a hasher.
    //!
    virtual void Serialize(CHashWriter& s, const ContractAction action) const = 0;

    //!
    //! \brief Write the contract data to a size computer.
    //!
    virtual void Serialize(CSizeComputer& s, const ContractAction action) const = 0;
};

//!
//! \brief A smart pointer with shared ownership semantics that wraps an object
//! that implements the IContractPayload interface.
//!
class ContractPayload
{
public:
    //!
    //! \brief Wrap the provided contract payload object.
    //!
    ContractPayload(std::shared_ptr<IContractPayload> payload)
        : m_payload(std::move(payload))
    {
    }

    //!
    //! \brief Construct a contract payload of the specified type.
    //!
    //! \tparam PayloadType The IContractPayload implementation to construct.
    //!
    //! \param args Arguments to pass to the constructor of the contract payload.
    //!
    //! \return A wrapped contract payload of the specified type.
    //!
    template <typename PayloadType, typename... Args>
    static ContractPayload Make(Args&&... args)
    {
        static_assert(
            std::is_base_of<IContractPayload, PayloadType>::value,
            "ContractPayload::As<T>: T not derived from IContractPayload.");

        return ContractPayload(
            std::make_shared<PayloadType>(std::forward<Args>(args)...));
    }

    const IContractPayload& operator*() const noexcept { return *m_payload; }
    IContractPayload& operator*() noexcept { return *m_payload; }
    const IContractPayload* operator->() const noexcept { return m_payload.get(); }
    IContractPayload* operator->() noexcept { return m_payload.get(); }

    //!
    //! \brief Cast a wrapped contract payload as a reference to the specified
    //! type.
    //!
    //! \tparam PayloadType Type of the wrapped IContractPayload implementation.
    //!
    template <typename PayloadType>
    const PayloadType& As() const
    {
        static_assert(
            std::is_base_of<IContractPayload, PayloadType>::value,
            "ContractPayload::As<T>: T not derived from IContractPayload.");

        // We use static_cast here instead of dynamic_cast to avoid the lookup.
        // Since only handlers for a particular contract type should access the
        // the payload, the derived type is known at the casting site.
        //
        return static_cast<const PayloadType&>(*m_payload);
    }

    //!
    //! \brief Replace the wrapped contract payload object.
    //!
    //! \tparam PayloadType The IContractPayload implementation to replace with.
    //!
    //! \param payload A contract payload object.
    //!
    template <typename PayloadType>
    void Reset(PayloadType* payload)
    {
        static_assert(
            std::is_base_of<IContractPayload, PayloadType>::value,
            "ContractPayload::As<T>: T not derived from IContractPayload.");

        m_payload.reset(payload);
    }

private:
    std::shared_ptr<IContractPayload> m_payload; //!< The wrapped payload.
};

//!
//! \brief A smart pointer with shared ownership semantics that provides read-
//! only access that an object that implements the IContractPayload interface.
//!
//! \tparam PayloadType Type of the wrapped IContractPayload implementation.
//!
template <typename PayloadType>
class ReadOnlyContractPayload
{
    static_assert(
        std::is_base_of<IContractPayload, PayloadType>::value,
        "ReadOnlyContractPayload<T>: T not derived from IContractPayload.");

public:
    //!
    //! \brief Wrap the provided contract payload object.
    //!
    ReadOnlyContractPayload(ContractPayload payload)
        : m_payload(std::move(payload))
    {
    }

    const PayloadType& operator*() const noexcept { return m_payload.As<PayloadType>(); }
    const PayloadType* operator->() const noexcept { return &m_payload.As<PayloadType>(); }

private:
    const ContractPayload m_payload; //!< The wrapped payload.
};
}
