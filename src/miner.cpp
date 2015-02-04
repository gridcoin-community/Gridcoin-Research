// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "miner.h"
#include "kernel.h"
#include "cpid.h"
#include <boost/lexical_cast.hpp>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

extern unsigned int nMinerSleep;
MiningCPID GetNextProject(bool bForce);
void ThreadCleanWalletPassphrase(void* parg);

double GetBlockDifficulty(unsigned int nBits);

double MintLimiter(double PORDiff,int64_t RSA_WEIGHT,std::string cpid,int64_t locktime);

std::string ComputeCPIDv2(std::string email, std::string bpk, uint256 blockhash);
std::string GetBestBlockHash(std::string sCPID);

double CoinToDouble(double surrogate);

void ThreadTopUpKeyPool(void* parg);
extern int64_t CPIDChronoStart(std::string cpid);
extern bool IsCPIDTimeValid(std::string cpid, int64_t locktime);
extern double CPIDTime(std::string cpid);



bool IsLockTimeWithinMinutes(int64_t locktime, int minutes);
bool AmIGeneratingBackToBackBlocks();
double GetDifficulty(const CBlockIndex* blockindex = NULL);
uint256 GetBlockHash256(const CBlockIndex* pindex_hash);

int64_t GetRSAWeightByCPID(std::string cpid);

std::string RoundToString(double d, int place);

bool OutOfSyncByAgeWithChanceOfMining();

bool TallyNetworkAverages(bool ColdBoot);
MiningCPID DeserializeBoincBlock(std::string block);

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

		if (fDebug) printf("Net averages not yet loaded...");
		MilliSleep(500);
		return NULL;
	}

	if (OutOfSyncByAgeWithChanceOfMining())
	{
	    printf("Wallet out of sync - unable to mine...");
		MilliSleep(500);
		return NULL;
	}


    // Create new block
    auto_ptr<CBlock> pblock(new CBlock());
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
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = tx.GetLegacySigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Timestamp limit
            if (tx.nTime > GetAdjustedTime() || (fProofOfStake && tx.nTime > pblock->vtx[0].nTime))
                continue;

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
                continue;

            int64_t nTxFees = tx.GetValueIn(mapInputs)-tx.GetValueOut();
            if (nTxFees < nMinFee)
                continue;

            nTxSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            if (!tx.ConnectInputs(txdb, mapInputs, mapTestPoolTmp, CDiskTxPos(1,1,1), pindexPrev, false, true))
                continue;
            mapTestPoolTmp[tx.GetHash()] = CTxIndex(CDiskTxPos(1,1,1), tx.vout.size());
            swap(mapTestPool, mapTestPoolTmp);

            // Added
            pblock->vtx.push_back(tx);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fDebug && GetBoolArg("-printpriority"))
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

        if (fDebug && GetBoolArg("-printpriority"))
            printf("CreateNewBlock(): total size %"PRIu64"\n", nBlockSize);
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

		//Halford: Use current time since we are creating a new stake
		int64_t nNewBlockReward = GetProofOfStakeReward(1,nFees,GlobalCPUMiningCPID.cpid,false,GetAdjustedTime(),
			out_por,out_interest,miningcpid.RSAWeight);


		miningcpid.ResearchSubsidy = out_por;
		miningcpid.InterestSubsidy = out_interest;
		//if (fDebug3) printf("Calc Research Subsidy %f, Interest %f \r\n",(double)miningcpid.ResearchSubsidy,(double)miningcpid.InterestSubsidy);

		msMiningErrors4 = "BRSA: " + RoundToString(miningcpid.RSAWeight,0);
		//12-27-2014 Strip CPID out of hashboinc
		miningcpid.enccpid = ""; //CPID V1 Boinc RunTime enc key
		miningcpid.encboincpublickey = "";
		miningcpid.encaes = "";
		
		double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
			
		std::string hashBoinc = SerializeBoincBlock(miningcpid);

		
		//printf("Creating boinc hash : prevblock %s, boinchash %s",pindexPrev->GetBlockHash().GetHex().c_str(),hashBoinc.c_str());
	    if (fDebug)  printf("Current hashboinc: %s\r\n",hashBoinc.c_str());
		pblock->vtx[0].hashBoinc = hashBoinc;

        if (!fProofOfStake)
            pblock->vtx[0].vout[0].nValue = GetProofOfWorkReward(nFees, GetAdjustedTime(),nHeight);

        if (pFees)
            *pFees = nFees;

        // Fill in header  1-1-2015
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
		if (fDebug) printf("ZX271");

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
			//1-10-2015
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
  for (int i = 1; i <= str.length(); i++)
  {
    r = str.length() - i;
    pp =   ch[r];
    nn = strtoul(&pp, &p, 16 );
    hx = hx + nn * pow(16 , i-1);
   }
  return hx;
}

double Round_Legacy(double d, int place)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(place) << d ;
	double r = boost::lexical_cast<double>(ss.str());
	return r;
}

	
double CPIDTime(std::string cpid)
{
	if (cpid.length() < 10) return 0;
	if (cpid=="INVESTOR" || cpid=="investor") return 0;
	std::string h65535 = cpid.substr(0,4);
	double hash = HexToDouble(h65535);
	double timeslice = hash * 1.318;
	return timeslice;
}



int64_t SecondsSinceMidnight(int64_t timestamp)
{
	time_t bigtime;
	bigtime = (time_t)timestamp;
	struct tm *Tm = gmtime(&bigtime);
	double secs = (Tm->tm_hour*60*60) + (Tm->tm_min*60) + (Tm->tm_sec);
	return secs;
}



int64_t CPIDChronoStart(std::string cpid)
{
	double offset = CPIDTime(cpid);
	int64_t NetworkTimeAtMidnight = GetAdjustedTime()-SecondsSinceMidnight(GetAdjustedTime());
	//if (fDebug) printf("Offset %f, NetTime %f, Midnight %f",(double)offset,(double)GetAdjustedTime(),(double)NetworkTimeAtMidnight);
	int64_t CPIDTime = NetworkTimeAtMidnight+(int64_t)offset;
	return CPIDTime;
}

bool IsCPIDTimeValid(std::string cpid, int64_t locktime)
{
	return true;

	if (cpid.empty()) return true;
	if (cpid=="INVESTOR" || cpid=="investor") return true;
	double offset = CPIDTime(cpid);
	int64_t passed = SecondsSinceMidnight(locktime);
	if (fDebug3) printf("In locktime %f, Passed %f, Offset %f \r\n",(double)locktime,(double)passed,(double)offset);
	if (passed >= offset && passed <= (offset+(30*60))) return true;
	return false;
}

int SecondFromGRCAddress()
{
	std::string sAddress = DefaultWalletAddress();
	//GRC Classic: Convert a public GRC address to its corresponding payment hour
	if (sAddress.length() == 0) return 0;
	char ch = *sAddress.rbegin();
	char ch2=tolower(ch);
	int asc = ch2; //Ascii value 0-9 = 48-57, a-z = 97 - 122
	if (asc > 96) asc=asc-39;	
	int lookup = asc-47;	
	double hour = Round_Legacy(lookup/1.5,0);  //convert to 24 hour format, hour now equals 1-24
	if (hour > 24) hour=24;
	if (hour < 1) hour = 1;
	int clockhour = hour-1; //Return 0-23 (Military Clock hours)
	int sec = clockhour * 2.5;
	if (sec < 0) sec = 0;
	if (sec > 59) sec = 59;
	return sec;
}


bool CheckStake(CBlock* pblock, CWallet& wallet)
{
    uint256 proofHash = 0, hashTarget = 0;
    uint256 hashBlock = pblock->GetHash();

    if(!pblock->IsProofOfStake())
        return error("CheckStake() : %s is not a proof-of-stake block", hashBlock.GetHex().c_str());

    // verify hash target and signature of coinstake tx
	if (pblock->vtx.size() < 1)
	{
		printf("CheckStake::HashBoinc too small\r\n");
		return error("CheckStake()::HashBoinc too small");
	}


	if (pblock->nNonce < 100)
	{
		if (fDebug) printf("CheckStake::Nonce too low\r\n");
		return error("CheckStake()::Nonce too low");
	}

	//1-20-2015 Ensure this stake is above the minimum threshhold; otherwise, vehemently reject
	double PORDiff = GetBlockDifficulty(pblock->nBits);
	//double mint1 = CoinToDouble(Mint);
	MiningCPID boincblock = DeserializeBoincBlock(pblock->vtx[0].hashBoinc);
	double total_subsidy = boincblock.ResearchSubsidy + boincblock.InterestSubsidy;
    
	if (fDebug) printf("CheckStake[]: TotalSubsidy %f, cpid %s, Res %f, Interest %f, hb: %s \r\n",
					(double)total_subsidy, boincblock.cpid.c_str(),	boincblock.ResearchSubsidy,boincblock.InterestSubsidy,pblock->vtx[0].hashBoinc.c_str());
			
	if (total_subsidy < MintLimiter(PORDiff,boincblock.RSAWeight,boincblock.cpid,pblock->GetBlockTime() ))
	{
			//Prevent Hackers from spamming the network with small blocks
			printf("****CheckStake[]: Total Mint too Small %s, Res %f, Interest %f, hash %s \r\n",boincblock.cpid.c_str(),
						boincblock.ResearchSubsidy,boincblock.InterestSubsidy,pblock->vtx[0].hashBoinc.c_str());
			return error("*****CheckStake[] : Total Mint too Small, %f",(double)boincblock.ResearchSubsidy+boincblock.InterestSubsidy);
	}
			

	if (!CheckProofOfStake(mapBlockIndex[pblock->hashPrevBlock], pblock->vtx[1], pblock->nBits, proofHash, hashTarget, pblock->vtx[0].hashBoinc, true, pblock->nNonce))
	{	
		if (fDebug) printf("Hash boinc %s",pblock->vtx[0].hashBoinc.c_str());
        return error("CheckStake() : proof-of-stake checking failed");
	}

	
    //// debug print
	double block_value = CoinToDouble(pblock->vtx[1].GetValueOut());
	std::string sBlockValue = RoundToString(block_value,4);	
	//Submit the block during the public key address second
	int wallet_second = 0;

    if (fDebug3) printf("CheckStake() : new proof-of-stake block found, BlockValue %s, \r\n hash: %s \nproofhash: %s  \ntarget: %s\n",
					sBlockValue.c_str(), hashBlock.GetHex().c_str(), proofHash.GetHex().c_str(), hashTarget.GetHex().c_str());
	if (fDebug)     pblock->print();
    if (fDebug) printf("out %s\n", FormatMoney(pblock->vtx[1].GetValueOut()).c_str());

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != hashBestChain)
		{
			msMiningErrors6="CheckStake(): Generated block is stale.";
            return error("CheckStake() : generated block is stale");
		}

		if (false)
		{
		std::string GlobalBlockSolved = GetBestBlockHash(GlobalCPUMiningCPID.cpidv2);
		if (fDebug2) printf("GBS->%s",GlobalBlockSolved.c_str());
		if (GlobalBlockSolved != "NULL" && GlobalBlockSolved.length() > 8 && GlobalBlockSolved != "")
		{
			return error("CheckStake() : generated block is stale - Solved at %s, Block Height: %f \r\n",GlobalBlockSolved.c_str(),(double)nBestHeight+1);
		}
		}

		

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[hashBlock] = 0;
        }

        // Process this block the same as if we had received it from another node
		//Halford - 1-14-2015 - Ensure Blocks have a minimum time spacing
		if (!IsLockTimeWithinMinutes(nLastBlockSubmitted,3)) 
		{
			nLastBlockSubmitted = GetAdjustedTime();
			if (!ProcessBlock(NULL, pblock, true))
			{
				msMiningErrors6="Block vehemently rejected.";
				return error("CheckStake() : ProcessBlock (by me), but block not accepted");
			}
		}
    }
	nLastBlockSolved = GetAdjustedTime();
	
    return true;


}



void StakeMiner(CWallet *pwallet)
{
    SetThreadPriority(THREAD_PRIORITY_LOWEST);

    // Make this thread recognisable as the mining thread
    RenameThread("grc-stake-miner");

    bool fTryToSync = true;

    while (true)
    {
Inception:
        if (fShutdown)
		{
			printf("StakeMiner:ShuttingDown..");
            return;
		}

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
				std::string decrypted = AdvancedDecrypt(passphrase);
				//Unlock the wallet for 10 days (Equivalent to: walletpassphrase mylongpass 999999)
				int64_t nSleepTime = 9999999;
				SecureString strWalletPass;
				strWalletPass.reserve(100);
				strWalletPass = decrypted.c_str();
			    if (strWalletPass.length() > 0)
			    {
					if (!pwallet->Unlock(strWalletPass))
					{
						printf("GridcoinStakeMiner:AutoUnlock:Error: The wallet passphrase entered was incorrect.");
					}
					else
					{
						NewThread(ThreadTopUpKeyPool,NULL);
						int64_t* pnSleepTime = new int64_t(nSleepTime);
						NewThread(ThreadCleanWalletPassphrase, pnSleepTime);
					}
   
				}
			}
		}
	
		// End of encrypted pass feature

        while (pwallet->IsLocked())
        {
            nLastCoinStakeSearchInterval = 0;
            MilliSleep(1000);
            if (fShutdown)
			{
				printf("StakeMiner:Exiting(WalletLocked)");
                return;
			}
        }

		int iFutile=0;
		while (!bNetAveragesLoaded)
		{
			iFutile++;
			if (iFutile > 50) break;
			MilliSleep(100);
			if (LessVerbose(100)) printf("StakeMiner:Net averages not yet loaded...");
		}
	

        while (vNodes.empty() || IsInitialBlockDownload())
        {
            nLastCoinStakeSearchInterval = 0;
            fTryToSync = true;
            MilliSleep(5000);
            if (fShutdown)
			{
				printf("StakeMiner:Exiting(InitialBlockDownload)");
				return;
			}
        }

        if (fTryToSync)
        {
            fTryToSync = false;
            if (vNodes.size() < 3 || nBestHeight < GetNumBlocksOfPeers())
            {
				printf("tryingtosync.");
                MilliSleep(5000);
                continue;
            }
        }
Begin:
		MilliSleep(500);

        //
        // Create new block
        //
        int64_t nFees;
		
        auto_ptr<CBlock> pblock(CreateNewBlock(pwallet, true, &nFees));
        if (!pblock.get())
		{
			//This can happen after reharvesting CPIDs... Because CreateNewBlock() requires a valid CPID..  Start Over.
			printf("a1.");
			MilliSleep(1000);
			goto Begin;
		}
		

		//Verify we are still on the main chain
	
		if (IsLockTimeWithinMinutes(nLastBlockSolved,10)) 
		{
				if (fDebug) printf("=");
				MilliSleep(200);
				goto Begin;
		}

	    // Trying to sign a block
			
        if (pblock->SignBlock(*pwallet, nFees))
        {

            SetThreadPriority(THREAD_PRIORITY_NORMAL);
			bool Staked = CheckStake(pblock.get(), *pwallet);

			if (Staked)
			{
				msMiningErrors = "Stake block accepted!";
				printf("Stake block accepted!\r\n");
		  	    SetThreadPriority(THREAD_PRIORITY_LOWEST);
        		mdPORNonce=0;
			}
			else
			{
				msMiningErrors = "Stake block rejected";
				if (fDebug) printf("Stake block rejected \r\n");
				MilliSleep(1000);
			}
            SetThreadPriority(THREAD_PRIORITY_LOWEST);
            MilliSleep(500);
        }
        else
            MilliSleep(nMinerSleep);
    }
}
