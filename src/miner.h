// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef NOVACOIN_MINER_H
#define NOVACOIN_MINER_H

#include "main.h"

#include <boost/optional/optional_fwd.hpp>

class CWallet;
class CWalletTx;

typedef std::vector< std::pair<std::string, double> > SideStakeAlloc;

extern unsigned int nMinerSleep;

// Note the below constant controls the minimum value allowed for post
// split UTXO size. It is int64_t but in GRC so that it matches the entry in the config file.
// It will be converted to Halfords in GetNumberOfStakeOutputs by multiplying by COIN.
static const int64_t MIN_STAKE_SPLIT_VALUE_GRC = 800;

boost::optional<CWalletTx> GetLastStake(CWallet& wallet);

void SplitCoinStakeOutput(CBlock &blocknew, int64_t &nReward, bool &fEnableStakeSplit, bool &fEnableSideStaking, SideStakeAlloc &vSideStakeAlloc, double &dEfficiency);
unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency);
bool GetSideStakingStatusAndAlloc(SideStakeAlloc& vSideStakeAlloc);
bool GetStakeSplitStatusAndParams(int64_t& nMinStakeSplitValue, double& dEfficiency, int64_t& nDesiredStakeOutputValue);

#endif // NOVACOIN_MINER_H
