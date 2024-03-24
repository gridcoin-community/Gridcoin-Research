// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "main.h"
#include "gridcoin/sidestake.h"


class CWallet;
class CWalletTx;

typedef std::vector<GRC::SideStake_ptr> SideStakeAlloc;

extern unsigned int nMinerSleep;

// Note the below constant controls the minimum value allowed for post
// split UTXO size. It is int64_t but in GRC so that it matches the entry in the config file.
// It will be converted to Halfords in GetNumberOfStakeOutputs by multiplying by COIN.
static const int64_t MIN_STAKE_SPLIT_VALUE_GRC = 800;

void SplitCoinStakeOutput(CBlock &blocknew, int64_t &nReward, bool &fEnableStakeSplit, bool &fEnableSideStaking,
                          SideStakeAlloc &vSideStakeAlloc, double &dEfficiency);
unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency);
bool GetStakeSplitStatusAndParams(int64_t& nMinStakeSplitValue, double& dEfficiency, int64_t& nDesiredStakeOutputValue);

bool CreateMRCRewards(CBlock &blocknew,
                      std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>& mrc_map,
                      std::map<GRC::Cpid, uint256>& mrc_tx_map,
                      CAmount& reward,
                      uint32_t& claim_contract_version,
                      GRC::Claim& claim,
                      CWallet* pwallet) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool CreateRestOfTheBlock(CBlock &block, CBlockIndex* pindexPrev, std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>& mrc_map);
bool CreateGridcoinReward(CBlock &blocknew, CBlockIndex* pindexPrev, int64_t &nReward, GRC::Claim& claim) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

#endif // BITCOIN_MINER_H
