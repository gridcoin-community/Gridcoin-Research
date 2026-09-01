// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "gridcoin/mrc.h"
#include "gridcoin/sidestake.h"


class CWallet;
class CWalletTx;
struct CMutableTransaction;

typedef std::vector<GRC::SideStake_ptr> SideStakeAlloc;

extern unsigned int nMinerSleep;

//! Extra data appended to the coinbase scriptSig of blocks we create.
//! Defined in miner.cpp (moved from main.{h,cpp}, issue #3125 C9).
extern CScript COINBASE_FLAGS;

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

//! Whether the development-build staking cripple is active. When true,
//! ThreadStakeMiner refuses to stake and CommitTransaction refuses
//! reward-bearing sends: a development build disables all staking unless the
//! hidden `devbuild=override` setting is passed. State lives in miner.cpp;
//! extracted from the former util.h fDevbuildCripple global so util.h is
//! stateless. Set from init.
namespace GRC { class Superblock; }

//! \brief Stage an explicit superblock for the next block the miner assembles.
//!
//! Regtest only. Scrapers are disabled under -regtest, so Quorum::CreateSuperblock()
//! has nothing to build from and AddSuperblockContractOrVote suppresses auto-attach.
//! The generatesuperblock RPC stages a caller-specified superblock here; the next
//! call to AddSuperblockContractOrVote consumes it exactly once. If the block
//! attempt that consumed it fails to sign or be accepted, TryMineRegtestBlock
//! restores the staged value so the retry attempt carries it. Staging again
//! before consumption replaces the pending value (last writer wins).
void StageRegtestSuperblock(GRC::Superblock superblock) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

bool GetDevbuildCripple();
void SetDevbuildCripple(bool crippled);

void SplitCoinStakeOutput(CMutableTransaction& mtxCoinstake, CBlock &blocknew, int64_t &nReward,
                          bool &fEnableStakeSplit, bool &fEnableSideStaking,
                          int64_t &nMinStakeSplitValue, double &dEfficiency) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
//! \brief Efficiency-optimal post-stake UTXO size (equation (27)) for the current network
//! difficulty, from -stakingefficiency and -minstakesplitvalue (with their clamps and floors),
//! independent of whether -enablestakesplit is set. Out-params receive the effective minimum
//! stake split value (in Halfords) and the efficiency.
int64_t GetOptimalStakeSplitValue(int64_t& nMinStakeSplitValue, double& dEfficiency) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
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

//! \brief The proof-of-stake miner loop. Runs until ShutdownInProgress().
void StakeMiner(CWallet* pwallet);

//! \brief Thread entry point wrapping StakeMiner. \p parg is the CWallet to
//! stake with. Launched from AppInit2 and joined in Shutdown() before the
//! block-file flush, so a block staked during shutdown cannot land after the
//! flush (see issue #2865).
void ThreadStakeMiner(void* parg);

#endif // BITCOIN_MINER_H
