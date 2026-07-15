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
struct CMutableTransaction;

typedef std::vector<GRC::SideStake_ptr> SideStakeAlloc;

extern unsigned int nMinerSleep;

//! \brief Select the block header version the miner must stamp for a block at
//! the given height. Mirrors Bitcoin Core's ComputeBlockVersion: a pure,
//! side-effect-free function of \p height and the active chain parameters, so
//! the version ladder that used to live inline in the two StakeBlock assembly
//! paths is now unit-testable.
//!
//! \param height The height of the block being produced, i.e.
//!               pindexPrev->nHeight + 1.
//! \return 12 below V13, 13 in [V13, V14), 14 in [V14, V15), and
//!         CBlock::CURRENT_VERSION at or above V15. The V15 rung must return
//!         CURRENT_VERSION (not a literal 15) so that a future CURRENT_VERSION
//!         bump keeps the miner default aligned with the AcceptBlock lower
//!         bound (issue #3121 / #2955: the miner emitting a stale version its
//!         own validator rejects self-halts the chain at the gate).
int32_t ComputeBlockVersion(int height);

// Regtest-only staking-height ceiling. When > 0, ThreadStakeMiner pauses
// once `nBestHeight >= g_stakelimit_height`. Set via the `stakelimit` RPC
// (Particl-analog) and consumed in StakeMiner. Unused on testnet / mainnet.
extern std::atomic<int> g_stakelimit_height;

// Note the below constant controls the minimum value allowed for post
// split UTXO size. It is int64_t but in GRC so that it matches the entry in the config file.
// It will be converted to Halfords in GetNumberOfStakeOutputs by multiplying by COIN.
static const int64_t MIN_STAKE_SPLIT_VALUE_GRC = 800;

void SplitCoinStakeOutput(CMutableTransaction& mtxCoinstake, CBlock &blocknew, int64_t &nReward,
                          bool &fEnableStakeSplit, bool &fEnableSideStaking,
                          int64_t &nMinStakeSplitValue, double &dEfficiency) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool GetStakeSplitStatusAndParams(int64_t& nMinStakeSplitValue, double& dEfficiency, int64_t& nDesiredStakeOutputValue) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

bool CreateMRCRewards(CMutableTransaction& mtxCoinbase, CMutableTransaction& mtxCoinstake,
                      CBlock &blocknew,
                      std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>& mrc_map,
                      std::map<GRC::Cpid, uint256>& mrc_tx_map,
                      CAmount& reward,
                      uint32_t& claim_contract_version,
                      GRC::Claim& claim,
                      CWallet* pwallet) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool CreateRestOfTheBlock(CBlock &block, CMutableTransaction& mtxCoinbase,
                          const CMutableTransaction& mtxCoinstake, CBlockIndex* pindexPrev,
                          std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>& mrc_map);
bool CreateGridcoinReward(CMutableTransaction& mtxCoinbase, CMutableTransaction& mtxCoinstake,
                          CBlock &blocknew, CBlockIndex* pindexPrev, int64_t &nReward,
                          GRC::Claim& claim) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

// Regtest-only single-iteration of the staking pipeline. Returns true and
// fills blocknew_out on success; returns false and sets err on failure. Used
// by `generatetoaddress` / `generatesuperblock` RPCs to advance the chain
// deterministically.
//
// stake_time_slot_offset advances the candidate stake timestamp by that many
// 16-second STAKE_TIMESTAMP_MASK slots. The kernel is evaluated at a single
// masked timestamp (no time search), so a given slot either yields a passing
// kernel for the wallet's coins or does not; the caller passes the retry
// attempt number here so each retry rolls a fresh slot instead of
// re-evaluating the same dead one (see the retry loop in generatetoaddress).
bool TryMineRegtestBlock(CWallet* pwallet,
                         CBlock& blocknew_out,
                         std::string& err,
                         int stake_time_slot_offset = 0);

//! \brief The proof-of-stake miner loop. Runs until \ref fShutdown is set.
void StakeMiner(CWallet* pwallet);

//! \brief Thread entry point wrapping StakeMiner. \p parg is the CWallet to
//! stake with. Launched from AppInit2 and joined in Shutdown() before the
//! block-file flush, so a block staked during shutdown cannot land after the
//! flush (see issue #2865).
void ThreadStakeMiner(void* parg);

#endif // BITCOIN_MINER_H
