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
#include "gridcoin/upgrade.h"
#include "init.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/mrc.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "node/ui_interface.h"
#include "rpc/client.h"
#include "rpc/server.h"
#include "sync.h"
#include "util.h"
#include "util/strencodings.h"
#include "wallet/diagnose.h"

#include <univalue.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

// The converged scraper stats cache has no header declaration; every consumer
// declares the extern locally (quorum.cpp, scraper_net.cpp, rpc/blockchain.cpp).
extern ConvergedScraperStats ConvergedScraperStatsCache;
extern std::atomic<bool> bGridcoinCoreInitComplete;

namespace interfaces {
namespace {

//! Substitute a diagnostic template's positional placeholders the same way the
//! GUI formerly did with QString::arg(): each argument, in order, replaces the
//! lowest-numbered "%N" marker still present in the string (all occurrences of
//! that marker). The diagnostic templates only ever use "%1", but this mirrors
//! QString::arg's lowest-marker-first rule so multi-arg templates behave
//! identically if any are added. This matches QString::arg exactly for the
//! diagnostic args, which are all numeric (no '%'); an argument that itself
//! contained "%N" could be rescanned by a later argument (QString::arg tracks
//! already-filled positions), but no diagnostic produces such an argument.
std::string ArgSubstitute(std::string text, const std::vector<std::string>& args)
{
    for (const std::string& arg : args) {
        // Find the lowest-numbered %N marker currently in the string. IsDigit()
        // (util/strencodings.h) is used rather than std::isdigit, which is
        // locale-dependent (banned by lint-locale-dependence.sh).
        int lowest = -1;
        for (size_t i = 0; i + 1 < text.size(); ++i) {
            if (text[i] != '%' || !IsDigit(text[i + 1])) continue;

            size_t j = i + 1;
            int value = 0;
            while (j < text.size() && IsDigit(text[j])) {
                value = value * 10 + (text[j] - '0');
                ++j;
            }

            if (lowest == -1 || value < lowest) lowest = value;
        }

        if (lowest == -1) break; // No markers left; extra args are dropped (as QString::arg would).

        const std::string marker = "%" + ToString(lowest);
        for (size_t pos = text.find(marker); pos != std::string::npos; pos = text.find(marker, pos)) {
            text.replace(pos, marker.size(), arg);
            pos += arg.size();
        }
    }

    return text;
}

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

    bool isTestNet() override { return OnTestnet(); }

    void startShutdown() override { StartShutdown(); }

    LatestVersionInfo checkForLatestUpdate() override
    {
        LatestVersionInfo info;
        GRC::Upgrade::UpgradeType upgrade_type = GRC::Upgrade::UpgradeType::Unknown;
        GRC::Upgrade::CheckForLatestUpdate(info.version, info.details, upgrade_type, false);
        // Lock the int contract carried in LatestVersionInfo::upgrade_type (and
        // mirrored GUI-side by UpdateDialog::UpdateType) to the core enum.
        static_assert(static_cast<int>(GRC::Upgrade::UpgradeType::Unknown) == 0
                      && static_cast<int>(GRC::Upgrade::UpgradeType::Leisure) == 1
                      && static_cast<int>(GRC::Upgrade::UpgradeType::Mandatory) == 2
                      && static_cast<int>(GRC::Upgrade::UpgradeType::Unsupported) == 3,
                      "LatestVersionInfo::upgrade_type int contract must match GRC::Upgrade::UpgradeType");
        info.upgrade_type = static_cast<int>(upgrade_type);
        return info;
    }

    std::vector<DiagnosticResult> runDiagnostics() override
    {
        // The GUI-facing test-id and status mirrors must match the core enums.
        static_assert(static_cast<int>(DiagnosticTest::CheckConnectionCount) == DiagnoseLib::Diagnose::CheckConnectionCount
                      && static_cast<int>(DiagnosticTest::CheckOutboundConnectionCount) == DiagnoseLib::Diagnose::CheckOutboundConnectionCount
                      && static_cast<int>(DiagnosticTest::VerifyWalletIsSynced) == DiagnoseLib::Diagnose::VerifyWalletIsSynced
                      && static_cast<int>(DiagnosticTest::CheckClientVersion) == DiagnoseLib::Diagnose::CheckClientVersion
                      && static_cast<int>(DiagnosticTest::VerifyBoincPath) == DiagnoseLib::Diagnose::VerifyBoincPath
                      && static_cast<int>(DiagnosticTest::VerifyCPIDHasRAC) == DiagnoseLib::Diagnose::VerifyCPIDHasRAC
                      && static_cast<int>(DiagnosticTest::VerifyCPIDIsActive) == DiagnoseLib::Diagnose::VerifyCPIDIsActive
                      && static_cast<int>(DiagnosticTest::VerifyCPIDValid) == DiagnoseLib::Diagnose::VerifyCPIDValid
                      && static_cast<int>(DiagnosticTest::VerifyClock) == DiagnoseLib::Diagnose::VerifyClock
                      && static_cast<int>(DiagnosticTest::VerifyTCPPort) == DiagnoseLib::Diagnose::VerifyTCPPort
                      && static_cast<int>(DiagnosticTest::CheckDifficulty) == DiagnoseLib::Diagnose::CheckDifficulty
                      && static_cast<int>(DiagnosticTest::CheckETTS) == DiagnoseLib::Diagnose::CheckETTS,
                      "interfaces::DiagnosticTest must mirror DiagnoseLib::Diagnose::TestNames");
        static_assert(static_cast<int>(DiagnosticStatus::PASS) == DiagnoseLib::Diagnose::PASS
                      && static_cast<int>(DiagnosticStatus::WARNING) == DiagnoseLib::Diagnose::WARNING
                      && static_cast<int>(DiagnosticStatus::FAIL) == DiagnoseLib::Diagnose::FAIL
                      && static_cast<int>(DiagnosticStatus::NONE) == DiagnoseLib::Diagnose::NONE,
                      "interfaces::DiagnosticStatus must mirror DiagnoseLib::Diagnose::diagnoseResults");

        // Resolve the researcher mode the tests branch on (reads core researcher
        // state, not the GUI model).
        DiagnoseLib::Diagnose::setResearcherModel();

        // Construct all tests up front so they are all registered before any
        // runs: two tests (VerifyClock, VerifyTCPPort) read CheckConnectionCount's
        // result through the shared test map, and CheckConnectionCount -- enum id
        // 0 -- is run first below, so its result is available to them.
        std::vector<std::unique_ptr<DiagnoseLib::Diagnose>> tests;
        tests.push_back(std::make_unique<DiagnoseLib::CheckConnectionCount>());
        tests.push_back(std::make_unique<DiagnoseLib::CheckOutboundConnectionCount>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyWalletIsSynced>());
        tests.push_back(std::make_unique<DiagnoseLib::CheckClientVersion>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyBoincPath>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyCPIDHasRAC>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyCPIDIsActive>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyCPIDValid>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyClock>());
        tests.push_back(std::make_unique<DiagnoseLib::VerifyTCPPort>());
        tests.push_back(std::make_unique<DiagnoseLib::CheckDifficulty>());
        tests.push_back(std::make_unique<DiagnoseLib::CheckETTS>());

        std::vector<DiagnosticResult> results;
        results.reserve(tests.size());

        for (auto& test : tests) {
            test->runCheck();

            DiagnosticResult result;
            result.test_id = static_cast<int>(test->getTestName());
            result.status = static_cast<int>(test->getResults());
            result.result_string = ArgSubstitute(test->getResultsString(), test->getStringArgs());
            result.tip_string = ArgSubstitute(test->getResultsTip(), test->getTipArgs());
            results.push_back(std::move(result));
        }

        return results;
    }

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

    std::vector<PeerInfo> getPeers() override
    {
        std::vector<PeerInfo> peers;

        if (!g_connman) {
            return peers;
        }

        std::vector<CNodeStats> vstats;
        g_connman->GetNodeStats(vstats);
        peers.reserve(vstats.size());

        for (const CNodeStats& s : vstats) {
            PeerInfo p;
            p.id = s.id;
            p.addr_name = s.addrName;
            p.addr_local = s.addrLocal;
            p.subversion = s.strSubVer;
            p.services = s.nServices;
            p.version = s.nVersion;
            p.starting_height = s.nStartingHeight;
            p.misbehavior = s.nMisbehavior;
            p.inbound = s.fInbound;
            p.last_send = s.nLastSend;
            p.last_recv = s.nLastRecv;
            p.send_bytes = s.nSendBytes;
            p.recv_bytes = s.nRecvBytes;
            p.time_connected = s.nTimeConnected;
            p.time_offset = s.nTimeOffset;
            p.ping_time = s.dPingTime;
            p.ping_wait = s.dPingWait;
            p.min_ping = s.dMinPing;
            peers.push_back(std::move(p));
        }

        return peers;
    }

    void banNode(int64_t node_id, int64_t ban_time_seconds) override
    {
        if (!g_connman || !g_banman) {
            return;
        }

        // Look the peer's address up by connection id, then ban it and drop the
        // connection -- mirroring the GUI's ban action (ban address + disconnect).
        // ForEachNode walks m_nodes under m_nodes_mutex; copy out just the address
        // and do the Ban()/DisconnectNode() afterwards (outside that lock) rather
        // than snapshotting every peer's full CNodeStats to find one address.
        CAddress addr;
        bool found = false;
        g_connman->ForEachNode([&](CNode* pnode) {
            if (!found && pnode->GetId() == node_id) {
                addr = pnode->addr;
                found = true;
            }
        });

        if (found) {
            g_banman->Ban(addr, BanReasonManuallyAdded, ban_time_seconds);
            g_connman->DisconnectNode(node_id);
        }
    }

    bool unban(const std::string& subnet) override
    {
        if (!g_banman) {
            return false;
        }

        CSubNet sub;
        LookupSubNet(subnet.c_str(), sub);

        return sub.IsValid() && g_banman->Unban(sub);
    }

    bool disconnectNode(int64_t node_id) override
    {
        return g_connman && g_connman->DisconnectNode(node_id);
    }

    RpcConsoleResult executeRpcConsoleCommand(const std::string& method,
                                              const std::vector<std::string>& args) override
    {
        RpcConsoleResult out;

        try {
            // Convert the positional string args in the command-dependent way,
            // dispatch, and format the reply -- all UniValue handling stays here.
            UniValue result = tableRPC.execute(method, RPCConvertValues(method, args));

            if (result.isNull()) {
                out.output.clear();
            } else if (result.isStr()) {
                out.output = result.get_str();
            } else {
                out.output = result.write(2);
            }
            out.ok = true;
        } catch (UniValue& objError) {
            try { // Nice formatting for a standard-format error
                const int code = find_value(objError, "code").get_int();
                const std::string message = find_value(objError, "message").get_str();
                out.output = strprintf("%s (code %d)", message, code);
            } catch (const std::runtime_error&) { // missing code/message: raw JSON
                out.output = objError.write();
            }
            out.ok = false;
        } catch (const std::exception& e) {
            out.output = std::string("Error: ") + e.what();
            out.ok = false;
        }

        return out;
    }

    std::vector<std::string> listRpcCommands() override
    {
        return tableRPC.listCommands();
    }

    bool getSettingBool(const std::string& name, bool default_val) override
    {
        return gArgs.GetBoolArg("-" + name, default_val);
    }

    int64_t getSettingInt(const std::string& name, int64_t default_val) override
    {
        return gArgs.GetArg("-" + name, default_val);
    }

    std::string getSettingStr(const std::string& name, const std::string& default_val) override
    {
        return gArgs.GetArg("-" + name, default_val);
    }

    bool isSettingSet(const std::string& name) override
    {
        return gArgs.IsArgSet("-" + name);
    }

    SettingChangeResult changeSettings(
        const std::vector<std::pair<std::string, std::string>>& settings) override
    {
        SettingChangeResult out;
        out.ok = ChangeSettings(settings, out.requires_restart, out.no_change, out.immediate,
                                out.requires_restart_settings, out.invalid_input, out.error);
        return out;
    }

    std::unique_ptr<Handler> handleRwSettingsUpdated(RwSettingsUpdatedFn fn) override
    {
        return MakeSignalHandler(uiInterface.RwSettingsUpdated_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handleInitShutdown(InitShutdownFn fn) override
    {
        return MakeSignalHandler(uiInterface.QueueShutdown_connect(interfaces::GuardNotify(std::move(fn))));
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

        // Current scraper status (last non-Log event), so the GUI can initialize
        // its icon at connect time. Atomic read, no external locks; leaves the
        // -1/-1 defaults when no event has fired yet.
        uiInterface.GetLastScraperEvent(snapshot.current_event_type, snapshot.current_event_status);

        return snapshot;
    }

    std::unique_ptr<Handler> handleNotifyBlocksChanged(NotifyBlocksChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyBlocksChanged_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyNumConnectionsChanged_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.BannedListChanged_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyAlertChanged_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handleMinerStatusChanged(MinerStatusChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.MinerStatusChanged_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handlePSGTPoolChanged(PSGTPoolChangedFn fn) override
    {
        return MakeSignalHandler(uiInterface.PSGTPoolChanged_connect(interfaces::GuardNotify(std::move(fn))));
    }

    std::unique_ptr<Handler> handleNotifyScraperEvent(NotifyScraperEventFn fn) override
    {
        return MakeSignalHandler(uiInterface.NotifyScraperEvent_connect(interfaces::GuardNotify(std::move(fn))));
    }
};

//! Monolithic-build bootstrap: the factories construct the in-process
//! wrappers around this process's node and wallet.
class InitImpl : public Init
{
public:
    bool isCoreReady() override { return bGridcoinCoreInitComplete.load(); }

    std::unique_ptr<Node> makeNode() override { return MakeNode(); }

    std::unique_ptr<StakingStatus> makeStakingStatus() override { return MakeStakingStatus(); }

    std::unique_ptr<MRC> makeMRC() override
    {
        return pwalletMain ? MakeMRC(pwalletMain) : nullptr;
    }

    std::unique_ptr<SideStakeManager> makeSideStakeManager() override
    {
        return MakeSideStakeManager();
    }

    std::unique_ptr<VotingManager> makeVotingManager() override
    {
        return MakeVotingManager();
    }

    std::unique_ptr<ResearcherContext> makeResearcherContext() override
    {
        return pwalletMain ? MakeResearcherContext(pwalletMain) : nullptr;
    }

    std::unique_ptr<PSGTPoolContext> makePSGTPoolContext() override
    {
        return pwalletMain ? MakePSGTPoolContext(pwalletMain) : nullptr;
    }

    std::unique_ptr<Wallet> makeWallet() override
    {
        return pwalletMain ? MakeWallet(pwalletMain) : nullptr;
    }

    std::shared_ptr<WalletTxSource> makeWalletTxSource() override
    {
        return pwalletMain ? MakeWalletTxSource(pwalletMain) : nullptr;
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
