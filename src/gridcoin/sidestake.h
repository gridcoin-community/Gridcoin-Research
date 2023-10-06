// Copyright (c) 2014-2023 The Gridcoin developers
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

class CBitcoinAddressForStorage : public CBitcoinAddress
{
public:
    CBitcoinAddressForStorage();

    CBitcoinAddressForStorage(CBitcoinAddress address);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        // Note that (de)serializing the raw underlying vector char data for the address is safe here
        // because this is only used in this module and validations were performed before serialization into
        // storage.
        READWRITE(nVersion);
        READWRITE(vchData);
    }
};


enum class SideStakeStatus
{
    UNKNOWN,
    ACTIVE,         //!< A user specified sidestake that is active
    INACTIVE,       //!< A user specified sidestake that is inactive
    DELETED,        //!< A mandatory sidestake that has been deleted by contract
    MANDATORY,      //!< An active mandatory sidetake by contract
    OUT_OF_BOUND
};

//!
//! \brief The SideStake class. This class formalizes the "sidestake", which is a directive to apportion
//! a percentage of the total stake value to a designated address. This address must be a valid address, but
//! may or may not be owned by the staker. This is the primary mechanism to do automatic "donations" to
//! defined network addresses.
//!
//! The class supports two modes of operation. Local (voluntary) entries will be picked up from the config file(s)
//! and will be managed dynamically based on the initial load of the config file + the r-w file + any changes to
//! in any GUI implementation on top of this. Mandatory entries will be picked up by contract handlers similar to
//! other contract types (cf. protocol entries).
//!
class SideStake
{
public:
    //!
    //! \brief Wrapped Enumeration of sidestake entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<SideStakeStatus>;

    CBitcoinAddressForStorage m_key;    //!< The key here is the Gridcoin Address of the sidestake destination.

    double m_allocation;                //!< The allocation is a double precision floating point between 0.0 and 1.0 inclusive

    std::string m_description;            //!< The description of the sidestake (optional)

    int64_t m_timestamp;                //!< Time of the sidestake contract transaction.

    uint256 m_hash;                     //!< The hash of the transaction that contains a mandatory sidestake.

    uint256 m_previous_hash;            //!< The m_hash of the previous mandatory sidestake allocation with the same address.

    Status m_status;                    //!< The status of the sidestake. It is of type int instead of enum for serialization.

    //!
    //! \brief Initialize an empty, invalid sidestake instance.
    //!
    SideStake();

    //!
    //! \brief Initialize a sidestake instance with the provided address and allocation. This is used to construct a user
    //! specified sidestake.
    //!
    //! \param address
    //! \param allocation
    //! \param description (optional)
    //!
    SideStake(CBitcoinAddressForStorage address, double allocation, std::string description);

    //!
    //! \brief Initialize a sidestake instance with the provided parameters.
    //!
    //! \param address
    //! \param allocation
    //! \param description (optional)
    //! \param status
    //!
    SideStake(CBitcoinAddressForStorage address, double allocation, std::string description, SideStakeStatus status);

    //!
    //! \brief Initialize a sidestake instance with the provided parameters. This form is normally used to construct a
    //! mandatory sidestake from a contract.
    //!
    //! \param address
    //! \param allocation
    //! \param description (optional)
    //! \param timestamp
    //! \param hash
    //! \param status
    //!
    SideStake(CBitcoinAddressForStorage address, double allocation, std::string description, int64_t timestamp, uint256 hash, SideStakeStatus status);

    //!
    //! \brief Determine whether a sidestake contains each of the required elements.
    //! \return true if the sidestake is well-formed.
    //!
    bool WellFormed() const;

    //!
    //! \brief This is the standardized method that returns the key value for the sidestake entry (for
    //! the registry_db.h template.)
    //!
    //! \return CBitcoinAddress key value for the sidestake entry
    //!
    CBitcoinAddressForStorage Key() const;

    //!
    //! \brief Provides the sidestake address and status (value) as a pair of strings.
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
    std::string StatusToString(const SideStakeStatus& status, const bool& translated = true) const;

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!

    bool operator==(SideStake b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side sidestake to compare for equality.
    //!
    //! \return Equal or not.
    //!

    bool operator!=(SideStake b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_key);
        READWRITE(m_allocation);
        READWRITE(m_description);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a sidestake
//!
typedef std::shared_ptr<SideStake> SideStake_ptr;

//!
//! \brief A type that either points to some sidestake or does not.
//!
//typedef const SideStake_ptr SideStakeOption;

//!
//! \brief The body of a sidestake entry contract. Note that this body is bimodal. It
//! supports both the personality of the "LegacyPayload", and also the new native
//! sidestakeEntry format. In the Contract::Body::ConvertFromLegacy call, by the time
//! this call has been reached, the contract will have already been deserialized.
//! This will follow the legacy mode. For contracts at version 3+, the
//! Contract::SharePayload() will NOT call the ConvertFromLegacy. Note that because
//! the existing legacyPayloads are not versioned, the deserialization of
//! the payload first (de)serializes m_key, which is guaranteed to exist in either
//! legacy or native. If the key is empty, then payload v2+ is being deserialized
//! and the m_version and m_value are (de)serialized. This is ugly
//! but necessary to deal with the unversioned Legacy Payloads and maintain
//! compatibility.
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

    SideStake m_entry; //!< The sidestake entry in the payload.

    //!
    //! \brief Initialize an empty, invalid sidestake entry payload.
    //!
    SideStakePayload(uint32_t version = CURRENT_VERSION);

    //!
    //! \brief Initialize a sidestakeEntryPayload from a sidestake entry constructed from
    //! string key and value. Not to be used for version 1 payloads. Will assert. Does NOT
    //! initialize hash fields.
    //!
    //! \param key. Key string for the sidestake entry
    //! \param value. Value string for the sidestake entry
    //! \param status. Status of the sidestake entry
    //!
    SideStakePayload(const uint32_t version, CBitcoinAddressForStorage key, double value,
                     std::string description, SideStakeStatus status);

    //!
    //! \brief Initialize a sidestake entry payload from the given sidestake entry
    //! with the provided version number (and format).
    //!
    //! \param version Version of the serialized sidestake entry format.
    //! \param sidestake_entry The sidestake entry itself.
    //!
    SideStakePayload(const uint32_t version, SideStake sidestake_entry);

    //!
    //! \brief Initialize a sidestake entry payload from the given sidestake entry
    //! with the CURRENT_VERSION.
    //!
    //! \param sidestake_entry The sidestake entry itself.
    //!
    SideStakePayload(SideStake sidestake_entry);

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
                     m_entry.m_key.ToString(),
                     m_entry.m_allocation,
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
        return m_entry.m_key.ToString();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return ToString(m_entry.m_allocation);
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
//! \brief Stores and manages sidestake entries.
//!
class SideStakeRegistry : public IContractHandler
{
public:
    //!
    //! \brief sidestakeRegistry constructor. The parameter is the version number of the underlying
    //! sidestake entry db. This must be incremented when implementing format changes to the sidestake
    //! entries to force a reinit.
    //!
    //! Version 1: TBD.
    //!
    SideStakeRegistry()
        : m_sidestake_db(1)
          {
          };

    //!
    //! \brief The type that keys sidestake entries by their key strings. Note that the entries
    //! in this map are actually smart shared pointer wrappers, so that the same actual object
    //! can be held by both this map and the historical map without object duplication.
    //!
    typedef std::map<CBitcoinAddressForStorage, SideStake_ptr> SideStakeMap;

    //!
    //! \brief PendingSideStakeMap. This is not actually used but defined to satisfy the template.
    //!
    typedef SideStakeMap PendingSideStakeMap;

    //!
    //! \brief The type that keys historical sidestake entries by the contract hash (txid).
    //! Note that the entries in this map are actually smart shared pointer wrappers, so that
    //! the same actual object can be held by both this map and the (current) sidestake entry map
    //! without object duplication.
    //!
    typedef std::map<uint256, SideStake_ptr> HistoricalSideStakeMap;

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
    //! Mandatory sidestakes come before local ones, and the method ensures that the sidestakes
    //! returned do not total an allocation greater than 1.0.
    //!
    //! \param bool true to return local sidestakes only
    //!
    //! \return A vector of smart pointers to sidestake entries.
    //!
    const std::vector<SideStake_ptr> ActiveSideStakeEntries(const bool& local_only, const bool& include_zero_alloc);

    //!
    //! \brief Get the current sidestake entry for the specified key string.
    //!
    //! \param key The key string of the sidestake entry.
    //! \param local_only If true causes Try to only check the local sidestake map. Defaults to false.
    //!
    //! \return A vector of smart pointers to entries matching the provided key (address). Up to two elements
    //! are returned, mandatory entry first, unless local only boolean is set true.
    //!
    std::vector<SideStake_ptr> Try(const CBitcoinAddressForStorage& key, const bool& local_only = false) const;

    //!
    //! \brief Get the current sidestake entry for the specified key string if it has a status of ACTIVE or MANDATORY.
    //!
    //! \param key The key string of the sidestake entry.
    //! \param local_only If true causes Try to only check the local sidestake map. Defaults to false.
    //!
    //! \return A vector of smart pointers to entries matching the provided key (address) that are in status of
    //! MANDATORY or ACTIVE. Up to two elements are returned, mandatory entry first, unless local only boolean
    //! is set true.
    //!
    std::vector<SideStake_ptr> TryActive(const CBitcoinAddressForStorage& key, const bool& local_only = false) const;

    //!
    //! \brief Destroy the contract handler state in case of an error in loading
    //! the sidestake entry registry state from LevelDB to prepare for reload from contract
    //! replay. This is not used for sidestake entries, unless -clearSideStakehistory is specified
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
    //! block level specific validation to be done.
    //!
    //! \param ctx ContractContext containing the sidestake entry data to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return  \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    //!
    //! \brief Allows local (voluntary) sidestakes to be added to the in-memory map and not persisted to
    //! the registry db.
    //!
    //! \param SideStake object to add
    //! \param bool save_to_file if true causes SaveLocalSideStakesToConfig() to be called.
    //!
    void NonContractAdd(const SideStake& sidestake, const bool& save_to_file = true);

    //!
    //! \brief Add a sidestake entry to the registry from contract data. For the sidestake registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //!
    //! \param ctx
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Provides for deletion of local (voluntary) sidestakes from the in-memory map that are not persisted
    //! to the registry db. Deletion is by the map key (CBitcoinAddress).
    //!
    //! \param address
    //! \param bool save_to_file if true causes SaveLocalSideStakesToConfig() to be called.
    //!
    void NonContractDelete(const CBitcoinAddressForStorage& address, const bool& save_to_file = true);

    //!
    //! \brief Mark a sidestake entry deleted in the registry from contract data. For the sidestake registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //! \param ctx
    //!
    void Delete(const ContractContext& ctx) override;

    //!
    //! \brief Revert the registry state for the sidestake entry to the state prior
    //! to this ContractContext application. This is typically an issue
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
    uint64_t PassivateDB();

    //!
    //! \brief This method parses the config file for local sidestakes. It is based on the original GetSideStakingStatusAndAlloc()
    //! that was in miner.cpp prior to the implementation of the SideStake class.
    //!
    void LoadLocalSideStakesFromConfig();

    //!
    //! \brief A static function that is called by the scheduler to run the sidestake entry database passivation.
    //!
    static void RunDBPassivation();

    //!
    //! \brief Specializes the template RegistryDB for the SideStake class
    //!
    typedef RegistryDB<SideStake,
                       SideStake,
                       SideStakeStatus,
                       SideStakeMap,
                       PendingSideStakeMap,
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

    void SubscribeToCoreSignals();
    void UnsubscribeFromCoreSignals();

    SideStakeMap m_local_sidestake_entries;             //!< Contains the local (non-contract) sidestake entries.
    SideStakeMap m_sidestake_entries;                   //!< Contains the mandatory sidestake entries, including DELETED.
    PendingSideStakeMap m_pending_sidestake_entries {}; //!< Not used. Only to satisfy the template.

    SideStakeDB m_sidestake_db;

    bool m_local_entry_already_saved_to_config = false; //!< Flag to prevent reload on signal if individual entry saved already.

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
