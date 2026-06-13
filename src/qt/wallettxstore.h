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
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CWallet;

//! Forward declaration so applyChainTipRefresh() can carry an
//! EXCLUSIVE_LOCKS_REQUIRED(cs_main) annotation without pulling in the heavy
//! main.h here (mirrors the same extern in net.h / validation.h). CCriticalSection
//! comes from sync.h, included above.
extern CCriticalSection cs_main;

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
//! \brief Atomic result of a windowed read (getRows): the requested row slice
//! plus the view's metadata, all sampled under the SAME cs_store hold.
//!
//! Returning the four together is the generalization of PR4-fix B to the PR5
//! scroll path. A windowed consumer keeps two reconciliation channels: a
//! STRUCTURAL channel (the virtual rowCount + the ordered Insert/Remove/Change/
//! Reset delta stream) and a CONTENT channel (the scroll-driven slice fetch).
//! The content fetch must observe the rowCount (\ref total_accepted), the
//! cursor's sort/filter generation (\ref epoch) and the view's event high-water
//! (\ref high_water) at the EXACT instant the rows were copied — a second,
//! separately-locked call (the removed totalAccepted(viewId)) could observe a
//! different store state and misalign the slice against the structural channel,
//! dropping or double-counting a row (the PR4-fix B bug class). The consumer
//! adopts a content fetch only when its \ref epoch and \ref high_water match the
//! structural channel's, and NEVER advances the structural seqno from it.
//!
struct RowsResult {
    std::vector<TransactionRecord> records; //!< the [first, first+count) slice (a copy)
    int total_accepted = 0;                 //!< the view's full accepted count (virtual rowCount)
    uint64_t epoch = 0;                      //!< cursor sort/filter generation at the read instant
    uint64_t high_water = 0;                 //!< seqno of the last event emitted for the view at the read
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
    //! \brief Producer: an address-book entry changed (label added / renamed /
    //! removed). Enqueues the address + its new label for the worker (O(1)); the
    //! worker re-snapshots TransactionRecord::label on every stored record with
    //! that address and re-evaluates each cursor (the label is the Address-column
    //! sort key and an address-substring filter target). Restores the live-label
    //! filter/sort the deleted TransactionFilterProxy had (windowed-model PR4-C).
    //! \p label is "" when the entry was deleted.
    //!
    void enqueueAddressBookChange(const std::string& address, const std::string& label);

    //!
    //! \brief Producer (core thread, cs_main held): the chain tip advanced.
    //! Re-runs updateStatus on the bounded set of records whose displayed status
    //! is still height-dependent (Confirming / Immature / Unconfirmed / ...), then
    //! drives each cursor so the detailed/overview views show advancing
    //! confirmation counts and maturity — the per-block refresh the deleted proxy
    //! got from TransactionTableModel::index()'s lazy updateStatus (windowed-model
    //! PR4-A). Runs INLINE (not on the store-worker) so it can take cs_wallet +
    //! cs_store under the caller's cs_main without the worker ever needing
    //! cs_main/cs_wallet (which would deadlock reloadAndSnapshot's park protocol);
    //! cs_main also mutually excludes this from reloadAndSnapshot.
    //!
    void applyChainTipRefresh() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

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

    //! Qt thread: read [first, first+count) served rows of \p viewId, plus the
    //! view's total_accepted / epoch / high_water, ALL under one cs_store hold
    //! (\ref RowsResult). Clamps the slice to the served window; returns fewer
    //! rows if it is shorter. \p count < 0 means "all served rows from \p first".
    //! Unknown viewId / first < 0 / count == 0 -> empty slice, but the metadata
    //! fields are still valid for a registered view (total_accepted/epoch/
    //! high_water reflect the current cursor) so a caller can refresh its
    //! reconciliation baseline without a slice copy. Returning the metadata
    //! WITH the slice atomically is what closes the PR4-fix B race for both the
    //! Reset refetch and the PR5 scroll fetch — see \ref RowsResult.
    RowsResult getRows(int viewId, int first, int count);

    //! Qt thread: ALL accepted rows of \p viewId in view order, plus the same
    //! metadata as getRows, under ONE cs_store hold. CAP-INDEPENDENT — iterates the
    //! full accepted set (totalAccepted), not the served window — so a CSV export
    //! covers every matching row even if a finite served cap is ever introduced
    //! (windowed-model PR5-B; the detailed view's cap stays unlimited today, so this
    //! equals getRows(viewId,0,-1), but the contract is the distinction).
    RowsResult getAllRows(int viewId);

    //! Qt thread: the accepted-view row of the record identified by (\p hash,
    //! \p idx), or -1 if the key is absent, filtered out, or the view is unknown.
    //! \p idx < 0 matches the FIRST part of the transaction — the lowest accepted
    //! row across all parts sharing \p hash — reproducing the old indexForTxid
    //! hash-only semantics. Resolves \p hash via m_by_hash to absolute record
    //! indices, then Cursor::positionOf to the accepted row. Backs click-through and
    //! anchor-on-resort (windowed-model PR5-B). Pure read; no projector calls.
    int rowForKey(int viewId, const uint256& hash, int idx = -1);

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
        enum Kind { Insert, Remove, Update, AddressBook } kind;
        std::vector<TransactionRecord> records; //!< Insert/Update: the decomposed parts.
        uint256 hash;                           //!< Remove/Update: the tx hash.
        std::string ab_address;                 //!< AddressBook: the changed address.
        std::string ab_label;                   //!< AddressBook: its new label ("" if removed).
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
    //! Worker (cs_store only): re-snapshot TransactionRecord::label on every stored
    //! record whose address matches, refresh its cached projector outputs, and
    //! re-evaluate each cursor (the label drives the Address sort + label filter).
    //! The new label is carried in, so no cs_wallet is needed — keeping the worker
    //! free of cs_main/cs_wallet (reloadAndSnapshot's park protocol depends on it).
    void applyAddressBookChange(const std::string& address, const std::string& label);
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
    //! Return the CACHED projector outputs for record i by const ref (PR4-fix F):
    //! the cursor reads these per comparison without re-projecting. The reference
    //! is into m_fields_cache / m_keys_cache, valid while the caller holds cs_store.
    const TxFilterFields& projectFieldsAt(std::size_t i) const NO_THREAD_SAFETY_ANALYSIS;
    const SortKey& projectKeysAt(std::size_t i) const NO_THREAD_SAFETY_ANALYSIS;
    //! Build the (Fields, Keys) projector pair bound to this store for a cursor.
    void makeCursorProjectors(Cursor::FieldsFn& fields, Cursor::KeysFn& keys);

    //! (Re)compute the cached projector outputs for record i from m_records[i].
    //! Call after any insert/in-place mutation of m_records[i]; the parallel cache
    //! vectors are kept exactly the same size and order as m_records under cs_store.
    void recomputeCacheAt(std::size_t i) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! True if record r's displayed status is still height-dependent (it will
    //! change as blocks advance: Unconfirmed / Confirming / Immature / Open* /
    //! MaturesWarning). Terminal states (Confirmed / Conflicted / NotAccepted /
    //! Offline) are excluded — applyChainTipRefresh skips them.
    static bool isVolatile(const TransactionRecord& r);

    //! Re-evaluate whether `hash` belongs in m_volatile from its records' current
    //! status, inserting or erasing it. Caller holds cs_store.
    void updateVolatileForHash(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    CWallet* const m_wallet;
    GRC::WalletEventQueue& m_queue;

    mutable Mutex cs_store;

    //! Full authoritative records, sorted by TxOrderLess (RecordOrder). Position
    //! oracle for the native VIEW_FULL stream and the backing table the per-view
    //! cursors index into.
    std::vector<TransactionRecord> m_records GUARDED_BY(cs_store);
    //! tx hash -> positions in m_records. Same-hash records are contiguous.
    std::unordered_multimap<uint256, std::size_t, TxHashHasher> m_by_hash GUARDED_BY(cs_store);

    //! Cached projector outputs, kept exactly parallel to m_records (PR4-fix F):
    //! computed once per record at insert/update/refresh so a cursor sort or
    //! lower_bound reads them by const ref — no strprintf / string allocation per
    //! comparison. Every m_records mutation updates these in lockstep under cs_store.
    std::vector<TxFilterFields> m_fields_cache GUARDED_BY(cs_store);
    std::vector<SortKey> m_keys_cache GUARDED_BY(cs_store);

    //! Tx hashes with at least one height-volatile record (Confirming / Immature /
    //! ...). applyChainTipRefresh() re-runs updateStatus only over this bounded set
    //! each block, so the per-block cost is O(volatile) rather than O(N) (PR4-fix A).
    std::unordered_set<uint256, TxHashHasher> m_volatile GUARDED_BY(cs_store);

    //! Registered per-view cursors (server-side filter+sort), keyed by VIEW_*.
    //! Each indexes into m_records; maintained on the worker thread (insert/
    //! remove/update) and on the Qt thread (register/setView*/getRows), all under
    //! cs_store. std::map for stable references across insertion.
    std::map<int, Cursor> m_cursors GUARDED_BY(cs_store);

    //! Per-view high-water: the seqno of the last event emitted for each view
    //! (PR4-fix B). getRows returns it so a consumer can discard any event already
    //! reflected in a refetched snapshot, closing the reset/delta double-apply race
    //! (a worker insert/remove landing between a Reset emission and the consumer's
    //! getRows would otherwise be applied twice).
    std::map<int, uint64_t> m_view_seqno GUARDED_BY(cs_store);

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
