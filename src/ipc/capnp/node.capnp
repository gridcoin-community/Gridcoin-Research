# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xe6826a6267f49ab4;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/node.h");
$Proxy.includeTypes("ipc/capnp/node-types.h");

using Handler = import "handler.capnp";

# Mirrors interfaces::Node (src/interfaces/node.h). uint256 crosses as Data
# (32 raw bytes, see common-types.h); std::optional<int> returns as a
# (result, hasResult) pair; the core enums ChangeType / scrapereventtypes and the
# GUI enums (DiagnosticTest/DiagnosticStatus, carried as ints in the DTOs) cross
# as their integer values.
interface Node $Proxy.wrap("interfaces::Node") {
    destroy @0 (context :Proxy.Context) -> ();

    getNodeCount @1 (context :Proxy.Context) -> (result :Int32);
    getTotalBytesRecv @2 (context :Proxy.Context) -> (result :UInt64);
    getTotalBytesSent @3 (context :Proxy.Context) -> (result :UInt64);
    getNumBlocks @4 (context :Proxy.Context) -> (result :Int32);
    getBestBlockHash @5 (context :Proxy.Context) -> (result :Data);
    getLastBlockTime @6 (context :Proxy.Context) -> (result :Int64);
    getNumBlocksOfPeers @7 (context :Proxy.Context) -> (result :Int32);
    tryGetNumBlocksOfPeers @8 (context :Proxy.Context) -> (result :Int32, hasResult :Bool);
    getDifficulty @9 (context :Proxy.Context) -> (result :Float64);
    isInitialBlockDownload @10 (context :Proxy.Context) -> (result :Bool);
    isOutOfSyncByAge @11 (context :Proxy.Context) -> (result :Bool);
    getWarnings @12 (context :Proxy.Context) -> (result :Text);
    getClientVersion @13 (context :Proxy.Context) -> (result :Text);
    isTestNet @14 (context :Proxy.Context) -> (result :Bool);
    startShutdown @15 (context :Proxy.Context) -> ();
    checkForLatestUpdate @16 (context :Proxy.Context) -> (result :LatestVersionInfo);
    runDiagnostics @17 (context :Proxy.Context) -> (result :List(DiagnosticResult));
    getBlockDifficulty @18 (context :Proxy.Context, targetBits :UInt32) -> (result :Float64);
    getAlertStatusBarMessage @19 (context :Proxy.Context, hash :Data) -> (result :Text);
    getBanned @20 (context :Proxy.Context) -> (result :List(BannedNode));
    getPeers @21 (context :Proxy.Context) -> (result :List(PeerInfo));
    banNode @22 (context :Proxy.Context, nodeId :Int64, banTimeSeconds :Int64) -> ();
    unban @23 (context :Proxy.Context, subnet :Text) -> (result :Bool);
    disconnectNode @24 (context :Proxy.Context, nodeId :Int64) -> (result :Bool);
    executeRpcConsoleCommand @25 (context :Proxy.Context, method :Text, args :List(Text)) -> (result :RpcConsoleResult);
    listRpcCommands @26 (context :Proxy.Context) -> (result :List(Text));
    getSettingBool @27 (context :Proxy.Context, name :Text, defaultVal :Bool) -> (result :Bool);
    getSettingInt @28 (context :Proxy.Context, name :Text, defaultVal :Int64) -> (result :Int64);
    getSettingStr @29 (context :Proxy.Context, name :Text, defaultVal :Text) -> (result :Text);
    isSettingSet @30 (context :Proxy.Context, name :Text) -> (result :Bool);
    changeSettings @31 (context :Proxy.Context, settings :List(Pair(Text, Text))) -> (result :SettingChangeResult);
    handleRwSettingsUpdated @32 (context :Proxy.Context, callback :VoidCallback) -> (result :Handler.Handler);
    handleInitShutdown @33 (context :Proxy.Context, callback :VoidCallback) -> (result :Handler.Handler);
    getScraperConvergenceSnapshot @34 (context :Proxy.Context) -> (result :ScraperConvergenceSnapshot);
    handleNotifyBlocksChanged @35 (context :Proxy.Context, callback :NotifyBlocksChangedCallback) -> (result :Handler.Handler);
    handleNotifyNumConnectionsChanged @36 (context :Proxy.Context, callback :NotifyNumConnectionsChangedCallback) -> (result :Handler.Handler);
    handleBannedListChanged @37 (context :Proxy.Context, callback :VoidCallback) -> (result :Handler.Handler);
    handleNotifyAlertChanged @38 (context :Proxy.Context, callback :NotifyAlertChangedCallback) -> (result :Handler.Handler);
    handleMinerStatusChanged @39 (context :Proxy.Context, callback :MinerStatusChangedCallback) -> (result :Handler.Handler);
    handlePSGTPoolChanged @40 (context :Proxy.Context, callback :PSGTPoolChangedCallback) -> (result :Handler.Handler);
    handleNotifyScraperEvent @41 (context :Proxy.Context, callback :NotifyScraperEventCallback) -> (result :Handler.Handler);
}

# --- Notification callbacks (interfaces::Node::*Fn std::function types) ---
# Each wraps mp::ProxyCallback<the std::function type>; call() carries the
# callback's arguments. Several handlers share the void() signature.

interface VoidCallback $Proxy.wrap("ProxyCallback<std::function<void()>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context) -> ();
}

interface NotifyBlocksChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(bool, int, int64_t, uint32_t)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, syncing :Bool, height :Int32, bestTime :Int64, targetBits :UInt32) -> ();
}

interface NotifyNumConnectionsChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(int)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, numConnections :Int32) -> ();
}

interface NotifyAlertChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(const uint256&, ChangeType)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, hash :Data, status :Int32) -> ();
}

interface MinerStatusChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(bool, double)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, staking :Bool, coinWeight :Float64) -> ();
}

interface PSGTPoolChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(const uint256&, ChangeType, int)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, revisionHash :Data, status :Int32, reason :Int32) -> ();
}

interface NotifyScraperEventCallback $Proxy.wrap("ProxyCallback<std::function<void(const scrapereventtypes&, ChangeType, const std::string&)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, eventType :Int32, status :Int32, message :Text) -> ();
}

# --- Value DTOs (interfaces:: structs in node.h) ---

struct BannedNode $Proxy.wrap("interfaces::BannedNode") {
    address @0 :Text;
    banUntil @1 :Int64 $Proxy.name("ban_until");
}

struct PeerInfo $Proxy.wrap("interfaces::PeerInfo") {
    id @0 :Int64;
    addrName @1 :Text $Proxy.name("addr_name");
    addrLocal @2 :Text $Proxy.name("addr_local");
    subversion @3 :Text;
    services @4 :UInt64;
    version @5 :Int32;
    startingHeight @6 :Int32 $Proxy.name("starting_height");
    misbehavior @7 :Int32;
    inbound @8 :Bool;
    lastSend @9 :Int64 $Proxy.name("last_send");
    lastRecv @10 :Int64 $Proxy.name("last_recv");
    sendBytes @11 :UInt64 $Proxy.name("send_bytes");
    recvBytes @12 :UInt64 $Proxy.name("recv_bytes");
    timeConnected @13 :Int64 $Proxy.name("time_connected");
    timeOffset @14 :Int64 $Proxy.name("time_offset");
    pingTime @15 :Float64 $Proxy.name("ping_time");
    pingWait @16 :Float64 $Proxy.name("ping_wait");
    minPing @17 :Float64 $Proxy.name("min_ping");
}

struct RpcConsoleResult $Proxy.wrap("interfaces::RpcConsoleResult") {
    ok @0 :Bool;
    output @1 :Text;
}

struct SettingChangeResult $Proxy.wrap("interfaces::SettingChangeResult") {
    ok @0 :Bool;
    invalidInput @1 :Bool $Proxy.name("invalid_input");
    error @2 :Text;
    requiresRestart @3 :Bool $Proxy.name("requires_restart");
    noChange @4 :List(Text) $Proxy.name("no_change");
    immediate @5 :List(Text);
    requiresRestartSettings @6 :List(Text) $Proxy.name("requires_restart_settings");
}

struct ScraperConvergenceSnapshot $Proxy.wrap("interfaces::ScraperConvergenceSnapshot") {
    time @0 :Int64;
    excludedProjects @1 :List(Text) $Proxy.name("excluded_projects");
    includedScrapers @2 :List(Text) $Proxy.name("included_scrapers");
    excludedScrapers @3 :List(Text) $Proxy.name("excluded_scrapers");
    scrapersNotPublishing @4 :List(Text) $Proxy.name("scrapers_not_publishing");
}

struct LatestVersionInfo $Proxy.wrap("interfaces::LatestVersionInfo") {
    version @0 :Text;
    details @1 :Text;
    upgradeType @2 :Int32 $Proxy.name("upgrade_type");
}

struct DiagnosticResult $Proxy.wrap("interfaces::DiagnosticResult") {
    testId @0 :Int32 $Proxy.name("test_id");
    status @1 :Int32;
    resultString @2 :Text $Proxy.name("result_string");
    tipString @3 :Text $Proxy.name("tip_string");
}

# Generic key/value pair, for changeSettings' vector<pair<string,string>>.
struct Pair(T1, T2) {
    first @0 :T1;
    second @1 :T2;
}
