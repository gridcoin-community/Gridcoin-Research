// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONTRACT_REGISTRY_DB_H
#define GRIDCOIN_CONTRACT_REGISTRY_DB_H

#include "dbwrapper.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/support/enumbytes.h"

using LogFlags = BCLog::LogFlags;

namespace GRC {
//!
//! \brief This is a template class generalization of the original beacon registry db in leveldb. This has been made into
//! a template to facilitate usage of the db with strong typing for additional registries, but not have to repeat essentially
//! the same code over and over. The original beacon registry db code has been refactored to use this template, with some
//! necessary specializations.
//!
//! The class template parameters are
//! E:  the entry type
//! SE: the storage entry type. For all except beacons this is the same as E.
//! S:  the entry status enum
//! M:  the map type for the entries
//! P:  the map type for pending entries. This is really only used for beacons. In all other registries it is typedef'd to
//!     the same as M.
//! X:  the map type for expired pending entries. This is really only used for beacons. In all other registries it is typedef'd to
//!     the same as M.
//! H:  the historical map type for historical entries
//!
template<class E, class SE, class S, class M, class P, class X, class H>
class RegistryDB
{
public:
    //!
    //! \brief The RegistryDB template constructor. The parameter is the version for the DB for the registry class.
    //! \param version
    //!
    RegistryDB(uint32_t version)
        : m_version(version)
    {
    };

    //!
    //! \brief Version number of the template entry db. This is dependent on the registry class instantiated, because
    //! different registry classes were done at different times.
    //!
    //! CONSENSUS: Increment this value in the constructor of the registry class when introducing a breaking change
    //! to the corresponding entry class. This will ensure that when the wallet is restarted, the level db entry storage
    //! for the appropriate class will be cleared and reloaded from the contract replay with the correct lookback scope.
    //!
    const uint32_t m_version;

    typedef const std::shared_ptr<E> entry_ptr;

    typedef const entry_ptr entry_option;

    //!
    //! \brief Initializes the template registry entry map structures from the replay of the entry states stored
    //! in the entry database.
    //!
    //! \param entries The map of current entries.
    //! \param pending_entries. The map of pending entries. This is not used in the general template, only in the beacons
    //! specialization.
    //! \param expired_entries. The map of expired pending entries. This is not used in the general template, only in the
    //! beacons specialization.
    //! \param first_entries: The map of the first entry for the given key. This is only currently used for the whitelist
    //! (projects).
    //!
    //! \param populate_first_entries. This is a boolean that controls whether the first entries map is populated.
    //!
    //! \return block height up to and including which the entry records were stored.
    //!
    int Initialize(M& entries, P& pending_entries, X& expired_entries, M& first_entries, const bool& populate_first_entries)
    {
        bool status = true;
        int height = 0;
        uint32_t version = 0;
        std::string key_type = KeyType();

        // First load the KeyType() db version from LevelDB and check it against the constant in the class.
        {
            CTxDB txdb("r");

            std::pair<std::string, std::string> key = std::make_pair(key_type + "_db", "version");

            bool status = txdb.ReadGenericSerializable(key, version);

            if (!status) version = 0;
        }

        if (version != m_version) {
            LogPrint(LogFlags::CONTRACT, "WARNING: %s: Version level of the %s entry db stored in LevelDB, %u, does not "
                                        "match that required in this code level, version %u. Clearing the LevelDB %s entry "
                                        "storage and setting version level to match this code level.",
                     __func__,
                     key_type,
                     version,
                     m_version,
                     key_type);

            clear_leveldb();

            LogPrint(LogFlags::CONTRACT, "INFO: %s: LevelDB %s area cleared. Version level set to %u.",
                     __func__,
                     key_type,
                     m_version);
        }

        // If LoadDBHeight not successful or height is zero then LevelDB has not been initialized before.
        // LoadDBHeight will also set the private member variable m_height_stored from LevelDB for this first call.
        if (!LoadDBHeight(height) || !height) {
            return height;
        } else { // LevelDB already initialized from a prior run.

            // Set m_database_init to true. This will cause LoadDBHeight hereinafter to simply report
            // the value of m_height_stored rather than loading the stored height from LevelDB.
            m_database_init = true;
        }

        LogPrint(LogFlags::CONTRACT, "INFO: %s: db stored height at block %i.",
                 __func__,
                 height);

        // Now load the entries from LevelDB.

        // This temporary map is keyed by record number, which insures the replay down below occurs in the right order.
        StorageMapByRecordNum storage_by_record_num;

        // Code block to scope the txdb object.
        {
            CTxDB txdb("r");

            uint256 hash_hint = uint256();

            // Load the temporary which is similar to m_historical, except the key is by record number not hash.
            status = txdb.ReadGenericSerializablesToMapWithForeignKey(key_type, storage_by_record_num, hash_hint);
        }

        if (!status) {
            if (height > 0){
                // For the height be greater than zero from the height K-V, but the read into the map to fail
                // means the storage in LevelDB must be messed up in the template type area and not be in concordance with
                // the template type K-V's. Therefore clear the whole thing.
                clear();
            }

            // Return height of zero.
            return 0;
        }

        uint64_t recnum_high_watermark = 0;
        uint64_t number_passivated = 0;

        for (const auto& iter : storage_by_record_num) {
            const uint64_t& recnum = iter.first;
            const E& entry = iter.second;

            recnum_high_watermark = std::max(recnum_high_watermark, recnum);

            LogPrint(LogFlags::CONTRACT, "INFO: %s: %s historical entry insert: key %s, value %s, timestamp %" PRId64 ", hash %s, "
                                         "previous_hash %s, status %s, recnum %" PRId64 ".",
                     __func__,
                     key_type,
                     entry.KeyValueToString().first, // key
                     entry.KeyValueToString().second, //value
                     entry.m_timestamp, // timestamp
                     entry.m_hash.GetHex(), // transaction hash
                     entry.m_previous_hash.GetHex(), // prev entry transaction hash
                     entry.StatusToString(), // status
                     recnum
                     );

            // Insert the entry into the historical map.
            m_historical[iter.second.m_hash] = std::make_shared<E>(entry);
            entry_ptr& historical_entry_ptr = m_historical[iter.second.m_hash];

            HandleCurrentHistoricalEntries(entries, pending_entries, expired_entries, first_entries, entry,
                                                historical_entry_ptr, recnum, key_type, populate_first_entries);

            number_passivated += (uint64_t) HandlePreviousHistoricalEntries(historical_entry_ptr);
        } // storage_by_record_num iteration

        LogPrint(LogFlags::CONTRACT, "INFO: %s: number of historical records passivated: %" PRId64 ".",
                 __func__,
                 number_passivated);

        // Set the in-memory record number stored variable to the highest recnum encountered during the replay above.
        m_recnum_stored = recnum_high_watermark;

        // Set the needs passivation flag to true, because the one-by-one passivation done above may not catch everything.
        m_needs_passivation = true;

        return height;
    }

    //!
    //! \brief Handles the insertion/deletion of entries in the current entry map and pending entry map. Note that this
    //! method is specialized for beacons, and the standard template method does not actually use pending_entries.
    //!
    //! \param entries
    //! \param pending_entries
    //! \param expired_entries
    //! \param first_entries
    //! \param entry
    //! \param historical_entry_ptr
    //! \param recnum
    //! \param key_type
    //!
    void HandleCurrentHistoricalEntries(M& entries, P& pending_entries, X& expired_entries, M& first_entries, const E& entry,
                                        entry_ptr& historical_entry_ptr, const uint64_t& recnum,
                                        const std::string& key_type, const bool& populate_first_entries)
    {
        // The unknown or out of bound status conditions should have never made it into leveldb to begin with, since
        // the entry contract will fail validation, but to be thorough, include the filter condition anyway.
        // Unlike beacons, this is a straight replay.
        if (entry.m_status != S::UNKNOWN && entry.m_status != S::OUT_OF_BOUND) {
            LogPrint(LogFlags::CONTRACT, "INFO: %s: %s entry insert: key %s, value %s, timestamp %" PRId64 "; hash %s; "
                                         "previous_hash %s; status %s, recnum %" PRId64 ".",
                     __func__,
                     key_type,
                     entry.KeyValueToString().first, // key
                     entry.KeyValueToString().second, //value
                     entry.m_timestamp, // timestamp
                     entry.m_hash.GetHex(), // transaction hash
                     entry.m_previous_hash.GetHex(), // prev entry transaction hash
                     entry.StatusToString(), // status
                     recnum
                     );

            // Insert or replace the existing map entry with the latest.
            entries[entry.Key()] = historical_entry_ptr;

            if (populate_first_entries && historical_entry_ptr->m_previous_hash.IsNull()) {
                first_entries[entry.Key()] = historical_entry_ptr;
            }
        }
    }

    //!
    //! \brief Handles the passivation of previous historical entries that have been superseded by current entries.
    //!
    //! \param historical_entry_ptr. Shared smart pointer to current historical entry already inserted into historical map.
    //!
    //! \return Boolean - true if a passivation occurred.
    //!
    bool HandlePreviousHistoricalEntries(const entry_ptr& historical_entry_ptr)
    {
        bool passivated = false;

        typename H::iterator prev_historical_iter = m_historical.end();

        // prev_historical_iter here is for purposes of passivation later. If insertion of records by recnum results in a
        // second or succeeding record for the same key, then m_previous_hash will not be null. If the prior record
        // pointed to by that hash is found, then it can be removed from memory, since only the current record by recnum
        // needs to be retained.
        if (!historical_entry_ptr->m_previous_hash.IsNull()) {
            prev_historical_iter = m_historical.find(historical_entry_ptr->m_previous_hash);
        }

        if (prev_historical_iter != m_historical.end()) {
            // Note that passivation is not expected to be successful for every call. See the comments
            // in the passivate() function.
            std::pair<typename H::iterator, bool> passivation_result
                    = passivate(prev_historical_iter);

            passivated = passivation_result.second;
        }

        return passivated;
    }

    //!
    //! \brief Clears the historical entry map of the database. This is only used during testing.
    //!
    void clear_in_memory_only()
    {
        m_historical.clear();
        m_database_init = false;
        m_height_stored = 0;
        m_recnum_stored = 0;
        m_needs_passivation = false;
    }

    //!
    //! \brief Clears the LevelDB entry storage area.
    //!
    //! \return Success or failure.
    //!
    bool clear_leveldb()
    {
        bool status = true;

        CTxDB txdb("rw");

        std::string key_type = KeyType();
        uint256 start_key_hint = uint256();

        status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint);

        key_type = KeyType() + "_db";
        std::string start_key_hint_db {};

        status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_db);

        // We want to write back into LevelDB the revision level of the db in the running code.
        std::pair<std::string, std::string> key = std::make_pair(key_type, "version");
        status &= txdb.WriteGenericSerializable(key, m_version);

        m_height_stored = 0;
        m_recnum_stored = 0;
        m_database_init = false;
        m_needs_passivation = false;

        return status;
    }

    //!
    //! \brief Removes in memory elements for all historical records not in the entries map.
    //! \return Number of elements passivated.
    //!
    uint64_t passivate_db()
    {
        uint64_t number_passivated = 0;

        // Don't bother to go through the historical entry map unless the needs passivation flag is set. This makes
        // this function extremely light for most calls from the periodic schedule.
        if (m_needs_passivation) {
            for (auto iter = m_historical.begin(); iter != m_historical.end(); /*no-op*/) {
                // The passivate function increments the iterator.
                std::pair<typename H::iterator, bool> result = passivate(iter);

                iter = result.first;
                number_passivated += result.second;

            }
        }

        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Passivated %" PRId64 " elements from %s entry db.",
                 __func__,
                 number_passivated,
                 KeyType());

        // Set needs passivation flag to false after passivating the db.
        m_needs_passivation = false;

        return number_passivated;
    }

    //!
    //! \brief Clear the historical map and LevelDB entry storage area.
    //!
    //! \return Success or failure.
    //!
    bool clear()
    {
        clear_in_memory_only();

        return clear_leveldb();
    }

    //!
    //! \brief The number of entry historical elements in the entry database. This includes in memory
    //! entries only and not passivated entries.
    //!
    //! \return The number of elements.
    //!
    size_t size()
    {
        return m_historical.size();
    }

    //!
    //! \brief Returns whether IsContract correction is needed in ReplayContracts during initialization.
    //! \return
    //!
    bool NeedsIsContractCorrection()
    {
        return m_needs_IsContract_correction;
    }

    //!
    //! \brief Sets the state of the IsContract needs correction flag in memory and LevelDB
    //! \param flag The state to set
    //! \return
    //!
    bool SetNeedsIsContractCorrection(bool flag)
    {
        // Update the in-memory flag.
        m_needs_IsContract_correction = flag;

        // Update LevelDB
        CTxDB txdb("rw");

        std::pair<std::string, std::string> key = std::make_pair(KeyType() + "_db", "needs_IsContract_correction");

        return txdb.WriteGenericSerializable(key, m_needs_IsContract_correction);
    }

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
    bool StoreDBHeight(const int& height_stored)
    {
        // Update the in-memory bookmark variable.
        m_height_stored = height_stored;

        // Update LevelDB.
        CTxDB txdb("rw");

        std::string key_name = KeyType() + "_db";

        std::pair<std::string, std::string> key = std::make_pair(key_name, "height_stored");

        return txdb.WriteGenericSerializable(key, height_stored);
    }


    //!
    //! \brief Provides the block height to which the entry db covers. This is persisted in LevelDB.
    //!
    //! \param height_stored
    //!
    //! \return
    //!
    bool LoadDBHeight(int& height_stored)
    {
        bool status = true;

        // If the database has already been initialized (which includes loading the height to what the
        // template type entry storage was updated), then just report the value of m_height_stored, otherwise
        // pull the value from LevelDB.
        if (m_database_init) {
            height_stored = m_height_stored;
        } else {
            CTxDB txdb("r");

            std::string key_name = KeyType() + "_db";

            std::pair<std::string, std::string> key = std::make_pair(key_name, "height_stored");

            bool status = txdb.ReadGenericSerializable(key, height_stored);

            if (!status) height_stored = 0;

            m_height_stored = height_stored;
        }

        return status;
    }

    //!
    //! \brief Insert an entry record into the historical database.
    //!
    //! \param hash The hash for the key to the historical record which is the txid (hash) of the transaction
    //! containing the entry contract.
    //! \param height The height of the block from which the entry record originates.
    //! \param entry The entry record to insert (which includes the appropriate status). Note that this entry
    //! will be cast into the SE type for storage if the SE type is different from the E type. In general if
    //! SE is different from E, it will be to implement different serialization for storage (such as the beacon
    //! implementation).
    //!
    //! \return Success or Failure. This will fail if a record with the same key already exists in the
    //! database.
    //!
    bool insert(const uint256& hash, const int& height, const E& entry)
    {
        bool status = false;

        if (m_historical.find(hash) != m_historical.end()) {
            return status;
        } else {
            LogPrint(LogFlags::CONTRACT, "INFO: %s: store %s entry: key %s, value %s, height %i, timestamp %" PRId64
                     ", hash %s, previous_hash %s, status %s.",
                     __func__,
                     KeyType(),
                     entry.KeyValueToString().first, // key
                     entry.KeyValueToString().second, //value
                     height, // height
                     entry.m_timestamp, // timestamp
                     entry.m_hash.GetHex(), // transaction hash
                     entry.m_previous_hash.GetHex(), // prev entry transaction hash
                     entry.StatusToString() // status
                     );

            m_historical.insert(std::make_pair(hash, std::make_shared<E>(entry)));

            status = Store(hash, static_cast<SE>(entry));

            if (height) {
                status &= StoreDBHeight(height);
            }

            // Set needs passivation flag to true to allow the scheduled passivation to remove unnecessary records from
            // memory.
            m_needs_passivation = true;

            return status;
        }
    }

    //!
    //! \brief Erase a record from the database.
    //!
    //! \param hash The key of the record to erase.
    //!
    //! \return Success or failure.
    //!
    bool erase(const uint256& hash)
    {
        auto iter = m_historical.find(hash);

        if (iter != m_historical.end()) {
            m_historical.erase(hash);
        }

        return Delete(hash);
    }

    //!
    //! \brief Remove an individual in memory element that is backed by LevelDB that is not in the active entry map.
    //!
    //! \param hash The hash that is the key to the element.
    //!
    //! \return A pair, the first part of which is an iterator to the next element, or map::end() if the last one, and
    //! the second is success or failure of the passivation.
    //!
    std::pair<typename H::iterator, bool>
    passivate(typename H::iterator& iter)
    {
        // m_historical itself holds one reference, additional references can be held by the active entries map.
        // If there is only one reference then remove the shared_pointer from m_historical, which will implicitly destroy
        // the shared_pointer object.
        if (iter->second.use_count() == 1) {
            iter = m_historical.erase(iter);
            return std::make_pair(iter, true);
        } else {
            LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Passivate called for historical entry record with hash %s that "
                                               "has existing reference count %li. This is expected under certain situations.",
                     __func__,
                     iter->second->m_hash.GetHex(),
                     iter->second.use_count());

            ++iter;
            return std::make_pair(iter, false);
        }
    }

    //!
    //! \brief Iterator to the beginning of the database records.
    //!
    //! \return Iterator.
    //!
    typename H::iterator begin()
    {
        return m_historical.begin();
    }

    //!
    //! \brief Iterator to end().
    //!
    //! \return Iterator.
    //!
    typename H::iterator end()
    {
        return m_historical.end();
    }

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
    typename H::iterator find(const uint256& hash)
    {
        // See if entry from that ctx_hash is already in the historical map. If so, get iterator.
        auto iter = m_historical.find(hash);

        // If it isn't, attempt to load the entry from LevelDB into the map.
        if (iter == m_historical.end()) {
            SE entry;

            // If the load from LevelDB is successful, insert into the historical map and return the iterator.
            if (Load(hash, entry)) {
                iter = m_historical.insert(std::make_pair(hash, std::make_shared<E>(entry))).first;

                // Set the needs passivation flag to true
                m_needs_passivation = true;
            }
        }

        // Note that if there is no entry in m_historical, and also there is no K-V in LevelDB, then an
        // iterator at end() will be returned.
        return iter;
    }

    //!
    //! \brief Advances the iterator to the next element.
    //!
    //! \param iter
    //!
    //! \return iter
    //!
    typename H::iterator advance(typename H::iterator iter)
    {
        return ++iter;
    }

private:
    //!
    //! \brief Type definition for the storage typename SE entry map used in Initialize. Note that the uint64_t
    //! is the record number, which unfortunately is required to preserve the contract application order
    //! since they are applied in the order of the block's transaction vector rather than the transaction time.
    //!
    typedef std::map<uint256, std::pair<uint64_t, SE>> StorageMap;

    //!
    //! \brief Type definition for the map used to replay state from LevelDB KeyType() entry area.
    //!
    typedef std::map<uint64_t, SE> StorageMapByRecordNum;

    //!
    //! \brief This is a map keyed by uint256 (SHA256) hash that stores the historical entry elements.
    //! It is persisted in LevelDB storage.
    //!
    H m_historical;

    //!
    //! \brief Boolan to indicate whether the database has been successfully initialized from LevelDB during
    //! startup.
    //!
    bool m_database_init = false;

    //!
    //! \brief The block height for entry records stored in the database. This is a bookmark. It is
    //! adjusted by StoreDBHeight, persisted in memory by this private member variable, and persisted in storage
    //! to LevelDB.
    //!
    int m_height_stored = 0;

    //!
    //! \brief The record number stored watermark. This effectively a sequence number for records stored in
    //! the LevelDB entry area. The value in memory will be at the highest record number inserted (or played
    //! back during initialization).
    //!
    uint64_t m_recnum_stored = 0;

    //!
    //! \brief The flag that indicates whether memory optimization can occur by passivating the database. This
    //! flag is set true when find() retrieves an entry element from LevelDB to satisfy a hash search.
    //! This would typically occur on a reorganization (revert).
    //!
    bool m_needs_passivation = false;

    //!
    //! \brief The flag that indicates whether IsContract correction is needed in ReplayContracts during initialization.
    //! This is relevant only for the Beacon Registry specialization.
    //!
    bool m_needs_IsContract_correction = false;

    //!
    //! \brief The function that returns the string value to be used in leveldb as the key prefix for the registry.
    //! For example, the ScaperRegistry uses "scraper". This must be implemented in the class specialization.

    //! \return std::string representing the key prefix to be used for the registry db entries.
    //!
    const std::string KeyType();

    //!
    //! \brief Store an entry object in LevelDB with the provided key value.
    //!
    //! \param hash The SHA256 hash key value for the element.
    //! \param entry The entry historical state element to be stored.
    //!
    //! \return Success or failure.
    //!
    bool Store(const uint256& hash, const SE& entry)
    {
        CTxDB txdb("rw");

        ++m_recnum_stored;

        std::pair<std::string, uint256> key = std::make_pair(KeyType(), hash);

        return txdb.WriteGenericSerializable(key, std::make_pair(m_recnum_stored, entry));
    }

    //!
    //! \brief Load an entry object from LevelDB using the provided key value.
    //!
    //! \param hash The SHA256 hash key value for the element.
    //! \param entry The entry historical state element loaded.
    //!
    //! \return Success or failure.
    //!
    bool Load(const uint256& hash, SE& entry)
    {
        CTxDB txdb("r");

        std::pair<std::string, uint256> key = std::make_pair(KeyType(), hash);

        std::pair<uint64_t, SE> entry_pair;

        bool status = txdb.ReadGenericSerializable(key, entry_pair);

        entry = entry_pair.second;

        return status;
    }

    //!
    //! \brief Delete an entry object from LevelDB with the provided key value (if it exists).
    //!
    //! \param hash The SHA256 hash key value for the element.
    //!
    //! \return Success or failure.
    //!
    bool Delete(const uint256& hash)
    {
        CTxDB txdb("rw");

        std::pair<std::string, uint256> key = std::make_pair(KeyType(), hash);

        return txdb.EraseGenericSerializable(key);
    }

}; // RegistryDB class template

} // namespace GRC

#endif // GRIDCOIN_CONTRACT_REGISTRY_DB_H
