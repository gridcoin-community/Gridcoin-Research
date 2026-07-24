// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_WALLETCOINSTORE_H
#define GRIDCOIN_WALLET_WALLETCOINSTORE_H

#include "interfaces/wallet_coin_channel.h"
#include "wallet/coinviews.h"
#include "wallet/wallet_event_queue.h"
#include "sync.h"
#include "uint256.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CWallet;
class CWalletTx;

//! Forward declaration so applyChainTipRefresh() can carry an
//! EXCLUSIVE_LOCKS_REQUIRED(cs_main) annotation without pulling in the heavy
//! main.h here (the WalletTxStore precedent). CCriticalSection comes from
//! sync.h, included above.
extern CCriticalSection cs_main;

namespace GRC {

//!
//! \brief Qt-free hasher for uint256-keyed store indices (the wallettxstore.h
//! TxHashHasher pattern, redeclared under its own name so this header does
//! not pull in the tx store).
//!
struct CoinTxHashHasher {
    std::size_t operator()(const uint256& h) const { return static_cast<std::size_t>(h.GetUint64(0)); }
};

//!
//! \brief Producer-owned authoritative store for the windowed coin-control
//! selection model (issue #3183) — the coin channel's sibling of
//! GRC::WalletTxStore, with the same threading shape: producers (core
//! threads, under the locks they already hold) enqueue O(1) intake items;
//! a store-worker thread drains them and performs the O(N) maintenance off
//! the core locks; the Qt thread reads windowed slices and drives selection
//! operations. GRC::CoinViews (the grouped index core) does the positional
//! arithmetic; this class owns the record table, the wallet interaction, the
//! selection mirror, and the event emission.
//!
//! Lock discipline (identical to WalletTxStore): cs_intake and cs_store are
//! independent leaves, never held together; canonical order is
//! cs_main -> cs_wallet -> cs_store; cs_store is NEVER held while acquiring
//! cs_main / cs_wallet; the worker never takes cs_main / cs_wallet (the
//! reloadAndSnapshot park protocol depends on it). Events are pushed while
//! cs_store is held, so queue seqno order equals store mutation order.
//!
//! Coin-specific responsibilities beyond the tx store's shape:
//!
//!  - UPSERT DIFFING: a producer hands the store the FULL set of currently
//!    available outputs of one transaction; the worker diffs it against the
//!    stored outpoints of that hash into removals (spent/conflicted),
//!    inserts (new/confirmed/restored) and in-place updates. One path covers
//!    receive, spend, confirmation, maturity and reorg-restore.
//!
//!  - PENDING AVAILABILITY: a coin's availability depends on global chain
//!    state (IsTrusted's confirmation gate, coinbase/coinstake maturity,
//!    finality) — a received coin becomes spendable at depth 3 with no
//!    notification for its own hash. Producers flag any tx excluded for such
//!    a depth/time-dependent reason; applyChainTipRefresh() re-decomposes
//!    the flagged set inline under the emitter's cs_main each tip advance.
//!
//!  - SELECTION MIRROR: the GUI-side interfaces::WalletCoinControl set stays
//!    authoritative; the store mirrors it (validated against the outpoint
//!    index — a toggle racing a worker removal must not plant a phantom
//!    entry) so per-group tristate aggregates and the bulk operations
//!    (selectGroup / selectAll / applyValueFilter) run server-side without
//!    materializing children. Removal events carry outpoints so the drain
//!    path prunes the GUI set unconditionally.
//!
//!  - SUPPRESS WHEN UNWATCHED: with no view registered the store still
//!    maintains its records (reopen stays warm) but pushes no events; the
//!    last unregisterView clears the queue, and a register triggers the
//!    consumer's reseed via its Reset.
//!
class WalletCoinStore
{
public:
    WalletCoinStore(CWallet* wallet, GRC::WalletCoinEventQueue& queue);
    ~WalletCoinStore();

    WalletCoinStore(const WalletCoinStore&) = delete;
    WalletCoinStore& operator=(const WalletCoinStore&) = delete;

    //! Start the store-worker thread. Idempotent; called once at attach.
    void start();

    // ---- producers (any core thread; leaf cs_intake only) ---------------

    //! The full currently-available output set of one transaction (possibly
    //! empty — an empty set removes every stored coin of the hash).
    //! \p pending flags a depth/time-dependent exclusion (see class comment).
    void enqueueUpsert(const uint256& hash, std::vector<CoinRecord> records,
                       bool pending);

    //! Remove every stored coin of \p hash (CT_DELETED — no wallet locks held).
    void enqueueRemove(const uint256& hash);

    //! An address-book entry changed. The worker re-labels affected records;
    //! the regroup re-walk (IsChange flips) is performed by the producer side
    //! before enqueueing (the worker never takes wallet locks), arriving here
    //! as ordinary upserts — this intake item only refreshes label snapshots.
    void enqueueAddressBookChange(const std::string& address, const std::string& label);

    //! Core thread, cs_main held (the tip-advance emitter): update the cached
    //! tip height, re-decompose the pending-availability set (takes cs_wallet
    //! under the caller's cs_main, then cs_store — canonical order; runs
    //! INLINE, never on the worker), and push one CoinDepthRefresh marker.
    //! Null-wallet stores (unit tests) update the tip and marker only.
    void applyChainTipRefresh(int height) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! A bulk wallet mutation pass (rescan/reaccept) bypassed per-tx
    //! notifications: flag that a full resync is needed. The GUI drain path
    //! polls consumeNeedsResync() and schedules reloadAndSnapshot off the
    //! paint path (never inline in the signal — its lock state varies).
    void markNeedsResync() { m_needs_resync = true; }
    bool consumeNeedsResync() { return m_needs_resync.exchange(false); }

    // ---- view lifecycle (Qt thread) -------------------------------------

    void registerView(int view_id, CoinViewMode mode, int sort_column, int sort_order);
    void unregisterView(int view_id);
    void setViewMode(int view_id, CoinViewMode mode);
    void setViewSort(int view_id, int sort_column, int sort_order);

    // ---- reads (Qt thread; each atomic under one cs_store hold) ---------

    CoinRowsResult getRows(int view_id, int first, int count);
    CoinGroupsResult getGroups(int view_id, int first, int count);
    CoinRowsResult getGroupRows(int view_id, const std::string& group_address,
                                int first, int count);
    std::vector<CoinGroupInfo> getGroupDirectory();

    // ---- selection (Qt thread; synchronous, validated) ------------------

    std::set<COutPoint> reconcileSelection(std::set<COutPoint> selection);
    CoinSelectionUpdate setSelected(const COutPoint& outpoint, bool selected);
    CoinBulkSelectionResult selectGroup(const std::string& group_address, bool selected);
    CoinBulkSelectionResult selectAll(bool selected);
    CoinBulkSelectionResult applyValueFilter(bool less_or_equal, int64_t value,
                                             uint32_t max_inputs);

    // ---- load / resync (Qt thread or a one-shot load thread) ------------

    //! Rebuild the store from the wallet: AvailableCoins conditions +
    //! change-walk grouping (memoized). Holds cs_main + cs_wallet across the
    //! scan with the worker parked (the WalletTxStore park protocol); swaps
    //! under cs_store; prunes the selection mirror to surviving outpoints;
    //! discards the queue and publishes each view's Reset. O(wallet) — run it
    //! off the paint path. Returns the address-ordered group directory.
    CoinGroupsResult reloadAndSnapshot();

    //! DEV HARNESS ONLY (-devsyntheticcoins): install a synthetic record
    //! snapshot on a null-wallet store — the reloadAndSnapshot install steps
    //! (park worker, swap, rebuild views, discard queue, publish Resets)
    //! without the wallet scan or wallet locks. Lets the real model/view
    //! stack be exercised at pathological scale (the #3183 500k-child expand
    //! acceptance gate) with every windowing/reconciliation semantic intact.
    void seedSynthetic(std::vector<CoinRecord> records, int tip_height);

    //! Decompose one wallet transaction into its currently-available coin
    //! records, reproducing CWallet::AvailableCoins's listCoins conditions
    //! (final, trusted, mature, depth >= 0, unspent, mine, above the minimum
    //! input value) plus the change-walk group key and label snapshot.
    //! \p pending_out reports a depth/time-dependent exclusion.
    //! \p walk_memo optionally memoizes the change-walk across a bulk scan
    //! (txid -> group address). Requires cs_main + wallet->cs_wallet — the
    //! lock annotation lives on the definition, where CWallet is complete.
    static std::vector<CoinRecord> DecomposeCoins(
        CWallet* wallet, const CWalletTx& wtx, bool& pending_out,
        std::map<uint256, std::string>* walk_memo = nullptr);

private:
    //! One unit of deferred maintenance handed from a producer to the worker.
    struct IntakeItem {
        enum Kind { Upsert, Remove, AddressBook } kind;
        uint256 hash;
        std::vector<CoinRecord> records;
        bool pending{false};
        std::string ab_address;
        std::string ab_label;
    };

    //! Worker entry point: the WalletTxStore park/drain loop, verbatim shape.
    void workerLoop();
    void applyIntake(IntakeItem item);

    //! Worker / tip-refresh core: diff \p records against the stored
    //! outpoints of \p hash and apply removals / inserts / in-place updates.
    //! Takes cs_store.
    void upsertCoins(const uint256& hash, std::vector<CoinRecord> records,
                     bool pending);
    void removeCoins(const uint256& hash);
    void applyAddressBookChange(const std::string& address, const std::string& label);

    //! Remove the record at \p absidx: CoinViews first (positional deltas +
    //! its shift), then compact m_records and shift this class's maps.
    //! Caller holds cs_store.
    void removeRecordAt(std::size_t absidx) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! Translate CoinViews deltas into channel events, push them (only while
    //! a view is registered) and record their seqnos in the per-scope /
    //! floor bookkeeping. \p removed_outpoint rides on Remove deltas so the
    //! payload carries the outpoint for the GUI-side selection prune.
    //! Caller holds cs_store.
    void emitDeltas(const std::vector<CoinViewDelta>& deltas,
                    const COutPoint* removed_outpoint = nullptr)
        EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! Serve-time depth fill on a copied slice (from the atomic tip height).
    void fillDepth(std::vector<CoinRecord>& records) const;

    //! Re-apply the selection mirror's aggregates into CoinViews after a
    //! rebuild cleared them. Caller holds cs_store.
    void reapplyMirrorAggregates() EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! Mirror mutation core shared by the bulk operations: toggle \p absidx
    //! and record the outpoint delta. Caller holds cs_store.
    void toggleLocked(std::size_t absidx, bool selected,
                      CoinBulkSelectionResult& result) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! One step of applyValueFilter's cap pass (a member rather than a lambda
    //! so the thread-safety analyzer can see the held lock — it does not
    //! propagate capabilities into lambda bodies).
    void capPassLocked(std::size_t absidx, uint32_t max_inputs, uint32_t& kept,
                       CoinBulkSelectionResult& result) EXCLUSIVE_LOCKS_REQUIRED(cs_store);

    //! CoinViews projector over m_records[i]. Invoked through the core's
    //! std::function indirection, which the thread-safety analyzer cannot see
    //! the lock through (the WalletTxStore projectFieldsAt precedent); every
    //! call path holds cs_store.
    const CoinRecord& recordAt(std::size_t i) const NO_THREAD_SAFETY_ANALYSIS;

    CWallet* const m_wallet;
    GRC::WalletCoinEventQueue& m_queue;

    mutable Mutex cs_store;

    //! The record table (append + compact-on-remove; CoinViews mirrors the
    //! compaction shifts) and its identity indices.
    std::vector<CoinRecord> m_records GUARDED_BY(cs_store);
    std::unordered_map<COutPoint, std::size_t, OutPointHasher> m_by_outpoint GUARDED_BY(cs_store);
    std::unordered_multimap<uint256, std::size_t, CoinTxHashHasher> m_by_hash GUARDED_BY(cs_store);

    //! The grouped index core (flat/tree/directory positional arithmetic,
    //! aggregates, scope high-waters).
    CoinViews m_views GUARDED_BY(cs_store);

    //! The validated selection mirror (see class comment).
    std::set<COutPoint> m_selected GUARDED_BY(cs_store);

    //! Tx hashes excluded for a depth/time-dependent reason, re-decomposed
    //! at each tip advance.
    std::unordered_set<uint256, CoinTxHashHasher> m_pending GUARDED_BY(cs_store);

    //! Cached tip height for serve-time depth fill (atomic: written under
    //! cs_main by applyChainTipRefresh, read on the Qt thread without locks).
    std::atomic<int> m_tip_height{0};

    std::atomic<bool> m_needs_resync{false};

    //! Intake queue + worker lifecycle: the WalletTxStore double-queue,
    //! verbatim (cs_intake is an independent leaf, never held with cs_store).
    mutable Mutex cs_intake;
    std::deque<IntakeItem> m_intake GUARDED_BY(cs_intake);
    bool m_stop GUARDED_BY(cs_intake){false};
    bool m_rebuilding GUARDED_BY(cs_intake){false};
    bool m_worker_parked GUARDED_BY(cs_intake){false};
    CConditionVariable m_intake_cv;
    CConditionVariable m_idle_cv;
    std::thread m_worker;
    bool m_started{false};
};

} // namespace GRC

#endif // GRIDCOIN_WALLET_WALLETCOINSTORE_H
