// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_PROTOCOL_H
#define GRIDCOIN_PROTOCOL_H


#include "amount.h"
#include "serialize.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "gridcoin/support/enumbytes.h"

namespace GRC {

//!
//! \brief Enumeration of protocol status. Unlike beacons this is for both storage
//! and memory.
//!
//! UNKNOWN status is only encountered in trivially constructed empty
//! protocol entries and should never be seen on the blockchain.
//!
//! DELETED status corresponds to the functionality implemented in the AppCacheEntryExt
//! structure in the protocol to compensate for the lack of presence of a deleted record
//! in the old appcache. Rather than removing a record when a REMOVE contract is
//! encountered, The existing record is linked to the new one, which will have a
//! DELETED status.
//!
//! ACTIVE corresponds to an active entry.
//!
//! OUT_OF_BOUND must go at the end and be retained for the EnumBytes wrapper.
//!
enum class ProtocolEntryStatus
{
    UNKNOWN,
    DELETED,
    ACTIVE,
    OUT_OF_BOUND
};

//!
//! \brief This class formalizes the concept of an authorized protocol entry and replaces
//! the older appcache "protocol" section. For protocol entries, the "key" field is a string.
//! The datetime of the transaction containing the contract that gives rise to the entry is
//! stored as m_timestamp. The hash of the transaction is stored as m_hash. m_previous_hash
//! stores the transaction hash of a previous protocol entry with the same key, if there is
//! one. This has the effect of creating "chainlets", or a one-way linked list by hash of
//! protocol entries with the same key. This becomes very important to support reversion and
//! avoid expensive forward contract replays during a blockchain reorganization event. The
//! status of the protocol entry is stored as m_status in accordance with the class enum above.
//!
class ProtocolEntry
{
public:
    //!
    //! \brief Wrapped Enumeration of protocol entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<ProtocolEntryStatus>;

    std::string m_key;        //!< Identifies the protocol entry key.
    std::string m_value;      //!< Identifies the protocol entry value.
    int64_t m_timestamp;      //!< Time of the protocol entry contract transaction.
    uint256 m_hash;           //!< The txid of the transaction that contains the protocol entry.
    uint256 m_previous_hash;  //!< The m_hash of the previous protocol entry with the same key.
    Status m_status;          //!< The status of the protocol entry. (Note serialization converts to/from int.)

    //!
    //! \brief Initialize an empty, invalid protocol entry instance.
    //!
    ProtocolEntry();

    //!
    //! \brief Initialize a new protocol entry for submission in a contract (with ACTIVE status)
    //!
    //! \param key. The key of the protocol entry.
    //! \param value. The value of the protocol entry.
    //!
    ProtocolEntry(std::string key, std::string value);

    //!
    //! \brief Initialize a new protocol entry for submission in a contract with provided status.
    //!
    //! \param key. The key of the protocol entry.
    //! \param value. The value of the protocol entry.
    //! \param status. the status of the protocol entry.
    //!
    ProtocolEntry(std::string key, std::string value, Status status);

    //!
    //! \brief Initialize a protocol entry instance with data from a contract.
    //!
    //! \param key_id. The key of the protocol entry.
    //! \param value. The value of the protocol entry.
    //! \param tx_timestamp. Time of the transaction with the protocol entry contract.
    //! \param hash. Hash of the transaction with the protocol entry contract.
    //!
    ProtocolEntry(std::string key, std::string value, Status status, int64_t tx_timestamp, uint256 hash);

    //!
    //! \brief Determine whether a protocol entry contains each of the required elements.
    //!
    //! \return \c true if the protocol entry is complete.
    //!
    bool WellFormed() const;

    //!
    //! \brief This is the standardized method that returns the key value for the protocol entry (for
    //! the registry_db.h template.)
    //!
    //! \return std::string key value for the protocol entry
    //!
    std::string Key() const;

    //!
    //! \brief Provides the protocol key and value as a pair.
    //! \return std::pair of strings
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Returns the string representation of the current protocol entry status
    //!
    //! \return Translated string representation of protocol status
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the translated or untranslated string of the input protocol entry status
    //!
    //! \param status. ProtocolEntryStatus
    //! \param translated. True for translated, false for not translated. Defaults to true.
    //!
    //! \return Protocol entry status string.
    //!
    std::string StatusToString(const ProtocolEntryStatus& status, const bool& translated = true) const;

    //!
    //! \brief A shim method to cross-wire this into the existing scraper code
    //! for compatibility purposes until the scraper code can be upgraded to use the
    //! native structures here.
    //!
    //! \return \c AppCacheEntryExt consisting of value, timestamp, and deleted boolean.
    //!
    AppCacheEntryExt GetLegacyProtocolEntry();

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side protocol entry to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator==(ProtocolEntry b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side protocol entry to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator!=(ProtocolEntry b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_key);
        READWRITE(m_value);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a ProtocolEntry
//!
typedef std::shared_ptr<ProtocolEntry> ProtocolEntry_ptr;

//!
//! \brief A type that either points to some ProtocolEntry or does not.
//!
typedef const ProtocolEntry_ptr ProtocolEntryOption;

//!
//! \brief The body of a protocol entry contract. Note that this body is bimodal. It
//! supports both the personality of the "LegacyPayload", and also the new native
//! ProtocolEntry format. In the Contract::Body::ConvertFromLegacy call, by the time
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
class ProtocolEntryPayload : public LegacyPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized protocol entry.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief Version number of the serialized protocol entry format.
    //!
    //! Initializes to the CURRENT_VERSION Note the
    //! constructor that takes a ProtocolEntry defaults to CURRENT_VERSION. When the legacy
    //! K-V fields are used which correspond to the legacy appcache implementation, the version is 1.
    //!
    //! Version 1: appcache string key value:
    //!
    //! Version 2: Protocol entry data serializable in binary format. Stored in a
    //! contract's value field as bytes.
    //!
    uint32_t m_version = CURRENT_VERSION;

    ProtocolEntry m_entry; //!< The protocol entry in the payload.

    //!
    //! \brief Initialize an empty, invalid protocol entry payload.
    //!
    ProtocolEntryPayload(uint32_t version = CURRENT_VERSION);

    //!
    //! \brief Initialize a ProtocolEntryPayload from a protocol entry constructed from
    //! string key and value. Not to be used for version 1 payloads. Will assert. Does NOT
    //! initialize hash fields.
    //!
    //! \param key. Key string for the protocol entry
    //! \param value. Value string for the protocol entry
    //! \param status. Status of the protocol entry
    //!
    ProtocolEntryPayload(const uint32_t version, std::string key, std::string value, ProtocolEntryStatus status);

    //!
    //! \brief Initialize a protocol entry payload from the given protocol entry
    //! with the provided version number (and format).
    //!
    //! \param version Version of the serialized protocol entry format.
    //! \param protocol_entry The protocol entry itself.
    //!
    ProtocolEntryPayload(const uint32_t version, ProtocolEntry protocol_entry);

    //!
    //! \brief Initialize a protocol entry payload from the given protocol entry
    //! with the CURRENT_VERSION.
    //!
    //! \param protocol_entry The protocol entry itself.
    //!
    ProtocolEntryPayload(ProtocolEntry protocol_entry);

    //!
    //! \brief Initialize a protocol entry payload from legacy data. Does NOT
    //! initialize hash fields.
    //!
    //! \param key
    //! \param value
    //!
    ProtocolEntryPayload(const std::string& key, const std::string& value);

    //!
    //! \brief Initialize a protocol entry payload from the legacy (appcache) string format.
    //!
    //! \param key. Key string for the protocol entry
    //! \param value. Value string for the protocol entry
    //!
    //! \return The resultant protocol entry payload. The status is set to ACTIVE.
    //!
    static ProtocolEntryPayload Parse(const std::string& key, const std::string& value);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::PROTOCOL;
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        if (m_version <= 0 || m_version > CURRENT_VERSION) {
            return false;
        }

        // Upon contract receipt for version 1 payload, need to follow the rules for
        // legacy payload.
        if (m_version == 1) {
            return !m_key.empty() && (action == GRC::ContractAction::REMOVE || !m_value.empty());
        }

        // Protocol Entry Payloads version 2+ follow full protocol entry validation rules
        return m_entry.WellFormed();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return m_entry.m_key;
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        return m_entry.m_value;
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract. This
    //! is the same as the LegacyPayload to insure compatibility between the protocol
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
        // These will be filled in for legacy protocol entries, but will also be present as empties in
        // native protocol records to solve the (de)serialization problem between legacy and native.

        READWRITE(m_key);

        if (contract_action != ContractAction::REMOVE) {
            READWRITE(m_value);

        }

        if (m_key.empty()) {
            READWRITE(m_version);
            READWRITE(m_entry);
        } else {
            m_version = 1;

        }
    }
}; // ProtocolEntryPayload

//!
//! \brief Stores and manages protocol entries.
//!
class ProtocolRegistry : public IContractHandler
{
public:
    //!
    //! \brief ProtocolRegistry constructor. The parameter is the version number of the underlying
    //! protocol entry db. This must be incremented when implementing format changes to the protocol
    //! entries to force a reinit.
    //!
    //! Version 0: <= 5.4.2.0
    //! Version 1: TBD.
    //!
    ProtocolRegistry()
        : m_protocol_db(1)
    {
    };

    //!
    //! \brief The type that keys protocol entries by their key strings. Note that the entries
    //! in this map are actually smart shared pointer wrappers, so that the same actual object
    //! can be held by both this map and the historical map without object duplication.
    //!
    typedef std::map<std::string, ProtocolEntry_ptr> ProtocolEntryMap;

    //!
    //! \brief PendingProtocolEntryMap. This is not actually used but defined to satisfy the template.
    //!
    typedef ProtocolEntryMap PendingProtocolEntryMap;

    //!
    //! \brief The type that keys historical protocol entries by the contract hash (txid).
    //! Note that the entries in this map are actually smart shared pointer wrappers, so that
    //! the same actual object can be held by both this map and the (current) protocol entry map
    //! without object duplication.
    //!
    typedef std::map<uint256, ProtocolEntry_ptr> HistoricalProtocolEntryMap;

    //!
    //! \brief Get the collection of current protocol entries. Note that this INCLUDES deleted
    //! protocol entries.
    //!
    //! \return \c A reference to the current protocol entries stored in the registry.
    //!
    const ProtocolEntryMap& ProtocolEntries() const;

    //!
    //! \brief A shim method to cross-wire this into the existing scraper code
    //! for compatibility purposes until the scraper code can be upgraded to use the
    //! native structures here. Does NOT include deleted entries (entries with a status of
    //! ProtocolEntryStatus::DELETED).
    //!
    //! \return \c AppCacheEntrySection consisting of key (address string) and
    //! { value, timestamp }.
    //!
    const AppCacheSection GetProtocolEntriesLegacy() const;

    //!
    //! \brief A shim method to cross-wire this into the existing scraper code
    //! for compatibility purposes until the scraper code can be upgraded to use the
    //! native structures here.
    //!
    //! \param authorized_only Boolean that if true requires that results include only those
    //! protocol entries that are ACTIVE.
    //!
    //! \return \c AppCacheEntrySectionExt consisting of key string and
    //! { value, timestamp, deleted }.
    //!
    const AppCacheSectionExt GetProtocolEntriesLegacyExt(const bool& active_only = false) const;

    const AppCacheEntry GetProtocolEntryByKeyLegacy(std::string key) const;

    //!
    //! \brief Get the current protocol entry for the specified key string.
    //!
    //! \param key The key string of the protocol entry.
    //!
    //! \return An object that either contains a reference to some protocol entry if it exists
    //! for the key or does not.
    //!
    ProtocolEntryOption Try(const std::string& key) const;

    //!
    //! \brief Get the current protocol entry for the specified key string if it has a status of ACTIVE.
    //!
    //! \param key The key string of the protocol entry.
    //!
    //! \return An object that either contains a reference to some protocol entry if it exists
    //! for the key and is in the required status or does not.
    //!
    ProtocolEntryOption TryActive(const std::string& key) const;

    //!
    //! \brief Destroy the contract handler state in case of an error in loading
    //! the protocol entry registry state from LevelDB to prepare for reload from contract
    //! replay. This is not used for protocol entries, unless -clearprotocolentryhistory is specified
    //! as a startup argument, because contract replay storage and full reversion has
    //! been implemented for protocol entries.
    //!
    void Reset() override;

    //!
    //! \brief Determine whether a protocol entry contract is valid.
    //!
    //! \param contract Contains the protocol entry contract to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior out.
    //!
    //! \return \c true if the contract contains a valid protocol entry.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override;

    //!
    //! \brief Determine whether a protocol entry contract is valid including block context. This is used
    //! in ConnectBlock. Note that for protocol entries this simply calls Validate as there is no
    //! block level specific validation to be done.
    //!
    //! \param ctx ContractContext containing the protocol entry data to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return  \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    //!
    //! \brief Add a protocol entry to the registry from contract data. For the protocol registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //! \param ctx
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Mark a protocol entry deleted in the registry from contract data. For the protocol registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //! \param ctx
    //!
    void Delete(const ContractContext& ctx) override;

    //!
    //! \brief Revert the registry state for the protocol entry to the state prior
    //! to this ContractContext application. This is typically an issue
    //! during reorganizations, where blocks are disconnected.
    //!
    //! \param ctx References the protocol entry contract and associated context.
    //!
    void Revert(const ContractContext& ctx) override;

    //!
    //! \brief Initialize the ProtocolRegistry, which now includes restoring the state of the ProtocolRegistry from
    //! LevelDB on wallet start.
    //!
    //! \return Block height of the database restored from LevelDB. Zero if no LevelDB protocol entry data is found or
    //! there is some issue in LevelDB protocol entry retrieval. (This will cause the contract replay to change scope
    //! and initialize the ProtocolRegistry from contract replay and store in LevelDB.)
    //!
    int Initialize() override;

    //!
    //! \brief Gets the block height through which is stored in the protocol entry registry database.
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
    //! \brief Resets the maps in the ProtocolRegistry but does not disturb the underlying LevelDB
    //! storage. This is only used during testing in the testing harness.
    //!
    void ResetInMemoryOnly();

    //!
    //! \brief Passivates the elements in the protocol db, which means remove from memory elements in the
    //! historical map that are not referenced by the active entry map. The backing store of the element removed
    //! from memory is retained and will be transparently restored if find() is called on the hash key
    //! for the element.
    //!
    //! \return The number of elements passivated.
    //!
    uint64_t PassivateDB() override;

    //!
    //! \brief Specializes the template RegistryDB for the ProtocolEntry class. Note that std::set<ProtocolEntry>
    //! is not actually used.
    //!
    typedef RegistryDB<ProtocolEntry,
                       ProtocolEntry,
                       ProtocolEntryStatus,
                       ProtocolEntryMap,
                       PendingProtocolEntryMap,
                       std::set<ProtocolEntry>,
                       HistoricalProtocolEntryMap> ProtocolEntryDB;

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

    ProtocolEntryMap m_protocol_entries;                   //!< Contains the current protocol entries including entries marked DELETED.
    PendingProtocolEntryMap m_pending_protocol_entries {}; //!< Not used. Only to satisfy the template.

    std::set<ProtocolEntry> m_expired_protocol_entries {}; //!< Not used. Only to satisfy the template.

    ProtocolEntryDB m_protocol_db;

public:

    ProtocolEntryDB& GetProtocolEntryDB();
}; // ProtocolRegistry

//!
//! \brief Get the global protocol entry registry.
//!
//! \return Current global protocol entry registry instance.
//!
ProtocolRegistry& GetProtocolRegistry();
} // namespace GRC

#endif // GRIDCOIN_PROTOCOL_H
