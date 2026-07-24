// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_COINVIEWS_H
#define GRIDCOIN_WALLET_COINVIEWS_H

#include "interfaces/wallet_coin_channel.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

//!
//! \file coinviews.h
//!
//! Qt-free grouped index core for the windowed coin-control selection model
//! (issue #3183) — the coin channel's analogue of the per-view GRC::Cursor,
//! extended with the tree dimension: besides a flat sorted index it maintains
//! the group DIRECTORY (address rows ordered by the parent sort) and a sorted
//! member index per group, plus the server-side aggregates that let a consumer
//! render parent rows — count, total, selection tristate — without
//! materializing children.
//!
//! The class holds no records, no locks and no Qt types; the store-worker
//! drives it under cs_store, and the same logic is unit-tested against
//! synthetic record tables in the ENABLE_GUI=OFF configuration CI exercises
//! (the windowed tx-table risk-control discipline). It is parameterized on a
//! projector callable RecordFn(absidx) -> const CoinRecord& reading the
//! backing table; callers MUST mutate the backing table BEFORE calling the
//! corresponding apply* method (applyRemove is the one exception — see its
//! comment).
//!
//! Views: up to a handful of consumers (the coin-control dialog and the
//! consolidate wizard page) register independently, each with its own display
//! mode, sort, epoch and floor. Ordered indices are per-view (two views can
//! sort differently); group MEMBERSHIP and aggregates are view-independent
//! and maintained once.
//!
//! Event bookkeeping: the deltas returned by the apply* methods are pushed as
//! events by the store, which feeds the queue-assigned seqnos back through
//! noteScopeEvent / noteDirectoryEvent / noteBroadcast. highWater() then
//! answers the scope-local-with-floor contract documented on
//! GRC::CoinRowsResult: max(last seqno emitted for the scope, the view's
//! broadcast floor). Keeping this arithmetic here — not in the store — puts
//! the design's central novel invariant in the Qt-free tested core.
//!

namespace GRC {

//!
//! \brief One structural change to a view's row universe, in positional
//! coordinates of the affected scope.
//!
//! scope names the flat list ("") or a group's child list (the group
//! address); the Group* types address the directory and carry no scope. The
//! store translates each delta into the matching wallet_coin_channel.h event
//! payload (fetching the inserted records / refreshed group info itself) and
//! stamps it with the view's epoch.
//!
struct CoinViewDelta {
    enum Type {
        Reset,        //!< the view rebuilt wholesale; consumer drops every scope cache
        Insert,       //!< scope rows [first, first+count) inserted
        Remove,       //!< scope rows [first, first+count) removed
        Change,       //!< scope rows [first, first+count) changed in place
        GroupInsert,  //!< directory rows [first, first+count) inserted
        GroupRemove,  //!< directory rows [first, first+count) removed
        GroupChange,  //!< directory rows [first, first+count) changed in place
    };

    int view_id;
    Type type;
    std::string scope;
    int first{0};
    int count{0};
};

class CoinViews
{
public:
    //! Return by const reference: the store owns the record table and holds
    //! cs_store across every call into this class, so the reference cannot
    //! dangle mid-operation. Record fields are the sort keys directly (raw
    //! int64/string compares, locale-free) — no separate key cache is needed.
    using RecordFn = std::function<const CoinRecord&(std::size_t)>;

    explicit CoinViews(RecordFn records);

    // ---- view lifecycle -------------------------------------------------

    //! Register (or replace) a view over the n records [0, n). Builds the
    //! mode's indices and returns one Reset. Bumps the epoch when replacing.
    std::vector<CoinViewDelta> registerView(int view_id, CoinViewMode mode,
                                            int sort_column, int sort_order,
                                            std::size_t n);

    //! Drop a view. Idempotent.
    void unregisterView(int view_id);

    bool hasViews() const { return !m_views.empty(); }
    bool hasView(int view_id) const { return m_views.count(view_id) > 0; }

    //! Switch display mode / change sort: epoch bump, index rebuild for the
    //! new mode over the current n records, one Reset.
    std::vector<CoinViewDelta> setViewMode(int view_id, CoinViewMode mode,
                                           std::size_t n);
    std::vector<CoinViewDelta> setViewSort(int view_id, int sort_column,
                                           int sort_order, std::size_t n);

    //! Rebuild every registered view over [0, n) (full resync). One Reset per
    //! view. Group membership/aggregates are rebuilt from the table;
    //! selection aggregates are cleared — the store re-applies its mirror via
    //! applySelection afterwards.
    std::vector<CoinViewDelta> rebuild(std::size_t n);

    // ---- table mutations ------------------------------------------------

    //! The table APPENDED a record at absidx (== previous n). Inserts it into
    //! every view's indices, creates/updates its group and aggregates.
    std::vector<CoinViewDelta> applyInsert(std::size_t absidx);

    //! The table is ABOUT to remove the record at absidx and compact (later
    //! absolute indices shift down by one). Unlike the other apply* methods
    //! this is called BEFORE the table mutation — the record's fields are
    //! needed to locate its group and adjust aggregates. \p was_selected
    //! tells the aggregates whether the store's selection mirror held it.
    std::vector<CoinViewDelta> applyRemove(std::size_t absidx, bool was_selected);

    //! Record absidx changed in place (label re-snapshot, height fill-in on
    //! confirmation, or a regroup when the change-walk result moved it to a
    //! different group). \p old_group_address / \p old_is_change are the
    //! values before the caller mutated the table (they drive the aggregate
    //! move out of the old group); \p was_selected moves the selection
    //! aggregates with the record when the group changed.
    std::vector<CoinViewDelta> applyUpdate(std::size_t absidx,
                                           const std::string& old_group_address,
                                           bool old_is_change,
                                           bool was_selected);

    //! The store's selection mirror toggled absidx. Updates the group
    //! aggregates and emits a directory Change for tree views (selection
    //! never moves a directory row: selected_* are not parent sort keys).
    std::vector<CoinViewDelta> applySelection(std::size_t absidx, bool selected);

    //! Aggregate-only form of applySelection: updates the group counters and
    //! returns nothing. The bulk paths (selectGroup / selectAll /
    //! applyValueFilter / the mirror restores) toggle thousands of records
    //! against a handful of groups, and every one of those toggles would
    //! otherwise emit an identical GroupChange for the same directory row —
    //! half a million redundant events for one click on a pathological group.
    //! They call this per record and coalesce ONE groupTouchDeltas() per
    //! touched group at the end.
    void applySelectionQuiet(std::size_t absidx, bool selected);

    //! One in-place GroupChange per tree view for \p address — the coalesced
    //! refresh the bulk selection paths emit after their applySelectionQuiet
    //! passes. Empty for an unknown group (it died mid-pass).
    std::vector<CoinViewDelta> groupTouchDeltas(const std::string& address) const;

    // ---- reads (positional, absidx out) ---------------------------------

    //! Flat-scope slice for a view: absolute indices [first, first+count) in
    //! view order (count < 0 = to the end) plus the scope's total.
    std::vector<std::size_t> flatSlice(int view_id, int first, int count,
                                       int& total_out) const;

    //! Directory slice for a view: group addresses in view order plus total.
    std::vector<std::string> directorySlice(int view_id, int first, int count,
                                            int& total_out) const;

    //! Group-scope slice for a view: the group's member absolute indices in
    //! view order plus the group's total. Unknown group => empty, total 0.
    std::vector<std::size_t> groupSlice(int view_id, const std::string& group_address,
                                        int first, int count, int& total_out) const;

    //! Aggregates for one group (address/label/counts/amounts). Returns a
    //! default-constructed info (empty address) for an unknown group.
    CoinGroupInfo groupInfo(const std::string& group_address) const;

    //! Every group's aggregates, address-ordered (view-independent — the
    //! consolidation pickers' directory).
    std::vector<CoinGroupInfo> groupDirectory() const;

    //! Membership of one group (unsorted absolute indices) — selectGroup.
    std::vector<std::size_t> groupMembers(const std::string& group_address) const;

    //! All records sorted by (amount, outpoint) ascending — the persistent
    //! index applyValueFilter walks so a 500k-coin filter is O(n) per call
    //! with no per-call sort under cs_store (GUI-thread hold budget).
    const std::vector<std::size_t>& amountOrder() const { return m_amount_order; }

    // ---- event bookkeeping (the scope-local high-water contract) --------

    //! Record the queue seqno of an event the store just pushed for a scope
    //! ("" = flat) / the directory of \p view_id.
    void noteScopeEvent(int view_id, const std::string& scope, uint64_t seqno);
    void noteDirectoryEvent(int view_id, uint64_t seqno);

    //! Record a broadcast event (CoinReset / resync): advances the view's
    //! floor, which every scope's high-water reflects from then on. Never
    //! touches any scope's own counter.
    void noteBroadcast(int view_id, uint64_t seqno);

    //! max(last scope event seqno, view floor) — what every read reports so
    //! a consumer cache reseeded after a broadcast can always gate content
    //! fetches against a reachable value (see CoinRowsResult).
    uint64_t highWater(int view_id, const std::string& scope) const;
    uint64_t directoryHighWater(int view_id) const;

    uint64_t epoch(int view_id) const;
    CoinViewMode mode(int view_id) const;

private:
    struct GroupData {
        std::string label;
        int64_t total_amount{0};
        int output_count{0};
        int direct_output_count{0};
        int selected_count{0};
        int64_t selected_amount{0};
        std::vector<std::size_t> members; //!< unsorted absolute indices
    };

    struct ViewState {
        CoinViewMode mode{CoinViewMode::Tree};
        int sort_column{COINCOL_AMOUNT};
        int sort_order{1}; // Qt::DescendingOrder — the dialog's default
        uint64_t epoch{0};
        uint64_t floor_seqno{0};

        //! Flat mode: all records sorted by the child comparator.
        std::vector<std::size_t> flat_index;
        //! Tree mode: group addresses sorted by the parent comparator, and
        //! per-group member indices sorted by the child comparator.
        std::vector<std::string> directory;
        std::unordered_map<std::string, std::vector<std::size_t>> group_index;

        //! Last event seqno per scope ("" = flat, address = group children).
        std::unordered_map<std::string, uint64_t> scope_seqno;
        uint64_t directory_seqno{0};
    };

    //! Child (output-row) total order for a view's column/order, outpoint
    //! tiebreak. Confirmations order is height order (depth is serve-time).
    bool lessRecord(const ViewState& v, std::size_t a, std::size_t b) const;
    //! Parent (group-row) total order: total_amount / (label, address) /
    //! address fallback for DATE + CONFS, address tiebreak.
    bool lessGroup(const ViewState& v, const std::string& a, const std::string& b) const;

    //! Sorted insert slot via lower_bound (never used to locate an existing
    //! entry). / Identity locate by value, npos when absent.
    std::size_t lowerBoundRecord(const ViewState& v,
                                 const std::vector<std::size_t>& index,
                                 std::size_t absidx) const;
    std::size_t findValue(const std::vector<std::size_t>& index,
                          std::size_t absidx) const;
    std::size_t lowerBoundGroup(const ViewState& v, const std::string& address) const;
    std::size_t findGroup(const ViewState& v, const std::string& address) const;

    //! Rebuild one view's indices for its current mode over [0, n).
    void buildView(ViewState& v, std::size_t n);
    //! Rebuild shared membership/aggregates over [0, n) (selection cleared).
    void buildGroups(std::size_t n);
    //! (amount, outpoint)-ascending comparator for m_amount_order.
    bool lessAmount(std::size_t a, std::size_t b) const;

    //! Directory maintenance after a group's parent sort key may have moved:
    //! emits GroupChange in place or a GroupRemove+GroupInsert reposition.
    void reslotDirectory(ViewState& v, int view_id, const std::string& address,
                         std::vector<CoinViewDelta>& out);

    //! Decrement every stored absolute index greater than absidx (the table
    //! compaction shift) across all views, membership and the amount order.
    void shiftDown(std::size_t absidx);

    RecordFn m_records;
    std::map<int, ViewState> m_views;
    std::map<std::string, GroupData> m_groups; //!< ordered: address-sorted directory reads
    std::vector<std::size_t> m_amount_order;
    std::size_t m_count{0}; //!< records currently indexed
};

} // namespace GRC

#endif // GRIDCOIN_WALLET_COINVIEWS_H
