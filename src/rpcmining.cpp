// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "db.h"
#include "txdb.h"
#include "init.h"
#include "miner.h"
#include "rpcserver.h"
#include "neuralnet/neuralnet.h"
#include "global_objects_noui.hpp"
using namespace std;

double GRCMagnitudeUnit(int64_t locktime);

namespace NN { std::string GetPrimaryCpid(); }

UniValue getmininginfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\n"
            "Returns an object containing mining-related information\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint64_t nWeight = 0;
    int64_t nTime= GetAdjustedTime();
    pwalletMain->GetStakeWeight(nWeight);
    UniValue obj(UniValue::VOBJ);
    UniValue diff(UniValue::VOBJ);
    UniValue weight(UniValue::VOBJ);
    UniValue stakesplitting(UniValue::VOBJ);
    UniValue stakesplittingparam(UniValue::VOBJ);
    UniValue sidestaking(UniValue::VOBJ);
    UniValue sidestakingalloc(UniValue::VOBJ);
    UniValue vsidestakingalloc(UniValue::VARR);

    double nNetworkWeight = GetEstimatedNetworkWeight();
    double nNetworkValue = nNetworkWeight / 80.0;
    obj.pushKV("blocks",        nBestHeight);
    diff.pushKV("proof-of-stake",    GetDifficulty(GetLastBlockIndex(pindexBest, true)));

    std::string current_poll;
    { LOCK(MinerStatus.lock);
        // not using real weigh to not break calculation
        bool staking = MinerStatus.nLastCoinStakeSearchInterval && MinerStatus.WeightSum;
        uint64_t nExpectedTime = GetEstimatedTimetoStake();
        diff.pushKV("last-search-interval", MinerStatus.nLastCoinStakeSearchInterval);
        weight.pushKV("minimum",    MinerStatus.WeightMin);
        weight.pushKV("maximum",    MinerStatus.WeightMax);
        weight.pushKV("combined",   MinerStatus.WeightSum);
        weight.pushKV("valuesum",   MinerStatus.ValueSum);
        weight.pushKV("legacy",   nWeight/(double)COIN);
        obj.pushKV("stakeweight", weight);
        obj.pushKV("netstakeweight", nNetworkWeight);
        obj.pushKV("netstakingGRCvalue", nNetworkValue);
        obj.pushKV("staking", staking);
        obj.pushKV("mining-error", MinerStatus.ReasonNotStaking);
        obj.pushKV("time-to-stake_days", nExpectedTime/86400.0);
        obj.pushKV("expectedtime", nExpectedTime);
        obj.pushKV("mining-version", MinerStatus.Version);
        obj.pushKV("mining-created", MinerStatus.CreatedCnt);
        obj.pushKV("mining-accepted", MinerStatus.AcceptedCnt);
        obj.pushKV("mining-kernels-found", MinerStatus.KernelsFound);
        obj.pushKV("kernel-diff-best",MinerStatus.KernelDiffMax);
        obj.pushKV("kernel-diff-sum",MinerStatus.KernelDiffSum);

        current_poll = GetCurrentOverviewTabPoll();
    }

    int64_t nMinStakeSplitValue = 0;
    double dEfficiency = 0;
    int64_t nDesiredStakeSplitValue = 0;
    SideStakeAlloc vSideStakeAlloc;

    // nMinStakeSplitValue, dEfficiency, and nDesiredStakeSplitValue are out parameters.
    bool fEnableStakeSplit = GetStakeSplitStatusAndParams(nMinStakeSplitValue, dEfficiency, nDesiredStakeSplitValue);

    // vSideStakeAlloc is an out parameter.
    bool fEnableSideStaking = GetSideStakingStatusAndAlloc(vSideStakeAlloc);

    stakesplitting.pushKV("stake-splitting-enabled", fEnableStakeSplit);
    if (fEnableStakeSplit)
    {
        stakesplittingparam.pushKV("min-stake-split-value", nMinStakeSplitValue / COIN);
        stakesplittingparam.pushKV("efficiency", dEfficiency);
        stakesplittingparam.pushKV("stake-split-UTXO-size-for-target-efficiency", nDesiredStakeSplitValue / COIN);
        stakesplitting.pushKV("stake-splitting-params", stakesplittingparam);
    }
    obj.pushKV("stake-splitting", stakesplitting);

    sidestaking.pushKV("side-staking-enabled", fEnableSideStaking);
    if (fEnableSideStaking)
    {
        for (const auto& alloc : vSideStakeAlloc)
        {
            sidestakingalloc.pushKV("address", alloc.first);
            sidestakingalloc.pushKV("allocation-pct", alloc.second * 100);

            vsidestakingalloc.push_back(sidestakingalloc);
        }
        sidestaking.pushKV("side-staking-allocations", vsidestakingalloc);
    }
    obj.pushKV("side-staking", sidestaking);

    obj.pushKV("difficulty",    diff);
    obj.pushKV("errors",        GetWarnings("statusbar"));
    obj.pushKV("pooledtx",      (uint64_t)mempool.size());
    //double nCutoff =  GetAdjustedTime() - (60*60*24*14);
    obj.pushKV("testnet",       fTestNet);
    double neural_popularity = 0;
    std::string neural_hash = GetNeuralNetworkSupermajorityHash(neural_popularity);
    obj.pushKV("PopularNeuralHash", neural_hash);
    obj.pushKV("NeuralPopularity", neural_popularity);
    //9-19-2015 - CM
    obj.pushKV("MyNeuralHash", NN::GetInstance()->GetNeuralHash());

    std::string primary_cpid = NN::GetPrimaryCpid();
    obj.pushKV("CPID", primary_cpid);

    if (IsResearcher(primary_cpid))
    {
        {
            double dMagnitudeUnit = GRCMagnitudeUnit(nTime);
            double dAccrualAge,AvgMagnitude;
            int64_t nBoinc = ComputeResearchAccrual(nTime, primary_cpid, "getmininginfo", pindexBest, false, 69, dAccrualAge, dMagnitudeUnit, AvgMagnitude);
            obj.pushKV("Magnitude Unit",dMagnitudeUnit);
            obj.pushKV("BoincRewardPending",nBoinc/(double)COIN);
        }
    }

    obj.pushKV("MiningInfo 1", msMiningErrors);
    obj.pushKV("MiningInfo 2", "Poll: " + current_poll);
    obj.pushKV("MiningInfo 5", msMiningErrors5);
    obj.pushKV("MiningInfo 6", msMiningErrors6);
    obj.pushKV("MiningInfo 7", msMiningErrors7);
    obj.pushKV("MiningInfo 8", msMiningErrors8);

    return obj;
}
