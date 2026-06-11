// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETTXSTORE_H
#define BITCOIN_QT_WALLETTXSTORE_H

#include "qt/cursor.h"
#include "qt/txorder.h"
#include "qt/transactionrecord.h"
#include "qt/wallet_event_queue.h"
#include "sync.h"
#include "uint256.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <thread>
#include <unordered_map>
#include <vector>

class CWallet;

namespace GRC {

//!
//! \brief Qt-free hasher for the tx-hash index. A correct internal hash over
//! uint256 (any well-distributed hash works for a private multimap); kept
//! header-light so wallettxstore.h need not pull in main.h's BlockHasher.
//!
struct TxHashHasher {
    std::size_t operator()(const uint256& h) const { return static_cast<std::size_t>(h.GetUint64(0)); }
};

//!
//! \brief Producer-owned authoritative ordering store for the windowed
//! transaction-table model (windowed-model PR2).
//!
//! Step 2 of the windowed-model rewrite relocates the cached-wallet ORDERING
//! out of the Qt-thread-exclusive TransactionTablePriv into this object, which
//! the producer (core threads, in WalletModel's NotifyTransactionChanged
//! handler) mutates. The store computes each new transaction's insert position
//! and each removal's contiguous range, then pushes a position-stamped
//! RowsInserted / RowsRemoved event onto the WalletEventQueue. The consumer
//! (TransactionTableModel) applies those events to its own replica in lockstep
//! — it never reads this store on the render path — which is what keeps the
//! still-present QSortFilterProxyModel consistent across a multi-insert drain
//! batch (a pure-passthrough-reading-the-eager-store consumer would mis-map
//! rows mid-replay).
//!
//! As of PR3 the store holds the FULL authoritative TransactionRecords (not just
//! ordering keys): the per-view cursors filter/sort over them and the store
//! serves reads (getRows) for the windowed consumers. The detailed-table consumer
//! still keeps its own full replica until it migrates to a viewport slice (PR5).
//!
//! Threading: m_records / m_by_hash are guarded by the leaf mutex cs_store. The
//! canonical lock order is cs_main -> cs_wallet -> cs_store; cs_store is NEVER
//! held while acquiring cs_main / cs_wallet. Producers finish all wallet work
//! (decompose under the locks they already hold) BEFORE calling into the store,
//! which takes cs_store last and for a microsecond-bounded window. The Rows*
//! event is pushed WHILE cs_store is held, so queue seqno-order is identical to
//! store-mutation-order across all producer threads.
//!
class WalletTxStore
{
public:
    WalletTxStore(CWallet* wallet, GRC::WalletEventQueue& queue);
    ~WalletTxStore();

    WalletTxStore(const WalletTxStore&) = delete;
    WalletTxStore& operator=(const WalletTxStore&) = delete;

    //!
    //! \brief Start the store-worker thread. Called once by WalletModel after
    //! construction, before the first reloadAndSnapshot. Idempotent.
    //!
    void start();

    //!
    //! \brief Producer: a transaction became visible. Enqueues the
    //! already-decomposed \p records onto the intake queue and returns
    //! immediately (O(1), no store mutation on the caller's thread); the worker
    //! applies the datetime cutoff, dedup, ordering, and the RowsInserted event
    //! off-lock. Called from core threads under cs_main/cs_wallet — takes only
    //! the leaf cs_intake, never cs_store, so it never waits on the O(N)
    //! ordering maintenance (PR2.5 double-queue).
    //!
    void enqueueInsert(std::vector<TransactionRecord> records);

    //!
    //! \brief Producer: a transaction is no longer visible / was deleted.
    //! Enqueues a removal by \p hash for the worker (O(1), same threading
    //! contract as enqueueInsert). Safe from the CT_DELETED path (no wallet
    //! locks held).
    //!
    void enqueueRemove(const uint256& hash);

    //!
    //! \brief Producer: a transaction already in the store changed (CT_UPDATED —
    //! e.g. first confirmation). Enqueues the freshly re-decomposed \p records
    //! (with producer-computed status) for the worker, which replaces the stored
    //! records in place and re-evaluates each cursor's membership/sort slot
    //! (applyStatusUpdate). Same O(1) threading contract as enqueueInsert.
    //!
    void enqueueUpsert(std::vector<TransactionRecord> records);

    //!
    //! \brief Qt thread: register a per-view cursor (server-side filter+sort).
    //! Builds the cursor over the current m_records and pushes a RowsReset so the
    //! consumer bulk-refills via getRows. Re-registering an existing viewId
    //! replaces it. \p viewId is one of the GRC::VIEW_* identifiers.
    //!
    void registerView(int viewId, FilterSpec filter, int sort_column, int sort_order);

    //! Qt thread: change a registered view's sort / filter / served-row cap.
    //! Each pushes the cursor's resulting events (Reset for sort/filter,
    //! Insert/Remove at the boundary for a limit change).
    void setViewSort(int viewId, int sort_column, int sort_order);
    void setViewFilter(int viewId, FilterSpec filter);
    void setViewLimit(int viewId, int limit);

    //! Qt thread: read [first, first+count) served rows of \p viewId as records
    //! (a copy of the slice). Clamps to the served window; returns fewer rows if
    //! the window is shorter. Unknown viewId -> empty.
    std::vector<TransactionRecord> getRows(int viewId, int first, int count);

    //! Qt thread: total accepted rows for \p viewId (the virtual rowCount), or 0
    //! if the view is not registered.
    int totalAccepted(int viewId);

    //!
    //! \brief Qt thread: rebuild the store from the wallet and return a full
    //! decomposed snapshot for the consumer's replica.
    //!
    //! Holds cs_main + cs_wallet across the whole rebuild (blocking producers,
    //! exactly as the old loadWallet did), swaps the index under cs_store, and
    //! drains+discards any pending queue events while producers are still
    //! blocked — so the returned snapshot, the rebuilt index, and the (now
    //! empty) queue are mutually consistent. Any producer event after this
    //! returns is computed against the rebuilt index and applied by the next
    //! drain. \p limit_enabled / \p limit_time are the OptionsModel
    //! datetime-display cutoff (read on the Qt thread by the caller); the store
    //! caches them and applies them to subsequent insertTransaction calls.
    //!
    std::vector<TransactionRecord> reloadAndSnapshot(bool limit_enabled, int64_t limit_time);

private:
    //! One unit of deferred store maintenance handed from a producer to the worker.
    struct IntakeItem {
        enum Kind { Insert, Remove, Update } kind;
        std::vector<TransactionRecord> records; //!< Insert/Update: the decomposed parts.
        uint256 hash;                           //!< Remove: the tx hash.
    };

    //! Worker entry point: drain the intake queue and apply each item. Parks
    //! while m_rebuilding is set (acknowledging via m_worker_parked); exits on
    //! m_stop. Holds cs_intake only while waiting/dequeuing — never with cs_store.
    void workerLoop();
    //! Worker: apply one intake item (the O(N) store maintenance). Must be called
    //! with NO lock held; insert/removeTransaction take cs_store internally.
    void applyIntake(IntakeItem item);

    //! The O(N) ordering maintenance, now worker-private (was the public PR2 API;
    //! producers call enqueue* instead). Each takes cs_store and pushes the native
    //! VIEW_FULL event AND drives the registered cursors.
    void insertTransaction(std::vector<TransactionRecord> records);
    void removeTransaction(const uint256& hash);
    //! Worker: a present tx's records changed in place (CT_UPDATED). Replaces the
    //! stored records and re-evaluates each cursor (applyStatusUpdate). Takes cs_store.
    void updateTransaction(std::vector<TransactionRecord> records);
    //! Lock-free cores (caller already holds cs_store): the actual O(N) maintenance
    //! + cursor drive. insert/removeTransaction are thin lock-taking wrappers;
    //! updateTransaction composes these directly for its not-present /
    //! part-count-changed fallback without re-entering the non-recursive cs_store.
    void insertLocked(std::vector<TransactionRecord> records) EXCLUSIVE_LOCKS_REQUIRED(cs_store);
    void removeLocked(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    void shiftIndex(std::size_t from, std::ptrdiff_t delta) EXCLUSIVE_LOCKS_REQUIRED(cs_store);
    void rebuildIndex() EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! Translate one cursor's served-window CursorDeltas into WalletEvents for
    //! \p viewId (Reset/Insert/Remove/Change), fetching records from m_records for
    //! the inserted/changed served rows. Caller holds cs_store.
    void emitCursorDeltas(int viewId, uint64_t epoch,
                          const std::vector<CursorDelta>& deltas) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! Cursor projectors over m_records[i]. Read m_records while the caller holds
    //! cs_store, but they are invoked through the cursor's std::function
    //! indirection, which the Clang thread-safety analyzer cannot see the lock
    //! through — hence NO_THREAD_SAFETY_ANALYSIS. Every call path holds cs_store
    //! (the worker's apply* and the Qt-thread register/getRows all take it).
    TxFilterFields projectFieldsAt(std::size_t i) const NO_THREAD_SAFETY_ANALYSIS;
    SortKey projectKeysAt(std::size_t i) const NO_THREAD_SAFETY_ANALYSIS;
    //! Build the (Fields, Keys) projector pair bound to this store for a cursor.
    void makeCursorProjectors(Cursor::FieldsFn& fields, Cursor::KeysFn& keys);

    CWallet* const m_wallet;
    GRC::WalletEventQueue& m_queue;

    mutable Mutex cs_store;

    //! Full authoritative records, sorted by TxOrderLess (RecordOrder). Position
    //! oracle for the native VIEW_FULL stream and the backing table the per-view
    //! cursors index into.
    std::vector<TransactionRecord> m_records GUARDED_BY(cs_store);
    //! tx hash -> positions in m_records. Same-hash records are contiguous.
    std::unordered_multimap<uint256, std::size_t, TxHashHasher> m_by_hash GUARDED_BY(cs_store);

    //! Registered per-view cursors (server-side filter+sort), keyed by VIEW_*.
    //! Each indexes into m_records; maintained on the worker thread (insert/
    //! remove/update) and on the Qt thread (register/setView*/getRows), all under
    //! cs_store. std::map for stable references across insertion.
    std::map<int, Cursor> m_cursors GUARDED_BY(cs_store);

    //! Cached OptionsModel datetime-display cutoff, set by reloadAndSnapshot.
    bool m_limit_enabled GUARDED_BY(cs_store){false};
    int64_t m_limit_time GUARDED_BY(cs_store){0};

    //! Intake queue (PR2.5). Producers enqueue here; the store-worker drains it
    //! and performs the O(N) maintenance off the core locks. cs_intake is an
    //! independent leaf — it is NEVER held while taking cs_store (and vice
    //! versa), so the two never invert.
    mutable Mutex cs_intake;
    std::deque<IntakeItem> m_intake GUARDED_BY(cs_intake);
    bool m_stop GUARDED_BY(cs_intake){false};          //!< shutdown request (dtor)
    bool m_rebuilding GUARDED_BY(cs_intake){false};    //!< reloadAndSnapshot wants the worker parked
    bool m_worker_parked GUARDED_BY(cs_intake){false}; //!< worker has acknowledged the pause
    CConditionVariable m_intake_cv;  //!< worker waits for work / stop / resume
    CConditionVariable m_idle_cv;    //!< reloadAndSnapshot waits for m_worker_parked
    std::thread m_worker;
    bool m_started{false};           //!< Qt-thread only: start() idempotency guard
};

} // namespace GRC

#endif // BITCOIN_QT_WALLETTXSTORE_H
