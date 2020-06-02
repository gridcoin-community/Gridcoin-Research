// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "miner.h"
#include "kernel.h"
#include "main.h"
#include "neuralnet/beacon.h"
#include "neuralnet/claim.h"
#include "neuralnet/contract/contract.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/tally.h"
#include "util.h"

#include <memory>
#include <algorithm>
#include <tuple>
#include <random>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// Gridcoin Miner
//

unsigned int nMinerSleep;
double CoinToDouble(double surrogate);
bool LessVerbose(int iMax1000);

namespace {
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
        LogPrintf("COrphan(hash=%s, dPriority=%.1f, dFeePerKb=%.1f)",
               ptx->GetHash().ToString().substr(0,10), dPriority, dFeePerKb);
        for (auto const& hash : setDependsOn)
            LogPrintf("   setDependsOn %s", hash.ToString().substr(0,10));
    }
};

//!
//! \brief Helper: update the miner status with a message that indicates why
//! \c CreateCoinStake() skipped a cycle and then return \c false .
//!
//! \param status  The global miner status object to update.
//! \param message Describes why the wallet contains no coins for staking.
//!
//! \return Always \false - suitable for returning from the call directly.
//!
bool ReturnMinerError(CMinerStatus& status, CMinerStatus::ReasonNotStakingCategory& not_staking_error)
{
    LOCK(status.lock);

    status.Clear();

    status.SetReasonNotStaking(not_staking_error);

    LogPrint(BCLog::LogFlags::VERBOSE, "CreateCoinStake: %s", MinerStatus.ReasonNotStaking);

    return false;
}

//!
//! \brief Sign the research reward claim context for a newly-minted block.
//!
//! \param pwallet         Supplys beacon private keys for signing.
//! \param claim           An initialized claim to sign for a block.
//! \param block_timestamp Creation time of the claim.
//! \param last_block_hash Hash of the previous block to sign into the claim.
//!
//! \return \c true if the miner holds active beacon keys used to successfully
//! sign the claim.
//!
bool SignClaim(
    CWallet* pwallet,
    NN::Claim& claim,
    const int64_t block_timestamp,
    const uint256& last_block_hash)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pwallet->cs_wallet);

    const NN::CpidOption cpid = claim.m_mining_id.TryCpid();

    if (!cpid) {
        return true; // Skip beacon signature for investors.
    }

    const NN::BeaconOption beacon = NN::GetBeaconRegistry().Try(*cpid);

    if (!beacon) {
        return error("%s: No active beacon", __func__);
    }

    if (beacon->Expired(block_timestamp)) {
        return error("%s: Beacon expired", __func__);
    }

    CKey beacon_key;

    if (!pwallet->GetKey(beacon->m_public_key.GetID(), beacon_key)) {
        return error("%s: Missing beacon private key", __func__);
    }

    if (!beacon_key.IsValid()) {
        return error("%s: Invalid beacon key", __func__);
    }

    if (!claim.Sign(beacon_key, last_block_hash)) {
        return error("%s: Signature failed. Check beacon key", __func__);
    }

    LogPrint(BCLog::LogFlags::MINER,
             "%s: Signed for CPID %s and block hash %s with signature %s",
             __func__,
             cpid->ToString(),
             last_block_hash.ToString(),
             HexStr(claim.m_signature));

    return true;
}
} // anonymous namespace

CMinerStatus::CMinerStatus(void)
{
    Clear();
    ClearReasonsNotStaking();
    CreatedCnt= AcceptedCnt= KernelsFound= 0;
    KernelDiffMax= 0;
}

void CMinerStatus::Clear()
{
    WeightSum= ValueSum= WeightMin= WeightMax= 0;
    Version= 0;
    KernelDiffSum = 0;
    nLastCoinStakeSearchInterval = 0;
}

bool CMinerStatus::SetReasonNotStaking(ReasonNotStakingCategory not_staking_error)
{
    bool inserted = false;

    if (std::find(vReasonNotStaking.begin(), vReasonNotStaking.end(), not_staking_error) == vReasonNotStaking.end())
    {
       vReasonNotStaking.insert(vReasonNotStaking.end(), not_staking_error);

       if (not_staking_error != NONE)
       {
           if (!ReasonNotStaking.empty()) ReasonNotStaking += "; ";
           ReasonNotStaking += vReasonNotStakingStrings[static_cast<int>(not_staking_error)];
       }

       if (not_staking_error > NO_MATURE_COINS) able_to_stake = false;

       inserted = true;
    }

    return inserted;
}

void CMinerStatus::ClearReasonsNotStaking()
{
    vReasonNotStaking.clear();
    ReasonNotStaking.clear();
    able_to_stake = true;
}

CMinerStatus MinerStatus;

// We want to sort transactions by priority and fee, so:
typedef std::tuple<double, double, CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;
public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }
    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (std::get<1>(a) == std::get<1>(b))
                return std::get<0>(a) < std::get<0>(b);
            return std::get<1>(a) < std::get<1>(b);
        }
        else
        {
            if (std::get<0>(a) == std::get<0>(b))
                return std::get<1>(a) < std::get<1>(b);
            return std::get<0>(a) < std::get<0>(b);
        }
    }
};

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

    if (block.nVersion <= 10) {
        CoinBase.nVersion = 1; // TODO: remove after mandatory
    }

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
    int64_t nMinTxFee = CoinBase.GetBaseFee();
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

                LogPrint(BCLog::LogFlags::NOISY, "Enumerating tx %s ",tx.GetHash().GetHex());

                if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input");
                        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE)) assert("mempool transaction missing input" == 0);
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
                        LogPrint(BCLog::LogFlags::NOISY, "Orphan tx %s ",tx.GetHash().GetHex());
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
            double dPriority = std::get<0>(vecPriority.front());
            double dFeePerKb = std::get<1>(vecPriority.front());
            CTransaction& tx = *(std::get<2>(vecPriority.front()));

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

            if (nBlockSize + nTxSize >= nBlockMaxSize)
            {
                LogPrintf("Tx size too large for tx %s blksize %" PRIu64 ", tx size %" PRId64, tx.GetHash().GetHex(), nBlockSize, nTxSize);
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
                LogPrint(BCLog::LogFlags::NOISY, "Unable to fetch inputs for tx %s ", tx.GetHash().GetHex());
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":UnableToFetchInputs;";
                continue;
            }

            int64_t nTxFees = tx.GetValueIn(mapInputs)-tx.GetValueOut();
            if (nTxFees < nMinFee)
            {
                LogPrint(BCLog::LogFlags::NOISY, "Not including tx %s  due to TxFees of %" PRId64 ", bare min fee is %" PRId64, tx.GetHash().GetHex(), nTxFees, nMinFee);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":FeeTooSmall("
                    + RoundToString(CoinToDouble(nFees),8) + "," +RoundToString(CoinToDouble(nMinFee),8) + ");";
                continue;
            }

            nTxSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                LogPrint(BCLog::LogFlags::NOISY, "Not including tx %s due to exceeding max sigops of %d, sigops is %d",
                    tx.GetHash().GetHex(), (nBlockSigOps+nTxSigOps), MAX_BLOCK_SIGOPS);
                msMiningErrorsExcluded += tx.GetHash().GetHex() + ":ExceededSigOps("
                    + ToString(nBlockSigOps) + "," + ToString(nTxSigOps) + ")("
                    + ToString(MAX_BLOCK_SIGOPS) + ");";

                continue;
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapTestPoolTmp, CDiskTxPos(1,1,1), pindexPrev, false, true))
            {
                LogPrint(BCLog::LogFlags::NOISY, "Unable to connect inputs for tx %s ",tx.GetHash().GetHex());
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

            if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY) || GetBoolArg("-printpriority"))
            {
                LogPrintf("priority %.1f feeperkb %.1f txid %s",
                       dPriority, dFeePerKb, tx.GetHash().ToString());
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

        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY) || GetBoolArg("-printpriority"))
            LogPrintf("CreateNewBlock(): total size %" PRIu64, nBlockSize);
    }

    //Add fees to coinbase
    block.vtx[0].vout[0].nValue= nFees;

    // Fill in header
    block.hashPrevBlock  = pindexPrev->GetBlockHash();
    block.vtx[0].nTime=block.nTime;

    return true;
}


bool CreateCoinStake( CBlock &blocknew, CKey &key,
    vector<const CWalletTx*> &StakeInputs,
    CWallet &wallet, CBlockIndex* pindexPrev )
{
    int64_t CoinWeight;
    CBigNum StakeKernelHash;
    CTxDB txdb("r");
    int64_t StakeWeightSum = 0;
    double StakeValueSum = 0;
    int64_t StakeWeightMin=MAX_MONEY;
    int64_t StakeWeightMax=0;
    double StakeDiffSum = 0;
    double StakeDiffMax = 0;
    CTransaction &txnew = blocknew.vtx[1]; // second tx is coinstake

    if (blocknew.nVersion <= 10) {
        txnew.nVersion = 1; // TODO: remove after mandatory
    }

    //initialize the transaction
    txnew.nTime = blocknew.nTime & (~STAKE_TIMESTAMP_MASK);
    txnew.vin.clear();
    txnew.vout.clear();

    // Choose coins to use
    vector<pair<const CWalletTx*,unsigned int>> CoinsToStake;
    CMinerStatus::ReasonNotStakingCategory not_staking_error;

    if (!wallet.SelectCoinsForStaking(txnew.nTime, CoinsToStake, not_staking_error, true))
    {
        ReturnMinerError(MinerStatus, not_staking_error);

        return false;
    }

    LogPrint(BCLog::LogFlags::MINER, "CreateCoinStake: Staking nTime/16= %d Bits= %u",
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

        StakeValueSum += CoinTx.vout[CoinTxN].nValue / (double)COIN;

        uint64_t StakeModifier = 0;
        if(!FindStakeModifierRev(StakeModifier,pindexPrev))
            continue;
        CoinWeight = CalculateStakeWeightV8(CoinTx,CoinTxN);
        StakeKernelHash.setuint256(CalculateStakeHashV8(CoinBlock,CoinTx,CoinTxN,txnew.nTime,StakeModifier));

        CBigNum StakeTarget;
        StakeTarget.SetCompact(blocknew.nBits);
        StakeTarget*=CoinWeight;
        StakeWeightSum += CoinWeight;
        StakeWeightMin=std::min(StakeWeightMin,CoinWeight);
        StakeWeightMax=std::max(StakeWeightMax,CoinWeight);
        double StakeKernelDiff = GetBlockDifficulty(StakeKernelHash.GetCompact())*CoinWeight;
        StakeDiffSum += StakeKernelDiff;
        StakeDiffMax = std::max(StakeDiffMax,StakeKernelDiff);

        LogPrint(BCLog::LogFlags::MINER,
                 "CreateCoinStake: V%d Time %d, Bits %u, Weight %" PRId64 "\n"
                 " Stk %72s\n"
                 " Trg %72s\n"
                 " Diff %0.7f of %0.7f",
                 blocknew.nVersion,
                 txnew.nTime,
                 blocknew.nBits,
                 CoinWeight,
                 StakeKernelHash.GetHex(),
                 StakeTarget.GetHex(),
                 StakeKernelDiff,
                 GetBlockDifficulty(blocknew.nBits));

        if( StakeKernelHash <= StakeTarget )
        {
            // Found a kernel
            LogPrintf("CreateCoinStake: Found Kernel;");
            vector<valtype> vSolutions;
            txnouttype whichType;
            CScript scriptPubKeyOut;
            CScript scriptPubKeyKernel;
            scriptPubKeyKernel = CoinTx.vout[CoinTxN].scriptPubKey;
            if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
            {
                LogPrintf("CreateCoinStake: failed to parse kernel");
                break;
            }
            if (whichType == TX_PUBKEYHASH) // pay to address type
            {
                // convert to pay to public key type
                if (!wallet.GetKey(uint160(vSolutions[0]), key))
                {
                    LogPrintf("CreateCoinStake: failed to get key for kernel type=%d", whichType);
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
                    LogPrintf("CreateCoinStake: failed to get key for kernel type=%d", whichType);
                    break;  // unable to find corresponding public key
                }

                scriptPubKeyOut = scriptPubKeyKernel;
            }
            else
            {
                LogPrintf("CreateCoinStake: no support for kernel type=%d", whichType);
                break;  // only support pay to public key and pay to address
            }

            txnew.vin.push_back(CTxIn(CoinTx.GetHash(), CoinTxN));
            StakeInputs.push_back(pcoin.first);

            int64_t nCredit = CoinTx.vout[CoinTxN].nValue;

            txnew.vout.push_back(CTxOut(0, CScript())); // First Must be empty
            txnew.vout.push_back(CTxOut(nCredit, scriptPubKeyOut));

            LogPrintf("CreateCoinStake: added kernel type=%d credit=%f", whichType,CoinToDouble(nCredit));

            LOCK(MinerStatus.lock);
            MinerStatus.KernelsFound++;
            MinerStatus.KernelDiffMax = 0;
            MinerStatus.KernelDiffSum = StakeDiffSum;
            return true;
        }
    }

    LOCK(MinerStatus.lock);
    MinerStatus.WeightSum = StakeWeightSum;
    MinerStatus.ValueSum = StakeValueSum;
    MinerStatus.WeightMin=StakeWeightMin;
    MinerStatus.WeightMax=StakeWeightMax;
    MinerStatus.KernelDiffMax = std::max(MinerStatus.KernelDiffMax,StakeDiffMax);
    MinerStatus.KernelDiffSum = StakeDiffSum;
    MinerStatus.nLastCoinStakeSearchInterval= txnew.nTime;
    return false;
}



void SplitCoinStakeOutput(CBlock &blocknew, int64_t &nReward, bool &fEnableStakeSplit, bool &fEnableSideStaking,
    SideStakeAlloc &vSideStakeAlloc, int64_t &nMinStakeSplitValue, double &dEfficiency)
{
    // When this function is called, CreateCoinStake and CreateGridcoinReward have already been called
    // and there will be a single coinstake output (besides the empty one) that has the combined stake + research
    // reward. This function does the following...
    // 1. Perform reward payment to specified addresses ("sidestaking") in the following manner...
    //      a. Check if both flags false and if so return with no action.
    //      b. Limit number of outputs based on bv. 3 for <=9 and 8 for >= 10.
    //      c. Pull the nValue from the original output and store locally. (nReward was passed in.)
    //      d. Pop the existing outputs.
    //      e. Validate each address provided for redirection in turn. If valid, create an output of the
    //         reward * the specified percentage at the validated address. Keep a running total of the reward
    //         redirected. If the percentages add up to less than 100%, then the running total will be less
    //         than the original specified reward after pushing up to two reward outputs. Check before each allocation
    //         that 100% will not be exceeded. If there is a residual reward left, then add (nReward - running total)
    //         back to the base coinstake. This also works beautifully if the redirection is disabled, because then
    //         no reward outputs will be created, the accumulated total of redirected rewards will be zero,
    //         and therefore the subtraction will put the entire rewards back on the base coinstake.
    // 2. Perform stake output splitting of remaining value.
    //      a. With the remainder value left which is now the original coinstake minus (rewards outputs pushed
    //         - up to two), call GGetNumberOfStakeOutputs(int64_t nValue, unsigned int nOutputsAlreadyPushed)
    //         to determine how many pieces to split into, limiting to 8 - OutputsAlreadyPushed.
    //      b. SplitCoinStakeOutput will then push the remaining value into the determined number of outputs
    //         of equal size using the original coinstake input address.
    //      c. The outputs will be in the wrong order, so add the empty vout to the end and then reverse
    //         vout.begin() to vout.end() to make sure the empty vout is in [0] position and a coinstake
    //         vout is in the [1] position. This is required by block validation.

    // If somehow we got here and both flags are false, return immediately. This should not happen if called
    // from StakeMiner, because one or both of the flags must be set to trigger the call, but check anyway.
    if (!fEnableStakeSplit && !fEnableSideStaking)
        return;

    // vtx[1].vout size should be 2 at this point. If not something is really wrong so assert immediately.
    assert(blocknew.vtx[1].vout.size() == 2);

    // Record the script public key for the base coinstake so we can reuse.
    CScript CoinStakeScriptPubKey = blocknew.vtx[1].vout[1].scriptPubKey;

    // The maximum number of outputs allowed on the coinstake txn is 3 for block version 9 and below and
    // 8 for 10 and above. The first one must be empty, so that gives 2 and 7 usable ones, respectively.
    unsigned int nMaxOutputs = (blocknew.nVersion >= 10) ? 8 : 3;
    // Set the maximum number of sidestake ouputs to two less than the maximum allowable coinstake outputs
    // to ensure outputs are reserved for the coinstake output itself and the empty one. Any sidestake
    // addresses and percentages in excess of this number will be ignored.
    unsigned int nMaxSideStakeOutputs = nMaxOutputs - 2;
    // Initialize nOutputUsed at 1, because one is already used for the empty coinstake flag output.
    unsigned int nOutputsUsed = 1;

    // If the number of sidestaking allocation entries exceeds nMaxSideStakeOutputs, then shuffle the vSideStakeAlloc
    // to support sidestaking with more than six entries. This is a super simple solution but has some disadvantages.
    // If the person made a mistake and has the entries in the config file add up to more than 100%, then those entries
    // resulting a cumulative total over 100% will always be excluded, not just randomly excluded, because the cumulative
    // check is done in the order of the entries in the config file. This is not regarded as a big issue, because
    // all of the entries are supposed to add up to less than or equal to 100%. Also when there are more than
    // mMaxSideStakeOutput entries, the residual returned to the coinstake will vary when the entries are shuffled,
    // because the total percentage of the selected entries will be randomized. No attempt to renormalize
    // the percentages is done.
    if (vSideStakeAlloc.size() > nMaxSideStakeOutputs)
    {
        unsigned int seed = static_cast<unsigned int>(GetAdjustedTime());

        std::shuffle(vSideStakeAlloc.begin(), vSideStakeAlloc.end(), std::default_random_engine(seed));
    }

    // Initialize remaining stake output value to the total value of output for stake, which also includes
    // (interest or CBR) and research rewards.
    int64_t nRemainingStakeOutputValue = blocknew.vtx[1].vout[1].nValue;

    // We need the input value later.
    int64_t nInputValue = nRemainingStakeOutputValue - nReward;

    // Remove the existing single stake output and the empty coinstake to prepare for splitting. (The empty one
    // needs to be removed too, because we need to reverse the order of the outputs at the end. See the bottom of
    // this function.
    blocknew.vtx[1].vout.pop_back();
    blocknew.vtx[1].vout.pop_back();

    CScript SideStakeScriptPubKey;
    double dSumAllocation = 0.0;

    if (fEnableSideStaking)
    {
        // Iterate through passed in SideStake vector until either all elements processed, the maximum number of
        // sidestake outputs is reached, or accumulated allocation will exceed 100%.
        for(auto iterSideStake = vSideStakeAlloc.begin(); (iterSideStake != vSideStakeAlloc.end()) && (nOutputsUsed <= nMaxSideStakeOutputs); ++iterSideStake)
        {
            CBitcoinAddress address(iterSideStake->first);
            if (!address.IsValid())
            {
                LogPrintf("WARN: SplitCoinStakeOutput: ignoring sidestake invalid address %s.", iterSideStake->first.c_str());
                continue;
            }

            // Do not process a distribution that would result in an output less than 1 CENT. This will flow back into the coinstake below.
            // Prevents dust build-up.
            if (nReward * iterSideStake->second < CENT)
            {
                LogPrintf("WARN: SplitCoinStakeOutput: distribution %f too small to address %s.", CoinToDouble(nReward * iterSideStake->second), iterSideStake->first.c_str());
                continue;
            }

            if (dSumAllocation + iterSideStake->second > 1.0)
            {
                LogPrintf("WARN: SplitCoinStakeOutput: allocation percentage over 100\%, ending sidestake allocations.");
                break;
            }

            // Push to an output the (reward times the allocation) to the address, increment the accumulator for allocation,
            // decrement the remaining stake output value, and increment outputs used.

            SideStakeScriptPubKey.SetDestination(address.Get());

            // It is entirely possible that the coinstake could be from an address that is specified in one of the sidestake entries
            // if the sidestake address(es) are local to the staking wallet. There is no reason to sidestake in that case. The
            // coins should flow down to the coinstake outputs and be returned there. This will also simplify the display logic in
            // the UI, because it makes the sidestake and coinstake outputs disjoint from an address point of view.
            if (SideStakeScriptPubKey == CoinStakeScriptPubKey)
                continue;

            int64_t nSideStake = 0;

            // For allocations ending less than 100% assign using sidestake allocation.
            if (dSumAllocation + iterSideStake->second < 1.0)
                nSideStake = nReward * iterSideStake->second;
            // We need to handle the final sidestake differently in the case it brings the total allocation up to 100%,
            // because testing showed in corner cases the output return to the staking address could be off by one Halford.
            else if (dSumAllocation + iterSideStake->second == 1.0)
                // Simply assign the special case final nSideStake the remaining output value minus input value to ensure
                // a match on the output flowing down.
                nSideStake = nRemainingStakeOutputValue - nInputValue;

            blocknew.vtx[1].vout.push_back(CTxOut(nSideStake, SideStakeScriptPubKey));

            LogPrintf("SplitCoinStakeOutput: create sidestake UTXO %i value %f to address %s", nOutputsUsed, CoinToDouble(nReward * iterSideStake->second), iterSideStake->first.c_str());
            dSumAllocation += iterSideStake->second;
            nRemainingStakeOutputValue -= nSideStake;
            nOutputsUsed++;
        }
        // If we get here and dSumAllocation is zero then the enablesidestaking flag was set, but no VALID distribution
        // was in the vSideStakeAlloc vector. (Note that this is also in the parsing routine in StakeMiner, so it will show
        // up when the wallet is first started, but also needs to be here, to remind the user periodically that something
        // is amiss.)
        if (dSumAllocation == 0.0)
            LogPrintf("WARN: SplitCoinStakeOutput: enablesidestaking was set in config but nothing has been allocated for distribution!");
    }

    // By this point, if SideStaking was used and 100% was allocated nRemainingStakeOutputValue will be
    // the original base coinstake. The other extreme is no sidestaking, in which case nRemainingStakeOutputValue is the
    // base coinstake + all of the reward. In any case, we will now compute the number of split stake outputs needed,
    // limiting actual number of split stake outputs to remaining available (nMaxOutputs - nOutputsUsed). There will be
    // at least one available, because if sidestaking is activated, it is limited to nMaxOutputs - 2.
    // Don't do any work here except pushing a single output if flag is false.
    if (fEnableStakeSplit)
    {
        unsigned int nSplitStakeOutputs = min((nMaxOutputs - nOutputsUsed), GetNumberOfStakeOutputs(nRemainingStakeOutputValue, nMinStakeSplitValue, dEfficiency));
        LogPrint(BCLog::LogFlags::MINER, "SplitCoinStakeOutput: nStakeOutputs = %u", nSplitStakeOutputs);

        // Set Actual Stake output value for split stakes to remaining output value divided by the number of split
        // stake outputs.
        int64_t nActualStakeOutputValue = nRemainingStakeOutputValue / nSplitStakeOutputs;

        LogPrint(BCLog::LogFlags::MINER, "SplitCoinStakeOutput: nSplitStakeOutputs = %f", nSplitStakeOutputs);
        LogPrint(BCLog::LogFlags::MINER, "SplitCoinStakeOutput: nActualStakeOutputValue = %f", CoinToDouble(nActualStakeOutputValue));

        int64_t nSumStakeOutputValue = 0;
        for (unsigned int i = 1; i < nSplitStakeOutputs; i++)
        {
            blocknew.vtx[1].vout.push_back(CTxOut(nActualStakeOutputValue, CoinStakeScriptPubKey));
            LogPrintf("SplitCoinStakeOutput: create stake UTXO %i value %f", nOutputsUsed, CoinToDouble(nActualStakeOutputValue));
            nOutputsUsed++;
            nSumStakeOutputValue += nActualStakeOutputValue;
        }
        // For the last UTXO, subtract the running sum from the desired total, which does not include the last UTXO
        // so far, to recover anything lost from rounding. This will be a very small difference from the others.
        // The reason we go through this trouble is that integer division rounds down, and we don't even want
        // to have the wallet lose 1 Halford.
        blocknew.vtx[1].vout.push_back(CTxOut((nRemainingStakeOutputValue - nSumStakeOutputValue), CoinStakeScriptPubKey));
        LogPrintf("SplitCoinStakeOutput: create stake UTXO %i value %f", nOutputsUsed, CoinToDouble(nRemainingStakeOutputValue - nSumStakeOutputValue));
    }
    else
    {
        // Stake splitting flag is false, so just push back a single output with the remaining output value.
        blocknew.vtx[1].vout.push_back(CTxOut(nRemainingStakeOutputValue, CoinStakeScriptPubKey));
    }

    // Now, the outputs are in the wrong order. We had to do the rewards distribution first
    // because it can be of variable number of elements, and if someone puts an address on the list,
    // there is a reasonable expectation it should succeed, while the stake splitting is a luxury.
    // One of the coinstake outputs MUST be at vout[1] to pass block validation, so we will
    // add the empty one at the end. And then use std::reverse to reverse vout.begin() to vout.end().
    // This will put the correct vout[0] and vout[1] in the right place.
    blocknew.vtx[1].vout.push_back(CTxOut(0, CScript()));
    reverse(blocknew.vtx[1].vout.begin(), blocknew.vtx[1].vout.end());
}



unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency)
{
    int64_t nDesiredStakeOutputValue = 0;
    unsigned int nStakeOutputs = 1;

    // For the definition of the constant G, please see
    // https://docs.google.com/document/d/1OyuTwdJx1Ax2YZ42WYkGn_UieN0uY13BTlA5G5IAN00/edit?usp=sharing
    // Refer to page 5 for G. This link is a draft of an upcoming bluepaper section.
    const double G = 9942.2056;

    // If the stake output provided to GetNumberofStakeOutputs is not greater than nMinStakeSplitValue then there will
    // be only one output, so skip calculations and return(nStakeOutputs) which is initialized to 1.
    if (nValue > nMinStakeSplitValue)
    {
        // Set Desired UTXO size post stake based on passed in efficiency and difficulty, but do not allow to go below
        // passed in MinStakeSplitValue. Note that we use GetAverageDifficulty over a 4 hour (160 block period) rather than
        // StakeKernelDiff, because the block to block difficulty has too much scatter. Please refer to the above link,
        // equation (27) on page 10 as a reference for the below formula.
        nDesiredStakeOutputValue = G * GetAverageDifficulty(160) * (3.0 / 2.0) * (1 / dEfficiency  - 1) * COIN;
        nDesiredStakeOutputValue = max(nMinStakeSplitValue, nDesiredStakeOutputValue);

        LogPrint(BCLog::LogFlags::MINER, "GetNumberOfStakeOutputs: nDesiredStakeOutputValue = %f", CoinToDouble(nDesiredStakeOutputValue));

        // Divide nValue by nDesiredStakeUTXOValue. We purposely want this to be integer division to round down.
        nStakeOutputs = max((int64_t) 1, nValue / nDesiredStakeOutputValue);

        LogPrint(BCLog::LogFlags::MINER, "GetNumberOfStakeOutputs: nStakeOutputs = %u", nStakeOutputs);
    }

    return(nStakeOutputs);
}

bool SignStakeBlock(CBlock &block, CKey &key, vector<const CWalletTx*> &StakeInputs, CWallet *pwallet)
{
    //Sign the coinstake transaction
    unsigned nIn = 0;
    for (auto const& pcoin : StakeInputs)
    {
        if (!SignSignature(*pwallet, *pcoin, block.vtx[1], nIn++))
        {
            return error("SignStakeBlock: failed to sign coinstake");
        }
    }

    //Sign the whole block
    block.hashMerkleRoot = block.BuildMerkleTree();
    if( !key.Sign(block.GetHash(), block.vchBlockSig) )
    {
        return error("SignStakeBlock: failed to sign block");
    }

    return true;
}

void AddNeuralContractOrVote(CBlock& blocknew)
{
    if (OutOfSyncByAge()) {
        LogPrintf("AddNeuralContractOrVote: Out of sync.");
        return;
    }

    if (!NN::Quorum::SuperblockNeeded()) {
        LogPrintf("AddNeuralContractOrVote: Not needed.");
        return;
    }

    if (NN::Quorum::HasPendingSuperblock()) {
        LogPrintf("AddNeuralContractOrVote: Already pending.");
        return;
    }

    if (blocknew.nVersion >= 11) {
        NN::Superblock superblock = NN::Quorum::CreateSuperblock();

        if (!superblock.WellFormed()) {
            LogPrintf("AddNeuralContractOrVote: Local contract empty.");
            return;
        }

        // TODO: fix the const cast:
        NN::Claim& claim = const_cast<NN::Claim&>(blocknew.GetClaim());

        claim.m_quorum_hash = superblock.GetHash();
        claim.m_superblock = std::move(superblock);

        LogPrintf(
            "AddNeuralContractOrVote: Added our Superblock (size %" PRIszu ").",
            GetSerializeSize(claim.m_superblock, SER_NETWORK, 1));

        return;
    }

    // TODO: remove the rest below after switch to block version 11:

    std::string quorum_address = DefaultWalletAddress();

    if (!NN::Quorum::Participating(quorum_address, blocknew.nTime)) {
        LogPrintf("AddNeuralContractOrVote: Not participating.");
        return;
    }

    // TODO: fix the const cast:
    NN::Claim& claim = const_cast<NN::Claim&>(blocknew.GetClaim());

    // Add our Neural Vote
    //
    // CreateSuperblock() will return an empty superblock when the node has not
    // yet received enough scraper data to resolve a convergence locally, so it
    // cannot vote for a superblock.
    //
    claim.m_quorum_hash = NN::Quorum::CreateSuperblock().GetHash();

    if (!claim.m_quorum_hash.Valid()) {
        LogPrintf("AddNeuralContractOrVote: Local contract empty.");
        return;
    }

    claim.m_quorum_address = std::move(quorum_address);

    LogPrintf(
        "AddNeuralContractOrVote: Added our quorum vote: %s",
        claim.m_quorum_hash.ToString());

    const NN::QuorumHash consensus_hash = NN::Quorum::FindPopularHash(pindexBest);

    if (claim.m_quorum_hash != consensus_hash) {
        LogPrintf("AddNeuralContractOrVote: Not in consensus.");
        return;
    }

    // We have consensus, add our superblock contract:
    claim.m_superblock = NN::Quorum::CreateSuperblock();

    LogPrintf(
        "AddNeuralContractOrVote: Added our Superblock (size %" PRIszu ").",
        GetSerializeSize(claim.m_superblock, SER_NETWORK, 1));
}

bool CreateGridcoinReward(
    CBlock &blocknew,
    CBlockIndex* pindexPrev,
    int64_t &nReward,
    CWallet* pwallet)
{
    // Remove fees from coinbase:
    int64_t nFees = blocknew.vtx[0].vout[0].nValue;
    blocknew.vtx[0].vout[0].SetEmpty();

    const NN::ResearcherPtr researcher = NN::Researcher::Get();

    NN::Claim claim;
    claim.m_mining_id = researcher->Id();

    // If a researcher's beacon expired, generate the block as an investor. We
    // cannot sign a research claim without the beacon key, so this avoids the
    // issue that prevents a researcher from staking blocks if the beacon does
    // not exist (it expired or it has yet to be advertised).
    //
    if (researcher->Status() == NN::ResearcherStatus::NO_BEACON) {
        LogPrintf(
            "CreateGridcoinReward: CPID eligible but no active beacon key "
            "found. Staking as investor.");

        claim.m_mining_id = NN::MiningId::ForInvestor();
    }

    // First argument is coin age - unused since CBR (block version 10)
    nReward = GetProofOfStakeReward(0, blocknew.nTime, pindexPrev);
    claim.m_block_subsidy = nReward;
    nReward += nFees;

    if (const NN::CpidOption cpid = claim.m_mining_id.TryCpid()) {
        CBlockIndex index;
        index.nVersion = blocknew.nVersion;
        index.nHeight = pindexPrev->nHeight + 1;

        const NN::AccrualComputer calc = NN::Tally::GetComputer(*cpid, blocknew.nTime, &index);
        claim.m_research_subsidy = calc->Accrual();

        // If no pending research subsidy value exists, build an investor claim.
        // This avoids polluting the block index with non-research reward blocks
        // that contain CPIDs which increases the effort needed to load research
        // age context at start-up:
        //
        if (claim.m_research_subsidy <= 0) {
            LogPrintf(
                "CreateGridcoinReward: No positive research reward pending at "
                "time of stake. Staking as investor.");

            claim.m_mining_id = NN::MiningId::ForInvestor();
        } else {
            nReward += claim.m_research_subsidy;
            claim.m_magnitude = NN::Quorum::GetMagnitude(*cpid).Floating();
        }
    }

    claim.m_client_version = FormatFullVersion().substr(0, NN::Claim::MAX_VERSION_SIZE);
    claim.m_organization = GetArgument("org", "").substr(0, NN::Claim::MAX_ORGANIZATION_SIZE);

    if (blocknew.nVersion <= 10) {
        claim.m_magnitude_unit = NN::Tally::GetMagnitudeUnit(pindexPrev);
    }

    if (!SignClaim(pwallet, claim, blocknew.nTime, blocknew.hashPrevBlock)) {
        LogPrintf("%s: Failed to sign researcher claim. Staking as investor", __func__);

        nReward -= claim.m_research_subsidy;
        claim.m_mining_id = NN::MiningId::ForInvestor();
        claim.m_research_subsidy = 0;
        claim.m_magnitude = 0;
    }

    LogPrintf(
        "CreateGridcoinReward: for %s mint %s magnitude %d Research %s, Interest %s",
        claim.m_mining_id.ToString(),
        FormatMoney(nReward),
        claim.m_magnitude,
        FormatMoney(claim.m_research_subsidy),
        FormatMoney(claim.m_block_subsidy));

    // Append the claim context to the block before signing the transactions:
    //
    if (blocknew.nVersion <= 10) {
        claim.m_version = 1;

        // Nodes that do not yet support block version 11 will parse the claim
        // from the coinbase hashBoinc field. Set the previous block hash that
        // old nodes check for research reward claims if necessary:
        //
        if (claim.HasResearchReward()) {
            claim.m_last_block_hash = blocknew.hashPrevBlock;
        }

        blocknew.vtx[0].hashBoinc = claim.ToString(blocknew.nVersion);
    } else {
        // After the mandatory switch to block version 11, the claim context is
        // serialized directly in the block, but we need to add the hash of the
        // claim to a transaction to protect the integrity of the data within:
        //
        blocknew.vtx[0].vContracts.emplace_back(NN::MakeContract<NN::Claim>(
            NN::ContractAction::ADD,
            std::move(claim)));
    }

    blocknew.vtx[1].vout[1].nValue += nReward;

    return true;
}

bool IsMiningAllowed(CWallet *pwallet)
{
    bool status = true;
    if(pwallet->IsLocked())
    {
        LOCK(MinerStatus.lock);
        MinerStatus.SetReasonNotStaking(CMinerStatus::WALLET_LOCKED);
        status=false;
    }

    if(fDevbuildCripple)
    {
        LOCK(MinerStatus.lock);
        MinerStatus.SetReasonNotStaking(CMinerStatus::TESTNET_ONLY);
        status=false;
    }

    if (vNodes.empty() || (!fTestNet&& IsInitialBlockDownload()) ||
        (!fTestNet&& vNodes.size() < 3)
        )
    {
        LOCK(MinerStatus.lock);
        MinerStatus.SetReasonNotStaking(CMinerStatus::OFFLINE);
        status=false;
    }

    return status;
}

// This function parses the config file for the directives for side staking. It is used
// in StakeMiner for the miner loop and also called by rpc getmininginfo.
bool GetSideStakingStatusAndAlloc(SideStakeAlloc& vSideStakeAlloc)
{
    vector<string> vSubParam;
    std::string sAddress;
    double dAllocation = 0.0;
    double dSumAllocation = 0.0;

    bool fEnableSideStaking = GetBoolArg("-enablesidestaking");
    LogPrint(BCLog::LogFlags::MINER, "StakeMiner: fEnableSideStaking = %u", fEnableSideStaking);

    // If side staking is enabled, parse destinations and allocations. We don't need to worry about any that are rejected
    // other than a warning message, because any unallocated rewards will go back into the coinstake output(s).
    if (fEnableSideStaking)
    {
        if (mapArgs.count("-sidestake") && mapMultiArgs["-sidestake"].size() > 0)
        {
            for (auto const& sSubParam : mapMultiArgs["-sidestake"])
            {
                ParseString(sSubParam, ',', vSubParam);
                if (vSubParam.size() != 2)
                {
                    LogPrintf("WARN: StakeMiner: Incompletely SideStake Allocation specified. Skipping SideStake entry.");
                    vSubParam.clear();
                    continue;
                }

                sAddress = vSubParam[0];

                CBitcoinAddress address(sAddress);
                if (!address.IsValid())
                {
                    LogPrintf("WARN: StakeMiner: ignoring sidestake invalid address %s.", sAddress.c_str());
                    vSubParam.clear();
                    continue;
                }

                try
                {
                    dAllocation = stof(vSubParam[1]) / 100.0;
                }
                catch(...)
                {
                    LogPrintf("WARN: StakeMiner: Invalid allocation provided. Skipping allocation.");
                    vSubParam.clear();
                    continue;
                }

                if (dAllocation <= 0)
                {
                    LogPrintf("WARN: StakeMiner: Negative or zero allocation provided. Skipping allocation.");
                    vSubParam.clear();
                    continue;
                }

                // The below will stop allocations if someone has made a mistake and the total adds up to more than 100%.
                // Note this same check is also done in SplitCoinStakeOutput, but it needs to be done here for two reasons:
                // 1. Early alertment in the debug log, rather than when the first kernel is found, and 2. When the UI is
                // hooked up, the SideStakeAlloc vector will be filled in by other than reading the config file and will
                // skip the above code.
                dSumAllocation += dAllocation;
                if (dSumAllocation > 1.0)
                {
                    LogPrintf("WARN: StakeMiner: allocation percentage over 100\%, ending sidestake allocations.");
                    break;
                }

                vSideStakeAlloc.push_back(std::pair<std::string, double>(sAddress, dAllocation));
                LogPrint(BCLog::LogFlags::MINER, "StakeMiner: SideStakeAlloc Address %s, Allocation %f", sAddress.c_str(), dAllocation);

                vSubParam.clear();
            }
        }
        // If we get here and dSumAllocation is zero then the enablesidestaking flag was set, but no VALID distribution
        // was provided in the config file, so warn in the debug log.
        if (!dSumAllocation)
            LogPrintf("WARN: StakeMiner: enablesidestaking was set in config but nothing has been allocated for distribution!");
    }

    return fEnableSideStaking;
}

// This function parses the config file for the directives for stake splitting. It is used
// in StakeMiner for the miner loop and also called by rpc getmininginfo.
bool GetStakeSplitStatusAndParams(int64_t& nMinStakeSplitValue, double& dEfficiency, int64_t& nDesiredStakeOutputValue)
{
    // Parse StakeSplit and SideStaking flags.
    bool fEnableStakeSplit = GetBoolArg("-enablestakesplit");
    LogPrint(BCLog::LogFlags::MINER, "StakeMiner: fEnableStakeSplit = %u", fEnableStakeSplit);

    // If stake output splitting is enabled, determine efficiency and minimum stake split value.
    if (fEnableStakeSplit)
    {
        // Pull efficiency for UTXO staking from config, but constrain to the interval [0.75, 0.98]. Use default of 0.90.
        dEfficiency = (double)GetArg("-stakingefficiency", 90) / 100;
        if (dEfficiency > 0.98)
            dEfficiency = 0.98;
        else if (dEfficiency < 0.75)
            dEfficiency = 0.75;

        LogPrint(BCLog::LogFlags::MINER, "StakeMiner: dEfficiency = %f", dEfficiency);

        // Pull Minimum Post Stake UTXO Split Value from config or command line parameter.
        // Default to 800 and do not allow it to be specified below 800 GRC.
        nMinStakeSplitValue = max(GetArg("-minstakesplitvalue", MIN_STAKE_SPLIT_VALUE_GRC), MIN_STAKE_SPLIT_VALUE_GRC) * COIN;

        LogPrint(BCLog::LogFlags::MINER, "StakeMiner: nMinStakeSplitValue = %f", CoinToDouble(nMinStakeSplitValue));

        // For the definition of the constant G, please see
        // https://docs.google.com/document/d/1OyuTwdJx1Ax2YZ42WYkGn_UieN0uY13BTlA5G5IAN00/edit?usp=sharing
        // Refer to page 5 for G. This link is a draft of an upcoming bluepaper section.
        const double G = 9942.2056;

        // Desired UTXO size post stake based on passed in efficiency and difficulty, but do not allow to go below
        // passed in MinStakeSplitValue. Note that we use GetAverageDifficulty over a 4 hour (160 block period) rather than
        // StakeKernelDiff, because the block to block difficulty has too much scatter. Please refer to the above link,
        // equation (27) on page 10 as a reference for the below formula.
        nDesiredStakeOutputValue = G * GetAverageDifficulty(160) * (3.0 / 2.0) * (1 / dEfficiency  - 1) * COIN;
        nDesiredStakeOutputValue = max(nMinStakeSplitValue, nDesiredStakeOutputValue);
    }

    return fEnableStakeSplit;
}



void StakeMiner(CWallet *pwallet)
{
    // Make this thread recognisable as the mining thread
    RenameThread("grc-stake-miner");

    int64_t nMinStakeSplitValue = 0;
    double dEfficiency = 0;
    int64_t nDesiredStakeOutputValue = 0;
    SideStakeAlloc vSideStakeAlloc = {};

    // nMinStakeSplitValue and dEfficiency are out parameters.
    bool fEnableStakeSplit = GetStakeSplitStatusAndParams(nMinStakeSplitValue, dEfficiency, nDesiredStakeOutputValue);

    // vSideStakeAlloc is an out parameter.
    bool fEnableSideStaking = GetSideStakingStatusAndAlloc(vSideStakeAlloc);

    while (!fShutdown)
    {
        //wait for next round
        MilliSleep(nMinerSleep);

        CBlockIndex* pindexPrev = pindexBest;
        CBlock StakeBlock;

        {
            LOCK(MinerStatus.lock);

            //clear miner messages
            MinerStatus.ClearReasonsNotStaking();

            //New versions
            if (IsV11Enabled(pindexPrev->nHeight + 1)) {
                StakeBlock.nVersion = 11;
            } else {
                StakeBlock.nVersion = 10;
            }

            MinerStatus.Version= StakeBlock.nVersion;

            // This is needed due to early initialization of bitcoingui
            miner_first_pass_complete = true;
        }

        if(!IsMiningAllowed(pwallet))
        {
            LOCK(MinerStatus.lock);

            MinerStatus.Clear();
            continue;
        }

        LOCK(cs_main);

        // * Create a bare block
        StakeBlock.nTime= GetAdjustedTime();
        StakeBlock.nNonce= 0;
        StakeBlock.nBits = GetNextTargetRequired(pindexPrev);
        StakeBlock.vtx.resize(2);
        //tx 0 is coin_base
        CTransaction &StakeTX= StakeBlock.vtx[1]; //tx 1 is coin_stake

        // * Try to create a CoinStake transaction
        CKey BlockKey;
        vector<const CWalletTx*> StakeInputs;

        if( !CreateCoinStake( StakeBlock, BlockKey, StakeInputs, *pwallet, pindexPrev ) )
            continue;
        StakeBlock.nTime= StakeTX.nTime;

        // * create rest of the block. This needs to be moved to after CreateGridcoinReward,
        // because stake output splitting needs to be done beforehand for size considerations.
        if( !CreateRestOfTheBlock(StakeBlock,pindexPrev) )
            continue;
        LogPrintf("StakeMiner: created rest of the block");

        // * add gridcoin reward to coinstake, fill-in nReward
        int64_t nReward = 0;
        if(!CreateGridcoinReward(StakeBlock, pindexPrev, nReward, pwallet)) {
            continue;
        }

        LogPrintf("StakeMiner: added gridcoin reward to coinstake");

        // * If argument is supplied desiring stake output splitting or side staking, then call SplitCoinStakeOutput.
        if (fEnableStakeSplit || fEnableSideStaking)
            SplitCoinStakeOutput(StakeBlock, nReward, fEnableStakeSplit, fEnableSideStaking, vSideStakeAlloc, nMinStakeSplitValue, dEfficiency);

        AddNeuralContractOrVote(StakeBlock);

        // * sign boinchash, coinstake, wholeblock
        if (!SignStakeBlock(StakeBlock, BlockKey, StakeInputs, pwallet)) {
            continue;
        }

        LogPrintf("StakeMiner: signed boinchash, coinstake, wholeblock");

        {
            LOCK(MinerStatus.lock);

            MinerStatus.CreatedCnt++;
        }

        // * delegate to ProcessBlock
        if (!ProcessBlock(NULL, &StakeBlock, true))
        {
            error("StakeMiner: Block vehemently rejected");
            continue;
        }

        LogPrintf("StakeMiner: block processed");

        {
            LOCK(MinerStatus.lock);

            MinerStatus.AcceptedCnt++;
        }

    } //end while(!fShutdown)
}

