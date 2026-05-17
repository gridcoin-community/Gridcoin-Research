// Copyright (c) 2009-2012 The Bitcoin Developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DBWRAPPER_H
#define BITCOIN_DBWRAPPER_H

#include "clientversion.h"
#include "main.h"
#include "streams.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>

// Note in Bitcoin these are in txdb.h, and we will eventually move them there when the db code is refactored.
//! -dbcache default (MiB). This is for both bdb (the wallet) and leveldb (the transaction db).
static const int64_t nDefaultDbCache = 100;
//! max. -dbcache (MiB)
//! Note that Bitcoin uses sizeof(void*) > 4 ? 16384 : 1024, but this includes the mempool. In Gridcoin it does not,
//! so we use 1024 as the max.
static const int64_t nMaxDbCache = 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB (leveldb) cache. There is little performance gain over 1024 MB.
static const int64_t nMaxTxIndexCache = 1024;

// Class that provides access to a LevelDB. Note that this class is frequently
// instantiated on the stack and then destroyed again, so instantiation has to
// be very cheap. Unfortunately that means, a CTxDB instance is actually just a
// wrapper around some global state.
//
// A LevelDB is a key/value store that is optimized for fast usage on hard
// disks. It prefers long read/writes to seeks and is based on a series of
// sorted key/value mapping files that are stacked on top of each other, with
// newer files overriding older files. A background thread compacts them
// together when too many files stack up.
//
// Learn more: http://code.google.com/p/leveldb/
class CTxDB
{
public:
    CTxDB(const char* pszMode="r+");
    ~CTxDB() {
        // Note that this is not the same as Close() because it deletes only
        // data scoped to this TxDB object.
        delete activeBatch;
    }

    // Destroys the underlying shared global state accessed by this TxDB.
    void Close();

private:
    leveldb::DB *pdb;  // Points to the global instance.

    // A batch stores up writes and deletes for atomic application. When this
    // field is non-nullptr, writes/deletes go there instead of directly to disk.
    leveldb::WriteBatch *activeBatch;
    leveldb::Options options;
    bool fReadOnly;
    int nVersion;

protected:
    // Returns true and sets (value,false) if activeBatch contains the given key
    // or leaves value alone and sets deleted = true if activeBatch contains a
    // delete for it.
    bool ScanBatch(const CDataStream &key, std::string *value, bool *deleted) const;

    template<typename K, typename T>
    bool Read(const K& key, T& value)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string strValue;

        bool readFromDb = true;
        if (activeBatch) {
            // First we must search for it in the currently pending set of
            // changes to the db. If not found in the batch, go on to read disk.
            bool deleted = false;
            readFromDb = ScanBatch(ssKey, &strValue, &deleted) == false;
            if (deleted) {
                return false;
            }
        }
        if (readFromDb) {
            leveldb::Status status = pdb->Get(leveldb::ReadOptions(),
                                              ssKey.str(), &strValue);
            if (!status.ok()) {
                if (status.IsNotFound())
                    return false;
                // Some unexpected error.
                LogPrintf("LevelDB read failure: %s", status.ToString());
                return false;
            }
        }
        // Unserialize value
        try {
            CDataStream ssValue(MakeByteSpan(strValue), SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        }
        catch (std::exception &e) {
            return false;
        }
        return true;
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value)
    {
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(90000);
        ssValue << value;

        if (activeBatch) {
            activeBatch->Put(ssKey.str(), ssValue.str());
            return true;
        }
        leveldb::Status status = pdb->Put(leveldb::WriteOptions(), ssKey.str(), ssValue.str());
        if (!status.ok()) {
            LogPrintf("LevelDB write failure: %s", status.ToString());
            return false;
        }
        return true;
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        if (activeBatch) {
            activeBatch->Delete(ssKey.str());
            return true;
        }
        leveldb::Status status = pdb->Delete(leveldb::WriteOptions(), ssKey.str());
        return (status.ok() || status.IsNotFound());
    }

    template<typename K>
    bool Exists(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string unused;

        if (activeBatch) {
            bool deleted;
            if (ScanBatch(ssKey, &unused, &deleted) && !deleted) {
                return true;
            }
        }


        leveldb::Status status = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &unused);
        return status.IsNotFound() == false;
    }


public:
    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort()
    {
        delete activeBatch;
        activeBatch = nullptr;
        return true;
    }

    //! Force a fsync of the LevelDB write-ahead log so that all block-index
    //! entries committed up to this point are durable on disk. Used as a
    //! barrier paired with a FileCommit() on the active blk*.dat so the
    //! block index DB cannot reference flat-file data that hasn't itself
    //! been flushed. See issue #2865.
    bool Sync();

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }

    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);

    //! Surgical chainstate cleanup after a Phase 2 abandonment-style rewind
    //! (see node/coherence.h and doc/block_corruption_recovery_design.md).
    //!
    //! Performs one sequential scan of the ("tx", *) keyspace and:
    //!   - Deletes any CTxIndex whose pos.{nFile, nBlockPos} (packed via
    //!     GRC::PackBlockFilePos) is in abandoned_positions. These are the
    //!     CTxIndex entries created by ConnectBlock for txs that lived in
    //!     the abandoned blocks.
    //!   - For any other CTxIndex, clears (sets null) vSpent[i] entries
    //!     whose value is in abandoned_positions. These are the spent
    //!     markers a parent tx received when a child tx in an abandoned
    //!     block spent its output. Without this clear, ConnectInputs would
    //!     silently reject the same input when the canonical chain
    //!     re-supplies the same block.
    //!
    //! The two passes (collect-then-apply) live in one method body because
    //! they share the iterator. Atomic via TxnBegin/TxnCommit -- a crash
    //! mid-scan leaves the txdb unchanged. Returns false on TxnCommit
    //! failure or any iterator error.
    //!
    //! Optional out-params surface counters for the recovery log narrative.
    bool CleanAbandonedRange(const std::unordered_set<uint64_t>& abandoned_positions,
                             uint64_t* out_entries_deleted = nullptr,
                             uint64_t* out_vspent_cleared = nullptr,
                             uint64_t* out_entries_scanned = nullptr);
    bool ContainsTx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool ReadBlockIndex(uint256 hash, CDiskBlockIndex& blockindex);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    //! Erase the on-disk CDiskBlockIndex record for `hash`. Used by the Phase 2
    //! abandonment path (PurgeOrphanedBlockIndexEntries) to make the rewind
    //! durable across restarts -- without this, LoadBlockIndex would rebuild
    //! the ghost forward linkage from stale hashNext fields. Returns true if
    //! LevelDB reports the erase succeeded (note: LevelDB Delete is a no-op
    //! when the key is absent, so "true" does not imply the key existed).
    bool EraseBlockIndex(uint256 hash);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool ReadSyncCheckpoint(uint256& hashCheckpoint);
    bool WriteSyncCheckpoint(uint256 hashCheckpoint);
    bool ReadCheckpointPubKey(std::string& strPubKey);
    bool WriteCheckpointPubKey(const std::string& strPubKey);

	bool ReadGenericData(std::string KeyName, std::string& strValue);
	bool WriteGenericData(const std::string& strKey,const std::string& strData);

    template <typename K, typename V>
    bool ReadGenericSerializable(K& key, V& serialized_data)
    {
        return Read(key, serialized_data);
    }

    template <typename K, typename V>
    bool WriteGenericSerializable(K& key, const V& serializable_data)
    {
        return Write(key, serializable_data);
    }

    template <typename K>
    bool EraseGenericSerializable(K& key)
    {

        return Erase(key);
    }

    //! Public template wrapper around the protected `Exists` primitive. Use to
    //! distinguish "key absent" (Exists returns false, Read would also return
    //! false) from "key present but Read failed" (Exists returns true, Read
    //! returns false) -- the two cases have different operational meanings for
    //! durability-flag-style keys.
    template <typename K>
    bool ExistsGenericSerializable(const K& key)
    {
        return Exists(key);
    }

    template <typename T, typename K, typename V>
    bool ReadGenericSerializablesToMap(T& key_type, std::map<K, V>& map, K& start_key_hint)
    {
        bool status = true;

        leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
        // Seek to start key.
        CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);

        std::pair<T, K> start_key = std::make_pair(key_type, start_key_hint);
        ssStartKey << start_key;
        iterator->Seek(ssStartKey.str());

        while (iterator->Valid())
        {
            try
            {
                // Unpack keys and values.
                CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                ssKey.write(MakeByteSpan(iterator->key()));

                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                ssValue.write(MakeByteSpan(iterator->value()));

                T str_key_type;
                ssKey >> str_key_type;

                // Did we reach the end of the data to read?
                if (str_key_type != key_type) break;

                K map_key;
                ssKey >> map_key;

                V map_element;
                ssValue >> map_element;

                map[map_key] = map_element;

            }
            catch (const std::exception& e)
            {
                LogPrintf("ERROR: %s: Error %s occurred during retrieval of value during map load from LevelDB.",
                         __func__, e.what());
                status = false;
            }

            iterator->Next();
        }

        delete iterator;

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Loaded %u elements from LevelDB into map.", __func__, map.size());

        return status;
    }

    //------- key type -- primary key --- map key --- value
    template <typename T, typename K, typename K2, typename V>
    bool ReadGenericSerializablesToMapWithForeignKey(T& key_type, std::map<K2, V>& map, K& start_key_hint)
    {
        bool status = true;

        leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
        // Seek to start key.
        CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);

        std::pair<T, K> start_key = std::make_pair(key_type, start_key_hint);
        ssStartKey << start_key;
        iterator->Seek(ssStartKey.str());

        while (iterator->Valid())
        {
            try
            {
                // Unpack keys and values.
                CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                ssKey.write(MakeByteSpan(iterator->key()));

                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                ssValue.write(MakeByteSpan(iterator->value()));

                T str_key_type;
                ssKey >> str_key_type;

                // Did we reach the end of the data to read?
                if (str_key_type != key_type) break;

                // The actual LevelDB key is not used.

                std::pair<K2, V> map_key_value_pair;
                V map_element;
                ssValue >> map_key_value_pair;

                map[map_key_value_pair.first] = map_key_value_pair.second;

            }
            catch (const std::exception& e)
            {
                LogPrintf("ERROR: %s: Error %s occurred during retrieval of value during map load from LevelDB.",
                         __func__, e.what());
                status = false;
            }

            iterator->Next();
        }

        delete iterator;

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Loaded %u elements from LevelDB into map.", __func__, map.size());

        return status;
    }

    template <typename T, typename K, typename V>
    bool WriteGenericSerializablesFromMap(T& key_type, std::map<K, V>& map)
    {
        bool status = true;

        for (const auto& iter : map)
        {
            std::pair<T, K> key = std::make_pair(key_type, iter.first);

            status &= Write(key, iter.second);
        }

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Stored %u elements from map into LevelDB.", __func__, map.size());

        return status;
    }

    //------- key type -- primary key --- map key --- value
    template <typename T, typename K, typename K2, typename V>
    bool WriteGenericSerializablesFromMapWithForeignKey(T& key_type, std::map<K2, V>& map, K& primary_key_example)
    {
        bool status = true;

        for (const auto& iter : map)
        {
            std::pair<T, K> key = std::make_pair(key_type, iter.first);

            status &= Write(key, std::make_pair(iter.first, iter.second));
        }

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Stored %u elements from map into LevelDB.", __func__, map.size());

        return status;
    }

    template <typename T, typename K>
    bool EraseGenericSerializablesByKeyType(T& key_type, K& start_key_hint)
    {
        bool status = true;

        leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
        // Seek to start key.
        CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);

        std::pair<T, K> start_key = std::make_pair(key_type, start_key_hint);
        ssStartKey << start_key;
        iterator->Seek(ssStartKey.str());

        unsigned int number_erased = 0;

        while (iterator->Valid())
        {
            // Unpack keys and values.
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            ssKey.write(MakeByteSpan(iterator->key()));

            T str_key_type;
            ssKey >> str_key_type;

            // Did we reach the end of the data to read?
            if (str_key_type != key_type) break;

            K map_key;
            ssKey >> map_key;

            std::pair<T, K> key = std::make_pair(str_key_type, map_key);

            status &= Erase(key);

            number_erased += status;

            iterator->Next();
        }

        delete iterator;

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Erased %u elements from LevelDB.",
                 __func__,
                 number_erased
                 );

        return status;
    }

    bool LoadBlockIndex();
private:
    bool LoadBlockIndexGuts();
};


#endif // BITCOIN_DBWRAPPER_H
