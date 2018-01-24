// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "db.h"
#include "txdb.h"
#include "init.h"
#include "miner.h"
#include "bitcoinrpc.h"
#include "global_objects_noui.hpp"
using namespace json_spirit;
using namespace std;

int64_t GetCoinYearReward(int64_t nTime);

//CCriticalSection cs_main;
//static boost::thread_group* postThreads = NULL;

double GRCMagnitudeUnit(int64_t locktime);
std::string qtGetNeuralHash(std::string data);
std::string GetNeuralNetworkSupermajorityHash(double& out_popularity);

int64_t GetRSAWeightByCPID(std::string cpid);

Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "Returns an object containing mining-related information.");

    uint64_t nWeight = 0;
    int64_t nTime= GetAdjustedTime();
    pwalletMain->GetStakeWeight(nWeight);
    Object obj, diff, weight;
    double nNetworkWeight = GetPoSKernelPS();
    obj.push_back(Pair("blocks",        nBestHeight));
    diff.push_back(Pair("proof-of-work",        GetDifficulty()));
    diff.push_back(Pair("proof-of-stake",    GetDifficulty(GetLastBlockIndex(pindexBest, true))));

    { LOCK(MinerStatus.lock);
        // not using real weigh to not break calculation
        bool staking = MinerStatus.nLastCoinStakeSearchInterval && MinerStatus.WeightSum;
        uint64_t nExpectedTime = staking ? (GetTargetSpacing(nBestHeight) * nNetworkWeight / MinerStatus.ValueSum) : 0;
        diff.push_back(Pair("last-search-interval", MinerStatus.nLastCoinStakeSearchInterval));
        weight.push_back(Pair("minimum",    MinerStatus.WeightMin));
        weight.push_back(Pair("maximum",    MinerStatus.WeightMax));
        weight.push_back(Pair("combined",   MinerStatus.WeightSum));
        weight.push_back(Pair("valuesum",   MinerStatus.ValueSum));
        weight.push_back(Pair("legacy",   nWeight/(double)COIN));
        obj.push_back(Pair("stakeweight", weight));
        obj.push_back(Pair("netstakeweight", nNetworkWeight));
        obj.push_back(Pair("staking", staking));
        obj.push_back(Pair("mining-error", MinerStatus.ReasonNotStaking));
        obj.push_back(Pair("mining-message", MinerStatus.Message));
        obj.push_back(Pair("time-to-stake_days", nExpectedTime/86400.0));
        obj.push_back(Pair("expectedtime", nExpectedTime));
        obj.push_back(Pair("mining-version", MinerStatus.Version));
        obj.push_back(Pair("mining-created", MinerStatus.CreatedCnt));
        obj.push_back(Pair("mining-accepted", MinerStatus.AcceptedCnt));
        obj.push_back(Pair("mining-kernels-found", MinerStatus.KernelsFound));
        // Avoid calculating coinage of all coins just to get interest,
        // reuse value found by miner which has to load blocks anyway
        double dInterest = MinerStatus.CoinAgeSum * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);
        obj.push_back(Pair("InterestPending",dInterest/(double)COIN));

        obj.push_back(Pair("kernel-diff-best",MinerStatus.KernelDiffMax));
        obj.push_back(Pair("kernel-diff-sum",MinerStatus.KernelDiffSum));
    }

    obj.push_back(Pair("difficulty",    diff));
    obj.push_back(Pair("pow_reward",    GetProofOfWorkReward(0,  GetAdjustedTime(),1)/(double)COIN));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    obj.push_back(Pair("pooledtx",      (uint64_t)mempool.size()));
    //double nCutoff =  GetAdjustedTime() - (60*60*24*14);
    obj.push_back(Pair("stakeinterest", GetCoinYearReward( GetAdjustedTime())/(double)COIN));
    obj.push_back(Pair("testnet",       fTestNet));
    double neural_popularity = 0;
    std::string neural_hash = GetNeuralNetworkSupermajorityHash(neural_popularity);
    obj.push_back(Pair("PopularNeuralHash", neural_hash));
    obj.push_back(Pair("NeuralPopularity", neural_popularity));
    //9-19-2015 - CM
    #if defined(WIN32) && defined(QT_GUI)
    obj.push_back(Pair("MyNeuralHash", qtGetNeuralHash("")));
    #endif

    obj.push_back(Pair("CPID",msPrimaryCPID));
    obj.push_back(Pair("RSAWeight",GetRSAWeightByCPID(msPrimaryCPID)));

    {
        double dMagnitudeUnit = GRCMagnitudeUnit(nTime);
        double dAccrualAge,AvgMagnitude;
        int64_t nBoinc = ComputeResearchAccrual(nTime, msPrimaryCPID, "getmininginfo", pindexBest, false, 69, dAccrualAge, dMagnitudeUnit, AvgMagnitude);
        obj.push_back(Pair("Magnitude Unit",dMagnitudeUnit));
        obj.push_back(Pair("BoincRewardPending",nBoinc/(double)COIN));
    }

    obj.push_back(Pair("MiningProject",msMiningProject));
    obj.push_back(Pair("MiningInfo 1", msMiningErrors));
    obj.push_back(Pair("MiningInfo 2", msPoll));
    obj.push_back(Pair("MiningInfo 5", msMiningErrors5));
    obj.push_back(Pair("MiningInfo 6", msMiningErrors6));
    obj.push_back(Pair("MiningInfo 7", msMiningErrors7));
    obj.push_back(Pair("MiningInfo 8", msMiningErrors8));

    return obj;
}
