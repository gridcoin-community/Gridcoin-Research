// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "init.h"
#include "miner.h"
#include "rpcserver.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/tally.h"

using namespace std;

UniValue getmininginfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\n"
            "Returns an object containing mining-related information\n");

    UniValue obj(UniValue::VOBJ);
    UniValue diff(UniValue::VOBJ);
    UniValue weight(UniValue::VOBJ);
    UniValue stakesplitting(UniValue::VOBJ);
    UniValue stakesplittingparam(UniValue::VOBJ);
    UniValue sidestaking(UniValue::VOBJ);
    UniValue sidestakingalloc(UniValue::VOBJ);
    UniValue vsidestakingalloc(UniValue::VARR);

    int64_t nTime = GetAdjustedTime();
    uint64_t nWeight = 0;
    double nNetworkWeight = 0;
    double nCurrentDiff = 0;
    double nTargetDiff = 0;
    uint64_t nExpectedTime = 0;
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        pwalletMain->GetStakeWeight(nWeight);

        nNetworkWeight = GetEstimatedNetworkWeight();
        nCurrentDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
        nTargetDiff = GetBlockDifficulty(GetNextTargetRequired(pindexBest));
        nExpectedTime = GetEstimatedTimetoStake();
    }

    obj.pushKV("blocks", nBestHeight);
    diff.pushKV("current", nCurrentDiff);
    diff.pushKV("target", nTargetDiff);

    { LOCK(MinerStatus.lock);
        // not using real weigh to not break calculation
        bool staking = MinerStatus.nLastCoinStakeSearchInterval && MinerStatus.WeightSum;
        diff.pushKV("last-search-interval", MinerStatus.nLastCoinStakeSearchInterval);
        weight.pushKV("minimum",    MinerStatus.WeightMin);
        weight.pushKV("maximum",    MinerStatus.WeightMax);
        weight.pushKV("combined",   MinerStatus.WeightSum);
        weight.pushKV("valuesum",   MinerStatus.ValueSum);
        weight.pushKV("legacy",   nWeight/(double)COIN);
        obj.pushKV("stakeweight", weight);
        obj.pushKV("netstakeweight", nNetworkWeight);
        obj.pushKV("netstakingGRCvalue", nNetworkWeight / 80.0);
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
    }

    int64_t nMinStakeSplitValue = 0;
    double dEfficiency = 0;
    int64_t nDesiredStakeSplitValue = 0;
    SideStakeAlloc vSideStakeAlloc;

    LOCK(cs_main);

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

    obj.pushKV("testnet",       fTestNet);

    const NN::MiningId mining_id = NN::Researcher::Get()->Id();
    obj.pushKV("CPID", mining_id.ToString());

    if (const NN::CpidOption cpid = mining_id.TryCpid())
    {
        const NN::ResearchAccount& account = NN::Tally::GetAccount(*cpid);
        const NN::AccrualComputer calc = NN::Tally::GetComputer(*cpid, nTime, pindexBest);

        obj.pushKV("Magnitude Unit", calc->MagnitudeUnit());
        obj.pushKV("BoincRewardPending", ValueFromAmount(calc->Accrual(account)));
    }

    std::string current_poll = "Poll: ";

    try {
        current_poll += GetCurrentOverviewTabPoll();
    } catch (std::exception &e) {
        current_poll += _("No current polls");
        LogPrintf("Error obtaining last poll: %s", e.what());
    }

    obj.pushKV("MiningInfo 1", msMiningErrors);
    obj.pushKV("MiningInfo 2", current_poll);
    obj.pushKV("MiningInfo 5", msMiningErrors5);
    obj.pushKV("MiningInfo 6", msMiningErrors6);
    obj.pushKV("MiningInfo 7", msMiningErrors7);
    obj.pushKV("MiningInfo 8", msMiningErrors8);

    return obj;
}
