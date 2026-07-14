// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_NODE_H
#define GRIDCOIN_INTERFACES_NODE_H

#include "interfaces/handler.h"
// For ChangeType; scrapereventtypes is forward-declared there. TODO(Stage 2):
// extract ChangeType into a standalone header (upstream: util/ui_change_type.h)
// so interface headers stop pulling the signal hub transitively.
#include "node/ui_interface.h"
#include "uint256.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace interfaces {

//! Top-level node interface for the GUI: chain and network state queries plus
//! notification registration. In the monolithic build the implementation
//! wraps the existing globals directly (see src/node/interfaces.cpp); in the
//! Stage 2 multiprocess build the same interface is served over IPC.
//!
//! Rules (see src/interfaces/README.md):
//! - only value types cross this boundary;
//! - notification callbacks run on the emitting core thread, frequently while
//!   the emitter holds core locks -- callbacks must enqueue and return, and
//!   must never take cs_main or re-enter interface methods that do.
//!
//! The method set intentionally covers only what the migrated consumers need
//! (Phase 1b starts with ClientModel); it grows with each migration.
class Node
{
public:
    virtual ~Node() = default;

    //! Number of connected peers.
    virtual int getNodeCount() = 0;

    //! Total bytes received / sent across all peers since startup.
    virtual uint64_t getTotalBytesRecv() = 0;
    virtual uint64_t getTotalBytesSent() = 0;

    //! Height of the best chain.
    virtual int getNumBlocks() = 0;

    //! Hash of the best chain tip.
    virtual uint256 getBestBlockHash() = 0;

    //! Timestamp of the best chain tip.
    virtual int64_t getLastBlockTime() = 0;

    //! Median best-chain height reported by connected peers, or the hardened
    //! checkpoint height, whichever is greater.
    virtual int getNumBlocksOfPeers() = 0;

    //! Proof-of-stake difficulty of the chain tip.
    virtual double getDifficulty() = 0;

    //! Whether the node considers itself in initial block download.
    virtual bool isInitialBlockDownload() = 0;

    //! Whether the chain tip is stale relative to adjusted time.
    virtual bool isOutOfSyncByAge() = 0;

    //! Status-bar warning string (empty when there is nothing to show).
    virtual std::string getWarnings() = 0;

    //! Client version string.
    virtual std::string getClientVersion() = 0;

    //! Whether the node runs on testnet.
    virtual bool isTestNet() = 0;

    //! Register a handler for block-tip changes.
    using NotifyBlocksChangedFn =
        std::function<void(bool syncing, int height, int64_t best_time, uint32_t target_bits)>;
    virtual std::unique_ptr<Handler> handleNotifyBlocksChanged(NotifyBlocksChangedFn fn) = 0;

    //! Register a handler for peer-connection-count changes.
    using NotifyNumConnectionsChangedFn = std::function<void(int num_connections)>;
    virtual std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) = 0;

    //! Register a handler for ban-list changes.
    using BannedListChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) = 0;

    //! Register a handler for alert changes. The hash is a key; consumers
    //! refetch the alert body through query methods.
    using NotifyAlertChangedFn = std::function<void(const uint256& hash, ChangeType status)>;
    virtual std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) = 0;

    //! Register a handler for staking-status changes.
    using MinerStatusChangedFn = std::function<void(bool staking, double coin_weight)>;
    virtual std::unique_ptr<Handler> handleMinerStatusChanged(MinerStatusChangedFn fn) = 0;

    //! Register a handler for PSGT pool changes. The revision hash is a key;
    //! reason carries a PSGTRemovalReason as int on CT_DELETED, else -1.
    using PSGTPoolChangedFn =
        std::function<void(const uint256& revision_hash, ChangeType status, int reason)>;
    virtual std::unique_ptr<Handler> handlePSGTPoolChanged(PSGTPoolChangedFn fn) = 0;

    //! Register a handler for scraper events.
    using NotifyScraperEventFn =
        std::function<void(const scrapereventtypes& event_type, ChangeType status, const std::string& message)>;
    virtual std::unique_ptr<Handler> handleNotifyScraperEvent(NotifyScraperEventFn fn) = 0;
};

//! Return an in-process Node interface implementation.
std::unique_ptr<Node> MakeNode();

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_NODE_H
