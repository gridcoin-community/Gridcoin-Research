// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_NODE_H
#define GRIDCOIN_INTERFACES_NODE_H

#include "interfaces/handler.h"
// For scrapereventtypes' enumerators, which consumers of the scraper-event
// handler discriminate on. TODO(Stage 2): extract the enum into a standalone
// header -- fwd.h transitively pulls heavyweight core headers (net.h, util.h)
// into every interface consumer.
#include "gridcoin/scraper/fwd.h"
// For ChangeType. TODO(Stage 2): extract ChangeType into a standalone header
// (upstream: util/ui_change_type.h) so interface headers stop pulling the
// signal hub transitively.
#include "node/ui_interface.h"
#include "uint256.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace interfaces {

//! A banned peer, as a value row for the GUI ban table.
struct BannedNode
{
    std::string address;  //!< Subnet/address string form.
    int64_t ban_until = 0; //!< Ban expiry, seconds since epoch.
};

//! Value snapshot of one connected peer's stats, as the GUI peer table and the
//! RPC-console peer-detail panel display them (the Qt-free mirror of the fields
//! the GUI read from the core CNodeStats). id is the connection's NodeId, which
//! the ban/disconnect commands act on.
struct PeerInfo
{
    int64_t id = 0;
    std::string addr_name;   //!< Display address (host:port).
    std::string addr_local;  //!< Local address the peer sees us as.
    std::string subversion;  //!< Reported user agent.
    uint64_t services = 0;
    int version = 0;
    int starting_height = 0;
    int misbehavior = 0;     //!< Ban score.
    bool inbound = false;
    int64_t last_send = 0;
    int64_t last_recv = 0;
    uint64_t send_bytes = 0;
    uint64_t recv_bytes = 0;
    int64_t time_connected = 0;
    int64_t time_offset = 0;
    double ping_time = 0;
    double ping_wait = 0;
    double min_ping = 0;
};

//! Result of running one RPC-console command. The whole UniValue round trip
//! (arg conversion, dispatch, result/error formatting) happens node-side; only
//! the already-formatted text crosses the boundary.
struct RpcConsoleResult
{
    //! false when the command threw (RPC error or std::exception).
    bool ok = false;
    //! Formatted reply text on success, or the formatted error message on
    //! failure ("<message> (code <n>)", or raw JSON / "Error: <what>").
    std::string output;
};

//! Result of Node::changeSettings, mirroring the changesettings RPC. `ok` false
//! means the change failed with `error` set: when `invalid_input` is true it was
//! a caller/validation error and nothing was changed; when false it was an
//! internal settings-file write error (and some earlier settings in the batch
//! may already have been applied).
struct SettingChangeResult
{
    bool ok = false;
    bool invalid_input = false;
    std::string error;
    //! At least one changed setting does not take effect until restart.
    bool requires_restart = false;
    //! name=value strings, categorized like the changesettings RPC.
    std::vector<std::string> no_change;
    std::vector<std::string> immediate;
    std::vector<std::string> requires_restart_settings;
};

//! Value snapshot of the scraper convergence status consumed by the GUI
//! status icon. Taken under the scraper cache lock inside the implementation;
//! callers hold nothing.
struct ScraperConvergenceSnapshot
{
    int64_t time = 0; //!< Convergence timestamp, 0 when no convergence yet.
    std::vector<std::string> excluded_projects;
    std::vector<std::string> included_scrapers;
    std::vector<std::string> excluded_scrapers;
    std::vector<std::string> scrapers_not_publishing;
};

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
    //! checkpoint height, whichever is greater. Blocking: takes cs_main.
    virtual int getNumBlocksOfPeers() = 0;

    //! Non-blocking variant of getNumBlocksOfPeers(): std::nullopt when
    //! cs_main is contended, for GUI-thread slots that must never wait on
    //! core locks.
    virtual std::optional<int> tryGetNumBlocksOfPeers() = 0;

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

    //! Proof-of-stake difficulty corresponding to a compact target-bits
    //! value (pure conversion; no chain state read).
    virtual double getBlockDifficulty(uint32_t target_bits) = 0;

    //! Status-bar message of the alert with the given hash, or an empty
    //! string when no such alert exists.
    virtual std::string getAlertStatusBarMessage(const uint256& hash) = 0;

    //! Current ban list.
    virtual std::vector<BannedNode> getBanned() = 0;

    //! Value snapshots of all currently-connected peers, for the peer table and
    //! the RPC-console peer-detail panel. Empty when the connection manager is
    //! not up.
    virtual std::vector<PeerInfo> getPeers() = 0;

    //! Ban and disconnect the peer with the given connection id for ban_time
    //! seconds (mirrors the GUI's ban action, which bans the address and drops
    //! the connection). No-op for an unknown id.
    virtual void banNode(int64_t node_id, int64_t ban_time_seconds) = 0;

    //! Remove a ban on the given subnet/address string (from the ban table).
    //! Returns false if the string is not a valid subnet or was not banned.
    virtual bool unban(const std::string& subnet) = 0;

    //! Disconnect the peer with the given connection id. Returns whether a peer
    //! was disconnected.
    virtual bool disconnectNode(int64_t node_id) = 0;

    //! Run one RPC-console command: convert the positional string args per the
    //! command's conversion table, dispatch it, and format the reply/error. All
    //! UniValue handling stays node-side. Blocks while the command runs, so the
    //! GUI calls it from its off-thread RPC executor.
    virtual RpcConsoleResult executeRpcConsoleCommand(const std::string& method,
                                                      const std::vector<std::string>& args) = 0;

    //! The list of RPC command names, for the console's autocomplete.
    virtual std::vector<std::string> listRpcCommands() = 0;

    //! Read the current effective value of a read-write/config setting, typed.
    //! `name` is the bare arg name without the leading dash. `default_val` is
    //! returned when the setting is unset. These forward to the node's argument
    //! store so the GUI (a separate process) never reads gArgs directly.
    virtual bool getSettingBool(const std::string& name, bool default_val) = 0;
    virtual int64_t getSettingInt(const std::string& name, int64_t default_val) = 0;
    virtual std::string getSettingStr(const std::string& name, const std::string& default_val) = 0;

    //! Whether the setting is explicitly set (config file, command line, or the
    //! read-write settings file) rather than falling back to its default. Used
    //! by the one-time GUI-settings migration to avoid overriding a value the
    //! user already set in gridcoinresearch.conf.
    virtual bool isSettingSet(const std::string& name) = 0;

    //! Change one or more settings (name/value, no leading dash). The node
    //! validates, persists to gridcoinsettings.json, and applies immediate side
    //! effects -- the same path as the changesettings RPC. An empty value erases
    //! the setting (unset -> default). This is deliberately a generic catch-all:
    //! the caller (OptionsModel) is typed and formats each value; the transport
    //! is name/value strings; the node coerces at read time.
    virtual SettingChangeResult changeSettings(
        const std::vector<std::pair<std::string, std::string>>& settings) = 0;

    //! Register a handler fired when the read-write settings file changes, so the
    //! GUI can refetch. Payload-free (bridges uiInterface.RwSettingsUpdated).
    using RwSettingsUpdatedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleRwSettingsUpdated(RwSettingsUpdatedFn fn) = 0;

    //! Value snapshot of the scraper convergence status.
    virtual ScraperConvergenceSnapshot getScraperConvergenceSnapshot() = 0;

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
