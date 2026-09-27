// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_INTERFACEFAKES_H
#define BITCOIN_QT_TEST_INTERFACEFAKES_H

#include "interfaces/handler.h"
#include "interfaces/node.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

//! Hand-rolled test doubles for the interfaces:: boundary (Phase 1f). A model
//! test constructs a GUI model against one of these fakes -- with no wallet,
//! registry, or core global in sight -- and asserts the model reads and maps
//! only interface value data. That is the decoupling proof: the model compiles
//! and behaves correctly against a pure in-memory interface, so it holds no
//! hidden core coupling. Each fake serves canned value structs and records the
//! commands the model issued, so a test can also assert the model delegated the
//! right arguments back across the boundary.
//!
//! These are deliberately hand-rolled (not gMock): the project's test stack is
//! Qt Test / Boost.Test with no gmock dependency, and the interfaces are small
//! enough that a plain override-and-record fake is clearer than a mock DSL.
namespace qt_test {

//! Fake interfaces::SideStakeManager: serves a canned entries() snapshot, returns
//! a canned result from every mutating command, and records the last add/delete
//! so a test can assert the model delegated to the interface with the right args.
class FakeSideStakeManager : public interfaces::SideStakeManager
{
public:
    // Canned responses (set by the test before use).
    interfaces::SideStakeSnapshot m_snapshot;
    interfaces::SideStakeEditResult m_edit_result;

    // Recorded calls.
    int m_entries_calls = 0;
    std::string m_last_add_address;
    double m_last_add_allocation = 0.0;
    std::string m_last_add_description;
    std::string m_last_deleted_address;

    interfaces::SideStakeSnapshot entries() override
    {
        ++m_entries_calls;
        return m_snapshot;
    }

    uint64_t localRevision() override { return m_snapshot.local_revision; }

    interfaces::SideStakeEditResult addLocal(const std::string& address,
                                             double allocation_percent,
                                             const std::string& description) override
    {
        m_last_add_address = address;
        m_last_add_allocation = allocation_percent;
        m_last_add_description = description;
        return m_edit_result;
    }

    interfaces::SideStakeEditResult setAllocation(const std::string& /*address*/,
                                                  double /*allocation_percent*/) override
    {
        return m_edit_result;
    }

    interfaces::SideStakeEditResult setDescription(const std::string& /*address*/,
                                                   const std::string& /*description*/) override
    {
        return m_edit_result;
    }

    interfaces::SideStakeEditResult deleteLocal(const std::string& address) override
    {
        m_last_deleted_address = address;
        return m_edit_result;
    }

    // The subscription callbacks are dropped: these tests exercise the query and
    // command paths, not notification delivery. A model's notification slots
    // marshal onto the GUI thread via a queued QMetaObject::invokeMethod, so a
    // future notification-path test must run a QCoreApplication event loop (or
    // pump it) for the queued call to dispatch -- store and invoke the fn here
    // once that harness exists.
    std::unique_ptr<interfaces::Handler> handleRwSettingsUpdated(interfaces::RwSettingsUpdatedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

    std::unique_ptr<interfaces::Handler> handleMandatorySideStakeChanged(
        interfaces::MandatorySideStakeChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
};

//! Fake interfaces::ResearcherContext: serves a canned snapshot and pool list,
//! answers every command with a default result, and counts mode switches so a
//! test can tell how many times a wizard page was entered.
class FakeResearcherContext : public interfaces::ResearcherContext
{
public:
    // Canned responses (set by the test before use).
    interfaces::ResearcherSnapshot m_snapshot;
    std::vector<interfaces::PoolRow> m_pools;

    // Recorded calls.
    int m_switch_mode_calls = 0;

    interfaces::ResearcherSnapshot snapshot() override { return m_snapshot; }
    std::optional<interfaces::ResearcherSnapshot> trySnapshot() override { return m_snapshot; }
    bool outOfSync() override { return m_snapshot.out_of_sync; }
    bool hasV3CapableProjects() override { return false; }
    std::vector<interfaces::ResearcherProjectRow> projects(bool /*extended*/) override { return {}; }
    std::vector<interfaces::WhitelistProject> whitelistProjects() override { return {}; }
    std::vector<interfaces::PoolRow> activePools() override { return m_pools; }
    int maxProjectNameLength() override { return 0; }
    int maxProjectUrlLength() override { return 0; }
    std::vector<interfaces::WhitelistProject> v3CapableProjects() override { return {}; }

    bool switchMode(interfaces::ResearcherMode /*mode*/, const std::string& /*email*/) override
    {
        ++m_switch_mode_calls;
        return true;
    }

    interfaces::BeaconAdvertiseResult advertiseBeacon() override { return {}; }
    std::string generateBeaconKeyForV3() override { return {}; }
    interfaces::BeaconAdvertiseResult advertiseBeaconV3(const std::string& /*ownership_proof_xml*/) override { return {}; }
    void reload() override {}

    // Notification delivery is not exercised here; see the note on
    // FakeSideStakeManager's handlers.
    std::unique_ptr<interfaces::Handler> handleResearcherChanged(interfaces::ResearcherChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

    std::unique_ptr<interfaces::Handler> handleBeaconChanged(interfaces::BeaconChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

    std::unique_ptr<interfaces::Handler> handleAccrualChanged(interfaces::AccrualChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

    std::unique_ptr<interfaces::Handler> handleBlocksChanged(interfaces::BlocksChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
};

//! Fake interfaces::Node: holds the read-write settings in an in-memory map and
//! records every changeSettings batch, so a test can assert what a model would
//! have written to gridcoinsettings.json (changeSettings is how
//! OptionsModel::setData persists a core setting). getSettingStr and isSettingSet
//! match the node; getSettingBool and getSettingInt are approximations (the node
//! reads "true"/"false" as false and parses integers leniently). Everything else
//! answers with an empty default.
class FakeNode : public interfaces::Node
{
public:
    // The effective settings, by name without the leading dash (set by the test).
    std::map<std::string, std::string> m_settings;

    // Recorded calls.
    std::vector<std::vector<std::pair<std::string, std::string>>> m_change_calls;

    std::string getSettingStr(const std::string& name, const std::string& default_val) override
    {
        const auto it = m_settings.find(name);
        return it == m_settings.end() ? default_val : it->second;
    }

    bool isSettingSet(const std::string& name) override { return m_settings.count(name) != 0; }

    bool getSettingBool(const std::string& name, bool default_val) override
    {
        const auto it = m_settings.find(name);
        return it == m_settings.end() ? default_val : it->second != "0";
    }

    int64_t getSettingInt(const std::string& name, int64_t default_val) override
    {
        const auto it = m_settings.find(name);
        return it == m_settings.end() ? default_val : std::stoll(it->second);
    }

    //! Applies the batch as the node does: an empty value erases the setting.
    interfaces::SettingChangeResult changeSettings(
        const std::vector<std::pair<std::string, std::string>>& settings) override
    {
        m_change_calls.push_back(settings);
        for (const auto& [name, value] : settings) {
            if (value.empty()) {
                m_settings.erase(name);
            } else {
                m_settings[name] = value;
            }
        }
        interfaces::SettingChangeResult result;
        result.ok = true;
        return result;
    }

    int getNodeCount() override { return 0; }
    uint64_t getTotalBytesRecv() override { return 0; }
    uint64_t getTotalBytesSent() override { return 0; }
    int getNumBlocks() override { return 0; }
    uint256 getBestBlockHash() override { return uint256(); }
    int64_t getLastBlockTime() override { return 0; }
    int getNumBlocksOfPeers() override { return 0; }
    std::optional<int> tryGetNumBlocksOfPeers() override { return std::nullopt; }
    double getDifficulty() override { return 0.0; }
    bool isInitialBlockDownload() override { return false; }
    bool isOutOfSyncByAge() override { return false; }
    std::string getWarnings() override { return {}; }
    std::string getClientVersion() override { return {}; }
    bool isTestNet() override { return false; }
    bool isMainNet() override { return false; }
    void startShutdown() override {}
    interfaces::LatestVersionInfo checkForLatestUpdate() override { return {}; }
    std::vector<interfaces::DiagnosticResult> runDiagnostics() override { return {}; }
    double getBlockDifficulty(uint32_t /*target_bits*/) override { return 0.0; }
    std::string getAlertStatusBarMessage(const uint256& /*hash*/) override { return {}; }
    std::vector<interfaces::BannedNode> getBanned() override { return {}; }
    std::vector<interfaces::PeerInfo> getPeers() override { return {}; }
    void banNode(int64_t /*node_id*/, int64_t /*ban_time_seconds*/) override {}
    bool unban(const std::string& /*subnet*/) override { return false; }
    bool disconnectNode(int64_t /*node_id*/) override { return false; }
    interfaces::RpcConsoleResult executeRpcConsoleCommand(const std::string& /*method*/,
                                                          const std::vector<std::string>& /*args*/) override
    {
        return {};
    }
    std::vector<std::string> listRpcCommands() override { return {}; }
    interfaces::ScraperConvergenceSnapshot getScraperConvergenceSnapshot() override { return {}; }

    // The subscription callbacks are dropped, as in the fakes above.
    std::unique_ptr<interfaces::Handler> handleRwSettingsUpdated(RwSettingsUpdatedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleInitShutdown(InitShutdownFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleNotifyBlocksChanged(NotifyBlocksChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleNotifyNumConnectionsChanged(
        NotifyNumConnectionsChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleBannedListChanged(BannedListChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleNotifyAlertChanged(NotifyAlertChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleMinerStatusChanged(MinerStatusChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handlePSGTPoolChanged(PSGTPoolChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleNotifyScraperEvent(NotifyScraperEventFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
};

} // namespace qt_test

#endif // BITCOIN_QT_TEST_INTERFACEFAKES_H
