// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "amount.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "index/disktxpos.h"
#include "index/txindex.h"
#include "util.h"
#include "net.h"
#include "gridcoin/block_index.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/cpid.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "script.h"
#include "scrypt.h"
#include "validation.h"

#include <map>
#include <unordered_map>
#include <set>

class CWallet;
class CBlock;
class CBlockIndex;
class CKeyItem;
class CReserveKey;
class COutPoint;
class CAddress;
class CInv;
class CNode;
class CTxMemPool;

namespace GRC {
class Claim;
class SuperblockPtr;

//!
//! \brief An optional type that either contains some claim object or does not.
//!
typedef boost::optional<Claim> ClaimOption;
}

static const int64_t DEFAULT_CBR = 10 * COIN;

/** Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp. */
static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Genesis - MainNet - Production Genesis: as of 10-20-2014:
static const uint256 hashGenesisBlock = uint256S("0x000005a247b397eadfefa58e872bc967c2614797bdc8d4d0e6b09fea5c191599");

//TestNet Genesis:
static const uint256 hashGenesisBlockTestNet = uint256S("0x00006e037d7b84104208ecf2a8638d23149d712ea810da604ee2f2cb39bae713");
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline int64_t FutureDrift(int64_t nTime, int nHeight) { return nTime + 20 * 60; }
inline unsigned int GetTargetSpacing(int nHeight) { return IsProtocolV2(nHeight) ? 90 : 60; }

struct BlockHasher
{
    size_t operator()(const uint256& hash) const { return hash.GetUint64(); }
};

typedef std::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;

extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern BlockMap mapBlockIndex;
extern CBlockIndex* pindexGenesisBlock;
extern unsigned int nStakeMinAge;
extern unsigned int nStakeMaxAge;
extern unsigned int nNodeLifespan;
extern int nCoinbaseMaturity;
extern int nBestHeight;
extern arith_uint256 nBestChainTrust;
extern uint256 hashBestChain;
extern CBlockIndex* pindexBest;
extern const std::string strMessageMagic;
extern CCriticalSection cs_setpwalletRegistered;
extern std::set<CWallet*> setpwalletRegistered;
extern unsigned char pchMessageStart[4];
extern std::map<uint256, CBlock*> mapOrphanBlocks;

// Settings
extern int64_t nTransactionFee;
extern int64_t nReserveBalance;
extern int64_t nMinimumInputValue;

extern bool fUseFastIndex;
extern unsigned int nDerivationMethodIndex;

extern bool fEnforceCanonical;

// Minimum disk space required - used in CheckDiskSpace()
static const uint64_t nMinDiskSpace = 52428800;

extern std::string  msMiningErrors;
extern std::string  msMiningErrorsIncluded;
extern std::string  msMiningErrorsExcluded;

extern int nGrandfather;
extern int nNewIndex;
extern int nNewIndex2;

class GlobalStatus
{
public:
    GlobalStatus()
    {
        update_time = 0;

        blocks = 0;
        difficulty = 0.0;
        netWeight = 0.0;
        coinWeight = 0.0;
        etts = 0.0;

        able_to_stake = false;
        staking = false;

        ReasonNotStaking = std::string();
        errors = std::string();
    }

    struct globalStatusType
    {
        int64_t update_time;

        int blocks;
        double difficulty;
        double netWeight;
        double coinWeight;
        double etts;

        bool able_to_stake;
        bool staking;

        std::string ReasonNotStaking;
        std::string errors;
    };

    struct globalStatusStringType
    {
        std::string blocks;
        std::string difficulty;
        std::string netWeight;
        std::string coinWeight;

        std::string errors;
    };

    void SetGlobalStatus(bool force = false);
    const globalStatusType GetGlobalStatus();
    const globalStatusStringType GetGlobalStatusStrings();

private:
    std::atomic<int64_t> update_time;

    std::atomic<int> blocks;
    std::atomic<double> difficulty;
    std::atomic<double> netWeight;
    std::atomic<double> coinWeight;
    std::atomic<double> etts;

    std::atomic<bool> able_to_stake;
    std::atomic<bool> staking;

    // This lock is only needed to protect the ReasonNotStaking and errors string.
    CCriticalSection cs_errors_lock;

    std::string ReasonNotStaking;
    std::string errors;
};

extern GlobalStatus g_GlobalStatus;

class CReserveKey;
class CTxDB;
class CTxIndex;

void RegisterWallet(CWallet* pwalletIn);
void UnregisterWallet(CWallet* pwalletIn);
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock = NULL, bool fUpdate = false, bool fConnect = true);
bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool Generated_By_Me);
bool CheckDiskSpace(uint64_t nAdditionalBytes=0);
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
FILE* AppendBlockFile(unsigned int& nFileRet);
bool LoadBlockIndex(bool fAllowNew=true);
void PrintBlockTree();

bool ProcessMessages(CNode* pfrom);
bool SendMessages(CNode* pto, bool fSendTrickle);
bool LoadExternalBlockFile(FILE* fileIn);

bool CheckProofOfWork(uint256 hash, unsigned int nBits);
GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex);

int GetNumBlocksOfPeers();
bool IsInitialBlockDownload();
std::string GetWarnings(std::string strFor);
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock);
void ResendWalletTransactions(bool fForce = false);
bool OutOfSyncByAge();

/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx,
                        bool* pfMissingInputs);
bool SetBestChain(CTxDB& txdb, CBlock &blockNew, CBlockIndex* pindexNew);


/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    int GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const;
public:
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    // memory only
    mutable bool fMerkleVerified;


    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock.SetNull();
        nIndex = -1;
        fMerkleVerified = false;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITEAS(CTransaction, *this);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    int SetMerkleBranch(const CBlock* pblock=NULL);

    // Return depth of transaction in blockchain:
    // -1  : not in blockchain, and not in memory pool (conflicted transaction)
    //  0  : in memory pool, waiting to be included in a block
    // >=1 : this many blocks deep in the main chain
    int GetDepthInMainChain(CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChainINTERNAL(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool();
};



/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 *
 * Blocks are appended to blk0001.dat files on disk.  Their location on disk
 * is indexed by CBlockIndex objects in memory.
 */
class CBlockHeader
{
public:
    static const int32_t CURRENT_VERSION = 11;

    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);

        // Besides early blocks, Gridcoin uses Proof-of-Stake for consensus,
        // so we don't need the nonce field. Don't serialize it after blocks
        // version 11 and later:
        //
        if (nVersion <= 10) {
            READWRITE(nNonce);
        }
    }

    void SetNull()
    {
        nVersion = CURRENT_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        m_hash_cache.SetNull();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash(const bool use_cache = false) const
    {
        // The block hash cache field prevents repeated computations of the
        // block's hash in the block acceptance pipeline. It's particularly
        // effective for early blocks with expensive scrypt hashes. Dynamic
        // caching isn't the prettiest solution, but it provides an interim
        // performance advantage as we refactor legacy code.
        //
        // use_cache defaults to false to discourage use of the cache except
        // in carefully chosen single-threaded scenarios. Avoid hash caching
        // for block objects except where thread-safety is obvious and where
        // performance improves significantly.
        //
        if (use_cache) {
            if (!m_hash_cache.IsNull()) {
                return m_hash_cache;
            }

            m_hash_cache = ComputeHash();

            return m_hash_cache;
        }

        return ComputeHash();
    }

    uint256 GetPoWHash() const
    {
        return scrypt_blockhash(CVOIDBEGIN(nVersion));
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

private:
    mutable uint256 m_hash_cache;

    uint256 ComputeHash() const
    {
        if (nVersion >= 7) {
            return SerializeHash(*this);
        }

        return GetPoWHash();
    }
};

namespace GRC {
//!
//! \brief A report that contains the calculated subsidy claimed in a block.
//! Produced by the CBlock::GetMint() method.
//!
class MintSummary
{
public:
    CAmount m_total = 0; //!< Total value claimed by the block producer.
    CAmount m_fees = 0;  //!< Fees paid for the block's transactions.
};
}

class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable std::vector<uint256> vMerkleTree;

    // Denial-of-service detection:
    mutable int nDoS;
    bool DoS(int nDoSIn, bool fIn) const { nDoS += nDoSIn; return fIn; }

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITEAS(CBlockHeader, *this);

        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(s.GetType() & (SER_GETHASH|SER_BLOCKHEADERONLY))) {
            READWRITE(vtx);
            READWRITE(vchBlockSig);
        } else if (ser_action.ForRead()) {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        }
    }

    void SetNull()
    {
        CBlockHeader::SetNull();

        vtx.clear();
        vchBlockSig.clear();
        vMerkleTree.clear();
        nDoS = 0;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    const GRC::Claim& GetClaim() const;
    GRC::Claim PullClaim();
    GRC::SuperblockPtr GetSuperblock() const;
    GRC::SuperblockPtr GetSuperblock(const CBlockIndex* const pindex) const;
    GRC::MintSummary GetMint() const;

    // entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit() const
    {
        // Take last bit of block hash as entropy bit
        unsigned int nEntropyBit = ((GetHash(true).GetUint64()) & 1llu);
        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && GetBoolArg("-printstakemodifier"))
            LogPrintf("GetStakeEntropyBit: hashBlock=%s nEntropyBit=%u", GetHash(true).ToString(), nEntropyBit);
        return nEntropyBit;
    }

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const
    {
        return (vtx.size() > 1 && vtx[1].IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !IsProofOfStake();
    }

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const
    {
        int64_t maxTransactionTime = 0;
        for (auto const& tx : vtx)
            maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx.nTime);
        return maxTransactionTime;
    }

    uint256 BuildMerkleTree() const
    {
        vMerkleTree.clear();
        for (auto const& tx : vtx)
            vMerkleTree.push_back(tx.GetHash());
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            for (int i = 0; i < nSize; i += 2)
            {
                int i2 = std::min(i+1, nSize-1);
                vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                           BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
            }
            j += nSize;
        }
        return (vMerkleTree.empty() ? uint256() : vMerkleTree.back());
    }

    std::vector<uint256> GetMerkleBranch(int nIndex) const
    {
        if (vMerkleTree.empty())
            BuildMerkleTree();
        std::vector<uint256> vMerkleBranch;
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            int i = std::min(nIndex^1, nSize-1);
            vMerkleBranch.push_back(vMerkleTree[j+i]);
            nIndex >>= 1;
            j += nSize;
        }
        return vMerkleBranch;
    }

    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
    {
        if (nIndex == -1)
            return uint256();
        for (auto const& otherside : vMerkleBranch)
        {
            if (nIndex & 1)
                hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
            else
                hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
            nIndex >>= 1;
        }
        return hash;
    }


    bool WriteToDisk(unsigned int& nFileRet, unsigned int& nBlockPosRet)
    {
        // Open history file to append
        CAutoFile fileout(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
        if (fileout.IsNull())
            return error("CBlock::WriteToDisk() : AppendBlockFile failed");

        // Write index header
        unsigned int nSize = GetSerializeSize(fileout, *this);
        fileout << pchMessageStart << nSize;

        // Write block
        long fileOutPos = ftell(fileout.Get());
        if (fileOutPos < 0)
            return error("CBlock::WriteToDisk() : ftell failed");
        nBlockPosRet = fileOutPos;
        fileout << *this;

        // Flush stdio buffers and commit to disk before returning
        fflush(fileout.Get());
        if (!IsInitialBlockDownload() || (nBestHeight+1) % 5000 == 0)
            FileCommit(fileout.Get());

        return true;
    }

    bool ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true)
    {
        SetNull();

        const int ser_flags = SER_DISK | (fReadTransactions ? 0 : SER_BLOCKHEADERONLY);

        // Open history file to read
        CAutoFile filein(OpenBlockFile(nFile, nBlockPos, "rb"), ser_flags, CLIENT_VERSION);
        if (filein.IsNull())
            return error("CBlock::ReadFromDisk() : OpenBlockFile failed");

        // Read block
        try {
            filein >> *this;
        }
        catch (std::exception &e) {
            return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
        }

        // Check the header
        if (fReadTransactions && IsProofOfWork() && !CheckProofOfWork(GetHash(true), nBits))
            return error("CBlock::ReadFromDisk() : errors in block header");

        return true;
    }



    void print() const
    {
        LogPrintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%" PRIszu ", vchBlockSig=%s)",
            GetHash().ToString(),
            nVersion,
            hashPrevBlock.ToString(),
            hashMerkleRoot.ToString(),
            nTime, nBits, nNonce,
            vtx.size(),
            HexStr(vchBlockSig.begin(), vchBlockSig.end()));
        for (unsigned int i = 0; i < vtx.size(); i++)
        {
            LogPrintf("  ");
            vtx[i].print();
        }
        LogPrintf("  vMerkleTree: ");
        for (unsigned int i = 0; i < vMerkleTree.size(); i++)
            LogPrintf("%s", vMerkleTree[i].ToString().substr(0,10));
    }


    bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck=false);
    bool ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions=true);
    bool AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof);
    bool CheckBlock(int height1, bool fCheckPOW=true, bool fCheckMerkleRoot=true, bool fCheckSig=true, bool fLoadingIndex=false) const;
    bool AcceptBlock(bool generated_by_me);
    bool CheckBlockSignature() const;

private:
};

/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block.  pprev and pnext link a path through the
 * main/longest chain.  A blockindex may have multiple pprev pointing back
 * to it, but pnext will only point forward to the longest branch, or will
 * be null if the block is not part of the longest chain.
 */
class CBlockIndex
{
public:
    const uint256* phashBlock;
    CBlockIndex* pprev;
    CBlockIndex* pnext;
    unsigned int nFile;
    unsigned int nBlockPos;
    int64_t nMoneySupply;
    GRC::ResearcherContext* m_researcher;
    int nHeight;

    unsigned int nFlags;  // ppcoin: block index flags
    enum
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier

        // Gridcoin
        EMPTY_CPID           = (1 << 3), // CPID is empty
        INVESTOR_CPID        = (1 << 4), // CPID equals "INVESTOR"
        SUPERBLOCK           = (1 << 5), // Block contains a superblock
        CONTRACT             = (1 << 6), // Block contains a contract
    };

    uint64_t nStakeModifier; // hash modifier for proof-of-stake
    uint256 hashProof;

    // block header
    int nVersion;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;
    uint256 hashMerkleRoot;

    CBlockIndex()
    {
        SetNull();
    }

    CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, CBlock& block)
    {
        SetNull();

        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        if (block.IsProofOfStake())
        {
            SetProofOfStake();
        }

        nVersion       = block.nVersion;
        hashMerkleRoot = block.hashMerkleRoot;
        nTime          = block.nTime;
        nBits          = block.nBits;
        nNonce         = block.nNonce;
    }

    void SetNull()
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = 0;
        nBlockPos = 0;
        nHeight = 0;
        nMoneySupply = 0;
        nFlags = EMPTY_CPID;
        nStakeModifier = 0;
        hashProof.SetNull();

        nVersion       = 0;
        hashMerkleRoot.SetNull();
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;

        m_researcher = nullptr;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        if (pprev)
            block.hashPrevBlock = pprev->GetBlockHash();
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    arith_uint256 GetBlockTrust() const;

    bool IsInMainChain() const
    {
        return (pnext || this == pindexBest);
    }

    int64_t GetPastTimeLimit() const
    {
        return GetMedianTimePast();
    }

    enum { nMedianTimeSpan=11 };

    int64_t GetMedianTimePast() const
    {
        int64_t pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    bool IsProofOfWork() const
    {
        return !(nFlags & BLOCK_PROOF_OF_STAKE);
    }

    bool IsProofOfStake() const
    {
        return (nFlags & BLOCK_PROOF_OF_STAKE);
    }

    void SetProofOfStake()
    {
        nFlags |= BLOCK_PROOF_OF_STAKE;
    }

    bool IsUserCPID() const
    {
        return !(nFlags & (INVESTOR_CPID | EMPTY_CPID));
    }

    unsigned int GetStakeEntropyBit() const
    {
        return ((nFlags & BLOCK_STAKE_ENTROPY) >> 1);
    }

    bool SetStakeEntropyBit(unsigned int nEntropyBit)
    {
        if (nEntropyBit > 1)
            return false;
        nFlags |= (nEntropyBit? BLOCK_STAKE_ENTROPY : 0);
        return true;
    }

    bool GeneratedStakeModifier() const
    {
        return (nFlags & BLOCK_STAKE_MODIFIER);
    }

    void SetStakeModifier(uint64_t nModifier, bool fGeneratedStakeModifier)
    {
        nStakeModifier = nModifier;
        if (fGeneratedStakeModifier)
            nFlags |= BLOCK_STAKE_MODIFIER;
    }

    void SetResearcherContext(
        const GRC::MiningId mining_id,
        const int64_t research_subsidy,
        const double magnitude)
    {
        nFlags &= ~(EMPTY_CPID | INVESTOR_CPID);

        if (const auto cpid_option = mining_id.TryCpid()) {
            if (research_subsidy > 0) {
                if (!m_researcher) {
                    m_researcher = GRC::BlockIndexPool::GetNextResearcherContext();
                }

                m_researcher->m_cpid = *cpid_option;
                m_researcher->m_research_subsidy = research_subsidy;
                m_researcher->m_magnitude = magnitude;

                return;
            }
        }

        if (m_researcher) {
            delete m_researcher;
            m_researcher = nullptr;
        }

        if (mining_id.Which() == GRC::MiningId::Kind::INVALID) {
            nFlags |= EMPTY_CPID;
        } else {
            nFlags |= INVESTOR_CPID;
        }
    }

    GRC::MiningId GetMiningId() const
    {
        if (m_researcher)
            return GRC::MiningId(m_researcher->m_cpid);

        if (nFlags & INVESTOR_CPID)
            return GRC::MiningId::ForInvestor();

        return GRC::MiningId();
    }

    int64_t ResearchSubsidy() const
    {
        if (m_researcher) {
            return m_researcher->m_research_subsidy;
        }

        return 0;
    }

    double Magnitude() const
    {
        if (m_researcher) {
            return m_researcher->m_magnitude;
        }

        return 0;
    }

    bool IsSuperblock() const
    {
        return nFlags & SUPERBLOCK;
    }

    void MarkAsSuperblock()
    {
        nFlags |= SUPERBLOCK;
    }

    bool IsContract() const
    {
        return nFlags & CONTRACT;
    }

    void MarkAsContract()
    {
        nFlags |= CONTRACT;
    }

    std::string ToString() const
    {
        return strprintf("CBlockIndex(nprev=%p, pnext=%p, nFile=%u, nBlockPos=%-6d nHeight=%d, nMoneySupply=%s, nFlags=(%s)(%d)(%s), nStakeModifier=%016" PRIx64 ", hashProof=%s, merkle=%s, hashBlock=%s)",
            pprev, pnext, nFile, nBlockPos, nHeight,
            FormatMoney(nMoneySupply),
            GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(), IsProofOfStake()? "PoS" : "PoW",
            nStakeModifier,
            hashProof.ToString(),
            hashMerkleRoot.ToString(),
            GetBlockHash().ToString());
    }

    void print() const
    {
        LogPrintf("%s", ToString());
    }
};



/** Used to marshal pointers into hashes for db storage. */
class CDiskBlockIndex : public CBlockIndex
{
private:
    uint256 blockHash;

public:
    uint256 hashPrev;
    uint256 hashNext;

    CDiskBlockIndex()
    {
        hashPrev.SetNull();
        hashNext.SetNull();
        blockHash.SetNull();
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : uint256());
        hashNext = (pnext ? pnext->GetBlockHash() : uint256());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }

        READWRITE(hashNext);
        READWRITE(nFile);
        READWRITE(nBlockPos);
        READWRITE(nHeight);
        int64_t nMint = 0; // removed
        READWRITE(nMint);
        READWRITE(nMoneySupply);
        READWRITE(nFlags);
        READWRITE(nStakeModifier);

        // Duplicate stake detection removed from the block index:
        if (IsProofOfStake()) {
            COutPoint prevoutStake;
            uint32_t nStakeTime = 0;

            READWRITE(prevoutStake);
            READWRITE(nStakeTime);
        }

        READWRITE(hashProof);

        // block header
        READWRITE(nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(blockHash);

        //7-11-2015 - Gridcoin - New Accrual Fields (Note, Removing the deterministic block number to make this happen all the time):
        std::string cpid_hex = GetMiningId().ToString();
        double research_subsidy_grc = ResearchSubsidy() / (double)COIN;
        double interest_subsidy_grc = 0; // removed
        double magnitude = Magnitude();

        READWRITE(cpid_hex);
        READWRITE(research_subsidy_grc);
        READWRITE(interest_subsidy_grc);
        READWRITE(magnitude);

        // Superblock and contract flags merged into nFlags:
        uint32_t is_superblock = this->IsSuperblock();
        uint32_t is_contract = this->IsContract();

        if (this->nHeight > nNewIndex2) {
            READWRITE(is_superblock);
            READWRITE(is_contract);

            std::string dummy;

            // Blocks used to contain the GRC address.
            READWRITE(dummy);

            // Blocks used to come with a reserved string. Keep (de)serializing
            // it until it's used.
            READWRITE(dummy);
        }

        // Translate legacy disk format to memory format:
        //
        if (ser_action.ForRead()) {
            NCONST_PTR(this)->SetResearcherContext(
                GRC::MiningId::Parse(cpid_hex),
                research_subsidy_grc * COIN,
                magnitude);

            if (is_superblock == 1) {
                NCONST_PTR(this)->MarkAsSuperblock();
            }

            if (is_contract == 1) {
                NCONST_PTR(this)->MarkAsContract();
            }
        }
    }

    uint256 GetBlockHash() const
    {
        if (fUseFastIndex && (nTime < GetAdjustedTime() - 24 * 60 * 60) && !blockHash.IsNull())
            return blockHash;

        CBlockHeader block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;

        const_cast<CDiskBlockIndex*>(this)->blockHash = block.GetHash();

        return blockHash;
    }

    std::string ToString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
            GetBlockHash().ToString(),
            hashPrev.ToString(),
            hashNext.ToString());
        return str;
    }

    void print() const
    {
        LogPrintf("%s", ToString());
    }
};





/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
class CBlockLocator
{
protected:
    std::vector<uint256> vHave;
public:

    CBlockLocator()
    {
    }

    explicit CBlockLocator(const CBlockIndex* pindex)
    {
        Set(pindex);
    }

    explicit CBlockLocator(uint256 hashBlock)
    {
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
            Set((*mi).second);
    }

    CBlockLocator(const std::vector<uint256>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            int nVersion = s.GetVersion();
            READWRITE(nVersion);
        }

        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull()
    {
        return vHave.empty();
    }

    void Set(const CBlockIndex* pindex)
    {
        vHave.clear();
        int nStep = 1;
        while (pindex)
        {
            vHave.push_back(pindex->GetBlockHash());

            // Exponentially larger steps back
            for (int i = 0; pindex && i < nStep; i++)
                pindex = pindex->pprev;
            if (vHave.size() > 10)
                nStep *= 2;
        }
        vHave.push_back((!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
    }

    int GetDistanceBack()
    {
        // Retrace how far back it was in the sender's branch
        int nDistance = 0;
        int nStep = 1;
        for (auto const& hash : vHave)
        {
            BlockMap::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return nDistance;
            }
            nDistance += nStep;
            if (nDistance > 10)
                nStep *= 2;
        }
        return nDistance;
    }

    CBlockIndex* GetBlockIndex()
    {
        // Find the first block the caller has in the main chain
        for (auto const& hash : vHave)
        {
            BlockMap::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return pindex;
            }
        }
        return pindexGenesisBlock;
    }

    uint256 GetBlockHash()
    {
        // Find the first block the caller has in the main chain
        for (auto const& hash : vHave)
        {
            BlockMap::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return hash;
            }
        }
        return (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet);
    }

    int GetHeight()
    {
        CBlockIndex* pindex = GetBlockIndex();
        if (!pindex)
            return 0;
        return pindex->nHeight;
    }
};






class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTransaction> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;

    bool addUnchecked(const uint256& hash, CTransaction &tx);
    bool remove(const CTransaction &tx, bool fRecursive = false);
    bool removeConflicts(const CTransaction &tx);
    void clear();
    void queryHashes(std::vector<uint256>& vtxid);

    unsigned long size() const
    {
        LOCK(cs);
        return mapTx.size();
    }

    bool exists(uint256 hash) const
    {
        LOCK(cs);
        return (mapTx.count(hash) != 0);
    }

    bool lookup(uint256 hash, CTransaction& result) const
    {
        LOCK(cs);
        std::map<uint256, CTransaction>::const_iterator i = mapTx.find(hash);
        if (i == mapTx.end()) return false;
        result = i->second;
        return true;
    }
};

extern CTxMemPool mempool;
#endif
