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

// The node-lifecycle flags (fQtActive, bGridcoinCoreInitComplete) moved to
// init.{h,cpp} (issue #3125 C9).

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

// The CBlock reward-claim accessors (GetClaim, PullClaim, GetSuperblock,
// GetMint, GetMRCFees) and the free GetClaimByIndex moved to
// gridcoin/block_claims.cpp; CBlockIndex::GetBlockTrust moved to chain.cpp
// (issue #3125 C9).

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

