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
    std::string Message;
    double WeightSum,WeightMin,WeightMax;
    double ValueSum;
    double CoinAgeSum;
    int Version;
    unsigned long CreatedCnt;
    unsigned long AcceptedCnt;
    unsigned long KernelsFound;
    int64_t nLastCoinStakeSearchInterval;

    void Clear()
    {
        Message= "";
        WeightSum= ValueSum= WeightMin= WeightMax= 0;
        Version= 0;
        CoinAgeSum= 0;
        nLastCoinStakeSearchInterval = 0;
    }
    CMinerStatus()
    {
        Clear();
        ReasonNotStaking= "";
        CreatedCnt= AcceptedCnt= KernelsFound= 0;
    }
};

extern CMinerStatus MinerStatus;
extern volatile unsigned int nMinerSleep;

#endif // NOVACOIN_MINER_H
