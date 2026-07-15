// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_WALLET_TX_SOURCE_H
#define GRIDCOIN_INTERFACES_WALLET_TX_SOURCE_H

#include "interfaces/wallet_tx_channel.h"
#include "uint256.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CWallet;

namespace interfaces {

//! The wallet transaction channel (doc/multiprocess_design.md §6): the
//! windowed transaction-table store/event contract, formalized as an
//! interface (Phase 1c-ii). The GUI drives per-view cursors and pulls the
//! event stream through this handle; the concrete implementation owns the
//! node-side store, its worker thread, and the producer subscriptions to
//! CWallet's transaction signals — none of which exist until a source is
//! constructed, so a headless node pays nothing ("headless pays nothing" is a
//! lifecycle property: the source's ctor/dtor is the attach/detach).
//!
//! Threading: the up-channel methods (registerView/getRows/...) are called on
//! the GUI thread; events are produced on core threads under core locks and
//! pulled by the GUI's periodic drainEvents(). The seqno/viewId/epoch keys on
//! the event and result value types make the drain/fetch race-free (see
//! GRC::RowsResult). Only value types cross this boundary.
class WalletTxSource
{
public:
    virtual ~WalletTxSource() = default;

    //! Register a per-view cursor (server-side filter + sort). Builds the
    //! cursor over the current records and pushes a RowsReset so the consumer
    //! bulk-refills via getRows. Re-registering an existing viewId replaces it.
    //! \p view_id is one of the GRC::VIEW_* identifiers.
    virtual void registerView(int view_id, GRC::FilterSpec filter,
                              int sort_column, int sort_order) = 0;

    //! Change a registered view's sort / filter / served-row cap. Each pushes
    //! the cursor's resulting events (Reset for sort/filter, Insert/Remove at
    //! the boundary for a limit change).
    virtual void setViewSort(int view_id, int sort_column, int sort_order) = 0;
    virtual void setViewFilter(int view_id, GRC::FilterSpec filter) = 0;
    virtual void setViewLimit(int view_id, int limit) = 0;

    //! Read [first, first+count) served rows of \p view_id plus the view's
    //! total_accepted / epoch / high_water, all under one store-lock hold
    //! (GRC::RowsResult). \p count < 0 means "all served rows from \p first".
    virtual GRC::RowsResult getRows(int view_id, int first, int count) = 0;

    //! ALL accepted rows of \p view_id in view order, cap-independent (CSV
    //! export covers every matching row even under a served-window cap).
    virtual GRC::RowsResult getAllRows(int view_id) = 0;

    //! The accepted-view row of the record identified by (\p hash, \p idx), or
    //! -1 if absent / filtered out / the view is unknown. \p idx < 0 matches
    //! the first part of the transaction.
    virtual int rowForKey(int view_id, const uint256& hash, int idx) = 0;

    //! Structured transaction detail for (\p hash, \p idx) — the double-click
    //! detail dialog. found == false for an unknown key or a transaction no
    //! longer in the wallet. Rendering and translation are GUI-side (design
    //! §4.1); see GRC::WalletTxDetail.
    virtual GRC::WalletTxDetail getRowDetail(const uint256& hash, int idx) = 0;

    //! Rebuild the store from the wallet and return a full decomposed snapshot
    //! for the consumer's replica. \p limit_enabled / \p limit_time are the
    //! GUI's datetime-display cutoff, cached by the source for later inserts.
    virtual std::vector<TransactionRecord> reloadAndSnapshot(bool limit_enabled,
                                                             int64_t limit_time) = 0;

    //! Pull up to \p max_batch pending events in seqno order (the GUI's
    //! periodic drain). Returns an empty vector when none are pending.
    virtual std::vector<GRC::WalletEvent> drainEvents(std::size_t max_batch) = 0;

    //! GUI up-channel: an address-book entry changed; re-snapshot the label on
    //! the stored records for \p address and re-drive the cursors (the label
    //! is the Address-column sort key and a filter target). \p label is the
    //! authoritative current label ("" after a delete). O(1) enqueue.
    virtual void noteAddressBookChanged(const std::string& address,
                                        const std::string& label) = 0;
};

//! Return an in-process WalletTxSource over the given wallet, which must
//! outlive the returned object. Constructing it starts the store worker and
//! subscribes the producer handlers to the wallet's transaction signals;
//! destroying it unsubscribes and joins.
std::unique_ptr<WalletTxSource> MakeWalletTxSource(CWallet* wallet);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_WALLET_TX_SOURCE_H
