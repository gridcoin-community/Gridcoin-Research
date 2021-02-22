// Copyright (c) 2009-2012 The Bitcoin Developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LEVELDB_H
#define BITCOIN_LEVELDB_H

#include "main.h"
#include "streams.h"

#include <string>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>

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
    // field is non-NULL, writes/deletes go there instead of directly to disk.
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
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(),
                                SER_DISK, CLIENT_VERSION);
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
        activeBatch = NULL;
        return true;
    }

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
    bool ContainsTx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
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
                ssKey.write(iterator->key().data(), iterator->key().size());

                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                ssValue.write(iterator->value().data(), iterator->value().size());

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
                LogPrintf("ERROR: %s: Error %s occurred during retrieval of value during map load from leveldb.",
                         __func__, e.what());
                status = false;
            }

            iterator->Next();
        }

        delete iterator;

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Loaded %u elements from leveldb into map.", __func__, map.size());

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
                ssKey.write(iterator->key().data(), iterator->key().size());

                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                ssValue.write(iterator->value().data(), iterator->value().size());

                T str_key_type;
                ssKey >> str_key_type;

                // Did we reach the end of the data to read?
                if (str_key_type != key_type) break;

                // The actual leveldb key is not used.

                std::pair<K2, V> map_key_value_pair;
                V map_element;
                ssValue >> map_key_value_pair;

                map[map_key_value_pair.first] = map_key_value_pair.second;

            }
            catch (const std::exception& e)
            {
                LogPrintf("ERROR: %s: Error %s occurred during retrieval of value during map load from leveldb.",
                         __func__, e.what());
                status = false;
            }

            iterator->Next();
        }

        delete iterator;

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Loaded %u elements from leveldb into map.", __func__, map.size());

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

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Stored %u elements from map into leveldb.", __func__, map.size());

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

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Stored %u elements from map into leveldb.", __func__, map.size());

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
            ssKey.write(iterator->key().data(), iterator->key().size());

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

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Erased %u elements from leveldb.",
                 __func__,
                 number_erased
                 );

        return status;
    }

    bool LoadBlockIndex();
private:
    bool LoadBlockIndexGuts();
};


#endif // BITCOIN_DB_H
