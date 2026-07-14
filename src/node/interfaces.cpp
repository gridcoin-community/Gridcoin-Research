// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/node.h"

#include "alert.h"
#include "clientversion.h"
#include "gridcoin/staking/difficulty.h"
#include "init.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/wallet.h"
#include "main.h"
#include "net.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "util.h"

#include <memory>
#include <utility>

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

    std::unique_ptr<Wallet> makeWallet() override
    {
        return pwalletMain ? MakeWallet(pwalletMain) : nullptr;
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
