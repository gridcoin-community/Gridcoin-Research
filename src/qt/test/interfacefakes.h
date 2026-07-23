// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_INTERFACEFAKES_H
#define BITCOIN_QT_TEST_INTERFACEFAKES_H

#include "interfaces/handler.h"
#include "interfaces/sidestake.h"

#include <memory>
#include <string>

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

} // namespace qt_test

#endif // BITCOIN_QT_TEST_INTERFACEFAKES_H
