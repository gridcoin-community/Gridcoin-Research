// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_INTERFACEFAKES_H
#define BITCOIN_QT_TEST_INTERFACEFAKES_H

#include "interfaces/file_transfer.h"
#include "interfaces/gui_state.h"
#include "interfaces/handler.h"
#include "interfaces/mrc.h"
#include "interfaces/node.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_source.h"
#include "interfaces/wallet_tx_source.h"
#include "util/strencodings.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
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
//! With m_needs_elevation set, every mutating command answers ELEVATION_REQUIRED
//! while m_elevated is false, as the node does without a Settings capability.
class FakeSideStakeManager : public interfaces::SideStakeManager
{
public:
    // Canned responses (set by the test before use).
    interfaces::SideStakeSnapshot m_snapshot;
    interfaces::SideStakeEditResult m_edit_result;
    bool m_needs_elevation = false;
    bool m_elevated = false;

    // Recorded calls.
    int m_entries_calls = 0;
    int m_edit_calls = 0;
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
        return EditResult();
    }

    interfaces::SideStakeEditResult setAllocation(const std::string& /*address*/,
                                                  double /*allocation_percent*/) override
    {
        return EditResult();
    }

    interfaces::SideStakeEditResult setDescription(const std::string& /*address*/,
                                                   const std::string& /*description*/) override
    {
        return EditResult();
    }

    interfaces::SideStakeEditResult deleteLocal(const std::string& address) override
    {
        m_last_deleted_address = address;
        return EditResult();
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

private:
    //! Counts the command and answers it.
    interfaces::SideStakeEditResult EditResult()
    {
        ++m_edit_calls;

        if (m_needs_elevation && !m_elevated) {
            interfaces::SideStakeEditResult refused;
            refused.status = interfaces::SideStakeEditStatus::ELEVATION_REQUIRED;
            return refused;
        }

        return m_edit_result;
    }
};

//! Fake interfaces::ResearcherContext: serves a canned snapshot, pool list and project list,
//! answers every command with a default result, and counts mode switches so a
//! test can tell how many times a wizard page was entered.
class FakeResearcherContext : public interfaces::ResearcherContext
{
public:
    // Canned responses (set by the test before use).
    interfaces::ResearcherSnapshot m_snapshot;
    std::vector<interfaces::PoolRow> m_pools;
    std::vector<interfaces::ResearcherProjectRow> m_projects;

    // Recorded calls. The model reads the snapshot from the pushed state and
    // takes none from here, so these stay 0. reload() runs on a worker thread
    // in the model, so its count is atomic.
    int m_switch_mode_calls = 0;
    int m_snapshot_calls = 0;
    int m_try_snapshot_calls = 0;
    int m_projects_calls = 0;
    std::atomic<int> m_reload_calls{0};

    interfaces::ResearcherSnapshot snapshot() override
    {
        ++m_snapshot_calls;
        return m_snapshot;
    }
    std::optional<interfaces::ResearcherSnapshot> trySnapshot() override
    {
        ++m_try_snapshot_calls;
        return m_snapshot;
    }
    bool outOfSync() override { return m_snapshot.out_of_sync; }
    bool hasV3CapableProjects() override { return false; }
    std::vector<interfaces::ResearcherProjectRow> projects(bool /*extended*/) override
    {
        ++m_projects_calls;
        return m_projects;
    }
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
    void reload() override { ++m_reload_calls; }

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

//! Fake interfaces::MRC: counts snapshot(), submit() and submitConfirmed() so a
//! test can show the model made no core read and sent nothing it should not,
//! records the last confirmed terms, and answers submitConfirmed with a scripted
//! result. submitConfirmed runs on a worker thread in the model, so the
//! recorded values are atomics.
class FakeMRC : public interfaces::MRC
{
public:
    // Canned response (set by the test before use).
    interfaces::MRCSubmitResult m_confirmed_result;

    // Recorded calls.
    std::atomic<int> m_snapshot_calls{0};
    std::atomic<int> m_submit_calls{0};
    std::atomic<int> m_submit_confirmed_calls{0};
    std::atomic<CAmount> m_last_confirmed_fee{0};
    std::atomic<CAmount> m_last_confirmed_net{0};
    std::atomic<CAmount> m_last_fee_boost{0};

    interfaces::MRCSnapshot snapshot(CAmount /*fee_boost*/, bool /*wallet_locked*/,
                                     bool /*researcher_eligible*/) override
    {
        ++m_snapshot_calls;
        return {};
    }

    interfaces::MRCSubmitResult submit(CAmount /*fee_boost*/, bool /*wallet_locked*/) override
    {
        ++m_submit_calls;
        return {};
    }

    interfaces::MRCSubmitResult submitConfirmed(CAmount fee_boost, bool /*wallet_locked*/,
                                                CAmount confirmed_fee, CAmount confirmed_net) override
    {
        m_last_fee_boost = fee_boost;
        m_last_confirmed_fee = confirmed_fee;
        m_last_confirmed_net = confirmed_net;
        ++m_submit_confirmed_calls;
        return m_confirmed_result;
    }

    bool isOutOfSync() override { return false; }

    std::unique_ptr<interfaces::Handler> handleMRCChanged(interfaces::MRCChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
};

//! Fake interfaces::Node: holds the read-write settings in an in-memory map and
//! records every changeSettings batch, so a test can assert what a model would
//! have written to gridcoinsettings.json (changeSettings is how
//! OptionsModel::setData persists a core setting). getSettingStr, getSettingInt
//! and isSettingSet match the node for the string values held here;
//! getSettingBool is an approximation (the node reads "true"/"false" as false).
//! The two console calls are recorded and answer canned results. Everything
//! else answers with an empty default. m_fail_changes makes changeSettings fail
//! without applying anything.
class FakeNode : public interfaces::Node
{
public:
    // The effective settings, by name without the leading dash (set by the test).
    std::map<std::string, std::string> m_settings;

    // startShutdown() calls, and what each call does besides (set by the test).
    int m_start_shutdown_calls{0};
    std::function<void()> m_on_start_shutdown;

    // Recorded calls.
    std::vector<std::vector<std::pair<std::string, std::string>>> m_change_calls;

    // When set, changeSettings records the batch, applies nothing and reports
    // failure, as the node does when it rejects a value or cannot store it.
    bool m_fail_changes{false};

    // Counts the chain-state reads a model makes of the node (the ones that
    // take cs_main in the real node), so a test can show a model made none.
    int m_chain_reads = 0;
    // A batch naming one of these is refused for want of the wallet passphrase
    // while m_elevated is false, as the node refuses an Elevate-class setting
    // without a Settings capability: nothing applied, elevation_required set.
    // As in the node, an erase of one that is not set changes nothing and is
    // not refused.
    std::set<std::string> m_needs_elevation;
    bool m_elevated{false};

    // The console commands run: the method and arguments of each
    // executeRpcConsoleCommand call, and each executeRpcConsoleSecretCommand
    // call with its passphrases read back as plain strings. A passphrase's
    // capacity is kept as well: one larger than an empty SecureString's means
    // its bytes were in the string's secure buffer, not its inline storage.
    std::vector<std::pair<std::string, std::vector<std::string>>> m_console_calls;
    struct SecretCall {
        std::string method;
        std::vector<std::string> passphrases;
        std::vector<std::string> args;
        std::vector<size_t> passphrase_capacities;
    };
    std::vector<SecretCall> m_secret_calls;

    // What the two console calls answer (set by the test).
    interfaces::RpcConsoleResult m_console_result{};
    interfaces::RpcConsoleResult m_secret_result{};

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
        if (it == m_settings.end()) return default_val;
        // As ArgsManager::GetArg: a value that does not parse reads as zero.
        int64_t value{0};
        if (!ParseInt64(it->second, &value)) return 0;
        return value;
    }

    //! Applies the batch as the node does: an empty value erases the setting.
    interfaces::SettingChangeResult changeSettings(
        const std::vector<std::pair<std::string, std::string>>& settings) override
    {
        m_change_calls.push_back(settings);
        if (m_fail_changes) {
            interfaces::SettingChangeResult failure;
            failure.ok = false;
            return failure;
        }
        for (const auto& setting : settings) {
            const bool erases_unset = setting.second.empty() && !m_settings.count(setting.first);
            if (!m_elevated && m_needs_elevation.count(setting.first) && !erases_unset) {
                interfaces::SettingChangeResult refused;
                refused.ok = false;
                refused.invalid_input = true;
                refused.elevation_required = true;
                refused.error = "Changing " + setting.first + " needs the wallet passphrase";
                return refused;
            }
        }
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
    int getNumBlocks() override { ++m_chain_reads; return 0; }
    uint256 getBestBlockHash() override { return uint256(); }
    int64_t getLastBlockTime() override { ++m_chain_reads; return 0; }
    int getNumBlocksOfPeers() override { ++m_chain_reads; return 0; }
    std::optional<int> tryGetNumBlocksOfPeers() override { ++m_chain_reads; return std::nullopt; }
    double getDifficulty() override { ++m_chain_reads; return 0.0; }
    bool isInitialBlockDownload() override { ++m_chain_reads; return false; }
    bool isOutOfSyncByAge() override { return false; }
    std::string getWarnings() override { return {}; }
    std::string getClientVersion() override { return {}; }
    bool isTestNet() override { return false; }
    bool isMainNet() override { return false; }
    interfaces::CorePaths getCorePaths() override
    {
        interfaces::CorePaths paths;
        paths.datadir = "/fake/data";
        paths.debug_log = "/fake/data/debug.log";
        paths.config_file = "/fake/data/gridcoinresearch.conf";
        paths.settings_file = "/fake/data/settings.json";
        paths.wallet_file = "/fake/data/wallet.dat";
        return paths;
    }
    void startShutdown() override
    {
        ++m_start_shutdown_calls;
        if (m_on_start_shutdown) m_on_start_shutdown();
    }
    interfaces::LatestVersionInfo checkForLatestUpdate() override { return {}; }
    std::vector<interfaces::DiagnosticResult> runDiagnostics() override { return {}; }
    double getBlockDifficulty(uint32_t /*target_bits*/) override { ++m_chain_reads; return 0.0; }
    std::string getAlertStatusBarMessage(const uint256& /*hash*/) override { return {}; }
    std::vector<interfaces::BannedNode> getBanned() override { return {}; }
    std::vector<interfaces::PeerInfo> getPeers() override { return {}; }
    void banNode(int64_t /*node_id*/, int64_t /*ban_time_seconds*/) override {}
    bool unban(const std::string& /*subnet*/) override { return false; }
    bool disconnectNode(int64_t /*node_id*/) override { return false; }
    interfaces::RpcConsoleResult executeRpcConsoleCommand(const std::string& method,
                                                          const std::vector<std::string>& args) override
    {
        m_console_calls.emplace_back(method, args);
        return m_console_result;
    }
    interfaces::RpcConsoleResult executeRpcConsoleSecretCommand(const std::string& method,
                                                                const std::vector<SecureString>& passphrases,
                                                                const std::vector<std::string>& args) override
    {
        SecretCall call;
        call.method = method;
        call.args = args;
        for (const SecureString& passphrase : passphrases) {
            call.passphrases.emplace_back(passphrase.begin(), passphrase.end());
            call.passphrase_capacities.push_back(passphrase.capacity());
        }
        m_secret_calls.push_back(std::move(call));
        return m_secret_result;
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

//! Fake interfaces::GuiState: serves a settable snapshot from current(), and
//! keeps the fns a model registers so a test can push a snapshot (Push) or
//! answer a quote (AnswerQuote) to them, synchronously on the test's thread,
//! where the node's cache would call them from its timer thread. Counts the
//! requests the model makes and keeps the last quote request's arguments.
//! Unlike the node, it computes nothing on a subscription and answers no
//! request by itself: the test does. A Handler it returns must not outlive it.
class FakeGuiState : public interfaces::GuiState
{
public:
    // Canned response (set by the test, or by Push).
    interfaces::GuiStateSnapshot m_snapshot;

    // Recorded calls.
    int m_request_update_calls = 0;
    int m_quote_requests = 0;
    CAmount m_last_quote_fee_boost = 0;
    uint64_t m_last_quote_request_id = 0;

    interfaces::GuiStateSnapshot current() override { return m_snapshot; }

    std::unique_ptr<interfaces::Handler> handleChanged(ChangedFn fn) override
    {
        const int id = m_next_handler_id++;
        m_changed_fns.emplace(id, std::move(fn));
        return interfaces::MakeCleanupHandler([this, id] { m_changed_fns.erase(id); });
    }

    void requestUpdate() override { ++m_request_update_calls; }

    void requestMRCQuote(CAmount fee_boost, uint64_t request_id) override
    {
        ++m_quote_requests;
        m_last_quote_fee_boost = fee_boost;
        m_last_quote_request_id = request_id;
    }

    std::unique_ptr<interfaces::Handler> handleMRCQuote(MRCQuoteFn fn) override
    {
        const int id = m_next_handler_id++;
        m_quote_fns.emplace(id, std::move(fn));
        return interfaces::MakeCleanupHandler([this, id] { m_quote_fns.erase(id); });
    }

    //! Make \p snapshot the current one and deliver it to every registered
    //! handleChanged fn, as the node's cache publishes one.
    void Push(const interfaces::GuiStateSnapshot& snapshot)
    {
        m_snapshot = snapshot;

        // A copy: a fn may unsubscribe while it runs.
        const std::map<int, ChangedFn> fns = m_changed_fns;
        for (const auto& entry : fns) entry.second(snapshot);
    }

    //! Deliver a quote answer to every registered handleMRCQuote fn.
    void AnswerQuote(uint64_t request_id, const interfaces::MRCSnapshot& quote)
    {
        const std::map<int, MRCQuoteFn> fns = m_quote_fns;
        for (const auto& entry : fns) entry.second(request_id, quote);
    }

    //! The handleChanged subscriptions alive.
    size_t ChangedSubscribers() const { return m_changed_fns.size(); }

private:
    int m_next_handler_id = 0;
    std::map<int, ChangedFn> m_changed_fns;
    std::map<int, MRCQuoteFn> m_quote_fns;
};

//! Fake interfaces::PSGTPoolContext: serves a canned poolStatus() and counts
//! the calls, so a test can show a page took its status from the push and made
//! no call. Everything else answers with an empty default.
class FakePSGTPoolContext : public interfaces::PSGTPoolContext
{
public:
    // Canned response (set by the test).
    interfaces::PSGTPoolStatus m_status;

    // Recorded calls.
    int m_pool_status_calls = 0;

    interfaces::PSGTPoolStatus poolStatus() override
    {
        ++m_pool_status_calls;
        return m_status;
    }

    std::vector<interfaces::PSGTPoolRow> entries() override { return {}; }
    interfaces::PSGTSignStatus signPoolEntry(const std::string&, std::string&, std::string&) override { return {}; }
    bool removePoolEntry(const std::string&) override { return false; }
    interfaces::PSGTBytes poolEntryPsgt(const std::string&) override { return {}; }
    interfaces::PSGTDescription describePSGT(const interfaces::PSGTBytes&) override { return {}; }
    interfaces::PSGTSignResult signPSGT(const interfaces::PSGTBytes&) override { return {}; }
    interfaces::PSGTCombineResult combinePSGTs(const std::vector<interfaces::PSGTBytes>&) override { return {}; }
    interfaces::PSGTSubmitResult submitPSGTToPool(const interfaces::PSGTBytes&) override { return {}; }
    interfaces::PSGTFinalizeResult finalizeToRawTxHex(const interfaces::PSGTBytes&) override { return {}; }
    interfaces::PSGTDecodeResult decodePSGT(const interfaces::PSGTBytes&) override { return {}; }
    bool walletHasSignature(const interfaces::PSGTBytes&) override { return false; }
    bool walletMustSignRevision(const std::string&) override { return false; }
};

//! Fake interfaces::VotingManager: counts the calls that reach the core's poll
//! state (the ones that take cs_main in the real node), so a test can show a
//! model made none while the wallet is not synced.
class FakeVotingManager : public interfaces::VotingManager
{
public:
    // Recorded calls.
    int m_poll_table_calls = 0;
    int m_current_title_calls = 0;
    int m_latest_poll_time_calls = 0;

    std::vector<interfaces::PollTableItem> buildPollTable(int) override
    {
        ++m_poll_table_calls;
        return {};
    }

    std::string currentPollTitle() override
    {
        ++m_current_title_calls;
        return "title";
    }

    int64_t latestActivePollTime() override
    {
        ++m_latest_poll_time_calls;
        return 0;
    }

    std::vector<interfaces::PollTypeMeta> getPollTypes() override { return {}; }
    bool pollV3Enabled() override { return false; }
    CAmount estimatePollFee() override { return 0; }
    interfaces::VotingSubmitResult submitPoll(const interfaces::PollSubmission&) override { return {}; }
    interfaces::VotingSubmitResult submitVote(const std::string&, const std::vector<uint8_t>&) override { return {}; }
    std::unique_ptr<interfaces::Handler> handleNewPollReceived(interfaces::NewPollReceivedFn) override { return interfaces::MakeCleanupHandler([] {}); }
    std::unique_ptr<interfaces::Handler> handleNewVoteReceived(interfaces::NewVoteReceivedFn) override { return interfaces::MakeCleanupHandler([] {}); }
};

//! Fake interfaces::Wallet, unencrypted and empty: counts tryGetBalances(), so a
//! test can show a wallet model took its balances from the push. Everything else
//! answers with an empty default.
class UnencryptedFakeWallet : public interfaces::Wallet
{
public:
    // Recorded calls.
    int m_try_get_balances_calls = 0;
    int m_lock_wallet_calls = 0;
    //! endElevation() calls: what a WalletModel::UnlockContext makes when released.
    int m_end_elevation_calls = 0;

    bool beginElevation(const SecureString& /*passphrase*/, CapabilityKind /*kind*/) override { return true; }
    void endElevation() override { ++m_end_elevation_calls; }

    bool tryGetBalances(interfaces::WalletBalances&) override
    {
        ++m_try_get_balances_calls;
        return false;
    }

    int64_t getBalance() override { return 0; }
    int64_t getStake() override { return 0; }
    int64_t getUnconfirmedBalance() override { return 0; }
    int64_t getImmatureBalance() override { return 0; }
    int getNumTransactions() override { return 0; }
    interfaces::WalletLockState getLockState() override { return {}; }
    bool isUnlockedForStakingOnly() override { return false; }
    bool getUnlockStakingOnlyFlag() override { return false; }
    bool encryptWallet(const SecureString&) override { return false; }
    bool lockWallet() override
    {
        ++m_lock_wallet_calls;
        return true;
    }
    bool restrictToStakingOnly() override { return false; }
    bool elevateWallet(const SecureString&) override { return false; }
    bool unlockWallet(const SecureString&, bool) override { return false; }
    bool changeWalletPassphrase(const SecureString&, const SecureString&) override { return false; }
    std::vector<std::string> getPreEncryptionBackups() override { return {}; }
    AutoUnlockSetupResult setAutoUnlockSecret(const SecureString&) override { return AutoUnlockSetupResult::Unsupported; }
    AutoUnlockSetupResult clearAutoUnlockSecret(const SecureString&) override { return AutoUnlockSetupResult::Unsupported; }
    AutoUnlockStoreStatus getAutoUnlockStatus() override { return {}; }
    bool getPubKey(const CKeyID&, CPubKey&) override { return false; }
    bool getKeyFromPool(const std::string&, CPubKey&) override { return false; }
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>&) override { return {}; }
    interfaces::CoinControlSummary computeCoinControlSummary(const interfaces::WalletCoinControl&,
                                                             const std::vector<int64_t>&, bool) override { return {}; }
    unsigned int getMaxConsolidationInputs() override { return 0; }
    CAmount getTransactionFeeRate() override { return 0; }
    interfaces::SendCoinsResult sendCoins(const std::vector<interfaces::WalletSendRecipient>&,
                                          const std::optional<interfaces::WalletCoinControl>&, int64_t) override { return {}; }
    bool backupWallet(const std::string&) override { return false; }
    bool backupConfigFile(const std::string&) override { return false; }
    std::unique_ptr<interfaces::FileSource> beginBackup(uint8_t, std::string&) override { return nullptr; }
    std::unique_ptr<interfaces::FileSource> beginConsoleDownload(const std::string&, const std::vector<std::string>&,
                                                                 std::string&) override { return nullptr; }
    std::unique_ptr<interfaces::FileSink> beginConsoleUpload(const std::string&, const std::vector<std::string>&,
                                                             uint64_t, std::string&) override { return nullptr; }
    interfaces::MessageSignStatus signMessage(const std::string&, const std::string&, std::string&) override { return {}; }
    interfaces::MessageVerifyStatus verifyMessage(const std::string&, const std::string&,
                                                  const std::vector<unsigned char>&) override { return {}; }
    std::vector<interfaces::WalletAddress> getAddresses() override { return {}; }
    bool getAddressLabel(const std::string&, std::string&) override { return false; }
    bool isMine(const std::string&) override { return false; }
    void setAddressBook(const std::string&, const std::string&) override {}
    bool delAddressBook(const std::string&) override { return false; }
    bool getNewReceiveAddress(std::string&) override { return false; }
    bool getNewReceiveAddressWithLabel(const std::string&, std::string&) override { return false; }
    std::vector<std::string> getUnbookedReceiveAddresses() override { return {}; }
    std::unique_ptr<interfaces::Handler> handleStatusChanged(StatusChangedFn) override { return interfaces::MakeCleanupHandler([] {}); }
    std::unique_ptr<interfaces::Handler> handleAddressBookChanged(AddressBookChangedFn) override { return interfaces::MakeCleanupHandler([] {}); }
    std::unique_ptr<interfaces::Handler> handleTransactionChanged(TransactionChangedFn) override { return interfaces::MakeCleanupHandler([] {}); }
};

//! Fake interfaces::Wallet: an encrypted wallet unlocked for staking only,
//! with nothing in it, for the passphrase prompt. beginElevation(),
//! changeWalletPassphrase() and the two autounlock calls block, as a passphrase
//! attempt waiting for its turn does, until the test releases them or until the attempt is cancelled on its
//! thread (PassphraseAttemptCancel), which is how the wallet's own wait ends in
//! the monolithic build. Everything else answers with an empty default.
class FakeWallet : public interfaces::Wallet
{
public:
    // Set by the test. m_release_result is written before m_release is set.
    std::atomic<bool> m_release{false};
    bool m_release_result{true};
    //! Keep waiting for the release even once cancelled, as a check already
    //! under way does, or as one waiting in the node's process does in the
    //! split build, where the cancel cannot reach it.
    std::atomic<bool> m_ignore_cancel{false};
    //! What encryptWallet() answers.
    std::atomic<bool> m_encrypt_result{false};
    //! What getPreEncryptionBackups() answers.
    std::vector<std::string> m_pre_encryption_backups;
    //! What setAutoUnlockSecret() and clearAutoUnlockSecret() answer once
    //! released (WrongPassphrase when cancelled), and what getAutoUnlockStatus()
    //! answers.
    AutoUnlockSetupResult m_autounlock_result{AutoUnlockSetupResult::Ok};
    AutoUnlockStoreStatus m_autounlock_status{};
    //! The two autounlock calls throw once released, as a call does whose
    //! connection to the core closes under it.
    std::atomic<bool> m_autounlock_throws{false};

    // Observed.
    std::atomic<bool> m_waiting{false};
    std::atomic<bool> m_returned{false};
    std::atomic<bool> m_saw_cancel{false};
    std::atomic<int> m_end_elevation_calls{0};
    std::atomic<int> m_set_autounlock_calls{0};
    std::atomic<int> m_clear_autounlock_calls{0};
    //! The thread the last passphrase call ran on, written before m_waiting.
    std::atomic<std::thread::id> m_call_thread{};

    bool beginElevation(const SecureString& /*passphrase*/, CapabilityKind /*kind*/) override { return Wait(); }

    void endElevation() override { ++m_end_elevation_calls; }

    bool changeWalletPassphrase(const SecureString& /*old_passphrase*/,
                                const SecureString& /*new_passphrase*/) override
    {
        return Wait();
    }

    AutoUnlockSetupResult setAutoUnlockSecret(const SecureString& /*passphrase*/) override
    {
        ++m_set_autounlock_calls;
        return AutoUnlockAnswer();
    }

    AutoUnlockSetupResult clearAutoUnlockSecret(const SecureString& /*passphrase*/) override
    {
        ++m_clear_autounlock_calls;
        return AutoUnlockAnswer();
    }

    AutoUnlockStoreStatus getAutoUnlockStatus() override { return m_autounlock_status; }

    interfaces::WalletLockState getLockState() override
    {
        interfaces::WalletLockState state;
        state.crypted = true;
        state.unlocked_for_staking_only = true;
        state.staking_only_flag = true;
        return state;
    }

    bool isUnlockedForStakingOnly() override { return true; }
    bool getUnlockStakingOnlyFlag() override { return true; }

    int64_t getBalance() override { return 0; }
    int64_t getStake() override { return 0; }
    int64_t getUnconfirmedBalance() override { return 0; }
    int64_t getImmatureBalance() override { return 0; }
    CAmount getTransactionFeeRate() override { return 0; }
    bool tryGetBalances(interfaces::WalletBalances& /*balances*/) override { return false; }
    int getNumTransactions() override { return 0; }
    bool encryptWallet(const SecureString& /*passphrase*/) override { return m_encrypt_result; }
    std::vector<std::string> getPreEncryptionBackups() override { return m_pre_encryption_backups; }
    bool lockWallet() override { return false; }
    bool restrictToStakingOnly() override { return false; }
    bool elevateWallet(const SecureString& /*passphrase*/) override { return false; }
    bool unlockWallet(const SecureString& /*passphrase*/, bool /*staking_only*/) override { return false; }
    bool getPubKey(const CKeyID& /*address*/, CPubKey& /*pub_key_out*/) override { return false; }
    bool getKeyFromPool(const std::string& /*label*/, CPubKey& /*pub_key_out*/) override { return false; }
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>& /*outpoints*/) override
    {
        return {};
    }
    interfaces::CoinControlSummary computeCoinControlSummary(const interfaces::WalletCoinControl& /*selection*/,
                                                             const std::vector<int64_t>& /*recipient_amounts*/,
                                                             bool /*subtract_fee_from_amount*/) override
    {
        return {};
    }
    unsigned int getMaxConsolidationInputs() override { return 0; }
    interfaces::SendCoinsResult sendCoins(const std::vector<interfaces::WalletSendRecipient>& /*recipients*/,
                                          const std::optional<interfaces::WalletCoinControl>& /*coin_control*/,
                                          int64_t /*accepted_fee*/) override
    {
        return {};
    }
    bool backupWallet(const std::string& /*dest*/) override { return false; }
    bool backupConfigFile(const std::string& /*dest*/) override { return false; }
    std::unique_ptr<interfaces::FileSource> beginBackup(uint8_t /*items*/, std::string& /*error_out*/) override
    {
        return nullptr;
    }
    std::unique_ptr<interfaces::FileSource> beginConsoleDownload(const std::string& /*method*/,
                                                                 const std::vector<std::string>& /*args*/,
                                                                 std::string& /*error_out*/) override
    {
        return nullptr;
    }
    std::unique_ptr<interfaces::FileSink> beginConsoleUpload(const std::string& /*method*/,
                                                             const std::vector<std::string>& /*args*/,
                                                             uint64_t /*size*/, std::string& /*error_out*/) override
    {
        return nullptr;
    }
    interfaces::MessageSignStatus signMessage(const std::string& /*address*/, const std::string& /*message*/,
                                              std::string& /*signature_out*/) override
    {
        return interfaces::MessageSignStatus::KeyNotAvailable;
    }
    interfaces::MessageVerifyStatus verifyMessage(const std::string& /*address*/, const std::string& /*message*/,
                                                  const std::vector<unsigned char>& /*signature*/) override
    {
        return interfaces::MessageVerifyStatus::RecoverFailed;
    }
    std::vector<interfaces::WalletAddress> getAddresses() override { return {}; }
    bool getAddressLabel(const std::string& /*address*/, std::string& /*label_out*/) override { return false; }
    bool isMine(const std::string& /*address*/) override { return false; }
    void setAddressBook(const std::string& /*address*/, const std::string& /*label*/) override {}
    bool delAddressBook(const std::string& /*address*/) override { return false; }
    bool getNewReceiveAddress(std::string& /*address_out*/) override { return false; }
    bool getNewReceiveAddressWithLabel(const std::string& /*label*/, std::string& /*address_out*/) override
    {
        return false;
    }
    std::vector<std::string> getUnbookedReceiveAddresses() override { return {}; }

    // Notification delivery is not exercised here; see the note on
    // FakeSideStakeManager's handlers.
    std::unique_ptr<interfaces::Handler> handleStatusChanged(StatusChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleAddressBookChanged(AddressBookChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleTransactionChanged(TransactionChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

private:
    //! What the autounlock calls answer once their wait is over.
    AutoUnlockSetupResult AutoUnlockAnswer()
    {
        const bool released = Wait();
        if (m_autounlock_throws) throw std::runtime_error("the connection to the core closed");
        return released ? m_autounlock_result : AutoUnlockSetupResult::WrongPassphrase;
    }

    //! A passphrase attempt's wait: until released, with m_release_result, or
    //! until cancelled, with false.
    bool Wait()
    {
        m_call_thread = std::this_thread::get_id();
        m_waiting = true;
        bool result{false};

        // Ten seconds at most, so that a test that never releases it still ends.
        for (int i = 0; i < 1000; ++i) {
            if (m_release) {
                result = m_release_result;
                break;
            }

            if (!m_ignore_cancel && PassphraseAttemptCancel::Requested()) {
                m_saw_cancel = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        m_returned = true;
        return result;
    }
};

//! Fake interfaces::Wallet, enough to construct a WalletModel for the settings
//! tests: it reports the lock state the test sets, answers a passphrase at once
//! and counts the capabilities begun and ended. While one lives it sets
//! *m_capability_flag, when the test points that at the flag its FakeNode or
//! FakeSideStakeManager reads (m_elevated), as the node then accepts what needs
//! the passphrase. The console's file transfers begin through the factories the
//! test sets, and each begin is recorded. Every other call answers with an
//! empty default and changes nothing.
class FakeLockStateWallet : public interfaces::Wallet
{
public:
    interfaces::WalletLockState m_lock_state;
    bool* m_capability_flag{nullptr};

    // Recorded calls.
    int m_begin_elevation_calls{0};
    int m_end_elevation_calls{0};
    std::optional<CapabilityKind> m_last_elevation_kind;

    //! What beginConsoleDownload() and beginConsoleUpload() return (set by the
    //! test). Unset, they refuse as the core does a method that is not a row
    //! in that direction.
    std::function<std::unique_ptr<interfaces::FileSource>(const std::string& method,
                                                          const std::vector<std::string>& args,
                                                          std::string& error_out)>
        m_console_download{};
    std::function<std::unique_ptr<interfaces::FileSink>(const std::string& method,
                                                        const std::vector<std::string>& args, uint64_t size,
                                                        std::string& error_out)>
        m_console_upload{};

    //! A console transfer begun. Written on the thread the begin runs on, and
    //! read by the test once the transfer has returned.
    struct ConsoleBegin {
        std::string method{};
        std::vector<std::string> args{};
        uint64_t size{0};
        std::thread::id thread{};
        //! The capabilities begun and ended before this begin.
        int begin_elevation_calls{0};
        int end_elevation_calls{0};
    };
    std::vector<ConsoleBegin> m_console_begins{};

    interfaces::WalletLockState getLockState() override { return m_lock_state; }
    bool isUnlockedForStakingOnly() override { return m_lock_state.unlocked_for_staking_only; }
    bool getUnlockStakingOnlyFlag() override { return m_lock_state.unlocked_for_staking_only; }

    bool beginElevation(const SecureString& /*passphrase*/, CapabilityKind kind) override
    {
        ++m_begin_elevation_calls;
        m_last_elevation_kind = kind;
        if (m_capability_flag) *m_capability_flag = true;
        return true;
    }

    void endElevation() override
    {
        ++m_end_elevation_calls;
        if (m_capability_flag) *m_capability_flag = false;
    }

    int64_t getBalance() override { return 0; }
    int64_t getStake() override { return 0; }
    int64_t getUnconfirmedBalance() override { return 0; }
    int64_t getImmatureBalance() override { return 0; }
    CAmount getTransactionFeeRate() override { return 0; }
    bool tryGetBalances(interfaces::WalletBalances& /*balances*/) override { return false; }
    int getNumTransactions() override { return 0; }
    bool encryptWallet(const SecureString& /*passphrase*/) override { return false; }
    std::vector<std::string> getPreEncryptionBackups() override { return {}; }
    bool lockWallet() override { return false; }
    bool restrictToStakingOnly() override { return false; }
    bool elevateWallet(const SecureString& /*passphrase*/) override { return false; }
    bool unlockWallet(const SecureString& /*passphrase*/, bool /*staking_only*/) override { return false; }
    bool changeWalletPassphrase(const SecureString& /*old_passphrase*/,
                                const SecureString& /*new_passphrase*/) override
    {
        return false;
    }
    AutoUnlockSetupResult setAutoUnlockSecret(const SecureString& /*passphrase*/) override
    {
        return AutoUnlockSetupResult::Unsupported;
    }
    AutoUnlockSetupResult clearAutoUnlockSecret(const SecureString& /*passphrase*/) override
    {
        return AutoUnlockSetupResult::Unsupported;
    }
    AutoUnlockStoreStatus getAutoUnlockStatus() override { return {}; }
    bool getPubKey(const CKeyID& /*address*/, CPubKey& /*pub_key_out*/) override { return false; }
    bool getKeyFromPool(const std::string& /*label*/, CPubKey& /*pub_key_out*/) override { return false; }
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>& /*outpoints*/) override
    {
        return {};
    }
    interfaces::CoinControlSummary computeCoinControlSummary(const interfaces::WalletCoinControl& /*selection*/,
                                                             const std::vector<int64_t>& /*recipient_amounts*/,
                                                             bool /*subtract_fee_from_amount*/) override
    {
        return {};
    }
    unsigned int getMaxConsolidationInputs() override { return 0; }
    interfaces::SendCoinsResult sendCoins(const std::vector<interfaces::WalletSendRecipient>& /*recipients*/,
                                          const std::optional<interfaces::WalletCoinControl>& /*coin_control*/,
                                          int64_t /*accepted_fee*/) override
    {
        return {};
    }
    bool backupWallet(const std::string& /*dest*/) override { return false; }
    bool backupConfigFile(const std::string& /*dest*/) override { return false; }
    std::unique_ptr<interfaces::FileSource> beginBackup(uint8_t /*items*/, std::string& /*error_out*/) override
    {
        return nullptr;
    }
    std::unique_ptr<interfaces::FileSource> beginConsoleDownload(const std::string& method,
                                                                 const std::vector<std::string>& args,
                                                                 std::string& error_out) override
    {
        RecordConsoleBegin(method, args, 0);
        if (!m_console_download) {
            error_out = "not a file command";
            return nullptr;
        }
        return m_console_download(method, args, error_out);
    }
    std::unique_ptr<interfaces::FileSink> beginConsoleUpload(const std::string& method,
                                                             const std::vector<std::string>& args, uint64_t size,
                                                             std::string& error_out) override
    {
        RecordConsoleBegin(method, args, size);
        if (!m_console_upload) {
            error_out = "not a file command";
            return nullptr;
        }
        return m_console_upload(method, args, size, error_out);
    }
    interfaces::MessageSignStatus signMessage(const std::string& /*address*/, const std::string& /*message*/,
                                              std::string& /*signature_out*/) override
    {
        return interfaces::MessageSignStatus::KeyNotAvailable;
    }
    interfaces::MessageVerifyStatus verifyMessage(const std::string& /*address*/, const std::string& /*message*/,
                                                  const std::vector<unsigned char>& /*signature*/) override
    {
        return interfaces::MessageVerifyStatus::RecoverFailed;
    }
    std::vector<interfaces::WalletAddress> getAddresses() override { return {}; }
    bool getAddressLabel(const std::string& /*address*/, std::string& /*label_out*/) override { return false; }
    bool isMine(const std::string& /*address*/) override { return false; }
    void setAddressBook(const std::string& /*address*/, const std::string& /*label*/) override {}
    bool delAddressBook(const std::string& /*address*/) override { return false; }
    bool getNewReceiveAddress(std::string& /*address_out*/) override { return false; }
    bool getNewReceiveAddressWithLabel(const std::string& /*label*/, std::string& /*address_out*/) override
    {
        return false;
    }
    std::vector<std::string> getUnbookedReceiveAddresses() override { return {}; }

    // The subscription callbacks are dropped, as in the fakes above.
    std::unique_ptr<interfaces::Handler> handleStatusChanged(StatusChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleAddressBookChanged(AddressBookChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleTransactionChanged(TransactionChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

private:
    void RecordConsoleBegin(const std::string& method, const std::vector<std::string>& args, uint64_t size)
    {
        ConsoleBegin begin;
        begin.method = method;
        begin.args = args;
        begin.size = size;
        begin.thread = std::this_thread::get_id();
        begin.begin_elevation_calls = m_begin_elevation_calls;
        begin.end_elevation_calls = m_end_elevation_calls;
        m_console_begins.push_back(std::move(begin));
    }
};

//! Fake interfaces::WalletTxSource with no transactions, so that a WalletModel
//! can be made over UnencryptedFakeWallet, FakeWallet or FakeLockStateWallet: an
//! empty store that answers every query with no rows and records nothing.
class FakeWalletTxSource : public interfaces::WalletTxSource
{
public:
    void registerView(int, GRC::FilterSpec, int, int) override {}
    void unregisterView(int) override {}
    void setViewSort(int, int, int) override {}
    void setViewFilter(int, GRC::FilterSpec) override {}
    void setViewLimit(int, int) override {}
    GRC::RowsResult getRows(int, int, int) override { return {}; }
    GRC::RowsResult getAllRows(int) override { return {}; }
    int rowForKey(int, const uint256&, int) override { return -1; }
    GRC::WalletTxDetail getRowDetail(const uint256&, int) override { return {}; }
    void prime(bool, int64_t) override {}
    std::vector<GRC::WalletEvent> drainEvents(std::size_t) override { return {}; }
    void noteAddressBookChanged(const std::string&, const std::string&) override {}
};

//! Fake interfaces::WalletCoinSource: a synthetic coin source
//! (interfaces::MakeSyntheticCoinSource) whose wallet scan, reloadAndSnapshot(),
//! records the thread it runs on, and can be held until the test releases it
//! or made to throw, as a call across IPC throws when the core drops the
//! connection. Everything else is the synthetic source's.
class FakeCoinSource : public interfaces::WalletCoinSource
{
public:
    // Set by the test.
    std::atomic<bool> m_throw{false};
    std::atomic<bool> m_hold{false};
    std::atomic<bool> m_release{false};

    // Observed.
    std::atomic<int> m_reloads{0};
    //! Scans that have returned or thrown.
    std::atomic<int> m_reloads_ended{0};
    std::atomic<bool> m_reload_waiting{false};
    std::atomic<std::thread::id> m_reload_thread{};

    GRC::CoinGroupsResult reloadAndSnapshot() override
    {
        m_reload_thread = std::this_thread::get_id();
        ++m_reloads;

        struct Ended
        {
            std::atomic<int>& count;
            ~Ended() { ++count; }
        } ended{m_reloads_ended};

        if (m_hold) {
            m_reload_waiting = true;

            // Ten seconds at most, so that a test that never releases it still ends.
            for (int i = 0; i < 1000 && !m_release; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if (m_throw) throw std::runtime_error("the core dropped the connection");

        return m_synthetic->reloadAndSnapshot();
    }

    void registerView(int view_id, GRC::CoinViewMode mode, int sort_column, int sort_order) override
    {
        m_synthetic->registerView(view_id, mode, sort_column, sort_order);
    }
    void unregisterView(int view_id) override { m_synthetic->unregisterView(view_id); }
    void setViewMode(int view_id, GRC::CoinViewMode mode) override { m_synthetic->setViewMode(view_id, mode); }
    void setViewSort(int view_id, int sort_column, int sort_order) override
    {
        m_synthetic->setViewSort(view_id, sort_column, sort_order);
    }
    GRC::CoinRowsResult getRows(int view_id, int first, int count) override
    {
        return m_synthetic->getRows(view_id, first, count);
    }
    GRC::CoinGroupsResult getGroups(int view_id, int first, int count) override
    {
        return m_synthetic->getGroups(view_id, first, count);
    }
    GRC::CoinRowsResult getGroupRows(int view_id, const std::string& group_address, int first, int count) override
    {
        return m_synthetic->getGroupRows(view_id, group_address, first, count);
    }
    std::vector<GRC::CoinGroupInfo> getGroupDirectory() override { return m_synthetic->getGroupDirectory(); }
    std::set<COutPoint> reconcileSelection(std::set<COutPoint> selection) override
    {
        return m_synthetic->reconcileSelection(std::move(selection));
    }
    GRC::CoinSelectionUpdate setSelected(const COutPoint& outpoint, bool selected) override
    {
        return m_synthetic->setSelected(outpoint, selected);
    }
    GRC::CoinBulkSelectionResult selectGroup(const std::string& group_address, bool selected) override
    {
        return m_synthetic->selectGroup(group_address, selected);
    }
    GRC::CoinBulkSelectionResult selectAll(bool selected) override { return m_synthetic->selectAll(selected); }
    GRC::CoinBulkSelectionResult applyValueFilter(bool less_or_equal, int64_t value, uint32_t max_inputs) override
    {
        return m_synthetic->applyValueFilter(less_or_equal, value, max_inputs);
    }
    std::vector<GRC::WalletCoinEvent> drainEvents(std::size_t max_batch) override
    {
        return m_synthetic->drainEvents(max_batch);
    }
    bool consumeNeedsResync() override { return m_synthetic->consumeNeedsResync(); }
    void noteAddressBookChanged(const std::string& address, const std::string& label) override
    {
        m_synthetic->noteAddressBookChanged(address, label);
    }

private:
    const std::shared_ptr<interfaces::WalletCoinSource> m_synthetic{interfaces::MakeSyntheticCoinSource(1, 1)};
};

} // namespace qt_test

#endif // BITCOIN_QT_TEST_INTERFACEFAKES_H
