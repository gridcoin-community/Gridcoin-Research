// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "miner.h"
#include "kernel.h"
#include "cpid.h"
#include "util.h"

#include <memory>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

volatile unsigned int nMinerSleep;
MiningCPID GetNextProject(bool bForce);
void ThreadCleanWalletPassphrase(void* parg);
double GetBlockDifficulty(unsigned int nBits);
double MintLimiter(double PORDiff,int64_t RSA_WEIGHT,std::string cpid,int64_t locktime);
double CoinToDouble(double surrogate);
StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sFrom);

void ThreadTopUpKeyPool(void* parg);

int64_t GetRSAWeightByCPID(std::string cpid);
bool OutOfSyncByAgeWithChanceOfMining();
std::string SerializeBoincBlock(MiningCPID mcpid);
bool LessVerbose(int iMax1000);

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

static const unsigned int pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    SHA256_CTX ctx;
    unsigned char data[64];

    SHA256_Init(&ctx);

    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = ByteReverse(((uint32_t*)pinput)[i]);

    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];

    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}

// Some explaining would be appreciated
class COrphan
{
public:
    CTransaction* ptx;
    set<uint256> setDependsOn;
    double dPriority;
    double dFeePerKb;

    COrphan(CTransaction* ptxIn)
    {
        ptx = ptxIn;
        dPriority = dFeePerKb = 0;
    }

    void print() const
    {
        printf("COrphan(hash=%s, dPriority=%.1f, dFeePerKb=%.1f)\n",
               ptx->GetHash().ToString().substr(0,10).c_str(), dPriority, dFeePerKb);
        BOOST_FOREACH(uint256 hash, setDependsOn)
            printf("   setDependsOn %s\n", hash.ToString().substr(0,10).c_str());
    }
};


uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
int64_t nLastCoinStakeSearchInterval = 0;
 
// We want to sort transactions by priority and fee, so:
typedef boost::tuple<double, double, CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;
public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }
    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};





// CreateNewBlock: create new block (without proof-of-work/proof-of-stake)
CBlock* CreateNewBlock(CWallet* pwallet, bool fProofOfStake, int64_t* pFees)
{

    if (!bCPIDsLoaded)
    {
        printf("CPIDs not yet loaded...");
        MilliSleep(500);
        return NULL;
    }

    if (!bNetAveragesLoaded)
    {
        if (fDebug10) printf("CNB: Net averages not yet loaded...");
        MilliSleep(1);
        return NULL;
    }

    if (OutOfSyncByAgeWithChanceOfMining())
    {
        if (msPrimaryCPID != "INVESTOR") printf("Wallet out of sync - unable to mine...");
        MilliSleep(1);
        return NULL;
    }

    // Create new block
    std::unique_ptr<CBlock> pblock(new CBlock());
    if (!pblock.get())
        return NULL;

    CBlockIndex* pindexPrev = pindexBest;
    int nHeight = pindexPrev->nHeight + 1;

    if (!IsProtocolV2(nHeight))
        pblock->nVersion = 6;

    // Create coinbase tx
    CTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    
    if (!fProofOfStake)
    {
        CReserveKey reservekey(pwallet);
        CPubKey pubkey;
        if (!reservekey.GetReservedKey(pubkey))
            return NULL;
        txNew.vout[0].scriptPubKey.SetDestination(pubkey.GetID());

        printf("Generating PoW standard payment\r\n");
    }
    else
    {
        // Height first in coinbase required for block.version=2
        txNew.vin[0].scriptSig = (CScript() << nHeight) + COINBASE_FLAGS;
        assert(txNew.vin[0].scriptSig.size() <= 100);
        txNew.vout[0].SetEmpty();
    
    }

    // Add our coinbase tx as first transaction
    pblock->vtx.push_back(txNew);

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", MAX_BLOCK_SIZE_GEN/2);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", 27000);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", 0);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Fee-per-kilobyte amount considered the same as "free"
    // Be careful setting this: if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    int64_t nMinTxFee = MIN_TX_FEE;
    if (mapArgs.count("-mintxfee"))
        ParseMoney(mapArgs["-mintxfee"], nMinTxFee);

    pblock->nBits = GetNextTargetRequired(pindexPrev, fProofOfStake);
    msMiningErrorsIncluded = "";
    msMiningErrorsExcluded = "";

    // Collect memory pool transactions into the block
    int64_t nFees = 0;
    {
        LOCK2(cs_main, mempool.cs);
        CTxDB txdb("r");

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTransaction>::iterator mi = mempool.mapTx.begin(); mi != mempool.mapTx.end(); ++mi)
        {
            CTransaction& tx = (*mi).second;
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            int64_t nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                CTransaction txPrev;
                CTxIndex txindex;
            
                if (fDebug10) printf("Enumerating tx %s ",tx.GetHash().GetHex().c_str());

                if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        printf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                        if (fDebug10) printf("Orphan tx %s ",tx.GetHash().GetHex().c_str());
                        msMiningErrorsExcluded += tx.GetHash().GetHex() + ":ORPHAN;";
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].vout[txin.prevout.n].nValue;
                    continue;
                }
                int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = txindex.GetDepthInMainChain();
                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority /= nTxSize;

            // This is a more accurate fee-per-kilobyte than is used by the client code, because the
            // client code rounds up the size to the nearest 1K. That's good, because it gives an
            // incentive to create smaller transactions.
            double dFeePerKb =  double(nTotalIn-tx.GetValueOut()) / (double(nTxSize)/1000.0);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->dFeePerKb = dFeePerKb;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, dFeePerKb, &(*mi).second));
        }

        // Collect transactions into block
        map<uint256, CTxIndex> mapTestPool;
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            double dFeePerKb = vecPriority.front().get<1>();
            CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (fDebug10) printf("Tx Size for %s  %f",tx.GetHash().GetHex().c_str(),(double)nTxSize);

            if (nBlockSize + nTxSize >= nBlockMaxSize)
            {
                if (fDebug10) printf("Tx size too large for tx %s  blksize %f , tx siz %f",tx.GetHash().GetHex().c_str(),(double)nBlockSize,(double)nTxSize);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":SizeTooLarge(" 
                    + RoundToString((double)nBlockSize,0) + "," +RoundToString((double)nTxSize,0) + ")(" 
                    + RoundToString((double)nBlockSize,0) + ");";
            
                continue;
            }

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = tx.GetLegacySigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":LegacySigOpLimit(" + 
                    RoundToString((double)nBlockSigOps,0) + "," +RoundToString((double)nTxSigOps,0) + ")(" 
                    + RoundToString((double)MAX_BLOCK_SIGOPS,0) + ");";
                continue;
            }

            // Timestamp limit
            if (tx.nTime > GetAdjustedTime() || (fProofOfStake && tx.nTime > pblock->vtx[0].nTime))
            {
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":TimestampLimit(" + RoundToString((double)tx.nTime,0) + "," 
                    +RoundToString((double)pblock->vtx[0].nTime,0) + ");";
                continue;
            }

            // Transaction fee
            int64_t nMinFee = tx.GetMinFee(nBlockSize, GMF_BLOCK);

            // Skip free transactions if we're past the minimum block size:
            if (fSortedByFee && (dFeePerKb < nMinTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritize by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || (dPriority < COIN * 144 / 250)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            // Connecting shouldn't fail due to dependency on other memory pool transactions
            // because we're already processing them in order of dependency
            map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
            MapPrevTx mapInputs;
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapTestPoolTmp, false, true, mapInputs, fInvalid))
            {
                if (fDebug10) printf("Unable to fetch inputs for tx %s ",tx.GetHash().GetHex().c_str());
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":UnableToFetchInputs;";
                continue;
            }

            int64_t nTxFees = tx.GetValueIn(mapInputs)-tx.GetValueOut();
            if (nTxFees < nMinFee)
            {
                if (fDebug10) printf("Not including tx %s  due to TxFees of %f ; bare min fee is %f",tx.GetHash().GetHex().c_str(),(double)nTxFees,(double)nMinFee);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":FeeTooSmall(" 
                    + RoundToString(CoinToDouble(nFees),8) + "," +RoundToString(CoinToDouble(nMinFee),8) + ");";
                continue;
            }

            nTxSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                if (fDebug10) printf("Not including tx %s  due to exceeding max sigops of %f ; sigops is %f",
                    tx.GetHash().GetHex().c_str(),(double)(nBlockSigOps+nTxSigOps),(double)MAX_BLOCK_SIGOPS);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":ExceededSigOps(" 
                    + RoundToString((double)nBlockSigOps,0) + "," +RoundToString((double)nTxSigOps,0) + ")(" 
                    + RoundToString((double)MAX_BLOCK_SIGOPS,0) + ");";
            
                continue;
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapTestPoolTmp, CDiskTxPos(1,1,1), pindexPrev, false, true))
            {
                if (fDebug10) printf("Unable to connect inputs for tx %s ",tx.GetHash().GetHex().c_str());
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":UnableToConnectInputs();";
                continue;
            }
            mapTestPoolTmp[tx.GetHash()] = CTxIndex(CDiskTxPos(1,1,1), tx.vout.size());
            swap(mapTestPool, mapTestPoolTmp);

            // Added
            msMiningErrorsIncluded += tx.GetHash().GetHex() + ";";
            pblock->vtx.push_back(tx);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fDebug10 || GetBoolArg("-printpriority"))
            {
                printf("priority %.1f feeperkb %.1f txid %s\n",
                       dPriority, dFeePerKb, tx.GetHash().ToString().c_str());
            }
        
            // Add transactions that depend on this one to the priority queue
            uint256 hash = tx.GetHash();
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->dFeePerKb, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;

        if (fDebug10 || GetBoolArg("-printpriority"))
            printf("CreateNewBlock(): total size %" PRIu64 "\n", nBlockSize);
        //Add Boinc Hash - R HALFORD - 11-28-2014 - Add CPID v2
        MiningCPID miningcpid = GetNextProject(false);
        uint256 pbh = 0;
        if (pindexPrev) pbh=pindexPrev->GetBlockHash();
        miningcpid.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, pbh);
        miningcpid.lastblockhash = pindexPrev->GetBlockHash().GetHex();
        //12-9-2014 Verify RSA Weight is actually in the block
        miningcpid.RSAWeight = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
        double out_por = 0;
        double out_interest=0;
        double dAccrualAge = 0;
        double dMagnitudeUnit = 0;
        double dAvgMag = 0;
        //Halford: Use current time since we are creating a new stake
        StructCPID st1 = GetLifetimeCPID(GlobalCPUMiningCPID.cpid,"CreateNewBlock()");

        GetProofOfStakeReward(1,nFees,GlobalCPUMiningCPID.cpid,false,0,pindexBest->nTime,pindexBest,"createnewblock",
            out_por,out_interest,dAccrualAge,dMagnitudeUnit,dAvgMag);
    
        miningcpid.ResearchSubsidy = out_por;
        miningcpid.InterestSubsidy = out_interest;
        miningcpid.enccpid = ""; //CPID V1 Boinc RunTime enc key
        miningcpid.encboincpublickey = "";
        miningcpid.encaes = "";
        std::string hashBoinc = SerializeBoincBlock(miningcpid);
        if (fDebug10 && LessVerbose(10))  printf("Current hashboinc: %s\r\n",hashBoinc.c_str());
        pblock->vtx[0].hashBoinc = hashBoinc;

        // Brod
        GlobalCPUMiningCPID.ResearchSubsidy = out_por;
        GlobalCPUMiningCPID.InterestSubsidy = out_interest;
        GlobalCPUMiningCPID.ResearchMagnitudeUnit = dMagnitudeUnit;

        if (!fProofOfStake)
            pblock->vtx[0].vout[0].nValue = GetProofOfWorkReward(nFees, GetAdjustedTime(),nHeight);

        if (pFees)
            *pFees = nFees;

        // Fill in header  
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        if (!fProofOfStake)
        {

            pblock->nTime          = max(pindexPrev->GetPastTimeLimit()+1, pblock->GetMaxTransactionTime());
            pblock->nTime          = max(pblock->GetBlockTime(), PastDrift(pindexPrev->GetBlockTime(), nHeight));
        }
        else
        {
                        pblock->nTime      =     max(pblock->GetMaxTransactionTime(),GetAdjustedTime());
                        pblock->nTime      =     pblock->vtx[1].nTime; //same as coinstake timestamp
        }

        if (!fProofOfStake)
            pblock->UpdateTime(pindexPrev);
        pblock->nNonce         = 0;
    }

    return pblock.release();
}


void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;

    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    pblock->vtx[0].vin[0].scriptSig = (CScript() << nHeight << CBigNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(pblock->vtx[0].vin[0].scriptSig.size() <= 100);

    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}


void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Pre-build hash buffers
    //
    struct
    {
        struct unnamed2
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    // Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}


bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    uint256 hashBlock = pblock->GetHash();
    uint256 hashProof = pblock->GetPoWHash();
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

    if(!pblock->IsProofOfWork())
        return error("CheckWork[] : %s is not a proof-of-work block", hashBlock.GetHex().c_str());

    if (hashProof > hashTarget)
        return error("CheckWork[] : proof-of-work not meeting target");

    //// debug print
    printf("CheckWork[] : new proof-of-work block found  \n  proof hash: %s  \ntarget: %s\n", hashProof.GetHex().c_str(), hashTarget.GetHex().c_str());
    pblock->print();
    printf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue).c_str());

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != hashBestChain)
        {
            msMiningErrors6 = "Generated block is stale.";
            return error("CheckWork[] : generated block is stale");
        }

        // Remove key from key pool
        reservekey.KeepKey();

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[hashBlock] = 0;
        }

        // Process this block the same as if we had received it from another node
        if (!ProcessBlock(NULL, pblock, true))
        {
            msMiningErrors6 = "Block not accepted.";
            return error("CheckWork[] : ProcessBlock, block not accepted");
        }
    }
        
    return true;
}


double HexToDouble(std::string str)
{
  double hx = 0;
  int nn = 0;
  int r= 0;
  char * ch = const_cast<char*>(str.c_str());
  char * p,pp;
  for (unsigned int i = 1; i <= str.length(); i++)
  {
    r = str.length() - i;
    pp =   ch[r];
    nn = strtoul(&pp, &p, 16 );
    hx = hx + nn * pow(16 , i-1);
   }
  return hx;
}


void MinerAutoUnlockFeature(CWallet *pwallet)
{
    ///////////////////////  Auto Unlock Feature for Research Miner
    if (pwallet->IsLocked())
        {
            //11-5-2014 R Halford - If wallet is locked - see if user has an encrypted password stored:
            std::string passphrase = "";
            if (mapArgs.count("-autounlock"))
            {
                passphrase = GetArg("-autounlock", "");
            }
            if (passphrase.length() > 1)
            {
                std::string decrypted = AdvancedDecryptWithHWID(passphrase);
                //Unlock the wallet for 10 days (Equivalent to: walletpassphrase mylongpass 999999) FOR STAKING ONLY!
                int64_t nSleepTime = 9999999;
                SecureString strWalletPass;
                strWalletPass.reserve(100);
                strWalletPass = decrypted.c_str();
                if (strWalletPass.length() > 0)
                {
                    if (!pwallet->Unlock(strWalletPass))
                    {
                        printf("GridcoinResearchMiner:AutoUnlock:Error: The wallet passphrase entered was incorrect.");
                    }
                    else
                    {
                        NewThread(ThreadTopUpKeyPool,NULL);
                        int64_t* pnSleepTime = new int64_t(nSleepTime);
                        NewThread(ThreadCleanWalletPassphrase, pnSleepTime);
                        fWalletUnlockStakingOnly = true;
                    }
                }
            }
        }
    return;
    // End of AutoUnlock Feature
}


// CreateRestOfTheBlock: collect transactions into block and fill in header
bool CreateRestOfTheBlock(CBlock &block, CBlockIndex* pindexPrev)
{

    int nHeight = pindexPrev->nHeight + 1;

    if (!IsProtocolV2(nHeight))
        block.nVersion = 6;

    // Create coinbase tx
    CTransaction &CoinBase= block.vtx[0];
    CoinBase.nTime=block.nTime;
    CoinBase.vin.resize(1);
    CoinBase.vin[0].prevout.SetNull();
    CoinBase.vout.resize(1);
    // Height first in coinbase required for block.version=2
    CoinBase.vin[0].scriptSig = (CScript() << nHeight) + COINBASE_FLAGS;
    assert(CoinBase.vin[0].scriptSig.size() <= 100);
    CoinBase.vout[0].SetEmpty();

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", MAX_BLOCK_SIZE_GEN/2);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", 27000);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", 0);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Fee-per-kilobyte amount considered the same as "free"
    // Be careful setting this: if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    int64_t nMinTxFee = MIN_TX_FEE;
    if (mapArgs.count("-mintxfee"))
        ParseMoney(mapArgs["-mintxfee"], nMinTxFee);

    // Collect memory pool transactions into the block
    int64_t nFees = 0;
    {
        LOCK2(cs_main, mempool.cs);
        CTxDB txdb("r");

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTransaction>::iterator mi = mempool.mapTx.begin(); mi != mempool.mapTx.end(); ++mi)
        {
            CTransaction& tx = (*mi).second;
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            int64_t nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                CTransaction txPrev;
                CTxIndex txindex;
            
                if (fDebug10) printf("Enumerating tx %s ",tx.GetHash().GetHex().c_str());

                if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        printf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                        if (fDebug10) printf("Orphan tx %s ",tx.GetHash().GetHex().c_str());
                        msMiningErrorsExcluded += tx.GetHash().GetHex() + ":ORPHAN;";
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].vout[txin.prevout.n].nValue;
                    continue;
                }
                int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = txindex.GetDepthInMainChain();
                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority /= nTxSize;

            // This is a more accurate fee-per-kilobyte than is used by the client code, because the
            // client code rounds up the size to the nearest 1K. That's good, because it gives an
            // incentive to create smaller transactions.
            double dFeePerKb =  double(nTotalIn-tx.GetValueOut()) / (double(nTxSize)/1000.0);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->dFeePerKb = dFeePerKb;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, dFeePerKb, &(*mi).second));
        }

        // Collect transactions into block
        map<uint256, CTxIndex> mapTestPool;
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            double dFeePerKb = vecPriority.front().get<1>();
            CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (fDebug10) printf("Tx Size for %s  %f",tx.GetHash().GetHex().c_str(),(double)nTxSize);

            if (nBlockSize + nTxSize >= nBlockMaxSize)
            {
                if (fDebug10) printf("Tx size too large for tx %s  blksize %f , tx siz %f",tx.GetHash().GetHex().c_str(),(double)nBlockSize,(double)nTxSize);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":SizeTooLarge(" 
                    + RoundToString((double)nBlockSize,0) + "," +RoundToString((double)nTxSize,0) + ")(" 
                    + RoundToString((double)nBlockSize,0) + ");";
            
                continue;
            }

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = tx.GetLegacySigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":LegacySigOpLimit(" + 
                    RoundToString((double)nBlockSigOps,0) + "," +RoundToString((double)nTxSigOps,0) + ")(" 
                    + RoundToString((double)MAX_BLOCK_SIGOPS,0) + ");";
                continue;
            }

            // Timestamp limit
            if (tx.nTime >  block.nTime)
            {
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":TimestampLimit(" + RoundToString((double)tx.nTime,0) + "," 
                    +RoundToString((double)block.vtx[0].nTime,0) + ");";
                continue;
            }

            // Transaction fee
            int64_t nMinFee = tx.GetMinFee(nBlockSize, GMF_BLOCK);

            // Skip free transactions if we're past the minimum block size:
            if (fSortedByFee && (dFeePerKb < nMinTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritize by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || (dPriority < COIN * 144 / 250)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            // Connecting shouldn't fail due to dependency on other memory pool transactions
            // because we're already processing them in order of dependency
            map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
            MapPrevTx mapInputs;
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapTestPoolTmp, false, true, mapInputs, fInvalid))
            {
                if (fDebug10) printf("Unable to fetch inputs for tx %s ",tx.GetHash().GetHex().c_str());
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":UnableToFetchInputs;";
                continue;
            }

            int64_t nTxFees = tx.GetValueIn(mapInputs)-tx.GetValueOut();
            if (nTxFees < nMinFee)
            {
                if (fDebug10) printf("Not including tx %s  due to TxFees of %f ; bare min fee is %f",tx.GetHash().GetHex().c_str(),(double)nTxFees,(double)nMinFee);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":FeeTooSmall(" 
                    + RoundToString(CoinToDouble(nFees),8) + "," +RoundToString(CoinToDouble(nMinFee),8) + ");";
                continue;
            }

            nTxSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                if (fDebug10) printf("Not including tx %s  due to exceeding max sigops of %f ; sigops is %f",
                    tx.GetHash().GetHex().c_str(),(double)(nBlockSigOps+nTxSigOps),(double)MAX_BLOCK_SIGOPS);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":ExceededSigOps(" 
                    + RoundToString((double)nBlockSigOps,0) + "," +RoundToString((double)nTxSigOps,0) + ")(" 
                    + RoundToString((double)MAX_BLOCK_SIGOPS,0) + ");";
            
                continue;
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapTestPoolTmp, CDiskTxPos(1,1,1), pindexPrev, false, true))
            {
                if (fDebug10) printf("Unable to connect inputs for tx %s ",tx.GetHash().GetHex().c_str());
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":UnableToConnectInputs();";
                continue;
            }
            mapTestPoolTmp[tx.GetHash()] = CTxIndex(CDiskTxPos(1,1,1), tx.vout.size());
            swap(mapTestPool, mapTestPoolTmp);

            // Added
            msMiningErrorsIncluded += tx.GetHash().GetHex() + ";";
            block.vtx.push_back(tx);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fDebug10 || GetBoolArg("-printpriority"))
            {
                printf("priority %.1f feeperkb %.1f txid %s\n",
                       dPriority, dFeePerKb, tx.GetHash().ToString().c_str());
            }
        
            // Add transactions that depend on this one to the priority queue
            uint256 hash = tx.GetHash();
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->dFeePerKb, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;

        if (fDebug10 || GetBoolArg("-printpriority"))
            printf("CreateNewBlock(): total size %" PRIu64 "\n", nBlockSize);
    }

    //Add fees to coinbase
    block.vtx[0].vout[0].nValue= nFees;

    // Fill in header  
    block.hashPrevBlock  = pindexPrev->GetBlockHash();
    block.vtx[0].nTime=block.nTime;

    return true;
}


double CalculatedMagnitude2(std::string cpid, int64_t locktime,bool bUseLederstrumpf);
int64_t GetRSAWeightByBlock(MiningCPID boincblock);
std::string SignBlockWithCPID(std::string sCPID, std::string sBlockHash);

bool CreateCoinStake( CBlock &blocknew, CKey &key,
    vector<const CWalletTx*> &StakeInputs, uint64_t &CoinAge, CWallet &wallet )
{
    CTxDB txdb("r");
    double StakeWeightSum = 0;
    CTransaction &txnew = blocknew.vtx[1]; // second tx is coinstake

    //initialize the transaction
    txnew.nTime = blocknew.nTime & (~STAKE_TIMESTAMP_MASK);
    txnew.vin.clear();
    txnew.vout.clear();

    // Choose coins to use
    set <pair <const CWalletTx*,unsigned int> > CoinsToStake;

    int64_t BalanceToStake = wallet.GetBalance();
    int64_t nValueIn = 0;
    //Request all the coins here, check reserve later

    if ( BalanceToStake<=0
        || !wallet.SelectCoinsForStaking(BalanceToStake*2, txnew.nTime, CoinsToStake, nValueIn) )
    {
        msMiningErrors5+="No coins to stake; ";
        if (fDebug) printf("CreateCoinStake: %s",msMiningErrors5.c_str());
        return false;
    }
    BalanceToStake -= nReserveBalance;

    if(fDebug) printf("\nCreateCoinStake: Staking nTime/16= %d Bits= %u\n",
    txnew.nTime/16,blocknew.nBits);

    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, CoinsToStake)
    {
        const CTransaction &CoinTx =*pcoin.first; //transaction that produced this coin
        unsigned int CoinTxN =pcoin.second; //index of this coin inside it

        CTxIndex txindex;
        {
            LOCK2(cs_main, wallet.cs_wallet);
            if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
                continue; //error?
        }

        CBlock CoinBlock; //Block which contains CoinTx
        {
            LOCK2(cs_main, wallet.cs_wallet);
            if (!CoinBlock.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                continue;
        }

        // only count coins meeting min age requirement
        if (CoinBlock.GetBlockTime() + nStakeMinAge > txnew.nTime)
            continue;

        if (CoinTx.vout[CoinTxN].nValue > BalanceToStake)
            continue;

        NetworkTimer();
        int64_t CoinWeight = CalculateStakeWeightV3(CoinTx,CoinTxN,GlobalCPUMiningCPID);
        CBigNum StakeTarget;
        StakeTarget.SetCompact(blocknew.nBits);
        StakeTarget*=CoinWeight;
        StakeWeightSum += CoinWeight;
        CBigNum StakeKernelHash= CalculateStakeHashV3(CoinBlock,CoinTx,CoinTxN,txnew.nTime,GlobalCPUMiningCPID,mdPORNonce);

        if (fDebug) {
            int64_t RSA_WEIGHT = GetRSAWeightByBlock(GlobalCPUMiningCPID);
            printf(
"CreateCoinStake: Time %.f, Por_Nonce %.f, Bits %jd, Weight %jd\n"
" RSA_WEIGHT %.f\n"
" Stk %72s\n"
" Trg %72s\n",
            (double)txnew.nTime, mdPORNonce,(intmax_t)blocknew.nBits,(intmax_t)CoinWeight,
            (double)RSA_WEIGHT,
            StakeKernelHash.GetHex().c_str(), StakeTarget.GetHex().c_str()
        );
        }

        nLastCoinStakeSearchInterval= txnew.nTime;

        if( StakeKernelHash <= StakeTarget )
        {
            // Found a kernel
            printf("\nCreateCoinStake: Found Kernel;\n");
            blocknew.nNonce= mdPORNonce;
            vector<valtype> vSolutions;
            txnouttype whichType;
            CScript scriptPubKeyOut;
            CScript scriptPubKeyKernel;
            scriptPubKeyKernel = CoinTx.vout[CoinTxN].scriptPubKey;
            if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
            {
                printf("CreateCoinStake: failed to parse kernel\n");
                break;
            }
            if (whichType == TX_PUBKEYHASH) // pay to address type
            {
                // convert to pay to public key type
                if (!wallet.GetKey(uint160(vSolutions[0]), key))
                {
                    printf("CreateCoinStake: failed to get key for kernel type=%d\n", whichType);
                    break;  // unable to find corresponding public key
                }
                scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
            }
            else if (whichType == TX_PUBKEY)  // pay to public key type
            {
                valtype& vchPubKey = vSolutions[0];
                if (!wallet.GetKey(Hash160(vchPubKey), key)
                    || key.GetPubKey() != vchPubKey)
                {
                    printf("CreateCoinStake: failed to get key for kernel type=%d\n", whichType);
                    break;  // unable to find corresponding public key
                }

                scriptPubKeyOut = scriptPubKeyKernel;
            }
            else
            {
                printf("CreateCoinStake: no support for kernel type=%d\n", whichType);
                break;  // only support pay to public key and pay to address
            }

            txnew.vin.push_back(CTxIn(CoinTx.GetHash(), CoinTxN));
            StakeInputs.push_back(pcoin.first);
            if (!txnew.GetCoinAge(txdb, CoinAge))
                return error("CreateCoinStake: failed to calculate coin age");
            int64_t nCredit = CoinTx.vout[CoinTxN].nValue;

            txnew.vout.push_back(CTxOut(0, CScript())); // First Must be empty
            txnew.vout.push_back(CTxOut(nCredit, scriptPubKeyOut));
            //txnew.vout.push_back(CTxOut(0, scriptPubKeyOut));

            printf("CreateCoinStake: added kernel type=%d credit=%f\n", whichType,CoinToDouble(nCredit));
            msMiningErrors5+="Found Kernel "+RoundToString(CoinToDouble(nCredit),0)+"; ";
            return true;
        }
    }
    msMiningErrors5+="Stake Weight "+RoundToString(StakeWeightSum,0)+"; ";
    return false;
}

bool SignStakeBlock(CBlock &block, CKey &key, vector<const CWalletTx*> &StakeInputs, CWallet *pwallet)
{
    //Append beacon signature to coinbase
    std::string PublicKey = GlobalCPUMiningCPID.BoincPublicKey;
    if (!PublicKey.empty())
    {
        std::string BoincSignature = SignBlockWithCPID(
            GlobalCPUMiningCPID.cpid,GlobalCPUMiningCPID.lastblockhash);
        if(fDebug3) printf("Signing BoincBlock for cpid %s and blockhash %s with sig %s\r\n",GlobalCPUMiningCPID.cpid.c_str(),GlobalCPUMiningCPID.lastblockhash.c_str(),BoincSignature.c_str());
        block.vtx[0].hashBoinc += BoincSignature;
    }

    //Sign the coinstake transaction
    unsigned nIn = 0;
    BOOST_FOREACH(const CWalletTx* pcoin, StakeInputs)
    {
        if (!SignSignature(*pwallet, *pcoin, block.vtx[1], nIn++)) 
        {
            msMiningErrors5+="Failed to sign coinstake; ";
            return error("SignStakeBlock: failed to sign coinstake");
        }
    }

    //Sign the whole block
    block.hashMerkleRoot = block.BuildMerkleTree();
    if( !key.Sign(block.GetHash(), block.vchBlockSig) )
    {
        msMiningErrors5+="Failed to sign block; ";
        return error("SignStakeBlock: failed to sign block");
    }

    return true;
}

bool CreateGridcoinReward(CBlock &blocknew, uint64_t &nCoinAge, CBlockIndex* pindexPrev)
{
    //remove fees from coinbase
    int64_t nFees = blocknew.vtx[0].vout[0].nValue;
    blocknew.vtx[0].vout[0].SetEmpty();

    double OUT_POR = 0;
    double out_interest = 0;
    double dAccrualAge = 0;
    double dAccrualMagnitudeUnit = 0;
    double dAccrualMagnitude = 0;
        
    // ************************************************* CREATE PROOF OF RESEARCH REWARD ****************************** R HALFORD *************** 
    // ResearchAge 2
    // Note: Since research Age must be exact, we need to transmit the Block nTime here so it matches AcceptBlock


    int64_t nReward = GetProofOfStakeReward(
        nCoinAge, nFees, GlobalCPUMiningCPID.cpid, false, 0,
        pindexPrev->nTime, pindexPrev,"createcoinstake",
        OUT_POR,out_interest,dAccrualAge,dAccrualMagnitudeUnit,dAccrualMagnitude);

    
    MiningCPID miningcpid = GlobalCPUMiningCPID;
    uint256 pbh = 0;
    pbh=pindexPrev->GetBlockHash();

    miningcpid.cpidv2 = ComputeCPIDv2(
        GlobalCPUMiningCPID.email,
        GlobalCPUMiningCPID.boincruntimepublickey,
        pbh );


    miningcpid.Magnitude = CalculatedMagnitude2(
        GlobalCPUMiningCPID.cpid, blocknew.nTime,
        false );

    //miningcpid.Magnitude=30000;

    miningcpid.lastblockhash = pbh.GetHex();
    miningcpid.RSAWeight = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
    miningcpid.ResearchSubsidy = OUT_POR;
    miningcpid.ResearchSubsidy2 = OUT_POR;
    miningcpid.ResearchAge = dAccrualAge;
    miningcpid.ResearchMagnitudeUnit = dAccrualMagnitudeUnit;
    miningcpid.ResearchAverageMagnitude = dAccrualMagnitude;

    miningcpid.InterestSubsidy = out_interest;
    miningcpid.enccpid = "";
    miningcpid.encboincpublickey = "";
    miningcpid.encaes = "";
    miningcpid.BoincSignature = "";

    int64_t RSA_WEIGHT = GetRSAWeightByBlock(miningcpid);
    std::string SerializedBoincData = SerializeBoincBlock(miningcpid);
    GlobalCPUMiningCPID.lastblockhash = miningcpid.lastblockhash;

    if (fDebug)  printf("CreateGridcoinReward: %s\r\n",SerializedBoincData.c_str());
        
    double mint = CoinToDouble(nReward);
    double PORDiff = GetBlockDifficulty(blocknew.nBits);
    double mintlimit = MintLimiter(PORDiff,RSA_WEIGHT,miningcpid.cpid,blocknew.nTime);

    printf("CreateGridcoinReward: for %s mint %f {RSAWeight %f} Research %f, Interest %f \r\n",
        miningcpid.cpid.c_str(), mint, (double)RSA_WEIGHT,miningcpid.ResearchSubsidy,miningcpid.InterestSubsidy);

    //INVESTORS
    if (nReward == 0 || mint < mintlimit)
    {
            msMiningErrors5+="Mint "+RoundToString(mint,6)+" too small (min "+RoundToString(mintlimit,6)+"); ";
            return error("CreateGridcoinReward: Mint %f of %f too small",(double)mint,(double)mintlimit);
    }

    //fill in reward and boinc
    blocknew.vtx[1].vout[1].nValue += nReward;
    blocknew.vtx[0].hashBoinc= SerializedBoincData;
    msMiningErrors5+="Added Reward "+RoundToString(mint,3)
        +"("+RoundToString(CoinToDouble(nFees),4)+" "
        +RoundToString(out_interest,2)+" "
        +RoundToString(OUT_POR,2)+"); ";
    return true;
}

bool IsMiningAllowed(CWallet *pwallet)
{
    if(pwallet->IsLocked())
    {
        msMiningErrors5+="Wallet locked; ";
        nLastCoinStakeSearchInterval = 0;
        return false;
    }

    if (!bNetAveragesLoaded)
    {
        msMiningErrors5+="Net averages not yet loaded; ";
        if (LessVerbose(100) && msPrimaryCPID != "INVESTOR") printf("ResearchMiner:Net averages not yet loaded...");
        return false;
    }

    if (vNodes.empty() || IsInitialBlockDownload()
        || vNodes.size() < 3 || nBestHeight < GetNumBlocksOfPeers())
    {
        msMiningErrors5+="Offline; ";
        nLastCoinStakeSearchInterval = 0;
        return false;
    }
    
    //Verify we are still on the main chain
    if (IsLockTimeWithinMinutes(nLastBlockSolved,5))
    {
        if (fDebug10) printf("=");
        msMiningErrors5+="We are not on the main chain; ";
        return false;
    }

    if (OutOfSyncByAgeWithChanceOfMining())
    {
        msMiningErrors7="Out of Sync";
        if(fDebug) printf("Wallet out of sync - unable to stake.\n");
        return false;
    }


    return true;
}

void StakeMiner(CWallet *pwallet)
{

    // Make this thread recognisable as the mining thread
    RenameThread("grc-stake-miner");

    MinerAutoUnlockFeature(pwallet);

  while (!fShutdown)
  {

    //wait for next round
    MilliSleep(nMinerSleep);
    //clear miner messages
    msMiningErrors5="";
    nLastCoinStakeSearchInterval = 0;

    if(!IsMiningAllowed(pwallet))
        continue;

    GetNextProject(false);
    //GlobalCPUMiningCPID.Magnitude=30000;

    // * Create a bare block
    CBlockIndex* pindexPrev = pindexBest;
    CBlock StakeBlock;
    StakeBlock.nTime= GetAdjustedTime();
    StakeBlock.nNonce= 0;
    StakeBlock.nBits = GetNextTargetRequired(pindexPrev, true);
    StakeBlock.vtx.resize(2);
    //tx 0 is coin_base
    CTransaction &StakeTX= StakeBlock.vtx[1]; //tx 1 is coin_stake

    // * Try to create a CoinStake transaction
    CKey BlockKey;
    vector<const CWalletTx*> StakeInputs;
    uint64_t StakeCoinAge;
    if( !CreateCoinStake( StakeBlock, BlockKey, StakeInputs, StakeCoinAge, *pwallet ) )
        continue;
    StakeBlock.nTime= StakeTX.nTime;
    printf("StakeMiner: created coin stake\n");

    // * create rest of the block
    if( !CreateRestOfTheBlock(StakeBlock,pindexPrev) )
        continue;
    printf("StakeMiner: created rest of the block\n");

    // * add gridcoin reward to coinstake
    if( !CreateGridcoinReward(StakeBlock,StakeCoinAge,pindexPrev) )
        continue;
    printf("StakeMiner: added gridcoin reward to coinstake\n");

    // * sign boinchash, coinstake, wholeblock
    if( !SignStakeBlock(StakeBlock,BlockKey,StakeInputs,pwallet) )
        continue;
    printf("StakeMiner: signed boinchash, coinstake, wholeblock\n");

    // * delegate to ProcessBlock
    LOCK(cs_main);
    if (!ProcessBlock(NULL, &StakeBlock, true))
    {
        msMiningErrors5+="Block vehemently rejected; ";
        error("StakeMiner: Block vehemently rejected");
        continue;
    }
    printf("StakeMiner: block processed\n");

  } //end while(!fShutdown)
}

