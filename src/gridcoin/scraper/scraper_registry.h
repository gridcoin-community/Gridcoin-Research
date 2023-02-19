// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SCRAPER_SCRAPER_REGISTRY_H
#define GRIDCOIN_SCRAPER_SCRAPER_REGISTRY_H

#include "amount.h"
#include "base58.h"
#include "dbwrapper.h"
#include "serialize.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/support/enumbytes.h"


namespace GRC {

//!
//! \brief Enumeration of scraper status. Unlike beacons this is for both storage
//! and memory.
//!
//! UNKNOWN status is only encountered in trivially constructed empty
//! scraper entries and should never be seen on the blockchain.
//!
//! DELETED status corresponds to the functionality implemented in the AppCacheEntryExt
//! structure in the scraper to compensate for the lack of presence of a deleted record
//! in the old appcache. Rather than removing a record when a REMOVE contract is
//! encountered, The existing record is linked to the new one, which will have a
//! DELETED status.
//!
//! NOT_AUTHORIZED corresponds to the old appcache scraper section "false" value.
//!
//! AUTHORIZED corresponds to the old appcache scraper section "true" value
//!
//! EXPLORER is a new status that is meant to be able to establish differentiated
//! permission between a normal scraper node and one that is authorized to download
//! and retain all project stats files and store for an extended period. For legacy
//! purposes, this translates to "true' in the old appcache scraper section as well. Once
//! the scraper code has been converted over to use the native calls here from the
//! compatibility shims, then this new status can be used to specifically control
//! which scrapers are allowed to do extended explorer style statistics download and
//! retention.
//!
//! OUT_OF_BOUND must go at the end and be retained for the EnumBytes wrapper.
//!
enum class ScraperEntryStatus
{
    UNKNOWN,
    DELETED,
    NOT_AUTHORIZED,
    AUTHORIZED,
    EXPLORER,
    OUT_OF_BOUND
};

//!
//! \brief This class formalizes the concept of an authorized scraper entry and replaces
//! the older appcache "scraper" section. Scrapers are authorized by their address, i.e.
//! CKeyID (since the other modes in CTxDestination do not apply). So the "key" field in
//! the ScraperEntry is m_keyid. The datetime of the transaction containing the contract
//! that gives rise to the Entry is stored as m_timestamp. The hash of the transaction
//! is stored as m_hash. m_previous_hash stores the transaction hash of a previous
//! scraper entry with the same CKeyID, if there is one. This has the effect of creating
//! "chainlets", or a one-way linked list by hash of scraper entries with the same key.
//! This becomes very important to support reversion and avoid expensive forward contract
//! replays during a blockchain reorganization event. The status of the scraper entry
//! is stored as m_status in accordance with the class enum above.
//!
class ScraperEntry
{
public:
    //!
    //! \brief Wrapped Enumeration of scraper entry status, mainly for serialization/deserialization.
    //!
    using Status = EnumByte<ScraperEntryStatus>;

    CKeyID m_keyid;           //!< Identifies the scraper (address) for the entry.

    int64_t m_timestamp;      //!< Time of the scraper entry contract transaction.

    uint256 m_hash;           //!< The txid of the transaction that contains the scraper entry.

    uint256 m_previous_hash;  //!< The m_hash of the previous scraper entry with the same m_keyid.

    Status m_status;          //!< The status of the scraper entry. (Note serialization converts to/from int.)

    //!
    //! \brief Initialize an empty, invalid scraper entry instance.
    //!
    ScraperEntry();

    //!
    //! \brief Initialize a new scraper entry for submission in a contract.
    //!
    //! \param key_id. The CkeyID (i.e. address) of the scraper.
    //!
    ScraperEntry(CKeyID key_id, Status status);

    //!
    //! \brief Initialize a scraper entry instance with data from a contract.
    //!
    //! \param key_id. The CkeyID (i.e. address) of the scraper.
    //! \param status. The scraper entry status.
    //! \param tx_timestamp. Time of the transaction with the scraper entry contract.
    //! \param hash. Hash of the transaction with the scraper entry contract.
    //!
    ScraperEntry(CKeyID key_id, Status status, int64_t tx_timestamp, uint256 hash);

    //!
    //! \brief Determine whether a scraper entry contains each of the required elements.
    //!
    //! \return \c true if the scraper entry is complete.
    //!
    bool WellFormed() const;

    //!
    //! \brief Return the hash of scraper entry's public key (equivalent to address).
    //!
    //! \return RIPEMD-160 hash corresponding to the public key/address of the scraper.
    //!
    CKeyID GetId() const;

    //!
    //! \brief Return the address from the m_keyid.
    //!
    //! \return \c CBitcoinAddress derived from the m_keyid.
    //!
    CBitcoinAddress GetAddress() const;

     //!
    //! \brief Determine whether the given wallet contains a private key for
    //! this scraper entry's m_keyid. Because this function is intended to work
    //! even if the wallet is locked, it does not check whether the key pair is
    //! actually valid. This is used in authorizing a node to operate as a scraper
    //! and in which mode.
    //!
    //! \return \c true if the wallet contains a matching private key.
    //!
    bool WalletHasPrivateKey(const CWallet* const wallet) const;

    //!
    //! \brief A shim method to cross-wire this into the existing scraper code
    //! for compatibility purposes until the scraper code can be upgraded to use the
    //! native structures here.
    //!
    //! \return \c AppCacheEntryExt consisting of value, timestamp, and deleted boolean.
    //!
    AppCacheEntryExt GetLegacyScraperEntry();

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side scraper entry to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator==(ScraperEntry b);

    //!
    //! \brief Comparison operator overload used in the unit test harness.
    //!
    //! \param b The right hand side scraper entry to compare for equality.
    //!
    //! \return Equal or not.
    //!
    bool operator!=(ScraperEntry b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_keyid);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a ScraperEntry
//!
typedef std::shared_ptr<ScraperEntry> ScraperEntry_ptr;

//!
//! \brief A type that either points to some ScraperEntry or does not.
//!
typedef const ScraperEntry_ptr ScraperEntryOption;

//!
//! \brief The body of a scraper entry contract. Note that this body is bimodal. It
//! supports both the personality of the "LegacyPayload" which is found in the anonymous
//! namespace of the contracts.cpp, and also the new native ScraperEntry format.
//! Because the payloads in legacy contracts are not versioned, the default version
//! for this class is 1, which causes it to use the legacy-like deserialization. The
//! constructor of a new object, if a version is not specified, uses CURRENT_VERSION,
//! which is 2+, and will follow the native (de)serialization. In the
//! Contract::Body::ConvertFromLegacy call, by the time this call has been reached, the
//! contract will have already been deserialized. This will follow the legacy mode.
//! For contracts at version 3+, the Contract::SharePayload() will NOT call the
//! ConvertFromLegacy.
//!
class ScraperEntryPayload : public LegacyPayload
{
public:
    //!
    //! \brief Version number of the current format for a serialized scraper entry.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief Version number of the serialized scraper entry format.
    //!
    //! Initializes to the CURRENT_VERSION Note the
    //! constructor that takes a ScraperEntry defaults to CURRENT_VERSION. When the legacy
    //! K-V fields are used which correspond to the legacy appcache implementation, the version is 1.
    //!
    //! Version 1: appcache string key value:
    //!
    //! Version 2: Scraper entry data serializable in binary format. Stored in a
    //! contract's value field as bytes.
    //!
    uint32_t m_version = CURRENT_VERSION;

    //std::string m_key;     //!< The legacy string key.
    //std::string m_value;   //!< The legacy string value.
    ScraperEntry m_scraper_entry; //!< The scraper entry in the payload.

    //!
    //! \brief Initialize an empty, invalid scraper entry payload.
    //!
    ScraperEntryPayload();

    //!
    //! \brief Initialize a ScraperEntryPayload from a scraper entry constructed from
    //! key_id and status
    //!
    //! \param version Version of the serialized scraper entry format.
    //! \param key_id CKeyID of the scraper entry
    //! \param status Status of the scraper entry
    //!
    ScraperEntryPayload(const uint32_t version, CKeyID key_id, ScraperEntryStatus status);

    //!
    //! \brief Initialize a scraper entry payload from the given scraper entry
    //! with the provided version number (and format).
    //!
    //! \param version Version of the serialized scraper entry format.
    //! \param scraper_entry The scraper entry itself.
    //!
    ScraperEntryPayload(const uint32_t version, ScraperEntry scraper_entry);

    //!
    //! \brief Initialize a scraper entry payload from the given scraper entry
    //! with the CURRENT_VERSION.
    //! \param scraper_entry The scraper entry itself.
    //!
    ScraperEntryPayload(ScraperEntry scraper_entry);

    //!
    //! \brief Initialize a scraper entry payload from legacy data.
    //!
    //! \param key
    //! \param value
    //!
    ScraperEntryPayload(const std::string& key, const std::string& value);

    //!
    //! \brief Initialize a scraper entry payload from the legacy (appcache) string format.
    //!
    //! \param key The address (NOT CKeyID) of the scraper (entry).
    //! \param value The value of whether the scraper is authorized, "true" or "false".
    //!
    //! \return The resultant scraper entry payload. Note that there is no concept
    //! of the "EXPLORER" status in the legacy parse, because it is introduced with
    //! this class.
    //!
    static ScraperEntryPayload Parse(const std::string& key, const std::string& value);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::SCRAPER;
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

        if (m_version == 1) {
            return m_scraper_entry.WellFormed() || action == ContractAction::REMOVE;
        }

        return m_scraper_entry.WellFormed();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        CBitcoinAddress address;

        address.Set(m_scraper_entry.m_keyid);

        return address.ToString();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        if (m_scraper_entry.m_status <= ScraperEntryStatus::NOT_AUTHORIZED) {
            return "false";
        } else {
            return "true";
        }
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract. This
    //! is the same as the LegacyPayload to insure compatibility between the scraper
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
        // These will be filled in for legacy scraper entries, but will also be present as empties in
        // native scraper records to solve the (de)serialization problem between legacy and native.
        READWRITE(m_key);

        if (contract_action != ContractAction::REMOVE) {
            READWRITE(m_value);
        }

        if (m_key.empty()) {
            READWRITE(m_version);
            READWRITE(m_scraper_entry);
        } else {
            m_version = 1;
        }
    }
}; // ScraperEntryPayload

//!
//! \brief Stores and manages scraper entries. These represent scrapers known to the
//! network and their status.
//!
class ScraperRegistry : public IContractHandler
{
public:
    //!
    //! \brief The type that keys scraper entries by their CKeyID, which is equivalent
    //! to the scraper's address. Note that the enties in this map are actually smart shared
    //! pointer wrappers, so that the same actual object can be held by both this map
    //! and the historical scraper map without object duplication.
    //!
    typedef std::map<CKeyID, ScraperEntry_ptr> ScraperMap;

    //!
    //! \brief The type that keys historical scraper entries by the contract hash (txid).
    //! Note that the entries in this map are actually smart shared pointer wrappers, so that
    //! the same actual object can be held by both this map and the (current) scraper map
    //! without object duplication.
    //!
    typedef std::map<uint256, ScraperEntry_ptr> HistoricalScraperMap;

    //!
    //! \brief Get the collection of current scraper entries. Note that this INCLUDES deleted
    //! scraper entries.
    //!
    //! \return \c A reference to the current scraper entries stored in the registry.
    //!
    const ScraperMap& Scrapers() const;

    //!
    //! \brief A shim method to cross-wire this into the existing scraper code
    //! for compatibility purposes until the scraper code can be upgraded to use the
    //! native structures here. Only includes AUTHORIZED and EXPLORER scrapers, which are
    //! both reported with a std::string status of "true".
    //!
    //! \return \c AppCacheEntrySection consisting of key (address string) and
    //! { value, timestamp }.
    //!
    const AppCacheSection GetScrapersLegacy() const;

    //!
    //! \brief A shim method to cross-wire this into the existing scraper code
    //! for compatibility purposes until the scraper code can be upgraded to use the
    //! native structures here.
    //!
    //! \param authorized_only Boolean that if true requires that results include scraper entries
    //! with AUTHORIZED or EXPLORER status only.
    //!
    //! \return \c AppCacheEntrySectionExt consisting of key (address string) and
    //! { value, timestamp, deleted }.
    //!
    const AppCacheSectionExt GetScrapersLegacyExt(const bool& authorized_only = false) const;

    //!
    //! \brief Get the current scraper entry for the specified CKeyID key_id.
    //!
    //! \param key_id The CKeyID of the public key of the scraper (essentially the address).
    //!
    //! \return An object that either contains a reference to some scraper entry if it exists
    //! for the key_id or does not.
    //!
    ScraperEntryOption Try(const CKeyID& key_id) const;

    //!
    //! \brief Get the current scraper entry for the specified CKeyID key_id if it is in
    //! status AUTHORIZED or EXPLORER.
    //!
    //! \param key_id The CKeyID of the public key of the scraper (essentially the address).
    //!
    //! \return An object that either contains a reference to some scraper entry if it exists
    //! for the key_id and is in the required status or does not.
    //!
    ScraperEntryOption TryAuthorized(const CKeyID& key_id) const;

    //!
    //! \brief Destroy the contract handler state in case of an error in loading
    //! the scraper entry registry state from LevelDB to prepare for reload from contract
    //! replay. This is not used for scraper entries, unless -clearscraperentryhistory is specified
    //! as a startup argument, because contract replay storage and full reversion has
    //! been implemented for scraper entries.
    //!
    void Reset() override;

    //!
    //! \brief Determine whether a scraper entry contract is valid.
    //!
    //! \param contract Contains the scraper entry contract to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior out.
    //!
    //! \return \c true if the contract contains a valid scraper entry.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override;

    //!
    //! \brief Determine whether a scraper entry contract is valid including block context. This is used
    //! in ConnectBlock. Note that for scraper entries this simply calls Validate as there is no
    //! block level specific validation to be done.
    //!
    //! \param ctx ContractContext containing the scraper entry data to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return  \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    //!
    //! \brief Add a scraper entry to the registry from contract data. For the scraper registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //! \param ctx
    //!
    void Add(const ContractContext& ctx) override;

    //!
    //! \brief Mark a scraper entry deleted in the registry from contract data. For the scraper registry
    //! both Add and Delete actually call a common helper function AddDelete, because the action
    //! is actually symmetric to both.
    //! \param ctx
    //!
    void Delete(const ContractContext& ctx) override;

    //!
    //! \brief Revert the registry state for the scraper entry to the state prior
    //! to this ContractContext application. This is typically an issue
    //! during reorganizations, where blocks are disconnected.
    //!
    //! \param ctx References the scraper entry contract and associated context.
    //!
    void Revert(const ContractContext& ctx) override;

    //!
    //! \brief Initialize the ScraperRegistry, which now includes restoring the state of the ScraperRegistry from
    //! LevelDB on wallet start.
    //!
    //! \return Block height of the database restored from LevelDB. Zero if no LevelDB scraper entry data is found or
    //! there is some issue in LevelDB scraper entry retrieval. (This will cause the contract replay to change scope
    //! and initialize the ScraperRegistry from contract replay and store in LevelDB.)
    //!
    int Initialize();

    //!
    //! \brief Gets the block height through which is stored in the scraper entry registry database.
    //!
    //! \return block height.
    //!
    int GetDBHeight();

    //!
    //! \brief Function normally only used after a series of reverts during block disconnects, because
    //! block disconnects are done in groups back to a common ancestor, and will include a series of reverts.
    //! This is essentially atomic, and therefore the final (common) height only needs to be set once. TODO:
    //! reversion should be done with a vector argument of the contract contexts, along with a final height to
    //! clean this up and move the logic to here from the calling function.
    //!
    //! \param height to set the storage DB bookmark.
    //!
    void SetDBHeight(int& height);

    //!
    //! \brief Resets the maps in the ScraperRegistry but does not disturb the underlying LevelDB
    //! storage. This is only used during testing in the testing harness.
    //!
    void ResetInMemoryOnly();

    //!
    //! \brief Passivates the elements in the scraper db, which means remove from memory elements in the
    //! historical map that are not referenced by m_scrapers. The backing store of the element removed
    //! from memory is retained and will be transparently restored if find() is called on the hash key
    //! for the element.
    //!
    //! \return The number of elements passivated.
    //!
    uint64_t PassivateDB();

    //!
    //! \brief A static function that is called by the scheduler to run the scraper entry database passivation.
    //!
    static void RunScraperDBPassivation();

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

    ScraperMap m_scrapers; //!< Contains the current scraper entries, including entries marked DELETED.

    //!
    //! \brief A class private to the ScraperRegistry class that implements LevelDB backing storage for scraper entries.
    //! This is very similar to the BeaconDB.
    //!
    class ScraperEntryDB
    {
    public:
        //!
        //! \brief Version number of the scraper entry db.
        //!
        //! CONSENSUS: Increment this value when introducing a breaking change to the scraper entry db. This
        //! will ensure that when the wallet is restarted, the level db scraper entry storage will be cleared and
        //! reloaded from the contract replay with the correct lookback scope.
        //!
        //! Version 0: <= TBD
        //! Version 1: = TBD
        //!
        static constexpr uint32_t CURRENT_VERSION = 1;

        //!
        //! \brief Initializes the Scraper Registry map structures from the replay of the scraper entry states stored
        //! in the scraper entry database.
        //!
        //! \param m_scraper The map of current scraper entries.
        //!
        //! \return block height up to and including which the scraper entry records were stored.
        //!
        int Initialize(ScraperMap& scrapers);

        //!
        //! \brief Clears the historical scraper entry map of the database. This is only used during testing.
        //!
        void clear_in_memory_only();

        //!
        //! \brief Clears the LevelDB scraper entry storage area.
        //!
        //! \return Success or failure.
        //!
        bool clear_leveldb();

        //!
        //! \brief Removes in memory elements for all historical records not in m_scrapers.
        //! \return Number of elements passivated.
        //!
        uint64_t passivate_db();

        //!
        //! \brief Clear the historical map and LevelDB scraper entry storage area.
        //!
        //! \return Success or failure.
        //!
        bool clear();

        //!
        //! \brief The number of scraper entry historical elements in the scraper entry database. This includes in memory
        //! entries only and not passivated entries.
        //!
        //! \return The number of elements.
        //!
        size_t size();

        //!
        //! \brief This stores the height to which the database entries are valid (the db scope). Note that it
        //! is not desired to expose this function as a public function, but currently the Revert function
        //! only operates on a single transaction context, and does not encapsulate the post reversion height
        //! after the reversion state. TODO: Create a Revert overload that takes a vector of contract contexts
        //! to be reverted (in order in which they are in the vector) and the post revert batch height (i.e.
        //! the common block of the fork/reorg).
        //!
        //! \param height_stored
        //!
        //! \return Success or failure.
        //!
        bool StoreDBHeight(const int& height_stored);

        //!
        //! \brief Provides the block height to which the scraper entry db covers. This is persisted in LevelDB.
        //!
        //! \param height_stored
        //!
        //! \return
        //!
        bool LoadDBHeight(int& height_stored);

        //!
        //! \brief Insert a scraper entry record into the historical database.
        //!
        //! \param hash The hash for the key to the historical record which is the txid (hash) of the transaction
        //! containing the scraper entry contract.
        //! \param height The height of the block from which the scraper entry record originates.
        //! \param scraper The scraper entry record to insert (which includes the appropriate status).
        //!
        //! \return Success or Failure. This will fail if a record with the same key already exists in the
        //! database.
        //!
        bool insert(const uint256& hash, const int& height, const ScraperEntry& scraper_entry);

        //!
        //! \brief Erase a record from the database.
        //!
        //! \param hash The key of the record to erase.
        //!
        //! \return Success or failure.
        //!
        bool erase(const uint256& hash);

        //!
        //! \brief Remove an individual in memory element that is backed by LevelDB that is not in m_scrapers.
        //!
        //! \param hash The hash that is the key to the element.
        //!
        //! \return A pair, the first part of which is an iterator to the next element, or map::end() if the last one, and
        //! the second is success or failure of the passivation.
        //!
        std::pair<ScraperRegistry::HistoricalScraperMap::iterator, bool>
            passivate(ScraperRegistry::HistoricalScraperMap::iterator& iter);

        //!
        //! \brief Iterator to the beginning of the database records.
        //!
        //! \return Iterator.
        //!
        HistoricalScraperMap::iterator begin();

        //!
        //! \brief Iterator to end().
        //!
        //! \return Iterator.
        //!
        HistoricalScraperMap::iterator end();

        //!
        //! \brief Provides an iterator pointing to the element which key value matches the provided hash. Note that
        //! this wrapper extends the behavior of the normal find function and will, in the case the element is not
        //! present in the in-memory map, look in LevelDB and attempt to load the element from LevelDB, place in the
        //! map, and return an iterator. end() is returned if the element is not found.
        //!
        //! \param hash The hash value with which to match on the key.
        //!
        //! \return Iterator.
        //!
        HistoricalScraperMap::iterator find(const uint256& hash);

        //!
        //! \brief Advances the iterator to the next element.
        //!
        //! \param iter
        //!
        //! \return iter
        //!
        HistoricalScraperMap::iterator advance(HistoricalScraperMap::iterator iter);

    private:
        //!
        //! \brief Type definition for the storage scraper entry map used in Initialize. Note that the uint64_t
        //! is the record number, which unfortunately is required to preserve the contract application order
        //! since they are applied in the order of the block's transaction vector rather than the transaction time.
        //!
        typedef std::map<uint256, std::pair<uint64_t, ScraperEntry>> StorageScraperMap;

        //!
        //! \brief Type definition for the map used to replay state from LevelDB scraper entry area.
        //!
        typedef std::map<uint64_t, ScraperEntry> StorageScraperMapByRecordNum;

        //!
        //! \brief This is a map keyed by uint256 (SHA256) hash that stores the historical scraper entry elements.
        //! It is persisted in LevelDB storage.
        //!
        HistoricalScraperMap m_historical;

        //!
        //! \brief Boolan to indicate whether the database has been successfully initialized from LevelDB during
        //! startup.
        //!
        bool m_database_init = false;

        //!
        //! \brief The block height for scraper entry records stored in the scraper database. This is a bookmark. It is
        //! adjusted by StoreDBHeight, persisted in memory by this private member variable, and persisted in storage
        //! to LevelDB.
        //!
        int m_height_stored = 0;

        //!
        //! \brief The record number stored watermark. This effectively a sequence number for records stored in
        //! the LevelDB scraper entry area. The value in memory will be at the highest record number inserted (or played
        //! back during initialization).
        //!
        uint64_t m_recnum_stored = 0;

        //!
        //! \brief The flag that indicates whether memory optimization can occur by passivating the database. This
        //! flag is set true when find() retrieves a scraper entry element from LevelDB to satisfy a hash search.
        //! This would typically occur on a reorganization (revert).
        //!
        bool m_needs_passivation = false;

        //!
        //! \brief Store a scraper entry object in LevelDB with the provided key value.
        //!
        //! \param hash The SHA256 hash key value for the element.
        //! \param scraper_entry The scraper entry historical state element to be stored.
        //!
        //! \return Success or failure.
        //!
        bool Store(const uint256& hash, const ScraperEntry& scraper_entry);

        //!
        //! \brief Load a scraper entry object from LevelDB using the provided key value.
        //!
        //! \param hash The SHA256 hash key value for the element.
        //! \param scraper_entry The scraper entry historical state element loaded.
        //!
        //! \return Success or failure.
        //!
        bool Load(const uint256& hash, ScraperEntry& scraper_entry);

        //!
        //! \brief Delete a scraper entry object from LevelDB with the provided key value (if it exists).
        //!
        //! \param hash The SHA256 hash key value for the element.
        //!
        //! \return Success or failure.
        //!
        bool Delete(const uint256& hash);
    }; // ScraperEntryDB

    ScraperEntryDB m_scraper_db;

public:

    ScraperEntryDB& GetScraperDB();
}; // ScraperRegistry

//!
//! \brief Get the global scraper entry registry.
//!
//! \return Current global scraper entry registry instance.
//!
ScraperRegistry& GetScraperRegistry();
} // namespace GRC

#endif // GRIDCOIN_SCRAPER_SCRAPER_REGISTRY_H
