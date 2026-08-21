// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_WALLET_COIN_SOURCE_H
#define GRIDCOIN_INTERFACES_WALLET_COIN_SOURCE_H

#include "interfaces/wallet_coin_channel.h"
#include "primitives/transaction.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

class CWallet;

namespace interfaces {

//! The wallet coin channel (issue #3183): the windowed coin-control selection
//! store/event contract, the coin-side sibling of WalletTxSource. The GUI
//! drives per-view cursors, windowed scope reads and server-side selection
//! operations through this handle; the concrete implementation owns the
//! node-side store, its worker thread, and the producer subscriptions to the
//! wallet's signals — none of which exist until a source is constructed, so a
//! headless node pays nothing (the ctor/dtor is the attach/detach).
//!
//! Threading: the up-channel methods are called on the GUI thread; events are
//! produced on core threads under core locks and pulled by the GUI's periodic
//! drainEvents(). The drain point is SINGLE and owned by WalletModel:
//! drainEvents() destructively pops the queue, so per-consumer drain timers
//! would steal each other's events (two coin views are alive simultaneously
//! in the consolidate-wizard flow). WalletModel drains and fans each batch
//! out; consumers filter by their view id.
//!
//! Selection ownership: the GUI-side interfaces::WalletCoinControl selection
//! set remains authoritative (Phase 1e contract). The source keeps a mirror
//! plus per-group aggregates so parent tristate and bulk operations never
//! materialize children. Every selection mutation flows through this
//! interface and returns the validated outpoint delta, which the GUI applies
//! to its own set — see CoinSelectionUpdate / CoinBulkSelectionResult.
//!
//! Only value types cross this boundary.
class WalletCoinSource
{
public:
    virtual ~WalletCoinSource() = default;

    //! Register a per-view cursor with a display mode and sort. The first
    //! registration on a cold source triggers the initial wallet scan (see
    //! reloadAndSnapshot). Re-registering an existing view_id replaces it and
    //! pushes a CoinReset. \p view_id is one of the GRC::VIEW_COIN_*
    //! identifiers; \p sort_column is a GRC::CoinSortColumn.
    virtual void registerView(int view_id, GRC::CoinViewMode mode,
                              int sort_column, int sort_order) = 0;

    //! Drop a registered view when its consumer tears down. Idempotent. Must
    //! be called while the source is still alive (the tx channel's
    //! teardown-ordering contract). When the last view unregisters the store
    //! stops enqueueing GUI events and clears the queue — records stay warm
    //! for an instant re-register, but nothing accumulates while no dialog is
    //! open.
    virtual void unregisterView(int view_id) = 0;

    //! Switch a registered view between flat and tree display mode / change
    //! its sort. Both bump the view's epoch and floor and push a CoinReset;
    //! the consumer drops its scope caches and reseeds.
    virtual void setViewMode(int view_id, GRC::CoinViewMode mode) = 0;
    virtual void setViewSort(int view_id, int sort_column, int sort_order) = 0;

    //! Read [first, first+count) rows of the FLAT scope plus its metadata,
    //! all under one store-lock hold. \p count < 0 means "all from first".
    virtual GRC::CoinRowsResult getRows(int view_id, int first, int count) = 0;

    //! Read [first, first+count) rows of the group DIRECTORY (tree-mode
    //! parent rows) with per-group aggregates, under one store-lock hold.
    virtual GRC::CoinGroupsResult getGroups(int view_id, int first, int count) = 0;

    //! Read [first, first+count) child rows of the group keyed by
    //! \p group_address (the stable group identity across epochs and sorts),
    //! under one store-lock hold. Unknown group => empty result with
    //! total_accepted 0.
    virtual GRC::CoinRowsResult getGroupRows(int view_id,
                                             const std::string& group_address,
                                             int first, int count) = 0;

    //! The full group directory (every group with aggregates), independent of
    //! any view's windowing — the consolidation flows' address pickers.
    virtual std::vector<GRC::CoinGroupInfo> getGroupDirectory() = 0;

    //! Seed or refresh the store's selection mirror from the GUI's
    //! authoritative selection set, returning the set pruned to outpoints
    //! that still exist as available coins (the legacy updateView() stale-
    //! selection reconciliation). Call at consumer attach and after every
    //! CoinReset / resync — a reset may have discarded queued removal events
    //! whose outpoints the GUI-side set still holds.
    virtual std::set<COutPoint> reconcileSelection(std::set<COutPoint> selection) = 0;

    //! Toggle one outpoint in the selection mirror. applied is false when the
    //! outpoint is unknown (e.g. spent and removed since render); the GUI
    //! must apply the toggle to its own set ONLY when applied is true.
    virtual GRC::CoinSelectionUpdate setSelected(const COutPoint& outpoint,
                                                 bool selected) = 0;

    //! Select or deselect every member of a group / every coin, server-side,
    //! without the consumer materializing children. Returns the exact
    //! outpoint delta applied to the mirror.
    virtual GRC::CoinBulkSelectionResult selectGroup(const std::string& group_address,
                                                     bool selected) = 0;
    virtual GRC::CoinBulkSelectionResult selectAll(bool selected) = 0;

    //! The legacy filterInputsByValue relocated server-side, over the CURRENT
    //! mirror selection (prune-only — never selects new coins): deselect
    //! members failing the (<= / >=) \p value predicate, then keep only the
    //! \p max_inputs smallest (less_or_equal) / largest (!less_or_equal),
    //! deselecting the remainder. Ties at the cap boundary break by
    //! (amount, outpoint). culled is set when the cap trimmed anything. Both
    //! legacy call shapes map here: the filter button passes max_inputs
    //! UINT_MAX; the consolidation flow passes an always-true predicate with
    //! the consolidation input cap.
    virtual GRC::CoinBulkSelectionResult applyValueFilter(bool less_or_equal,
                                                          int64_t value,
                                                          uint32_t max_inputs) = 0;

    //! Rebuild the store from the wallet (AvailableCoins scan + change-walk
    //! grouping) and return the directory snapshot. The first call performs
    //! the initial load; later calls resynchronize (queue discard + floor
    //! bump, consumers reseed). The scan takes cs_main + cs_wallet and is
    //! O(wallet); consumers invoke it off the paint path and render a loading
    //! state (never inline on first paint).
    virtual GRC::CoinGroupsResult reloadAndSnapshot() = 0;

    //! Pull up to \p max_batch pending events in seqno order. Destructive;
    //! called ONLY by the single WalletModel drain point (see class comment).
    virtual std::vector<GRC::WalletCoinEvent> drainEvents(std::size_t max_batch) = 0;

    //! GUI up-channel: an address-book entry changed. Labeling an own address
    //! flips IsChange for its outputs, so beyond re-snapshotting labels this
    //! re-walks the affected records' grouping and emits regroup deltas.
    //! Called on the GUI thread; the implementation may take cs_main +
    //! cs_wallet (the worker thread never does).
    virtual void noteAddressBookChanged(const std::string& address,
                                        const std::string& label) = 0;
};

//! Return an in-process WalletCoinSource over the given wallet, which must
//! outlive the returned object. Same ownership contract as MakeWalletTxSource:
//! shared_ptr, with producer subscriptions holding a weak reference locked per
//! callback, closing the cross-thread teardown race. The in-process
//! implementation lands with the coin store (the #3183 series' store PR);
//! until then this factory has no definition and nothing links against it.
std::shared_ptr<WalletCoinSource> MakeWalletCoinSource(CWallet* wallet);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_WALLET_COIN_SOURCE_H
