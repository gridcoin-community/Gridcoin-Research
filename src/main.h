// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "script.h"
#include "scrypt.h"
#include "zerocoin/Zerocoin.h"

#include "global_objects_noui.hpp"
#include <list>

class CWallet;
class CBlock;
class CBlockIndex;
class CKeyItem;
class CReserveKey;
class COutPoint;
class CAddress;
class CInv;
class CRequestTracker;
class CNode;
class CTxMemPool;

static const int LAST_POW_BLOCK = 2050;
extern unsigned int REORGANIZE_FAILED;
extern unsigned int WHITELISTED_PROJECTS;
extern unsigned int CHECKPOINT_VIOLATIONS;
static const int MAX_NEWBIE_BLOCKS = 200;
static const int MAX_NEWBIE_BLOCKS_LEVEL2 = 500;
static const int CHECKPOINT_DISTRIBUTED_MODE = 3;
extern int64_t nLastBlockSolved;
extern uint256 muGlobalCheckpointHash;
extern uint256 muGlobalCheckpointHashRelayed;
extern int muGlobalCheckpointHashCounter;

extern bool bNewUserWizardNotified;


/** The maximum allowed size for a serialized block, in bytes (network rule) */
static const unsigned int MAX_BLOCK_SIZE = 1000000;
static const double TOLERANCE_PERCENT = 1.40;  // The amount a network consensus magnitude can be skewed by before calling Netsoft
/** The maximum size for mined blocks */
static const unsigned int MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
/** The maximum size for transactions we're willing to relay/mine **/
static const unsigned int MAX_STANDARD_TX_SIZE = MAX_BLOCK_SIZE_GEN/5;
/** The maximum allowed number of signature check operations in a block (network rule) */
static const unsigned int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
/** The maximum number of orphan transactions kept in memory */
static const unsigned int MAX_ORPHAN_TRANSACTIONS = MAX_BLOCK_SIZE/100;
/** The maximum number of entries in an 'inv' protocol message */
static const unsigned int MAX_INV_SZ = 50000;
/** Fees smaller than this (in satoshi) are considered zero fee (for transaction creation) */
static const int64_t MIN_TX_FEE = 10000;
/** Fees smaller than this (in satoshi) are considered zero fee (for relaying) */
static const int64_t MIN_RELAY_TX_FEE = MIN_TX_FEE;
/** No amount larger than this (in satoshi) is valid */
static const int64_t MAX_MONEY = 2000000000 * COIN;
inline bool MoneyRange(int64_t nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }
/** Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp. */
static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC
static const unsigned int MINIMUM_CHECKPOINT_TRANSMISSION_BALANCE = 4000000;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Gridcoin - Genesis1 - MainNet - Used for R&D until 10-20-2014:
//static const uint256 hashGenesisBlock("0x0000026925f360c804a9b8410e656de447714d1fe39ff0de1002dcc2e457963b");

//Genesis - MainNet - Production Genesis: as of 10-20-2014:
static const uint256 hashGenesisBlock("0x000005a247b397eadfefa58e872bc967c2614797bdc8d4d0e6b09fea5c191599");

//TestNet Genesis:
static const uint256 hashGenesisBlockTestNet("0x");
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsProtocolV2(int nHeight) { return nHeight > 3500000; }

inline int64_t PastDrift(int64_t nTime, int nHeight)   { return IsProtocolV2(nHeight) ? nTime      : nTime - 10 * 60; }
inline int64_t FutureDrift(int64_t nTime, int nHeight) { return IsProtocolV2(nHeight) ? nTime + 15 : nTime + 10 * 60; }

inline unsigned int GetTargetSpacing(int nHeight) { return IsProtocolV2(nHeight) ? 64 : 60; }


extern libzerocoin::Params* ZCParams;
extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern std::map<uint256, CBlockIndex*> mapBlockIndex;
extern std::set<std::pair<COutPoint, unsigned int> > setStakeSeen;
extern CBlockIndex* pindexGenesisBlock;
extern unsigned int nStakeMinAge;
extern unsigned int nStakeMaxAge;
extern unsigned int nNodeLifespan;
extern int nCoinbaseMaturity;
extern int nBestHeight;
extern uint256 nBestChainTrust;
extern uint256 nBestInvalidTrust;
extern uint256 hashBestChain;
extern CBlockIndex* pindexBest;
extern unsigned int nTransactionsUpdated;

extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;
extern int64_t nLastCoinStakeSearchInterval;
extern const std::string strMessageMagic;
extern int64_t nTimeBestReceived;
extern CCriticalSection cs_setpwalletRegistered;
extern std::set<CWallet*> setpwalletRegistered;
extern unsigned char pchMessageStart[4];
extern std::map<uint256, CBlock*> mapOrphanBlocks;

extern int64_t COIN_YEAR_REWARD;


// Settings
extern int64_t nTransactionFee;
extern int64_t nReserveBalance;
extern int64_t nMinimumInputValue;
extern int64_t nLastTallied;
extern int64_t nCPIDsLoaded;


extern bool fUseFastIndex;
extern unsigned int nDerivationMethodIndex;

extern bool fEnforceCanonical;

// Minimum disk space required - used in CheckDiskSpace()
static const uint64_t nMinDiskSpace = 52428800;

// PoB Miner Global Vars:
extern std::string 	msMiningProject;
extern std::string 	msMiningCPID;
extern double    	mdMiningRAC;
extern double       mdMiningNetworkRAC;
extern std::string  msENCboincpublickey;
extern std::string  msMiningErrors;
extern std::string  msMiningErrors2;
// PoB GPU Miner Global Vars:
extern std::string 	msGPUMiningProject;
extern std::string 	msGPUMiningCPID;
extern double    	mdGPUMiningRAC;
extern double       mdGPUMiningNetworkRAC;
extern std::string  msGPUENCboincpublickey;
extern std::string  msGPUboinckey;
// Stats for Main Screen:
extern double         mdLastPoBDifficulty;
extern double         mdLastDifficulty;
extern std::string    msGlobalStatus;



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
CBlockIndex* FindBlockByHeight(int nHeight);
bool ProcessMessages(CNode* pfrom);
bool SendMessages(CNode* pto, bool fSendTrickle);
bool LoadExternalBlockFile(FILE* fileIn);

bool CheckProofOfWork(uint256 hash, unsigned int nBits);
unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake);
int64_t GetProofOfWorkReward(int64_t nFees, int64_t locktime, int64_t height);
int64_t GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees, std::string cpid, bool VerifyingBlock,int64_t locktime);

unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime);
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime);
int GetNumBlocksOfPeers();
bool IsInitialBlockDownload();
std::string GetWarnings(std::string strFor);
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock);
uint256 WantedByOrphan(const CBlock* pblockOrphan);
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake);
void StakeMiner(CWallet *pwallet);
void ResendWalletTransactions(bool fForce = false);

std::string DefaultWalletAddress();


/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx,
                        bool* pfMissingInputs);





bool GetWalletFile(CWallet* pwallet, std::string &strWalletFileOut);

/** Position on disk for a particular transaction. */
class CDiskTxPos
{
public:
    unsigned int nFile;
    unsigned int nBlockPos;
    unsigned int nTxPos;

    CDiskTxPos()
    {
        SetNull();
    }

    CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
    {
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nTxPos = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { nFile = (unsigned int) -1; nBlockPos = 0; nTxPos = 0; }
    bool IsNull() const { return (nFile == (unsigned int) -1); }

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nFile     == b.nFile &&
                a.nBlockPos == b.nBlockPos &&
                a.nTxPos    == b.nTxPos);
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return !(a == b);
    }


    std::string ToString() const
    {
        if (IsNull())
            return "null";
        else
            return strprintf("(nFile=%u, nBlockPos=%u, nTxPos=%u)", nFile, nBlockPos, nTxPos);
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }
};



/** An inpoint - a combination of a transaction and an index n into its vin */
class CInPoint
{
public:
    CTransaction* ptx;
    unsigned int n;

    CInPoint() { SetNull(); }
    CInPoint(CTransaction* ptxIn, unsigned int nIn) { ptx = ptxIn; n = nIn; }
    void SetNull() { ptx = NULL; n = (unsigned int) -1; }
    bool IsNull() const { return (ptx == NULL && n == (unsigned int) -1); }
};



/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    unsigned int n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, unsigned int nIn) { hash = hashIn; n = nIn; }
    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { hash = 0; n = (unsigned int) -1; }
    bool IsNull() const { return (hash == 0 && n == (unsigned int) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const
    {
        return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10).c_str(), n);
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    unsigned int nSequence;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = COutPoint(hashPrevTx, nOut);
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    )

    bool IsFinal() const
    {
        return (nSequence == std::numeric_limits<unsigned int>::max());
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const
    {
        return strprintf(" %s %d", prevout.hash.ToString().c_str(), prevout.n);
    }

    std::string ToString() const
    {
        std::string str;
        str += "CTxIn(";
        str += prevout.ToString();
        if (prevout.IsNull())
            str += strprintf(", coinbase %s", HexStr(scriptSig).c_str());
        else
            str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24).c_str());
        if (nSequence != std::numeric_limits<unsigned int>::max())
            str += strprintf(", nSequence=%u", nSequence);
        str += ")";
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    int64_t nValue;
    CScript scriptPubKey;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(int64_t nValueIn, CScript scriptPubKeyIn)
    {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    )

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull()
    {
        return (nValue == -1);
    }

    void SetEmpty()
    {
        nValue = 0;
        scriptPubKey.clear();
    }

    bool IsEmpty() const
    {
        return (nValue == 0 && scriptPubKey.empty());
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const
    {
        return strprintf(" out %s %s", FormatMoney(nValue).c_str(), scriptPubKey.ToString(true).c_str());
    }

    std::string ToString() const
    {
        if (IsEmpty()) return "CTxOut(empty)";
        return strprintf("CTxOut(nValue=%s, scriptPubKey=%s)", FormatMoney(nValue).c_str(), scriptPubKey.ToString().c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




enum GetMinFee_mode
{
    GMF_BLOCK,
    GMF_RELAY,
    GMF_SEND,
};

typedef std::map<uint256, std::pair<CTxIndex, CTransaction> > MapPrevTx;

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    unsigned int nTime;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    unsigned int nLockTime;

    // Denial-of-service detection:
    mutable int nDoS;
    bool DoS(int nDoSIn, bool fIn) const { nDoS += nDoSIn; return fIn; }
	std::string hashBoinc;

    CTransaction()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nTime);
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);

		READWRITE(hashBoinc);
    )

    void SetNull()
    {
        nVersion = CTransaction::CURRENT_VERSION;
        nTime = GetAdjustedTime();
        vin.clear();
        vout.clear();
        nLockTime = 0;
        nDoS = 0;  // Denial-of-service prevention
		hashBoinc="";
    }

    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    bool IsNewerThan(const CTransaction& old) const
    {
        if (vin.size() != old.vin.size())
            return false;
        for (unsigned int i = 0; i < vin.size(); i++)
            if (vin[i].prevout != old.vin[i].prevout)
                return false;

        bool fNewer = false;
        unsigned int nLowest = std::numeric_limits<unsigned int>::max();
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            if (vin[i].nSequence != old.vin[i].nSequence)
            {
                if (vin[i].nSequence <= nLowest)
                {
                    fNewer = false;
                    nLowest = vin[i].nSequence;
                }
                if (old.vin[i].nSequence < nLowest)
                {
                    fNewer = true;
                    nLowest = old.vin[i].nSequence;
                }
            }
        }
        return fNewer;
    }

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull() && vout.size() >= 1);
    }

    bool IsCoinStake() const
    {
        // ppcoin: the coin stake transaction is marked with the first output empty
        return (vin.size() > 0 && (!vin[0].prevout.IsNull()) && vout.size() >= 2 && vout[0].IsEmpty());
    }

    /** Check for standard transaction types
        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return True if all inputs (scriptSigs) use only standard transaction forms
        @see CTransaction::FetchInputs
    */
    bool AreInputsStandard(const MapPrevTx& mapInputs) const;

    /** Count ECDSA signature operations the old-fashioned (pre-0.6) way
        @return number of sigops this transaction's outputs will produce when spent
        @see CTransaction::FetchInputs
    */
    unsigned int GetLegacySigOpCount() const;

    /** Count ECDSA signature operations in pay-to-script-hash inputs.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return maximum number of sigops required to validate this transaction's inputs
        @see CTransaction::FetchInputs
     */
    unsigned int GetP2SHSigOpCount(const MapPrevTx& mapInputs) const;

    /** Amount of bitcoins spent by this transaction.
        @return sum of all outputs (note: does not include fees)
     */
    int64_t GetValueOut() const
    {
        int64_t nValueOut = 0;
        BOOST_FOREACH(const CTxOut& txout, vout)
        {
            nValueOut += txout.nValue;
            if (!MoneyRange(txout.nValue) || !MoneyRange(nValueOut))
                throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
        }
        return nValueOut;
    }

    /** Amount of bitcoins coming in to this transaction
        Note that lightweight clients may not know anything besides the hash of previous transactions,
        so may not be able to calculate this.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return	Sum of value of all inputs (scriptSigs)
        @see CTransaction::FetchInputs
     */
    int64_t GetValueIn(const MapPrevTx& mapInputs) const;

    int64_t GetMinFee(unsigned int nBlockSize=1, enum GetMinFee_mode mode=GMF_BLOCK, unsigned int nBytes = 0) const;

    bool ReadFromDisk(CDiskTxPos pos, FILE** pfileRet=NULL)
    {
        CAutoFile filein = CAutoFile(OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb"), SER_DISK, CLIENT_VERSION);
        if (!filein)
            return error("CTransaction::ReadFromDisk() : OpenBlockFile failed");

        // Read transaction
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error("CTransaction::ReadFromDisk() : fseek failed");

        try {
            filein >> *this;
        }
        catch (std::exception &e) {
            return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
        }

        // Return file pointer
        if (pfileRet)
        {
            if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
                return error("CTransaction::ReadFromDisk() : second fseek failed");
            *pfileRet = filein.release();
        }
        return true;
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return (a.nVersion  == b.nVersion &&
                a.nTime     == b.nTime &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const
    {
        std::string str;
        str += strprintf("%s %s", GetHash().ToString().c_str(), IsCoinBase()? "base" : (IsCoinStake()? "stake" : "user"));
        return str;
    }

    std::string ToString() const
    {
        std::string str;
        str += IsCoinBase()? "Coinbase" : (IsCoinStake()? "Coinstake" : "CTransaction");
        str += strprintf("(hash=%s, nTime=%d, ver=%d, vin.size=%"PRIszu", vout.size=%"PRIszu", nLockTime=%d)\n",
            GetHash().ToString().substr(0,10).c_str(),
            nTime,
            nVersion,
            vin.size(),
            vout.size(),
            nLockTime);
        for (unsigned int i = 0; i < vin.size(); i++)
            str += "    " + vin[i].ToString() + "\n";
        for (unsigned int i = 0; i < vout.size(); i++)
            str += "    " + vout[i].ToString() + "\n";
        return str;
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }


    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout);
    bool ReadFromDisk(COutPoint prevout);
    bool DisconnectInputs(CTxDB& txdb);

    /** Fetch from memory and/or disk. inputsRet keys are transaction hashes.

     @param[in] txdb	Transaction database
     @param[in] mapTestPool	List of pending changes to the transaction index database
     @param[in] fBlock	True if being called to add a new best-block to the chain
     @param[in] fMiner	True if being called by CreateNewBlock
     @param[out] inputsRet	Pointers to this transaction's inputs
     @param[out] fInvalid	returns true if transaction is invalid
     @return	Returns true if all inputs are in txdb or mapTestPool
     */
    bool FetchInputs(CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool,
                     bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid);

    /** Sanity check previous transactions, then, if all checks succeed,
        mark them as spent by this transaction.

        @param[in] inputs	Previous transactions (from FetchInputs)
        @param[out] mapTestPool	Keeps track of inputs that need to be updated on disk
        @param[in] posThisTx	Position of this transaction on disk
        @param[in] pindexBlock
        @param[in] fBlock	true if called from ConnectBlock
        @param[in] fMiner	true if called from CreateNewBlock
        @return Returns true if all checks succeed
     */
    bool ConnectInputs(CTxDB& txdb, MapPrevTx inputs,
                       std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx,
                       const CBlockIndex* pindexBlock, bool fBlock, bool fMiner);
    bool CheckTransaction() const;
    bool GetCoinAge(CTxDB& txdb, uint64_t& nCoinAge) const;  // ppcoin: get transaction coin age

protected:
    const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const;
};

/** Check for standard transaction types
    @return True if all outputs (scriptPubKeys) use only standard transaction forms
*/
bool IsStandardTx(const CTransaction& tx);

bool IsFinalTx(const CTransaction &tx, int nBlockHeight = 0, int64_t nBlockTime = 0);



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
        hashBlock = 0;
        nIndex = -1;
        fMerkleVerified = false;
    }


    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, *(CTransaction*)this, nType, nVersion, ser_action);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    )


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




/**  A txdb record that contains the disk location of a transaction and the
 * locations of transactions that spend its outputs.  vSpent is really only
 * used as a flag, but having the location is very helpful for debugging.
 */
class CTxIndex
{
public:
    CDiskTxPos pos;
    std::vector<CDiskTxPos> vSpent;

    CTxIndex()
    {
        SetNull();
    }

    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs)
    {
        pos = posIn;
        vSpent.resize(nOutputs);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(pos);
        READWRITE(vSpent);
    )

    void SetNull()
    {
        pos.SetNull();
        vSpent.clear();
    }

    bool IsNull()
    {
        return pos.IsNull();
    }

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        return (a.pos    == b.pos &&
                a.vSpent == b.vSpent);
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b)
    {
        return !(a == b);
    }
    int GetDepthInMainChain() const;

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
class CBlock
{
public:
    // header
    static const int CURRENT_VERSION = 7;
    int nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;

    unsigned int nNonce;
	
    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;




    // memory only
    mutable std::vector<uint256> vMerkleTree;

    // Denial-of-service detection:
    mutable int nDoS;
    bool DoS(int nDoSIn, bool fIn) const { nDoS += nDoSIn; return fIn; }

	//Gridcoin - 7/27/2014
	/////////////////////////////////////////
	
	
    CBlock()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
		//Gridcoin
	   
	    //


        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
		//? Some explaining would be prudent...
		
		//?
        READWRITE(nNonce);

		

        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(nType & (SER_GETHASH|SER_BLOCKHEADERONLY)))
        {
            READWRITE(vtx);

            READWRITE(vchBlockSig);

			/*
			
			*/



        }
        else if (fRead)
        {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        }





    )

    void SetNull()
    {
        nVersion = CBlock::CURRENT_VERSION;
        hashPrevBlock = 0;
        hashMerkleRoot = 0;
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        vtx.clear();
        vchBlockSig.clear();
	    vMerkleTree.clear();
        nDoS = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const
    {
        if (nVersion > 6)
            return Hash(BEGIN(nVersion), END(nNonce));
        else
            return GetPoWHash();
    }

    uint256 GetPoWHash() const
    {
        return scrypt_blockhash(CVOIDBEGIN(nVersion));
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    void UpdateTime(const CBlockIndex* pindexPrev);

    // entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit() const
    {
        // Take last bit of block hash as entropy bit
        unsigned int nEntropyBit = ((GetHash().Get64()) & 1llu);
        if (fDebug && GetBoolArg("-printstakemodifier"))
            printf("GetStakeEntropyBit: hashBlock=%s nEntropyBit=%u\n", GetHash().ToString().c_str(), nEntropyBit);
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

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        return IsProofOfStake()? std::make_pair(vtx[1].vin[0].prevout, vtx[1].nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const
    {
        int64_t maxTransactionTime = 0;
        BOOST_FOREACH(const CTransaction& tx, vtx)
            maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx.nTime);
        return maxTransactionTime;
    }

    uint256 BuildMerkleTree() const
    {
        vMerkleTree.clear();
        BOOST_FOREACH(const CTransaction& tx, vtx)
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
        return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
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
            return 0;
        BOOST_FOREACH(const uint256& otherside, vMerkleBranch)
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
        CAutoFile fileout = CAutoFile(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
        if (!fileout)
            return error("CBlock::WriteToDisk() : AppendBlockFile failed");

        // Write index header
        unsigned int nSize = fileout.GetSerializeSize(*this);
        fileout << FLATDATA(pchMessageStart) << nSize;

        // Write block
        long fileOutPos = ftell(fileout);
        if (fileOutPos < 0)
            return error("CBlock::WriteToDisk() : ftell failed");
        nBlockPosRet = fileOutPos;
        fileout << *this;

        // Flush stdio buffers and commit to disk before returning
        fflush(fileout);
        if (!IsInitialBlockDownload() || (nBestHeight+1) % 500 == 0)
            FileCommit(fileout);

        return true;
    }

    bool ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true)
    {
        SetNull();

        // Open history file to read
        CAutoFile filein = CAutoFile(OpenBlockFile(nFile, nBlockPos, "rb"), SER_DISK, CLIENT_VERSION);
        if (!filein)
            return error("CBlock::ReadFromDisk() : OpenBlockFile failed");
        if (!fReadTransactions)
            filein.nType |= SER_BLOCKHEADERONLY;

        // Read block
        try {
            filein >> *this;
        }
        catch (std::exception &e) {
            return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
        }

        // Check the header
        if (fReadTransactions && IsProofOfWork() && !CheckProofOfWork(GetPoWHash(), nBits))
            return error("CBlock::ReadFromDisk() : errors in block header");

        return true;
    }



    void print() const
    {
        printf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%"PRIszu", vchBlockSig=%s)\n",
            GetHash().ToString().c_str(),
            nVersion,
            hashPrevBlock.ToString().c_str(),
            hashMerkleRoot.ToString().c_str(),
            nTime, nBits, nNonce,
            vtx.size(),
            HexStr(vchBlockSig.begin(), vchBlockSig.end()).c_str());
        for (unsigned int i = 0; i < vtx.size(); i++)
        {
            printf("  ");
            vtx[i].print();
        }
        printf("  vMerkleTree: ");
        for (unsigned int i = 0; i < vMerkleTree.size(); i++)
            printf("%s ", vMerkleTree[i].ToString().substr(0,10).c_str());
        printf("\n");
    }


    bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck=false);
    bool ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions=true);
    bool SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew);
    bool AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof);
    bool CheckBlock(bool fCheckPOW=true, bool fCheckMerkleRoot=true, bool fCheckSig=true) const;
    bool AcceptBlock(bool generated_by_me);
    bool GetCoinAge(uint64_t& nCoinAge) const; // ppcoin: calculate total coin age spent in block
    bool SignBlock(CWallet& keystore, int64_t nFees);
    bool CheckBlockSignature() const;

private:
    bool SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew);
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
    uint256 nChainTrust; // ppcoin: trust score of block chain
    int nHeight;

    int64_t nMint;
    int64_t nMoneySupply;

    unsigned int nFlags;  // ppcoin: block index flags
    enum  
    {
        BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
        BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
        BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
    };

    uint64_t nStakeModifier; // hash modifier for proof-of-stake
    unsigned int nStakeModifierChecksum; // checksum of index; in-memeory only

    // proof-of-stake specific fields
    COutPoint prevoutStake;
    unsigned int nStakeTime;

    uint256 hashProof;

    // block header
    int nVersion;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;

    CBlockIndex()
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = 0;
        nBlockPos = 0;
        nHeight = 0;
        nChainTrust = 0;
        nMint = 0;
        nMoneySupply = 0;
        nFlags = 0;
        nStakeModifier = 0;
        nStakeModifierChecksum = 0;
        hashProof = 0;
        prevoutStake.SetNull();
        nStakeTime = 0;

        nVersion       = 0;
        hashMerkleRoot = 0;
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;
    }

    CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, CBlock& block)
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nHeight = 0;
        nChainTrust = 0;
        nMint = 0;
        nMoneySupply = 0;
        nFlags = 0;
        nStakeModifier = 0;
        nStakeModifierChecksum = 0;
        hashProof = 0;
        if (block.IsProofOfStake())
        {
            SetProofOfStake();
            prevoutStake = block.vtx[1].vin[0].prevout;
            nStakeTime = block.vtx[1].nTime;
        }
        else
        {
            prevoutStake.SetNull();
            nStakeTime = 0;
        }

        nVersion       = block.nVersion;
        hashMerkleRoot = block.hashMerkleRoot;
        nTime          = block.nTime;
        nBits          = block.nBits;
        nNonce         = block.nNonce;
    }

    CBlock GetBlockHeader() const
    {
        CBlock block;
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

    uint256 GetBlockTrust() const;

    bool IsInMainChain() const
    {
        return (pnext || this == pindexBest);
    }

    bool CheckIndex() const
    {
        return true;
    }

    int64_t GetPastTimeLimit() const
    {
        if (IsProtocolV2(nHeight))
            return GetBlockTime();
        else
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

    /**
     * Returns true if there are nRequired or more blocks of minVersion or above
     * in the last nToCheck blocks, starting at pstart and going backwards.
     */
    static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart,
                                unsigned int nRequired, unsigned int nToCheck);


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

    std::string ToString() const
    {
        return strprintf("CBlockIndex(nprev=%p, pnext=%p, nFile=%u, nBlockPos=%-6d nHeight=%d, nMint=%s, nMoneySupply=%s, nFlags=(%s)(%d)(%s), nStakeModifier=%016"PRIx64", nStakeModifierChecksum=%08x, hashProof=%s, prevoutStake=(%s), nStakeTime=%d merkle=%s, hashBlock=%s)",
            pprev, pnext, nFile, nBlockPos, nHeight,
            FormatMoney(nMint).c_str(), FormatMoney(nMoneySupply).c_str(),
            GeneratedStakeModifier() ? "MOD" : "-", GetStakeEntropyBit(), IsProofOfStake()? "PoS" : "PoW",
            nStakeModifier, nStakeModifierChecksum, 
            hashProof.ToString().c_str(),
            prevoutStake.ToString().c_str(), nStakeTime,
            hashMerkleRoot.ToString().c_str(),
            GetBlockHash().ToString().c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
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
        hashPrev = 0;
        hashNext = 0;
        blockHash = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : 0);
        hashNext = (pnext ? pnext->GetBlockHash() : 0);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);

        READWRITE(hashNext);
        READWRITE(nFile);
        READWRITE(nBlockPos);
        READWRITE(nHeight);
        READWRITE(nMint);
        READWRITE(nMoneySupply);
        READWRITE(nFlags);
        READWRITE(nStakeModifier);
        if (IsProofOfStake())
        {
            READWRITE(prevoutStake);
            READWRITE(nStakeTime);
        }
        else if (fRead)
        {
            const_cast<CDiskBlockIndex*>(this)->prevoutStake.SetNull();
            const_cast<CDiskBlockIndex*>(this)->nStakeTime = 0;
        }
        READWRITE(hashProof);

        // block header
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(blockHash);
    )

    uint256 GetBlockHash() const
    {
        if (fUseFastIndex && (nTime < GetAdjustedTime() - 24 * 60 * 60) && blockHash != 0)
            return blockHash;

        CBlock block;
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
            GetBlockHash().ToString().c_str(),
            hashPrev.ToString().c_str(),
            hashNext.ToString().c_str());
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
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
        std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
            Set((*mi).second);
    }

    CBlockLocator(const std::vector<uint256>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    )

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
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
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
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
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
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
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
