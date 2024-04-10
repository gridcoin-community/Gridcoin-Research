// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SIDESTAKE_H
#define GRIDCOIN_SIDESTAKE_H

#include "base58.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "gridcoin/support/enumbytes.h"
#include "serialize.h"
#include "logging.h"

namespace GRC {

//!
//! \brief The Allocation class extends the Fraction class to provide functionality useful for sidestake allocations.
//!
class Allocation : public Fraction
{
public:
    //!
    //! \brief Default constructor. Creates a zero allocation fraction.
    //!
    Allocation();

    //!
    //! \brief Allocation constructor from a double input. This multiplies the double by 1000, rounds, casts to int64_t,
    //! and then constructs Fraction(x, 1000, true), which essentially creates a fraction representative of the double
    //! to the third decimal place.
    //!
    //! \param double allocation
    //!
    Allocation(const double& allocation);

    //!
    //! \brief Initialize an allocation from a Fraction. This is primarily used for casting. Note that no attempt to
    //! limit the denominator size or simplify the fraction is made.
    //!
    //! \param Fraction f
    //!
    Allocation(const Fraction& f);

    //!
    //! \brief Initialize an allocation directly from specifying a numerator and denominator
    //! \param numerator
    //! \param denominator
    //!
    Allocation(const int64_t& numerator, const int64_t& denominator);

    //!
    //! \brief Initialize an allocation directly from specifying a numerator and denominator, specifying the simplification
    //! directive.
    //!
    //! \param numerator
    //! \param denominator
    //! \param simplify
    //!
    Allocation(const int64_t& numerator, const int64_t& denominator, const bool& simplify);

    //!
    //! \brief Allocations extend the Fraction class and can also represent the result of the allocation constructed fraction
    //! and the result of the multiplication of that fraction times the reward, which is in CAmount (i.e. int64_t).
    //!
    //! \return CAmount of the Fraction representation of the actual allocation.
    //!
    CAmount ToCAmount() const;

    //!
    //! \brief Returns a double equivalent of the allocation fraction multiplied times 100.
    //!
    //! \return double percent representation of the allocation fraction.
    //!
    double ToPercent() const;

    Allocation operator+(const Allocation& rhs) const;
    Allocation operator+(const int64_t& rhs) const;
    Allocation operator-(const Allocation& rhs) const;
    Allocation operator-(const int64_t& rhs) const;
    Allocation operator*(const Allocation& rhs) const;
    Allocation operator*(const int64_t& rhs) const;
    Allocation operator/(const Allocation& rhs) const;
    Allocation operator/(const int64_t& rhs) const;
    Allocation operator+=(const Allocation& rhs);
    Allocation operator+=(const int64_t& rhs);
    Allocation operator-=(const Allocation& rhs);
    Allocation operator-=(const int64_t& rhs);
    Allocation operator*=(const Allocation& rhs);
    Allocation operator*=(const int64_t& rhs);
    Allocation operator/=(const Allocation& rhs);
    Allocation operator/=(const int64_t& rhs);
    bool operator==(const Allocation& rhs) const;
    bool operator!=(const Allocation& rhs) const;
    bool operator<=(const Allocation& rhs) const;
    bool operator>=(const Allocation& rhs) const;
    bool operator<(const Allocation& rhs) const;
    bool operator>(const Allocation& rhs) const;
    bool operator==(const int64_t& rhs) const;
    bool operator!=(const int64_t& rhs) const;
    bool operator<=(const int64_t& rhs) const;
    bool operator>=(const int64_t& rhs) const;
    bool operator<(const int64_t& rhs) const;
    bool operator>(const int64_t& rhs) const;
};

//!
//! \brief The LocalSideStake class. This class formalizes the local sidestake, which is a directive to apportion
//! a percentage of the total stake value to a designated destination. This destination must be valid, but
//! may or may not be owned by the staker. This is the primary mechanism to do automatic "donations" to
//! defined network addresses.
//!
//! Local (voluntary) entries will be picked up from the config file(s) and will be managed dynamically based on the
//! initial load of the config file + the r-w file + any changes in any GUI implementation on top of this.
//!
class LocalSideStake
{
public:
    enum class LocalSideStakeStatus
    {
        UNKNOWN,
        ACTIVE,         //!< A user specified sidestake that is active
        INACTIVE,       //!< A user specified sidestake that is inactive
        OUT_OF_BOUND
    };

    //!
    //! \brief Wrapped Enumeration of sidestake entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<LocalSideStakeStatus>;

    CTxDestination m_destination; //!< The destination of the sidestake.

    Allocation m_allocation;      //!< The allocation is a Fraction in the form x / 1000 where x is between 0 and 1000 inclusive.

    std::string m_description;    //!< The description of the sidestake (optional)

    Status m_status;              //!< The status of the sidestake. It is of type int instead of enum for serialization.


    //!
    //! \brief Initialize an empty, invalid sidestake instance.
    //!
    LocalSideStake();

    //!
    //! \brief Initialize a sidestake instance with the provided destination and allocation. This is used to construct a user
    //! specified sidestake.
    //!
    //! \param destination
    //! \param allocation
    //! \param description (optional)
    //!
    LocalSideStake(CTxDestination destination, Allocation allocation, std::string description);

    //!
    //! \brief Initialize a sidestake instance with the provided parameters.
    //!
    //! \param destination
    //! \param allocation
    //! \param description (optional)
    //! \param status
    //!
    LocalSideStake(CTxDestination destination, Allocation allocation, std::string description, LocalSideStakeStatus status);

    //!
    //! \brief Determine whether a sidestake contains each of the required elements.
    //! \return true if the sidestake is well-formed.
    //!
    bool WellFormed() const;

    //!
    //! \brief Returns the string representation of the current sidestake status
    //!
    //! \return Translated string representation of sidestake status
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the translated or untranslated string of the input sidestake status
    //!
    //! \param status. SideStake status
    //! \param translated. True for translated, false for not translated. Defaults to true.
    //!
    //! \return SideStake status string.
    //!
    std::string StatusToString(const LocalSideStakeStatus& status, const bool& translated = true) const;

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator==(LocalSideStake b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator!=(LocalSideStake b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_destination);
        READWRITE(m_allocation);
        READWRITE(m_description);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a local sidestake
//!
typedef std::shared_ptr<LocalSideStake> LocalSideStake_ptr;

//!
//! \brief The MandatorySideStake class. This class formalizes the mandatory sidestake, which is a directive to apportion
//! a percentage of the total stake value to a designated destination. This destination must be valid, but
//! may or may not be owned by the staker. This is the primary mechanism to do automatic "donations" to
//! defined network addresses.
//!
//! Mandatory entries will be picked up by contract handlers similar to other contract types (cf. protocol entries).
//!
class MandatorySideStake
{
public:
    enum class MandatorySideStakeStatus
    {
        UNKNOWN,
        DELETED,        //!< A mandatory sidestake that has been deleted by contract
        MANDATORY,      //!< An active mandatory sidetake by contract
        OUT_OF_BOUND
    };

    //!
    //! \brief Wrapped Enumeration of sidestake entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<MandatorySideStakeStatus>;

    CTxDestination m_destination; //!< The destination of the sidestake.

    Allocation m_allocation;      //!< The allocation is a Fraction in the form x / 1000 where x is between 0 and 1000 inclusive.

    std::string m_description;    //!< The description of the sidestake (optional)

    int64_t m_timestamp;          //!< Time of the sidestake contract transaction.

    uint256 m_hash;               //!< The hash of the transaction that contains a mandatory sidestake.

    uint256 m_previous_hash;      //!< The m_hash of the previous mandatory sidestake allocation with the same destination.

    Status m_status;              //!< The status of the sidestake. It is of type EnumByte instead of enum for serialization.

    //!
    //! \brief Initialize an empty, invalid sidestake instance.
    //!
    MandatorySideStake();

    //!
    //! \brief Initialize a sidestake instance with the provided destination and allocation. This is used to construct a user
    //! specified sidestake.
    //!
    //! \param destination
    //! \param allocation
    //! \param description (optional)
    //!
    MandatorySideStake(CTxDestination destination, Allocation allocation, std::string description);

    //!
    //! \brief Initialize a sidestake instance with the provided parameters.
    //!
    //! \param destination
    //! \param allocation
    //! \param description (optional)
    //! \param status
    //!
    MandatorySideStake(CTxDestination destination, Allocation allocation, std::string description, MandatorySideStakeStatus status);

    //!
    //! \brief Initialize a sidestake instance with the provided parameters. This form is normally used to construct a
    //! mandatory sidestake from a contract.
    //!
    //! \param destination
    //! \param allocation
    //! \param description (optional)
    //! \param timestamp
    //! \param hash
    //! \param status
    //!
    MandatorySideStake(CTxDestination destination, Allocation allocation, std::string description, int64_t timestamp,
              uint256 hash, MandatorySideStakeStatus status);

    //!
    //! \brief Determine whether a sidestake contains each of the required elements.
    //! \return true if the sidestake is well-formed.
    //!
    bool WellFormed() const;

    //!
    //! \brief This is the standardized method that returns the key value (in this case the destination) for the sidestake entry (for
    //! the registry_db.h template.)
    //!
    //! \return CTxDestination key value for the sidestake entry
    //!
    CTxDestination Key() const;

    //!
    //! \brief Provides the sidestake destination address and status as a pair of strings.
    //! \return std::pair of strings
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Returns the string representation of the current sidestake status
    //!
    //! \return Translated string representation of sidestake status
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the translated or untranslated string of the input sidestake status
    //!
    //! \param status. SideStake status
    //! \param translated. True for translated, false for not translated. Defaults to true.
    //!
    //! \return SideStake status string.
    //!
    std::string StatusToString(const MandatorySideStakeStatus& status, const bool& translated = true) const;

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator==(MandatorySideStake b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator!=(MandatorySideStake b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_destination);
        READWRITE(m_allocation);
        READWRITE(m_description);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a mandatory sidestake
//!
typedef std::shared_ptr<MandatorySideStake> MandatorySideStake_ptr;

//!
//! \brief This is a facade that combines the mandatory and local sidestake classes into one for use in the registry
//! and the GUI code.
//!
class SideStake
{
public:
    enum class Type {
        UNKNOWN,
        LOCAL,
        MANDATORY,
        OUT_OF_BOUND
    };

    enum FilterFlag : uint8_t {
        NONE      = 0b00,
        LOCAL     = 0b01,
        MANDATORY = 0b10,
        ALL       = 0b11,
    };

    //!
    //! \brief A variant to hold the two different types of sidestake status enums.
    //!
    typedef std::variant<MandatorySideStake::Status, LocalSideStake::Status> Status;

    SideStake();

    SideStake(LocalSideStake_ptr sidestake_ptr);

    SideStake(MandatorySideStake_ptr sidestake_ptr);

    //!
    //! \brief IsMandatory returns true if the sidestake is mandatory
    //! \return true or false
    //!
    bool IsMandatory() const;

    //!
    //! \brief Gets the destination of the sidestake
    //! \return CTxDestination of the sidestake
    //!
    CTxDestination GetDestination() const;
    //!
    //! \brief Gets the allocation of the sidestake
    //! \return A Fraction representing the allocation fraction of the sidestake.
    //!
    Allocation GetAllocation() const;
    //!
    //! \brief Gets the description of the sidestake
    //! \return The description string of the sidestake
    //!
    std::string GetDescription() const;
    //!
    //! \brief Gets the timestamp of the transaction that contains the mandatory sidestake contract (now entry)
    //! \return
    //!
    int64_t GetTimeStamp() const;
    //!
    //! \brief Gets the hash of the transaction that contains the mandatory sidestake contract (now entry)
    //! \return uint256 hash
    //!
    uint256 GetHash() const;
    //!
    //! \brief Gets the hash of the transaction that contains the previous mandatory sidestake contract for the same key (address)
    //! \return uint256 hash
    //!
    uint256 GetPreviousHash() const;
    //!
    //! \brief Gets a variant containing either the mandatory sidestake status or local sidestake status, whichever
    //! is applicable.
    //! \return std::variant of the applicable sidestake status
    //!
    Status GetStatus() const;
    //!
    //! \brief Gets the status string associated with the applicable sidestake status.
    //! \return String of the applicable sidestake status
    //!
    std::string StatusToString() const;

private:
    //!
    //! \brief m_local_sidestake_ptr that points to the local sidestake object if this sidestake object is local;
    //! nullptr otherwise.
    //!
    LocalSideStake_ptr m_local_sidestake_ptr;
    //!
    //! \brief m_mandatory_sidestake_ptr that points to the mandatory sidestake object if this sidestake object is mandatory;
    //! nullptr otherwise.
    //!
    MandatorySideStake_ptr m_mandatory_sidestake_ptr;
    //!
    //! \brief m_type holds the type of the sidestake, either mandatory or local.
    //!
    Type m_type;
};

//!
//! \brief The type that defines a shared pointer to a sidestake. This is the facade and in turn will point to either a
//! mandatory or local sidestake as applicable.
//!
typedef std::shared_ptr<SideStake> SideStake_ptr;

//!
//! \brief The body of a sidestake entry contract. This payload does NOT support
//! legacy payload formatting, as this contract/payload type is introduced after
//! legacy payloads are retired.
//!
class SideStakePayload : public IContractPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized sidestake entry.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

    //!
    //! \brief Version number of the serialized sidestake entry format.
    //!
    //! Version 1: Initial version:
    //!
    uint32_t m_version = CURRENT_VERSION;

    MandatorySideStake m_entry; //!< The sidestake entry in the payload.

    //!
    //! \brief Initialize an empty, invalid sidestake entry payload.
    //!
    SideStakePayload(uint32_t version = CURRENT_VERSION);

    //!
    //! \brief Initialize a sidestakeEntryPayload from a sidestake destination, allocation,
    //! description, and status.
    //!
    //! \param destination. Destination for the sidestake entry
    //! \param allocation. Allocation for the sidestake entry
    //! \param description. Description string for the sidstake entry
    //! \param status. Status of the sidestake entry
    //!
    SideStakePayload(const uint32_t version, CTxDestination destination, Allocation allocation,
                     std::string description, MandatorySideStake::MandatorySideStakeStatus status);

    //!
    //! \brief Initialize a sidestake entry payload from the given sidestake entry
    //! with the provided version number (and format).
    //!
    //! \param version Version of the serialized sidestake entry format.
    //! \param sidestake_entry The sidestake entry itself.
    //!
    SideStakePayload(const uint32_t version, MandatorySideStake sidestake_entry);

    //!
    //! \brief Initialize a sidestake entry payload from the given sidestake entry
    //! with the CURRENT_VERSION.
    //!
    //! \param sidestake_entry The sidestake entry itself.
    //!
    SideStakePayload(MandatorySideStake sidestake_entry);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::SIDESTAKE;
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        bool valid = !(m_version <= 0 || m_version > CURRENT_VERSION);

        if (!valid) {
            LogPrint(BCLog::LogFlags::CONTRACT, "WARN: %s: Payload is not well formed. "
                                                "m_version = %u, CURRENT_VERSION = %u",
                     __func__,
                     m_version,
                     CURRENT_VERSION);

            return false;
        }

        valid = m_entry.WellFormed();

        if (!valid) {
            LogPrint(BCLog::LogFlags::CONTRACT, "WARN: %s: Sidestake entry is not well-formed. "
                                                "m_entry.WellFormed = %u, m_entry.m_key = %s, m_entry.m_allocation = %f, "
                                                "m_entry.StatusToString() = %s",
                     __func__,
                     valid,
                     CBitcoinAddress(m_entry.m_destination).ToString(),
                     m_entry.m_allocation.ToPercent(),
                     m_entry.StatusToString()
                     );

            return false;
        }

        return valid;
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return CBitcoinAddress(m_entry.m_destination).ToString();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return ToString(m_entry.m_allocation.ToDouble());
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract. This
    //! is the same as the LegacyPayload to insure compatibility between the sidestake
    //! registry and non-upgraded nodes before the block v13/contract version 3 height
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const override
    {
        return Contract::STANDARD_BURN_AMOUNT;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_entry);
    }
}; // SideStakePayload

//!
//! \brief Stores and manages sidestake entries. Note that the mandatory sidestakes are stored in leveldb using
//! the registry db template. The local sidestakes are maintained in sync with the read-write gridcoinsettings.json file.
//!
class SideStakeRegistry : public IContractHandler
{
public:
    //!
    //! \brief sidestakeRegistry constructor. The parameter is the version number of the underlying
    //! sidestake entry db. This must be incremented when implementing format changes to the sidestake
    //! entries to force a reinit.
    //!
    //! Version 1: 5.4.5.5+
    //!
    SideStakeRegistry()
        : m_sidestake_db(1)
          {
          };

    //!
    //! \brief The type that keys local sidestake entries by their destinations.
    //!
    typedef std::map<CTxDestination, LocalSideStake_ptr> LocalSideStakeMap;

    //!
    //! \brief The type that keys mandatory sidestake entries by their destinations. Note that the entries
    //! in this map are actually smart shared pointer wrappers, so that the same actual object
    //! can be held by both this map and the historical map without object duplication.
    //!
    typedef std::map<CTxDestination, MandatorySideStake_ptr> MandatorySideStakeMap;

    //!
    //! \brief PendingSideStakeMap. This is not actually used but defined to satisfy the template.
    //!
    typedef MandatorySideStakeMap PendingSideStakeMap;

    //!
    //! \brief The type that keys historical sidestake entries by the contract hash (txid).
    //! Note that the entries in this map are actually smart shared pointer wrappers, so that
    //! the same actual object can be held by both this map and the (current) sidestake entry map
    //! without object duplication.
    //!
    typedef std::map<uint256, MandatorySideStake_ptr> HistoricalSideStakeMap;

    //!
    //! \brief Get the collection of current sidestake entries. Note that this INCLUDES deleted
    //! sidestake entries.
    //!
    //! \return \c A reference to the current sidestake entries stored in the registry.
    //!
    const std::vector<SideStake_ptr> SideStakeEntries() const;

    //!
    //! \brief Get the collection of active sidestake entries. This is presented as a vector of
    //! smart pointers to the relevant sidestake entries in the database. The entries included have
    //! the status of active (for local sidestakes) and/or mandatory (for contract sidestakes).
    //! Mandatory sidestakes come before local ones, and the method ensures that the mandatory sidestakes
    //! returned do not total an allocation greater than MaxMandatorySideStakeTotalAlloc, and all of the
    //! sidestakes combined do not total an allocation greater than 1.0.
    //!
    //! \param bitmask filter to return mandatory only, local only, or all
    //!
    //! \return A vector of smart pointers to sidestake entries.
    //!
    const std::vector<SideStake_ptr> ActiveSideStakeEntries(const SideStake::FilterFlag& filter, const bool& include_zero_alloc) const;

    //!
    //! \brief Get the current sidestake entry for the specified destination.
    //!
    //! \param key The destination of the sidestake entry.
    //! \param bitmask filter to try mandatory only, local only, or all
    //!
    //! \return A vector of smart pointers to entries matching the provided destination. Up to two elements
    //! are returned, mandatory entry first, depending on the filter set.
    //!
    std::vector<SideStake_ptr> Try(const CTxDestination& key, const SideStake::FilterFlag& filter) const;

    //!
    //! \brief Get the current sidestake entry for the specified destination if it has a status of ACTIVE or MANDATORY.
    //!
    //! \param key The destination of the sidestake entry.
    //! \param bitmask filter to try mandatory only, local only, or all
    //!
    //! \return A vector of smart pointers to entries matching the provided destination that are in status of
    //! MANDATORY or ACTIVE. Up to two elements are returned, mandatory entry first,, depending on the filter set.
    //!
    std::vector<SideStake_ptr> TryActive(const CTxDestination& key, const SideStake::FilterFlag& filter) const;

    //!
    //! \brief Destroy the contract handler state in case of an error in loading
    //! the sidestake entry registry state from LevelDB to prepare for reload from contract
    //! replay. This is not used for sidestake entries, unless -clearsidestakehistory is specified
    //! as a startup argument, because contract replay storage and full reversion has
    //! been implemented for sidestake entries.
    //!
    void Reset() override;

    //!
    //! \brief Determine whether a sidestake entry contract is valid.
    //!
    //! \param contract Contains the sidestake entry contract to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior out.
    //!
    //! \return \c true if the contract contains a valid sidestake entry.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override;

    //!
    //! \brief Determine whether a sidestake entry contract is valid including block context. This is used
    //! in ConnectBlock. Note that for sidestake entries this simply calls Validate as there is no
    //! block level specific validation to be done at the current time.
    //!
    //! \param ctx ContractContext containing the sidestake entry data to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return  \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    //!
    //! \brief Add a sidestake entry to the registry from contract data. For the sidestake registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //!
    //! \param ctx
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Mark a sidestake entry deleted in the registry from contract data. For the sidestake registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //! \param ctx
    //!
    void Delete(const ContractContext& ctx) override;

    //!
    //! \brief Allows local (voluntary) sidestakes to be added to the in-memory local map and not persisted to
    //! the registry db.
    //!
    //! \param SideStake object to add
    //! \param bool save_to_file if true causes SaveLocalSideStakesToConfig() to be called.
    //!
    void NonContractAdd(const LocalSideStake& sidestake, const bool& save_to_file = true);

    //!
    //! \brief Provides for deletion of local (voluntary) sidestakes from the in-memory local map that are not persisted
    //! to the registry db. Deletion is by the map key (CTxDestination).
    //!
    //! \param destination
    //! \param bool save_to_file if true causes SaveLocalSideStakesToConfig() to be called.
    //!
    void NonContractDelete(const CTxDestination& destination, const bool& save_to_file = true);

    //!
    //! \brief Revert the registry state for the sidestake entry to the state prior
    //! to this ContractContext application. This is typically used
    //! during reorganizations, where blocks are disconnected.
    //!
    //! \param ctx References the sidestake entry contract and associated context.
    //!
    void Revert(const ContractContext& ctx) override;

    //!
    //! \brief Initialize the sidestakeRegistry, which now includes restoring the state of the sidestakeRegistry from
    //! LevelDB on wallet start.
    //!
    //! \return Block height of the database restored from LevelDB. Zero if no LevelDB sidestake entry data is found or
    //! there is some issue in LevelDB sidestake entry retrieval. (This will cause the contract replay to change scope
    //! and initialize the sidestakeRegistry from contract replay and store in LevelDB.)
    //!
    int Initialize() override;

    //!
    //! \brief Gets the block height through which is stored in the sidestake entry registry database.
    //!
    //! \return block height.
    //!
    int GetDBHeight() override;

    //!
    //! \brief Function normally only used after a series of reverts during block disconnects, because
    //! block disconnects are done in groups back to a common ancestor, and will include a series of reverts.
    //! This is essentially atomic, and therefore the final (common) height only needs to be set once. TODO:
    //! reversion should be done with a vector argument of the contract contexts, along with a final height to
    //! clean this up and move the logic to here from the calling function.
    //!
    //! \param height to set the storage DB bookmark.
    //!
    void SetDBHeight(int& height) override;

    //!
    //! \brief Resets the maps in the sidestakeRegistry but does not disturb the underlying LevelDB
    //! storage. This is only used during testing in the testing harness.
    //!
    void ResetInMemoryOnly();

    //!
    //! \brief Passivates the elements in the sidestake db, which means remove from memory elements in the
    //! historical map that are not referenced by the active entry map. The backing store of the element removed
    //! from memory is retained and will be transparently restored if find() is called on the hash key
    //! for the element.
    //!
    //! \return The number of elements passivated.
    //!
    uint64_t PassivateDB() override;

    //!
    //! \brief This method parses the config file for local sidestakes. It is based on the original GetSideStakingStatusAndAlloc()
    //! that was in miner.cpp prior to the implementation of the SideStake class.
    //!
    void LoadLocalSideStakesFromConfig();

    //!
    //! \brief Specializes the template RegistryDB for the SideStake class. Note that std::set<MandatorySideStake>
    //! is not actually used.
    //!
    typedef RegistryDB<MandatorySideStake,
                       MandatorySideStake,
                       MandatorySideStake::MandatorySideStakeStatus,
                       MandatorySideStakeMap,
                       PendingSideStakeMap,
                       std::set<SideStake>,
                       HistoricalSideStakeMap> SideStakeDB;

private:
    //!
    //! \brief Protects the registry with multithreaded access. This is implemented INTERNAL to the registry class.
    //!
    mutable CCriticalSection cs_lock;

    //!
    //! \brief Private helper method for the Add and Delete methods above. They both use identical code (with
    //! different input statuses).
    //!
    //! \param ctx The contract context for the add or delete.
    //!
    void AddDelete(const ContractContext& ctx);

    //!
    //! \brief Private helper function for non-contract add and delete to align the config r-w file with
    //! in memory local sidestake map.
    //!
    //! \return bool true if successful.
    //!
    bool SaveLocalSideStakesToConfig();

    //!
    //! \brief Provides the total allocation for all active mandatory sidestakes as a Fraction.
    //! \return total active mandatory sidestake allocation as a Fraction.
    //!
    Allocation GetMandatoryAllocationsTotal() const;

    void SubscribeToCoreSignals();
    void UnsubscribeFromCoreSignals();

    LocalSideStakeMap m_local_sidestake_entries;          //!< Contains the local (non-contract) sidestake entries.
    MandatorySideStakeMap m_mandatory_sidestake_entries;  //!< Contains the mandatory sidestake entries, including DELETED.
    PendingSideStakeMap m_pending_sidestake_entries {};   //!< Not used. Only to satisfy the template.

    std::set<SideStake> m_expired_sidestake_entries {};   //!< Not used. Only to satisfy the template.

    SideStakeDB m_sidestake_db;                           //!< The internal sidestake db object for leveldb persistence.

    bool m_local_entry_already_saved_to_config = false;   //!< Flag to prevent reload on signal if individual entry saved already.

public:

    SideStakeDB& GetSideStakeDB();
}; // sidestakeRegistry

//!
//! \brief Get the global sidestake entry registry.
//!
//! \return Current global sidestake entry registry instance.
//!
SideStakeRegistry& GetSideStakeRegistry();
} // namespace GRC

#endif // GRIDCOIN_SIDESTAKE_H
