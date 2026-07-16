// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_SIDESTAKE_H
#define GRIDCOIN_INTERFACES_SIDESTAKE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace interfaces {

class Handler;

//! One row of the sidestake table as pointer-free value data (Phase 1d-ii).
//! Replaces SideStakeTableModel's former handoff of a raw GRC::SideStake*
//! through QModelIndex::internalPointer; every field the table renders or sorts
//! on is precomputed on the node side.
struct SideStakeEntry
{
    //! Encoded destination address (EncodeDestination(GetDestination())).
    std::string address;

    //! Allocation as a percentage in [0, 100] (GetAllocation().ToPercent()).
    double allocation_percent = 0.0;

    //! Free-text description (already CSV-sanitized in the registry).
    std::string description;

    //! Human-readable status (StatusToString()).
    std::string status;

    //! Mandatory (contract) sidestake — the GUI blocks editing/deletion of these.
    bool is_mandatory = false;

    //! Sort key for the Status column: mandatory statuses keep their enum value,
    //! local statuses are shifted above the mandatory range, reproducing the
    //! former SideStakeLessThan ordering without exposing the status enums.
    int status_sort_key = 0;
};

//! A snapshot of the active sidestake table together with the local sidestake
//! revision it was read at (design §4.4 versioned refetch). The revision is read
//! just before the entries, so an interleaving local mutation only makes it
//! conservatively low (the GUI over-refetches at worst, never goes stale).
struct SideStakeSnapshot
{
    std::vector<SideStakeEntry> entries;
    uint64_t local_revision = 0;
};

//! Outcome of an add/edit/delete command, mirroring the former
//! SideStakeTableModel::EditStatus.
enum class SideStakeEditStatus
{
    OK,                  //!< Applied.
    NO_CHANGES,          //!< The value matched the existing entry; nothing done.
    INVALID_ADDRESS,     //!< Address does not decode to a valid destination.
    DUPLICATE_ADDRESS,   //!< A local sidestake for this address already exists.
    INVALID_ALLOCATION,  //!< Allocation negative or would push the total past 100%.
    INVALID_DESCRIPTION, //!< Description contains characters outside SAFE_CHARS_CSV.
};

//! Result of a mutating command. \p local_revision is the post-mutation local
//! sidestake revision (design §4.4 read-your-writes): the GUI advances its
//! high-water mark to it and refetches immediately rather than waiting for a
//! notification it may legitimately drop. \p address is the encoded address on a
//! successful add.
struct SideStakeEditResult
{
    SideStakeEditStatus status = SideStakeEditStatus::OK;
    std::string address;
    uint64_t local_revision = 0;
};

//! Called when the read-write settings change (uiInterface.RwSettingsUpdated).
//! Payload-free: the consumer must read the current revision via localRevision()
//! when handling this, NOT capture one at notification time. RwSettingsUpdated is
//! a shared signal and the node's own local-sidestake reload runs as a separate
//! slot whose order relative to this subscription is not guaranteed, so an
//! emission-time revision can predate the reload. Reading localRevision() on the
//! consumer's thread (after the whole synchronous emission, reload included, has
//! completed) yields the post-reload value and keeps the §4.4 high-water logic
//! correct regardless of slot order.
using RwSettingsUpdatedFn = std::function<void()>;

//! Called when a mandatory (contract) sidestake entry changes
//! (uiInterface.MandatorySideStakeChanged) — payload-free; the GUI refetches the
//! unified table. Mandatory changes are rare and already block-height ordered, so
//! no coalescing revision is carried (unlike the local side).
using MandatorySideStakeChangedFn = std::function<void()>;

//! The local sidestake registry boundary (Phase 1d-ii). Hands the GUI value-type
//! rows and command-style mutations, so no GUI code holds a GRC::SideStake or
//! calls GetSideStakeRegistry() directly. Every query and command runs under the
//! registry's internal lock on the node side, and all input validation
//! (address, allocation total, description sanitization, duplicates) lives here
//! rather than in the table model.
class SideStakeManager
{
public:
    virtual ~SideStakeManager() = default;

    //! The active sidestake entries (mandatory first, then local) as value rows,
    //! with the local sidestake revision they were read at. This is the single
    //! unified list the GUI table renders; is_mandatory distinguishes the two
    //! kinds for edit/delete gating.
    virtual SideStakeSnapshot entries() = 0;

    //! The current local sidestake revision (design §4.4) — read on a
    //! notification to compare against the GUI's high-water mark.
    virtual uint64_t localRevision() = 0;

    //! Add a local sidestake. \p allocation_percent is in percent [0, 100].
    virtual SideStakeEditResult addLocal(const std::string& address,
                                         double allocation_percent,
                                         const std::string& description) = 0;

    //! Change an existing local sidestake's allocation (percent [0, 100]).
    virtual SideStakeEditResult setAllocation(const std::string& address,
                                              double allocation_percent) = 0;

    //! Change an existing local sidestake's description.
    virtual SideStakeEditResult setDescription(const std::string& address,
                                               const std::string& description) = 0;

    //! Delete a local sidestake by address. Mandatory entries are refused.
    virtual SideStakeEditResult deleteLocal(const std::string& address) = 0;

    //! Subscribe to the versioned RwSettingsUpdated notification for local
    //! sidestake changes (design §4.4).
    virtual std::unique_ptr<Handler> handleRwSettingsUpdated(RwSettingsUpdatedFn fn) = 0;

    //! Subscribe to mandatory (contract) sidestake changes — a payload-free
    //! refetch trigger. Together with handleRwSettingsUpdated this keeps the
    //! single unified table fresh for both kinds of entry, without polling.
    virtual std::unique_ptr<Handler> handleMandatorySideStakeChanged(MandatorySideStakeChangedFn fn) = 0;
};

//! Return an in-process SideStakeManager over the global sidestake registry.
std::unique_ptr<SideStakeManager> MakeSideStakeManager();

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_SIDESTAKE_H
