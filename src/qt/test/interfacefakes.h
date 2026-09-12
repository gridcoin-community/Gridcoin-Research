// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_INTERFACEFAKES_H
#define BITCOIN_QT_TEST_INTERFACEFAKES_H

#include "interfaces/handler.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"

#include <memory>
#include <optional>
#include <string>
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

} // namespace qt_test

#endif // BITCOIN_QT_TEST_INTERFACEFAKES_H
