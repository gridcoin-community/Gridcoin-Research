// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef NOVACOIN_MINER_H
#define NOVACOIN_MINER_H

#include "main.h"
#include "wallet.h"

struct CMinerStatus
{
    CCriticalSection lock;
    std::string ReasonNotStaking;
    uint64_t WeightSum,WeightMin,WeightMax;
    double ValueSum;
    double CoinAgeSum;
    int Version;
    uint64_t CreatedCnt;
    uint64_t AcceptedCnt;
    uint64_t KernelsFound;
    int64_t nLastCoinStakeSearchInterval;
    double KernelDiffMax;
    double KernelDiffSum;

    void Clear();
    CMinerStatus();
};

typedef std::vector< std::pair<std::string, double> > SideStakeAlloc;

extern CMinerStatus MinerStatus;
extern unsigned int nMinerSleep;

namespace supercfwd
{
    int MaybeRequest();
    void HashResponseHook(CNode* fromNode, const std::string& neural_response);
    void QuorumResponseHook(CNode* fromNode, const std::string& neural_response);
    void SendResponse(CNode* fromNode, const std::string& req_hash);
}

// Note the below constant controls the minimum value allowed for post
// split UTXO size. It is int64_t but in GRC so that it matches the entry in the config file.
// It will be converted to Halfords in GetNumberOfStakeOutputs by multiplying by COIN.
static const int64_t MIN_STAKE_SPLIT_VALUE_GRC = 800;

void SplitCoinStakeOutput(CBlock &blocknew, int64_t &nReward, bool &fEnableStakeSplit, bool &fEnableSideStaking, SideStakeAlloc &vSideStakeAlloc, double &dEfficiency);
unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency);
bool GetSideStakingStatusAndAlloc(SideStakeAlloc& vSideStakeAlloc);
bool GetStakeSplitStatusAndParams(int64_t& nMinStakeSplitValue, double& dEfficiency, int64_t& nDesiredStakeOutputValue);

#endif // NOVACOIN_MINER_H
