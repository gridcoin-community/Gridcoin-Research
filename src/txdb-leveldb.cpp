// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/version.hpp>

#include <leveldb/env.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#include <leveldb/helpers/memenv/memenv.h>

#include "gridcoin/staking/kernel.h"
#include "txdb.h"
#include "main.h"
#include "ui_interface.h"
#include "util.h"

using namespace std;
using namespace boost;

extern bool fQtActive;

leveldb::DB *txdb; // global pointer for LevelDB object instance

static leveldb::Options GetOptions() {
    leveldb::Options options;
    int nCacheSizeMB = GetArg("-dbcache", 25);
    options.block_cache = leveldb::NewLRUCache(nCacheSizeMB * 1048576);
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    return options;
}

void init_blockindex(leveldb::Options& options, bool fRemoveOld = false) {
    // First time init.
    fs::path directory = GetDataDir() / "txleveldb";

    if (fRemoveOld) {
        fs::remove_all(directory); // remove directory
        unsigned int nFile = 1;

        while (true)
        {
            fs::path strBlockFile = GetDataDir() / strprintf("blk%04u.dat", nFile);

            // Break if no such file
            if( !fs::exists( strBlockFile ) )
                break;

            fs::remove(strBlockFile);

            nFile++;
        }
    }

    fs::create_directory(directory);
    LogPrintf("Opening LevelDB in %s", directory.string());
    leveldb::Status status = leveldb::DB::Open(options, directory.string(), &txdb);
    if (!status.ok()) {
        throw runtime_error(strprintf("init_blockindex(): error opening database environment %s", status.ToString()));
    }
}

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CTxDB::CTxDB(const char* pszMode)
{
    assert(pszMode);
    activeBatch = NULL;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (txdb) {
        pdb = txdb;
        return;
    }

    bool fCreate = strchr(pszMode, 'c');

    options = GetOptions();
    options.create_if_missing = fCreate;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);

    init_blockindex(options); // Init directory
    pdb = txdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        LogPrintf("Transaction index version is %d", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            LogPrintf("Required index version is %d, removing old database", DATABASE_VERSION);

            // Leveldb instance destruction
            delete txdb;
            txdb = pdb = NULL;
            delete activeBatch;
            activeBatch = NULL;

            init_blockindex(options, true); // Remove directory and create new database
            pdb = txdb;

            bool fTmp = fReadOnly;
            fReadOnly = false;
            WriteVersion(DATABASE_VERSION); // Save transaction index version
            fReadOnly = fTmp;
        }
    }
    else if (fCreate)
    {
        bool fTmp = fReadOnly;
        fReadOnly = false;
        WriteVersion(DATABASE_VERSION);
        fReadOnly = fTmp;
    }

    LogPrintf("Opened LevelDB successfully");
}

void CTxDB::Close()
{
    delete txdb;
    txdb = pdb = NULL;
    delete options.filter_policy;
    options.filter_policy = NULL;
    delete options.block_cache;
    options.block_cache = NULL;
    delete activeBatch;
    activeBatch = NULL;
}

bool CTxDB::TxnBegin()
{
    assert(!activeBatch);
    activeBatch = new leveldb::WriteBatch();
    return true;
}

bool CTxDB::TxnCommit()
{
    assert(activeBatch);
    leveldb::Status status = pdb->Write(leveldb::WriteOptions(), activeBatch);
    delete activeBatch;
    activeBatch = NULL;
    if (!status.ok()) {
        LogPrintf("LevelDB batch commit failure: %s", status.ToString());
        return false;
    }
    return true;
}

class CBatchScanner : public leveldb::WriteBatch::Handler {
public:
    std::string needle;
    bool *deleted;
    std::string *foundValue;
    bool foundEntry;

    CBatchScanner() : foundEntry(false) {}

    virtual void Put(const leveldb::Slice& key, const leveldb::Slice& value) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = false;
            *foundValue = value.ToString();
        }
    }

    virtual void Delete(const leveldb::Slice& key) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = true;
        }
    }
};

// When performing a read, if we have an active batch we need to check it first
// before reading from the database, as the rest of the code assumes that once
// a database transaction begins reads are consistent with it. It would be good
// to change that assumption in future and avoid the performance hit, though in
// practice it does not appear to be large.
bool CTxDB::ScanBatch(const CDataStream &key, string *value, bool *deleted) const {
    assert(activeBatch);
    *deleted = false;
    CBatchScanner scanner;
    scanner.needle = key.str();
    scanner.deleted = deleted;
    scanner.foundValue = value;
    leveldb::Status status = activeBatch->Iterate(&scanner);
    if (!status.ok()) {
        throw runtime_error(status.ToString());
    }
    return scanner.foundEntry;
}

bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    // Add to tx index
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();

    return Erase(make_pair(string("tx"), hash));
}

bool CTxDB::ContainsTx(uint256 hash)
{
    return Exists(make_pair(string("tx"), hash));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

bool CTxDB::ReadGenericData(std::string KeyName, std::string& strValue)
{
    return Read(string(KeyName.c_str()), strValue);
}

bool CTxDB::WriteGenericData(const std::string& strKey,const std::string& strData)
{
    return Write(string(strKey), strData);
}

static CBlockIndex *InsertBlockIndex(const uint256& hash)
{
    if (hash.IsNull())
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = GRC::BlockIndexPool::GetNextBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

//Halford - todo - 6/19/2015 - Load block index on dedicated thread to decrease startup time by 90% - move checkblocks to separate thread

bool CTxDB::LoadBlockIndex()
{
    int64_t nStart = GetTimeMillis();
    int nHighest = 0;
    uint32_t nBlockCount = 0;

    if (mapBlockIndex.size() > 0) {
        // Already loaded once in this session. It can happen during migration
        // from BDB.
        return true;
    }
    // The block index is an in-memory structure that maps hashes to on-disk
    // locations where the contents of the block can be found. Here, we scan it
    // out of the DB and into mapBlockIndex.
    leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
    // Seek to start key.
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256());
    iterator->Seek(ssStartKey.str());

    int nLoaded = 0;

    // Now read each entry.
    LogPrintf("Loading DiskIndex %d",nHighest);
    while (iterator->Valid())
    {
        // Unpack keys and values.
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iterator->key().data(), iterator->key().size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(iterator->value().data(), iterator->value().size());
        string strType;
        ssKey >> strType;
        // Did we reach the end of the data to read?
        if (fRequestShutdown || strType != "blockindex")
            break;
        CDiskBlockIndex diskindex;
        ssValue >> diskindex;

        uint256 blockHash = diskindex.GetBlockHash();

        // Construct block index object
        CBlockIndex* pindexNew    = InsertBlockIndex(blockHash);
        pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
        pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
        pindexNew->nFile          = diskindex.nFile;
        pindexNew->nBlockPos      = diskindex.nBlockPos;
        pindexNew->nHeight        = diskindex.nHeight;
        pindexNew->nMoneySupply   = diskindex.nMoneySupply;
        pindexNew->nFlags         = diskindex.nFlags;
        pindexNew->nStakeModifier = diskindex.nStakeModifier;
        pindexNew->hashProof      = diskindex.hashProof;
        pindexNew->nVersion       = diskindex.nVersion;
        pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
        pindexNew->nTime          = diskindex.nTime;
        pindexNew->nBits          = diskindex.nBits;
        pindexNew->nNonce         = diskindex.nNonce;
        pindexNew->m_researcher   = diskindex.m_researcher;

        nBlockCount++;
        // Watch for genesis block
        if (pindexGenesisBlock == NULL && blockHash == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
            pindexGenesisBlock = pindexNew;

        if(fQtActive)
        {
            if ((pindexNew->nHeight % 10000) == 0)
            {
                nLoaded +=10000;
                if (nLoaded > nHighest) nHighest=nLoaded;
                if (nHighest < nGrandfather) nHighest=nGrandfather;
                uiInterface.InitMessage(strprintf("%" PRId64 "/%" PRId64 " %s", nLoaded, nHighest, _("Blocks Loaded")));
                fprintf(stdout,"%d ",nLoaded); fflush(stdout);
            }
        }

        iterator->Next();
    }
    delete iterator;


    LogPrintf("Time to memorize diskindex containing %i blocks : %15" PRId64 "ms", nBlockCount, GetTimeMillis() - nStart);
    nStart = GetTimeMillis();


    // Load hashBestChain pointer to end of best chain
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;

    LogPrintf("LoadBlockIndex(): hashBestChain=%s  height=%d  date=%s",
      hashBestChain.ToString().substr(0,20),
      nBestHeight,
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()));

    nLoaded = 0;
    // Verify blocks in the best chain
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg( "-checkblocks", 1000);

    LogPrintf("Verifying last %i blocks at level %i", nCheckDepth, nCheckLevel);
    CBlockIndex* pindexFork = NULL;
    map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
    for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
    {
        if (fRequestShutdown || pindex->nHeight < nBestHeight-nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        // check level 1: verify block validity
        // check level 7: verify block signature too

        if(fQtActive)
        {
            if ((pindex->nHeight % 1000) == 0)
            {
                nLoaded +=1000;
                if (nLoaded > nHighest) nHighest=nLoaded;
                if (nHighest < nGrandfather) nHighest=nGrandfather;
                uiInterface.InitMessage(strprintf("%" PRId64 "/%" PRId64 " %s", nLoaded, nHighest, _("Blocks Verified")));
            }
        }

        if (nCheckLevel>0 && !block.CheckBlock(pindex->nHeight, true, true, (nCheckLevel>6), true))
        {
            LogPrintf("LoadBlockIndex() : *** found bad block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            pindexFork = pindex->pprev;
        }
        // check level 2: verify transaction index validity
        if (nCheckLevel>1)
        {
            pair<unsigned int, unsigned int> pos = make_pair(pindex->nFile, pindex->nBlockPos);
            mapBlockPos[pos] = pindex;
            for (auto const& tx : block.vtx)
            {
                uint256 hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex))
                {
                    // check level 3: checker transaction hashes
                    if (nCheckLevel>2 || pindex->nFile != txindex.pos.nFile || pindex->nBlockPos != txindex.pos.nBlockPos)
                    {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos))
                        {
                            LogPrintf("LoadBlockIndex() : *** cannot read mislocated transaction %s", hashTx.ToString());
                            pindexFork = pindex->pprev;
                        }
                        else
                            if (txFound.GetHash() != hashTx) // not a duplicate tx
                            {
                                LogPrintf("LoadBlockIndex(): *** invalid tx position for %s", hashTx.ToString());
                                pindexFork = pindex->pprev;
                            }
                    }
                    // check level 4: check whether spent txouts were spent within the main chain
                    unsigned int nOutput = 0;
                    if (nCheckLevel>3)
                    {
                        for (auto const& txpos : txindex.vSpent)
                        {
                            if (!txpos.IsNull())
                            {
                                pair<unsigned int, unsigned int> posFind = make_pair(txpos.nFile, txpos.nBlockPos);
                                if (!mapBlockPos.count(posFind))
                                {
                                    LogPrintf("LoadBlockIndex(): *** found bad spend at %d, hashBlock=%s, hashTx=%s", pindex->nHeight, pindex->GetBlockHash().ToString(), hashTx.ToString());
                                    pindexFork = pindex->pprev;
                                }
                                // check level 6: check whether spent txouts were spent by a valid transaction that consume them
                                if (nCheckLevel>5)
                                {
                                    CTransaction txSpend;
                                    if (!txSpend.ReadFromDisk(txpos))
                                    {
                                        LogPrintf("LoadBlockIndex(): *** cannot read spending transaction of %s:%i from disk", hashTx.ToString(), nOutput);
                                        pindexFork = pindex->pprev;
                                    }
                                    else if (!txSpend.CheckTransaction())
                                    {
                                        LogPrintf("LoadBlockIndex(): *** spending transaction of %s:%i is invalid", hashTx.ToString(), nOutput);
                                        pindexFork = pindex->pprev;
                                    }
                                    else
                                    {
                                        bool fFound = false;
                                        for (auto const& txin : txSpend.vin)
                                            if (txin.prevout.hash == hashTx && txin.prevout.n == nOutput)
                                                fFound = true;
                                        if (!fFound)
                                        {
                                            LogPrintf("LoadBlockIndex(): *** spending transaction of %s:%i does not spend it", hashTx.ToString(), nOutput);
                                            pindexFork = pindex->pprev;
                                        }
                                    }
                                }
                            }
                            nOutput++;
                        }
                    }
                }
                // check level 5: check whether all prevouts are marked spent
                if (nCheckLevel>4)
                {
                    for (auto const& txin : tx.vin)
                    {
                        CTxIndex txindex;
                        if (ReadTxIndex(txin.prevout.hash, txindex))
                            if (txindex.vSpent.size()-1 < txin.prevout.n || txindex.vSpent[txin.prevout.n].IsNull())
                            {
                                LogPrintf("LoadBlockIndex(): *** found unspent prevout %s:%i in %s", txin.prevout.hash.ToString(), txin.prevout.n, hashTx.ToString());
                                pindexFork = pindex->pprev;
                            }
                    }
                }
            }
        }
    }



    LogPrintf("Time to Verify Blocks %15" PRId64 "ms", GetTimeMillis() - nStart);

    if (pindexFork && !fRequestShutdown)
    {
        // Reorg back to the fork
        LogPrintf("LoadBlockIndex() : *** moving best chain pointer back to block %d", pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        CTxDB txdb;
        SetBestChain(txdb, block, pindexFork);
    }

    return true;
}
