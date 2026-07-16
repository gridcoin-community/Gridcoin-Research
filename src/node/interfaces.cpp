// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/node.h"

#include "alert.h"
#include "banman.h"
#include "clientversion.h"
#include "gridcoin/scraper/scraper.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/superblock.h"
#include "init.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/staking.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"
#include "main.h"
#include "net.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "util.h"

#include <memory>
#include <optional>
#include <utility>

// The converged scraper stats cache has no header declaration; every consumer
// declares the extern locally (quorum.cpp, scraper_net.cpp, rpc/blockchain.cpp).
extern ConvergedScraperStats ConvergedScraperStatsCache;

namespace interfaces {
namespace {

//! In-process Node implementation: thin wrappers over the existing globals.
//! Methods that read cs_main-guarded chain state take the lock themselves, so
//! callers never hold core locks (and per the boundary rules, must not).
class NodeImpl : public Node
{
public:
    int getNodeCount() override
    {
        return g_connman ? static_cast<int>(g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL)) : 0;
    }

    uint64_t getTotalBytesRecv() override { return CNode::GetTotalBytesRecv(); }

    uint64_t getTotalBytesSent() override { return CNode::GetTotalBytesSent(); }

    int getNumBlocks() override
    {
        LOCK(cs_main);
        return nBestHeight;
    }

    uint256 getBestBlockHash() override
    {
        LOCK(cs_main);
        return hashBestChain;
    }

    int64_t getLastBlockTime() override
    {
        LOCK(cs_main);
        return pindexBest ? pindexBest->GetBlockTime() : 0;
    }

    int getNumBlocksOfPeers() override { return GetNumBlocksOfPeers(); }

    std::optional<int> tryGetNumBlocksOfPeers() override
    {
        TRY_LOCK(cs_main, locked);

        if (!locked) {
            return std::nullopt;
        }

        // Holding cs_main here makes the internal (recursive) lock in
        // GetNumBlocksOfPeers() uncontended.
        return GetNumBlocksOfPeers();
    }

    double getDifficulty() override
    {
        LOCK(cs_main);
        return GRC::GetCurrentDifficulty();
    }

    bool isInitialBlockDownload() override { return IsInitialBlockDownload(); }

    bool isOutOfSyncByAge() override { return OutOfSyncByAge(); }

    std::string getWarnings() override { return GetWarnings("statusbar"); }

    std::string getClientVersion() override { return FormatFullVersion(); }

    bool isTestNet() override { return fTestNet; }

    double getBlockDifficulty(uint32_t target_bits) override
    {
        return GRC::GetBlockDifficulty(target_bits);
    }

    std::string getAlertStatusBarMessage(const uint256& hash) override
    {
        const CAlert alert = CAlert::getAlertByHash(hash);
        return alert.IsNull() ? std::string{} : alert.strStatusBar;
    }

    std::vector<BannedNode> getBanned() override
    {
        std::vector<BannedNode> banned;

        if (g_banman) {
            banmap_t ban_map;
            g_banman->GetBanned(ban_map);
            banned.reserve(ban_map.size());

            for (const auto& entry : ban_map) {
                banned.push_back({entry.first.ToString(), entry.second.nBanUntil});
            }
        }

        return banned;
    }

    ScraperConvergenceSnapshot getScraperConvergenceSnapshot() override
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ScraperConvergenceSnapshot snapshot;
        snapshot.time = ConvergedScraperStatsCache.nTime;
        snapshot.excluded_projects = ConvergedScraperStatsCache.Convergence.vExcludedProjects;
        snapshot.included_scrapers = ConvergedScraperStatsCache.Convergence.vIncludedScrapers;
        snapshot.excluded_scrapers = ConvergedScraperStatsCache.Convergence.vExcludedScrapers;
        snapshot.scrapers_not_publishing = ConvergedScraperStatsCache.Convergence.vScrapersNotPublishing;

        return snapshot;
    }

    std::unique_ptr<Handler> handleNotifyBlocksChanged(NotifyBlocksChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyBlocksChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyNumConnectionsChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.BannedListChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyAlertChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleMinerStatusChanged(MinerStatusChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.MinerStatusChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handlePSGTPoolChanged(PSGTPoolChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.PSGTPoolChanged_connect(std::move(fn)));
    }

    std::unique_ptr<Handler> handleNotifyScraperEvent(NotifyScraperEventFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyScraperEvent_connect(std::move(fn)));
    }
};

//! Monolithic-build bootstrap: the factories construct the in-process
//! wrappers around this process's node and wallet.
class InitImpl : public Init
{
public:
    std::unique_ptr<Node> makeNode() override { return MakeNode(); }

    std::unique_ptr<StakingStatus> makeStakingStatus() override { return MakeStakingStatus(); }

    std::unique_ptr<Wallet> makeWallet() override
    {
        return pwalletMain ? MakeWallet(pwalletMain) : nullptr;
    }

    std::shared_ptr<WalletTxSource> makeWalletTxSource(CWallet* wallet) override
    {
        return wallet ? MakeWalletTxSource(wallet) : nullptr;
    }
};

} // namespace

std::unique_ptr<Node> MakeNode()
{
    return std::make_unique<NodeImpl>();
}

std::unique_ptr<Init> MakeGridcoinInit()
{
    return std::make_unique<InitImpl>();
}

} // namespace interfaces
