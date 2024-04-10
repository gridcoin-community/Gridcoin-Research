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
#include "gridcoin/contract/registry_db.h"
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
//! the ScraperEntry is m_key of type CKeyID. The datetime of the transaction containing
//! the contract that gives rise to the Entry is stored as m_timestamp. The hash of the
//! transaction is stored as m_hash. m_previous_hash stores the transaction hash of a previous
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

    CKeyID m_key;             //!< Identifies the scraper (address) for the entry.
    int64_t m_timestamp;      //!< Time of the scraper entry contract transaction.
    uint256 m_hash;           //!< The txid of the transaction that contains the scraper entry.
    uint256 m_previous_hash;  //!< The m_hash of the previous scraper entry with the same m_key.
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
    //! \brief This is the standardized method that returns the key value for the scraper entry (for
    //! the registry_db.h template.) Here it is the same as the GetId() method below.
    //!
    //! \return CKeyID value for the scraper entry.
    //!
    CKeyID Key() const;

    //!
    //! \brief Provides the key (address) and status as string
    //! \return std::pair of strings
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Return the hash of scraper entry's public key (equivalent to address).
    //!
    //! \return RIPEMD-160 hash corresponding to the public key/address of the scraper.
    //!
    CKeyID GetId() const;

    //!
    //! \brief Return the address from the m_key.
    //!
    //! \return \c CBitcoinAddress derived from the m_key.
    //!
    CBitcoinAddress GetAddress() const;

    //!
    //! \brief Returns the string representation of the current scraper entry status
    //!
    //! \return Translated string representation of scraper status
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the translated or untranslated string of the input scraper entry status
    //!
    //! \param status. ScraperEntryStatus
    //! \param translated. True for translated, false for not translated. Defaults to true.
    //!
    //! \return Scraper entry status string.
    //!
    std::string StatusToString(const ScraperEntryStatus& status, const bool& translated = true) const;

    //!
    //! \brief Determine whether the given wallet contains a private key for
    //! this scraper entry's m_key. Because this function is intended to work
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
        READWRITE(m_key);
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
//! supports both the personality of the "LegacyPayload", and also the new native
//! ScraperEntry format. In the Contract::Body::ConvertFromLegacy call, by the time
//! this call has been reached, the contract will have already been deserialized.
//! This will follow the legacy mode. For contracts at version 3+, the
//! Contract::SharePayload() will NOT call the ConvertFromLegacy. Note that because
//! the existing legacyPayloads are not versioned, the deserialization of
//! the payload first (de)serializes m_key, which is guaranteed to exist in either
//! legacy or native. If the key is empty, then payload v2+ is being deserialized
//! and the m_version and m_scraper_entry are (de)serialized. This is ugly
//! but necessary to deal with the unversioned Legacy Payloads and maintain
//! compatibility.
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
    //! Initializes to the CURRENT_VERSION Note the constructor that takes a ScraperEntry
    //! defaults to CURRENT_VERSION. When the legacy K-V fields are used which correspond
    //! to the legacy appcache implementation, the version is 1.
    //!
    //! Version 1: appcache string key value:
    //!
    //! Version 2: Scraper entry data serializable in binary format. Stored in a
    //! contract's value field as bytes.
    //!
    uint32_t m_version = CURRENT_VERSION;

    ScraperEntry m_scraper_entry; //!< The scraper entry in the payload.

    //!
    //! \brief Initialize an empty, invalid scraper entry payload.
    //!
    ScraperEntryPayload(const uint32_t version = CURRENT_VERSION);

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
    //! with the provided version number (and format). This can only be used for
    //! payload version > 1. It will assert if used for payload version 1.
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

        // Upon contract receipt for version 1 payload, need to follow the rules for
        // legacy payload.
        if (m_version == 1) {
            return !m_key.empty() && (action == GRC::ContractAction::REMOVE || !m_value.empty());
        }

        // Scraper Entry Payloads version 2+ follow full scraper entry validation rules
        return m_scraper_entry.WellFormed();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        CBitcoinAddress address;

        address.Set(m_scraper_entry.m_key);

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
    //! \brief ScraperRegistry constructor. The parameter is the version number of the underlying
    //! protocol entry db. This must be incremented when implementing format changes to the protocol
    //! entries to force a reinit.
    //!
    //! Version 0: <= 5.4.2.0 (no backing db).
    //! Version 1: TBD.
    //!
    ScraperRegistry()
        : m_scraper_db(1)
    {
    };

    //!
    //! \brief The type that keys scraper entries by their CKeyID, which is equivalent
    //! to the scraper's address. Note that the enties in this map are actually smart shared
    //! pointer wrappers, so that the same actual object can be held by both this map
    //! and the historical scraper map without object duplication.
    //!
    typedef std::map<CKeyID, ScraperEntry_ptr> ScraperMap;

    //!
    //! \brief PendingScraperMap. This is not actually used but defined to satisfy the template.
    //!
    typedef ScraperMap PendingScraperMap;

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
    int Initialize() override;

    //!
    //! \brief Gets the block height through which is stored in the scraper entry registry database.
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
    uint64_t PassivateDB() override;

    //!
    //! \brief Specializes the template RegistryDB for the ScraperEntry class. Note that std::set<ScraperEntry> is
    //! not actually used.
    //!
    typedef RegistryDB<ScraperEntry,
                       ScraperEntry,
                       ScraperEntryStatus,
                       ScraperMap,
                       PendingScraperMap,
                       std::set<ScraperEntry>,
                       HistoricalScraperMap> ScraperEntryDB;

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

    ScraperMap m_scrapers;                   //!< Contains the current scraper entries, including entries marked DELETED.
    PendingScraperMap m_pending_scrapers {}; //!< Not actually used for scrapers. To satisfy the template only.

    std::set<ScraperEntry> m_expired_scraper_entries {}; //!< Not actually used for scrapers. To satisfy the template only.

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
