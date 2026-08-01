// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "validationinterface.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h"
#include "gridcoin/voting/registry.h"
#include "util.h"
#include "net.h"
#include "streams.h"
#include "alert.h"
#include "checkpoints.h"
#include "txdb.h"
#include "init.h"
#include "node/chainman.h"
#include "node/ui_interface.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/gridcoin.h"
#include "gridcoin/mrc.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/scraper/scraper_net.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/exceptions.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/reward.h"
#include "gridcoin/staking/spam.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/tally.h"
#include "gridcoin/tx_message.h"
#include "node/blockstorage.h"
#include "node/coherence.h"
#include "node/orphan_blocks.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "random.h"
#include "validation.h"

#include <boost/thread.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <ctime>
#include <math.h>
#include <util/string.h>

using namespace std;
using namespace boost;

//
// Global state
//

// cs_main, the block-index pools, and the active-chain globals
// (mapBlockIndex, pindexGenesisBlock, nBestHeight, hashBestChain, pindexBest,
// the sync clocks, g_reorg_in_progress) moved to chain.cpp (issue #3125 C9);
// their declarations remain in chain.h.

// cs_tx_val_commit_to_disk, nCoinbaseMaturity, cPeerBlockCounts,
// nGrandfather, fEnforceCanonical, g_v11_timestamp, and the sync-state
// helpers (GetNumBlocksOfPeers, IsInitialBlockDownload, OutOfSyncByAge)
// moved to validation.{h,cpp} (issue #3125 C9).

///////////////////////MINOR VERSION////////////////////////////////

extern int64_t GetCoinYearReward(int64_t nTime);

// nStakeMinAge/nStakeMaxAge moved to gridcoin/staking/kernel.{h,cpp};
// nNodeLifespan to net.{h,cpp}; strMessageMagic and CoinToDouble to
// util.{h,cpp} (issue #3125 C9).




// Orphan block storage managed by g_orphan_blocks (node/orphan_blocks.h)


// COINBASE_FLAGS moved to miner.{h,cpp}; the wallet settings
// (nTransactionFee, nReserveBalance, nMinimumInputValue) to
// wallet/wallet.{h,cpp}; the mining status variables (cs_msMiningErrors,
// msMiningErrors) to gridcoin/researcher.{h,cpp}; and fUseFastIndex to
// primitives/block.{h,cpp} (issue #3125 C9).

// Gridcoin - Rob Halford

bool fQtActive = false;
std::atomic<bool> bGridcoinCoreInitComplete{false};

// End of Gridcoin Global vars

// g_seen_stakes, g_chain_trust, and GetChainTrust moved to
// gridcoin/staking/chain_trust.cpp (issue #3125 C9); declarations live in
// gridcoin/staking/spam.h and gridcoin/staking/chain_trust.h.

// The setpwalletRegistered wallet registry has been fully retired (issues #3030
// and #3108): wallet notifications flow through CMainSignals, the wallet
// rebroadcast self-schedules on g_scheduler, and the net-half request-count /
// own-tx-trickle paths call pwalletMain directly. The canonical lock order is
// now cs_main -> cs_wallet.



//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

const GRC::Claim& CBlock::GetClaim() const
{
    if (nVersion >= 11 || !vtx[0].vContracts.empty()) {
        return *vtx[0].vContracts[0].SharePayloadAs<GRC::Claim>();
    }

    // Before block version 11, the Gridcoin reward claim context is stored
    // in the hashBoinc field of the first transaction. We cache the parsed
    // representation in the block to speed up subsequent access:
    //
    if (m_claim_contract_cache.m_type == GRC::ContractType::UNKNOWN) {
        m_claim_contract_cache = GRC::MakeContract<GRC::Claim>(
            GRC::ContractAction::ADD,
            GRC::Claim::Parse(vtx[0].hashBoinc, nVersion));
    }

    return *m_claim_contract_cache.SharePayloadAs<GRC::Claim>();
}

GRC::Claim CBlock::PullClaim()
{
    if (nVersion >= 11 || !vtx[0].vContracts.empty()) {
        // PullPayloadAs operates on the shared_ptr within the Contract,
        // not on the vector element itself, so const vContracts is fine.
        return vtx[0].vContracts[0].CopyPayloadAs<GRC::Claim>();
    }

    // Before block version 11, the Gridcoin reward claim context is stored
    // in the hashBoinc field of the first transaction.
    //
    return GRC::Claim::Parse(vtx[0].hashBoinc, nVersion);
}

GRC::SuperblockPtr CBlock::GetSuperblock() const
{
    return GetClaim().m_superblock;
}

GRC::SuperblockPtr CBlock::GetSuperblock(const CBlockIndex* const pindex) const
{
    GRC::SuperblockPtr superblock = GetSuperblock();
    superblock.Rebind(pindex);

    return superblock;
}

// CBlockIndex::GetBlockTrust moved to chain.cpp (issue #3125 C9).

void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = mi->second;
        mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        std::stringstream output;

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++) {
                output << "| \n";
            }

            output << "|\\\n";
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++) {
                output << "| \n";
            }

            output << "|\n";
        }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++) {
            output << "| \n";
        }

        // print item (and also prepend above formatting)
        CBlock block;
        ReadBlockFromDisk(block, pindex, Params().GetConsensus());
        LogPrintf("%s%d (%u,%u) %s  %08x  %s  tx %" PRIszu "",
                  output.str(),
                  pindex->nHeight,
                  pindex->nFile,
                  pindex->nBlockPos,
                  block.GetHash(true).ToString().c_str(),
                  block.nBits,
                  DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
                  block.vtx.size());

        // put the main time-chain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (unsigned int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex)
{
    CBlock block;

    if (!pblockindex || !pblockindex->IsInMainChain()
        || !ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
    {
        return std::nullopt;
    }

    return block.PullClaim();
}

GRC::MintSummary CBlock::GetMint() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    CTxDB txdb("r");
    GRC::MintSummary mint;

    for (const auto& tx : vtx) {
        const CAmount tx_amount_out = tx.GetValueOut();

        if (tx.IsCoinBase()) {
            mint.m_total += tx_amount_out;
            continue;
        }

        CAmount tx_amount_in = 0;

        for (const auto& input : tx.vin) {
            CTransaction input_tx;

            if (txdb.ReadDiskTx(input.prevout.hash, input_tx)) {
                tx_amount_in += input_tx.vout[input.prevout.n].nValue;
            }
        }

        if (tx.IsCoinStake()) {
            mint.m_total += tx_amount_out - tx_amount_in;
        } else {
            mint.m_fees += tx_amount_in - tx_amount_out;
        }
    }

    return mint;
}

GRC::MRCFees CBlock::GetMRCFees() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    GRC::MRCFees mrc_fees;
    unsigned int mrc_output_limit = GetMRCOutputLimit(nVersion, false);

    // Return zeroes for mrc fees if MRC not allowed. (This could have also been done
    // by block version check, but this is more correct.)
    if (!mrc_output_limit) {
        return mrc_fees;
    }

    Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

    const GRC::Claim claim = GetClaim();

    CAmount mrc_total_fees = 0;

    // This is similar to the code in CheckMRCRewards in the Validator class, but with the validation removed because
    // the block has already been validated. We also only need the MRC fee calculation portion.
    for (const auto& tx: vtx) {
        for (const auto& mrc : claim.m_mrc_tx_map) {
            if (mrc.second == tx.GetHash() && !tx.GetContracts().empty()) {
                // An MRC contract must be the first and only contract on a transaction by protocol.
                GRC::Contract contract = tx.GetContracts()[0];

                if (contract.m_type != GRC::ContractType::MRC) continue;

                GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                mrc_fees.m_mrc_minimum_calc_fees += mrc.ComputeMRCFee();

                mrc_total_fees += mrc.m_fee;
                mrc_fees.m_mrc_foundation_fees += mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                            / foundation_fee_fraction.GetDenominator();
            }
        }
    }

    mrc_fees.m_mrc_staker_fees = mrc_total_fees - mrc_fees.m_mrc_foundation_fees;

    return mrc_fees;
}
