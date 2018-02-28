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
#include "main.h"

#include <memory>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// Gridcoin Miner
//

unsigned int nMinerSleep;
MiningCPID GetNextProject(bool bForce);
void ThreadCleanWalletPassphrase(void* parg);
double MintLimiter(double PORDiff,int64_t RSA_WEIGHT,std::string cpid,int64_t locktime);
double CoinToDouble(double surrogate);
StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sFrom);

void ThreadTopUpKeyPool(void* parg);

std::string SerializeBoincBlock(MiningCPID mcpid);
bool LessVerbose(int iMax1000);

int64_t GetRSAWeightByBlock(MiningCPID boincblock);
bool SignBlockWithCPID(const std::string& sCPID, const std::string& sBlockHash, std::string& sSignature, std::string& sError, bool bAdvertising = false);
std::string qtGetNeuralContract(std::string data);

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
        for (auto const& hash : setDependsOn)
            printf("   setDependsOn %s\n", hash.ToString().substr(0,10).c_str());
    }
};

CMinerStatus::CMinerStatus(void)
{
    Clear();
    ReasonNotStaking= "";
    CreatedCnt= AcceptedCnt= KernelsFound= 0;
    KernelDiffMax= 0;
}

void CMinerStatus::Clear()
{
    Message= "";
    WeightSum= ValueSum= WeightMin= WeightMax= 0;
    Version= 0;
    CoinAgeSum= 0;
    KernelDiffSum = 0;
    nLastCoinStakeSearchInterval = 0;
}

CMinerStatus MinerStatus;
 
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
            for (auto const& txin : tx.vin)
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
                    + ToString(nBlockSize) + "," + ToString(nTxSize) + ")("
                    + ToString(nBlockSize) + ");";
            
                continue;
            }

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = tx.GetLegacySigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":LegacySigOpLimit(" + 
                    ToString(nBlockSigOps) + "," + ToString(nTxSigOps) + ")("
                    + ToString(MAX_BLOCK_SIGOPS) + ");";
                continue;
            }

            // Timestamp limit
            if (tx.nTime >  block.nTime)
            {
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":TimestampLimit(" + ToString(tx.nTime) + ","
                    + ToString(block.vtx[0].nTime) + ");";
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
                    + ToString(nBlockSigOps) + "," + ToString(nTxSigOps) + ")("
                    + ToString(MAX_BLOCK_SIGOPS) + ");";
            
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
                for (auto const& porphan : mapDependers[hash])
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


bool CreateCoinStake( CBlock &blocknew, CKey &key,
    vector<const CWalletTx*> &StakeInputs, uint64_t &CoinAge,
    CWallet &wallet, CBlockIndex* pindexPrev )
{
    int64_t CoinWeight;
    CBigNum StakeKernelHash;
    CTxDB txdb("r");
    int64_t StakeWeightSum = 0;
    double StakeValueSum = 0;
    int64_t StakeWeightMin=MAX_MONEY;
    int64_t StakeWeightMax=0;
    uint64_t StakeCoinAgeSum=0;
    double StakeDiffSum = 0;
    double StakeDiffMax = 0;
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
        LOCK(MinerStatus.lock);
        MinerStatus.ReasonNotStaking+="No coins to stake; ";
        if (fDebug) printf("CreateCoinStake: %s",MinerStatus.ReasonNotStaking.c_str());
        return false;
    }
    BalanceToStake -= nReserveBalance;

    if(fDebug2) printf("\nCreateCoinStake: Staking nTime/16= %d Bits= %u\n",
    txnew.nTime/16,blocknew.nBits);

    for(const auto& pcoin : CoinsToStake)
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

        {
            int64_t nStakeValue= CoinTx.vout[CoinTxN].nValue;
            StakeValueSum += nStakeValue /(double)COIN;
            //crazy formula...
            // todo: clean this
            // todo reuse calculated value for interst
            CBigNum bn = CBigNum(nStakeValue) * (blocknew.nTime-CoinTx.nTime) / CENT;
            bn = bn * CENT / COIN / (24 * 60 * 60);
            StakeCoinAgeSum += bn.getuint64();
        }

        if(blocknew.nVersion==7)
        {
            NetworkTimer();
            CoinWeight = CalculateStakeWeightV3(CoinTx,CoinTxN,GlobalCPUMiningCPID);
            StakeKernelHash= CalculateStakeHashV3(CoinBlock,CoinTx,CoinTxN,txnew.nTime,GlobalCPUMiningCPID,mdPORNonce);
        }
        else
        {
            uint64_t StakeModifier = 0;
            if(!FindStakeModifierRev(StakeModifier,pindexPrev))
                continue;
            CoinWeight = CalculateStakeWeightV8(CoinTx,CoinTxN,GlobalCPUMiningCPID);
            StakeKernelHash= CalculateStakeHashV8(CoinBlock,CoinTx,CoinTxN,txnew.nTime,StakeModifier,GlobalCPUMiningCPID);
        }

        CBigNum StakeTarget;
        StakeTarget.SetCompact(blocknew.nBits);
        StakeTarget*=CoinWeight;
        StakeWeightSum += CoinWeight;
        StakeWeightMin=std::min(StakeWeightMin,CoinWeight);
        StakeWeightMax=std::max(StakeWeightMax,CoinWeight);
        double StakeKernelDiff = GetBlockDifficulty(StakeKernelHash.GetCompact())*CoinWeight;
        StakeDiffSum += StakeKernelDiff;
        StakeDiffMax = std::max(StakeDiffMax,StakeKernelDiff);

        if (fDebug2) {
            int64_t RSA_WEIGHT = GetRSAWeightByBlock(GlobalCPUMiningCPID);
            printf(
"CreateCoinStake: V%d Time %.f, Por_Nonce %.f, Bits %jd, Weight %jd\n"
" RSA_WEIGHT %.f\n"
" Stk %72s\n"
" Trg %72s\n"
" Diff %0.7f of %0.7f\n",
            blocknew.nVersion,
            (double)txnew.nTime, mdPORNonce,
            (intmax_t)blocknew.nBits,(intmax_t)CoinWeight,
            (double)RSA_WEIGHT,
            StakeKernelHash.GetHex().c_str(), StakeTarget.GetHex().c_str(),
            StakeKernelDiff, GetBlockDifficulty(blocknew.nBits)
            );
        }

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

            LOCK(MinerStatus.lock);
            MinerStatus.Message+="Found Kernel "+ ToString(CoinToDouble(nCredit))+"; ";
            MinerStatus.KernelsFound++;
            MinerStatus.KernelDiffMax = 0;
            MinerStatus.KernelDiffSum = StakeDiffSum;
            return true;
        }
    }

    LOCK(MinerStatus.lock);
    MinerStatus.Message+="Stake Weight "+ ToString(StakeWeightSum)+"; ";
    MinerStatus.WeightSum = StakeWeightSum;
    MinerStatus.ValueSum = StakeValueSum;
    MinerStatus.WeightMin=StakeWeightMin;
    MinerStatus.WeightMax=StakeWeightMax;
    MinerStatus.CoinAgeSum=StakeCoinAgeSum;
    MinerStatus.KernelDiffMax = std::max(MinerStatus.KernelDiffMax,StakeDiffMax);
    MinerStatus.KernelDiffSum = StakeDiffSum;
    MinerStatus.nLastCoinStakeSearchInterval= txnew.nTime;
    return false;
}

bool SignStakeBlock(CBlock &block, CKey &key, vector<const CWalletTx*> &StakeInputs, CWallet *pwallet, MiningCPID& BoincData)
{
    //Append beacon signature to coinbase
    std::string PublicKey = GlobalCPUMiningCPID.BoincPublicKey;
    if (!PublicKey.empty())
    {
        std::string sBoincSignature;
        std::string sError;
        bool bResult = SignBlockWithCPID(GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.lastblockhash, sBoincSignature, sError);
        if (!bResult)
        {
            if (fDebug2) printf("SignStakeBlock: Failed to sign block -> %s\n", sError.c_str());
            return false;
        }
        BoincData.BoincSignature = sBoincSignature;
        if(fDebug2) printf("Signing BoincBlock for cpid %s and blockhash %s with sig %s\r\n",GlobalCPUMiningCPID.cpid.c_str(),GlobalCPUMiningCPID.lastblockhash.c_str(),BoincData.BoincSignature.c_str());
    }
    block.vtx[0].hashBoinc = SerializeBoincBlock(BoincData,block.nVersion);
    //if (fDebug2)  printf("SignStakeBlock: %s\r\n",SerializedBoincData.c_str());

    //Sign the coinstake transaction
    unsigned nIn = 0;
    for (auto const& pcoin : StakeInputs)
    {
        if (!SignSignature(*pwallet, *pcoin, block.vtx[1], nIn++)) 
        {
            LOCK(MinerStatus.lock);
            MinerStatus.Message+="Failed to sign coinstake; ";
            return error("SignStakeBlock: failed to sign coinstake");
        }
    }

    //Sign the whole block
    block.hashMerkleRoot = block.BuildMerkleTree();
    if( !key.Sign(block.GetHash(), block.vchBlockSig) )
    {
        LOCK(MinerStatus.lock);
        MinerStatus.Message+="Failed to sign block; ";
        return error("SignStakeBlock: failed to sign block");
    }

    return true;
}

int AddNeuralContractOrVote(const CBlock &blocknew, MiningCPID &bb)
{
    std::string sb_contract;

    if(OutOfSyncByAge())
        return printf("AddNeuralContractOrVote: Out Of Sync\n");

    /* Retrive the neural Contract */
    #if defined(WIN32) && defined(QT_GUI)
        sb_contract = qtGetNeuralContract("");
    #endif

    const std::string sb_hash = GetQuorumHash(sb_contract);

    /* To save network bandwidth, start posting the neural hashes in the
       CurrentNeuralHash field, so that out of sync neural network nodes can
       request neural data from those that are already synced and agree with the
       supermajority over the last 24 hrs
       Note: CurrentNeuralHash is not actually used for sb validity
    */
    bb.CurrentNeuralHash = sb_hash;

    if(sb_contract.empty())
        return printf("AddNeuralContractOrVote: Local Contract Empty\n");

    if(!IsNeuralNodeParticipant(bb.GRCAddress, blocknew.nTime))
        return printf("AddNeuralContractOrVote: Not Participating\n");

    if(blocknew.nVersion >= 9)
    {
        // break away from block timing
        if (fDebug) printf("AddNeuralContractOrVote: Updating Neural Supermajority (v9 M) height %d\n",nBestHeight);
        ComputeNeuralNetworkSupermajorityHashes();
    }

    if(!NeedASuperblock())
        return printf("AddNeuralContractOrVote: not Needed\n");

    int pending_height = RoundFromString(ReadCache("neuralsecurity","pending"),0);

    /* Add our Neural Vote */
    bb.NeuralHash = sb_hash;
    printf("AddNeuralContractOrVote: Added our Neural Vote %s\n",sb_hash.c_str());

    if (pending_height>=(pindexBest->nHeight-200))
        return printf("AddNeuralContractOrVote: already Pending\n");

    double popularity = 0;
    std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);

    if (consensus_hash!=sb_hash)
        return printf("AddNeuralContractOrVote: not in Consensus\n");

    /* We have consensus, Add our neural contract */
    bb.superblock = PackBinarySuperblock(sb_contract);
    printf("AddNeuralContractOrVote: Added our Superblock (size %" PRIszu ")\n",bb.superblock.length());

    return 0;
}

bool CreateGridcoinReward(CBlock &blocknew, MiningCPID& miningcpid, uint64_t &nCoinAge, CBlockIndex* pindexPrev)
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

    
    miningcpid = GlobalCPUMiningCPID;
    uint256 pbh = 0;
    pbh=pindexPrev->GetBlockHash();

    miningcpid.lastblockhash = pbh.GetHex();
    miningcpid.ResearchSubsidy = OUT_POR;
    miningcpid.ResearchSubsidy2 = OUT_POR;
    miningcpid.ResearchAge = dAccrualAge;
    miningcpid.ResearchMagnitudeUnit = dAccrualMagnitudeUnit;
    miningcpid.ResearchAverageMagnitude = dAccrualMagnitude;
    miningcpid.InterestSubsidy = out_interest;
    miningcpid.BoincSignature = "";
    miningcpid.CurrentNeuralHash = "";
    miningcpid.NeuralHash = "";
    miningcpid.superblock = "";
    miningcpid.GRCAddress = DefaultWalletAddress();

    // Make sure this deprecated fields are empty
    miningcpid.cpidv2.clear();
    miningcpid.email.clear();
    miningcpid.boincruntimepublickey.clear();
    miningcpid.aesskein.clear();
    miningcpid.enccpid.clear();
    miningcpid.encboincpublickey.clear();
    miningcpid.encaes.clear();


    int64_t RSA_WEIGHT = GetRSAWeightByBlock(miningcpid);
    GlobalCPUMiningCPID.lastblockhash = miningcpid.lastblockhash;

    double mint = CoinToDouble(nReward);
    double PORDiff = GetBlockDifficulty(blocknew.nBits);
    double mintlimit = MintLimiter(PORDiff,RSA_WEIGHT,miningcpid.cpid,blocknew.nTime);

    printf("CreateGridcoinReward: for %s mint %f {RSAWeight %f} Research %f, Interest %f \r\n",
        miningcpid.cpid.c_str(), mint, (double)RSA_WEIGHT,miningcpid.ResearchSubsidy,miningcpid.InterestSubsidy);

    //INVESTORS
    if(blocknew.nVersion < 8) mintlimit = std::max(mintlimit, 0.0051);
    if (nReward == 0 || mint < mintlimit)
    {
            LOCK(MinerStatus.lock);
            MinerStatus.Message+="Mint "+RoundToString(mint,6)+" too small (min "+RoundToString(mintlimit,6)+"); ";
            return error("CreateGridcoinReward: Mint %f of %f too small",(double)mint,(double)mintlimit);
    }

    //fill in reward and boinc
    blocknew.vtx[1].vout[1].nValue += nReward;
    LOCK(MinerStatus.lock);
    MinerStatus.Message+="Added Reward "+RoundToString(mint,3)
        +"("+RoundToString(CoinToDouble(nFees),4)+" "
        +RoundToString(out_interest,2)+" "
        +RoundToString(OUT_POR,2)+"); ";
    return true;
}

bool IsMiningAllowed(CWallet *pwallet)
{
    bool status = true;
    if(pwallet->IsLocked())
    {
        LOCK(MinerStatus.lock);
        MinerStatus.ReasonNotStaking+="Wallet locked; ";
        status=false;
    }

    if(fDevbuildCripple)
    {
        LOCK(MinerStatus.lock);
        MinerStatus.ReasonNotStaking+="Testnet-only version; ";
        status=false;
    }

    if (!bNetAveragesLoaded)
    {
        LOCK(MinerStatus.lock);
        MinerStatus.ReasonNotStaking+="Net averages not yet loaded; ";
        if (LessVerbose(100) && IsResearcher(msPrimaryCPID)) printf("ResearchMiner:Net averages not yet loaded...");
        status=false;
    }

    if (vNodes.empty() || (!fTestNet&& IsInitialBlockDownload()) ||
        (!fTestNet&& vNodes.size() < 3)
        )
    {
        LOCK(MinerStatus.lock);
        MinerStatus.ReasonNotStaking+="Offline; ";
        status=false;
    }

    return status;
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

        CBlockIndex* pindexPrev = pindexBest;
        CBlock StakeBlock;
        MiningCPID BoincData;
        { LOCK(MinerStatus.lock);
            //clear miner messages
            MinerStatus.Message="";
            MinerStatus.ReasonNotStaking="";

            //New versions
            StakeBlock.nVersion = 7;
            if(IsV8Enabled(pindexPrev->nHeight+1))
                StakeBlock.nVersion = 8;
            if(IsV9Enabled(pindexPrev->nHeight+1))
                StakeBlock.nVersion = 9;

            MinerStatus.Version= StakeBlock.nVersion;
        }

        if(!IsMiningAllowed(pwallet))
        {
            LOCK(MinerStatus.lock);
            MinerStatus.Clear();
            continue;
        }

        // Lock main lock since GetNextProject and subsequent calls
        // require the state to be static.
        LOCK(cs_main);

        GetNextProject(true);

        // * Create a bare block
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
        if( !CreateCoinStake( StakeBlock, BlockKey, StakeInputs, StakeCoinAge, *pwallet, pindexPrev ) )
            continue;
        StakeBlock.nTime= StakeTX.nTime;

        // * create rest of the block
        if( !CreateRestOfTheBlock(StakeBlock,pindexPrev) )
            continue;
        printf("StakeMiner: created rest of the block\n");

        // * add gridcoin reward to coinstake
        if( !CreateGridcoinReward(StakeBlock,BoincData,StakeCoinAge,pindexPrev) )
            continue;
        printf("StakeMiner: added gridcoin reward to coinstake\n");

        AddNeuralContractOrVote(StakeBlock, BoincData);

        // * sign boinchash, coinstake, wholeblock
        if( !SignStakeBlock(StakeBlock,BlockKey,StakeInputs,pwallet,BoincData) )
            continue;
        printf("StakeMiner: signed boinchash, coinstake, wholeblock\n");

        { LOCK(MinerStatus.lock);
            MinerStatus.CreatedCnt++;
        }

        // * delegate to ProcessBlock
        if (!ProcessBlock(NULL, &StakeBlock, true))
        {
            { LOCK(MinerStatus.lock);
                MinerStatus.Message+="Block vehemently rejected; ";
            }
            error("StakeMiner: Block vehemently rejected");
            continue;
        }

        printf("StakeMiner: block processed\n");
        { LOCK(MinerStatus.lock);
            MinerStatus.AcceptedCnt++;
            nLastBlockSolved = GetAdjustedTime();
        }

    } //end while(!fShutdown)
}

