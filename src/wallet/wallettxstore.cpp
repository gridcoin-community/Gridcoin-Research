// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/wallettxstore.h"

#include "wallet/txdetail.h"
#include "main.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <iterator>
#include <limits>

namespace {

//! Order a TransactionRecord by projecting it to a TxOrderKey and deferring to
//! the single Qt-free ordering definition. There is exactly one ordering: the
//! GUI-OFF-testable GRC::TxOrderLess.
struct RecordOrder {
    bool operator()(const TransactionRecord& a, const TransactionRecord& b) const
    {
        return GRC::TxOrderLess({a.time, a.hash, a.idx}, {b.time, b.hash, b.idx});
    }
};

//! Project a stored record to the Qt-free filter inputs. `label` is left empty:
//! the Overview view (PR3) does not filter by address substring, and the
//! address-book label needs cs_wallet, which the worker does not hold. The
//! detailed table's address filter populates it in PR4.
GRC::TxFilterFields projectFields(const TransactionRecord& r)
{
    // label is the address-book label snapshotted producer-side (PR4) — the
    // address-substring filter matches address OR label.
    return GRC::TxFilterFields{
        r.time, r.credit + r.debit, static_cast<int>(r.type),
        static_cast<int>(r.status.status), r.address, r.label};
}

//! Project a stored record to the Qt-free sort inputs (windowed-model PR4,
//! decision b — locale-free, so the off-cs_main store sorts these without
//! localizing, the multiprocess-clean choice):
//!  - Type sorts by the (type, generated_type) enum tuple — category-grouped and
//!    language-independent (digits only, so the case-insensitive compare is
//!    byte-safe).
//!  - Address sorts by (label_string, address_string) as two separate keys
//!    (CompareKeys compares label then address); no separator byte to collide with
//!    a control character inside a user label (PR4-fix G).
GRC::SortKey projectKeys(const TransactionRecord& r)
{
    return GRC::SortKey{
        r.time, r.credit + r.debit, r.status.sortKey,
        strprintf("%03d.%03d", static_cast<int>(r.type),
                  static_cast<int>(r.status.generated_type)),
        r.label,
        r.address};
}

//! Per-tip status volatility (PR4-fix A): a record whose displayed status can
//! still change as blocks connect, so it must stay in the per-block refresh set
//! until it reaches a terminal state. Only Confirmed and Offline are terminal.
//!
//! Conflicted and NotAccepted are NOT terminal — a record can land in one of
//! these states transiently and later resolve to an accepted/active state (a
//! coinstake whose block depth/acceptance is not yet established, or a tx that
//! briefly conflicts after a large send consumes overlapping UTXOs). The default
//! detailed/overview filter masks inactive (Conflicted/NotAccepted) rows
//! (show_orphans=false), so such a record is excluded from the view while in that
//! state. If it were ALSO excluded from the refresh set, its cached status would
//! never be re-evaluated and it could never reappear once it resolved — the row
//! would stay hidden permanently. This surfaced as the transaction list
//! "freezing" after a send: staking/sidestake rows show normally until a send,
//! after which the send and the coinstakes that follow it land
//! Conflicted/NotAccepted, get masked, and (without this) never come back.
//! Keeping them volatile lets applyChainTipRefresh -> applyStatusUpdate flip
//! them back into the view once they resolve.
bool recordStatusIsVolatile(const TransactionRecord& r)
{
    // A record is volatile only while the tip sits INSIDE the window where its
    // status can still change:
    //
    //     tx_height <= tip_height <= tx_height + (generated ? 110 : 10)
    //
    // The upper bound is expressed by the terminal statuses below (Confirmed at
    // depth >= RecommendedNumConfirmations, or maturity for generated records).
    // This is the lower bound, and without it the set is inverted exactly when it
    // hurts most.
    //
    // depth == -1 means GetDepthInMainChain found the tx neither in the active
    // chain nor in the mempool. During initial block download that is EVERY
    // transaction in the wallet: mapBlockIndex has not reached their confirming
    // blocks yet, so GetDepthInMainChainINTERNAL returns 0 and the mempool check
    // turns it into -1. Judging volatility by status alone then classifies the
    // whole wallet as Conflicted/NotAccepted -> volatile, and applyChainTipRefresh
    // re-derives every record on every accepted block for the entire sync. The
    // cost is O(blocks x wallet transactions), which is why a sync from zero
    // slowed by 5x with ~1000 records and produced a 156 GB debug.log under
    // -debug=verbose.
    //
    // Polling cannot make such a record converge sooner, because nothing about a
    // new tip changes it -- only its OWN block arriving does. And that arrival is
    // already delivered as an event: CWallet::BlockConnected walks the block's
    // transactions (including ones already in mapWallet, by the explicit
    // mapWallet.find clause) and calls SyncTransaction, which reaches
    // NotifyTransactionChanged(hash, CT_UPDATED). The store refreshes the record
    // there and it re-enters the volatile set with a real depth. The same holds
    // for the reorg paths, which SyncTransaction through TxStateInactive /
    // TxStateInMempool.
    //
    // Deliberately depth < 0 and not depth <= 0: depth == 0 means the tx is in the
    // mempool, which IS a genuinely polled state and is a handful of rows, not the
    // wallet. Keeping those volatile preserves the fix for the transaction list
    // freezing after a send (see the comment above).
    if (r.status.depth < 0) {
        return false;
    }

    switch (r.status.status) {
    case TransactionStatus::OpenUntilDate:
    case TransactionStatus::OpenUntilBlock:
    case TransactionStatus::Unconfirmed:
    case TransactionStatus::Confirming:
    case TransactionStatus::Immature:
    case TransactionStatus::MaturesWarning:
    case TransactionStatus::Conflicted:
    case TransactionStatus::NotAccepted:
        return true;
    case TransactionStatus::Confirmed:
    case TransactionStatus::Offline:
        return false;
    }
    return false;
}

} // anonymous namespace

namespace GRC {

WalletTxStore::WalletTxStore(CWallet* wallet, GRC::WalletEventQueue& queue)
    : m_wallet(wallet)
    , m_queue(queue)
{
}

WalletTxStore::~WalletTxStore()
{
    {
        LOCK(cs_intake);
        m_stop = true;
    }
    m_intake_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void WalletTxStore::start()
{
    // Qt thread, once. Launch the worker that drains the intake queue off the
    // core locks. Idempotent so a double-call (e.g. re-init) is harmless.
    if (m_started) {
        return;
    }
    m_started = true;
    // Backstop: workerLoop() already contains per-item failures, but nothing may
    // escape a std::thread body -- that is std::terminate, i.e. the node dies with
    // no unwinding. Anything reaching here has already defeated the inner handler,
    // so the worker stops; say so loudly rather than exiting silently, because a
    // stopped worker means the store never drains again (#3257).
    m_worker = std::thread([this] {
        try {
            workerLoop();
        } catch (const std::exception& e) {
            LogPrintf("ERROR: WalletTxStore: the store worker stopped after an unhandled "
                      "exception: %s. Transaction list updates will stop until restart.", e.what());
        } catch (...) {
            LogPrintf("ERROR: WalletTxStore: the store worker stopped after an unknown unhandled "
                      "exception. Transaction list updates will stop until restart.");
        }
    });
}

void WalletTxStore::warnIfIntakeBacklogged()
{
    if (m_intake.size() < m_intake_warn_at) {
        return;
    }
    LogPrintf("WARNING: %s: transaction-store intake backlog is %u items "
              "(worker parked=%d, rebuild in progress=%d) — new transactions are "
              "not reaching the GUI transaction views",
              __func__, static_cast<unsigned int>(m_intake.size()),
              static_cast<int>(m_worker_parked), static_cast<int>(m_rebuilding));
    m_intake_warn_at *= 2;
}

void WalletTxStore::enqueueInsert(std::vector<TransactionRecord> records)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Insert, std::move(records), uint256()});
        warnIfIntakeBacklogged();
    }
    m_intake_cv.notify_one();
}

void WalletTxStore::enqueueRemove(const uint256& hash)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Remove, {}, hash});
        warnIfIntakeBacklogged();
    }
    m_intake_cv.notify_one();
}

void WalletTxStore::enqueueUpsert(std::vector<TransactionRecord> records)
{
    if (records.empty()) {
        return;
    }
    const uint256 hash = records.front().hash;
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Update, std::move(records), hash, {}, {}});
        warnIfIntakeBacklogged();
    }
    m_intake_cv.notify_one();
}

void WalletTxStore::enqueueAddressBookChange(const std::string& address, const std::string& label)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::AddressBook, {}, uint256(), address, label});
        warnIfIntakeBacklogged();
    }
    m_intake_cv.notify_one();
}

void WalletTxStore::workerLoop()
{
    WAIT_LOCK(cs_intake, lock);
    while (true) {
        // Wait for work, a stop request, or a rebuild pause. Use the explicit
        // while-condition form (NOT a wait() predicate lambda): the Clang
        // thread-safety analyzer does not propagate the held lock into a lambda
        // body, but it does into this loop, so the guarded reads stay verified.
        while (!m_stop && (m_rebuilding || m_intake.empty())) {
            // While a rebuild is pending, park and tell prime we are
            // idle so it can clear the intake queue and rebuild the index with no
            // concurrent worker mutation.
            if (m_rebuilding && !m_worker_parked) {
                m_worker_parked = true;
                m_idle_cv.notify_all();
            }
            m_intake_cv.wait(lock);
        }
        if (m_stop) {
            return;
        }
        // We have work and are not rebuilding.
        m_worker_parked = false;

        IntakeItem item = std::move(m_intake.front());
        m_intake.pop_front();

        // Drop cs_intake while doing the O(N) store maintenance (which takes
        // cs_store). cs_intake and cs_store are NEVER held simultaneously, so the
        // two leaves cannot invert. The lock re-acquires at the end of this scope
        // before the loop re-evaluates its wait condition.
        {
            REVERSE_LOCK(lock);
            // Contain a failure to the one item that caused it. This body is the
            // raw std::thread entry, so an escaping exception is std::terminate --
            // the whole node killed by, say, a bad_alloc while projecting one
            // record's sort keys. Keep draining instead: dropping a single intake
            // item costs that transaction's row until the next prime() or chain-tip
            // refresh, whereas letting the worker die silently stops the store
            // draining forever and reproduces the #3257 freeze exactly.
            try {
                applyIntake(std::move(item));
            } catch (const std::exception& e) {
                LogPrintf("ERROR: %s: dropping one wallet transaction store intake item after an "
                          "exception: %s", __func__, e.what());
            } catch (...) {
                LogPrintf("ERROR: %s: dropping one wallet transaction store intake item after an "
                          "unknown exception", __func__);
            }
        }
    }
}

void WalletTxStore::applyIntake(IntakeItem item)
{
    // No lock held here; the apply* methods take cs_store internally.
    if (item.kind == IntakeItem::Insert) {
        insertTransaction(std::move(item.records));
    } else if (item.kind == IntakeItem::Update) {
        updateTransaction(std::move(item.records));
    } else if (item.kind == IntakeItem::AddressBook) {
        applyAddressBookChange(item.ab_address, item.ab_label);
    } else {
        removeTransaction(item.hash);
    }
}

void WalletTxStore::shiftIndex(std::size_t from, std::ptrdiff_t delta)
{
    // Bump every index entry at or after `from` by `delta`. Logical positions,
    // not iterators, so this is order-independent w.r.t. the vector splice.
    for (auto& kv : m_by_hash) {
        if (kv.second >= from) {
            kv.second = static_cast<std::size_t>(
                static_cast<std::ptrdiff_t>(kv.second) + delta);
        }
    }
}

void WalletTxStore::rebuildIndex()
{
    m_by_hash.clear();
    m_by_hash.reserve(m_records.size() * 2 + 1);
    for (std::size_t i = 0; i < m_records.size(); ++i) {
        m_by_hash.emplace(m_records[i].hash, i);
    }
}

void WalletTxStore::rebuildCaches()
{
    // Everything here is derived from m_records, which is the authoritative table:
    // the projector caches are parallel to it by position, and the volatile set is
    // the subset whose status can still change. Shared by prime() (which rebuilds
    // the whole store) and by insertLocked()'s failure path (which must restore the
    // cross-container invariant after a partially applied splice).
    m_fields_cache.assign(m_records.size(), TxFilterFields{});
    m_keys_cache.assign(m_records.size(), SortKey{});
    m_volatile.clear();
    for (std::size_t i = 0; i < m_records.size(); ++i) {
        recomputeCacheAt(i);
        if (isVolatile(m_records[i])) {
            m_volatile.insert(m_records[i].hash);
        }
    }
}

void WalletTxStore::insertTransaction(std::vector<TransactionRecord> records)
{
    LOCK(cs_store);
    insertLocked(std::move(records));
}

void WalletTxStore::insertLocked(std::vector<TransactionRecord> records)
{
    // Datetime-display cutoff (cached from the last prime). All
    // records of one tx share `time`, so this is all-or-nothing.
    if (m_limit_enabled) {
        // Hoist the guarded member into a local read under cs_store: the Clang
        // thread-safety analyzer does not propagate held-lock state into the
        // lambda body, so capture the value rather than read m_limit_time inside
        // the predicate.
        const int64_t limit_time = m_limit_time;
        records.erase(std::remove_if(records.begin(), records.end(),
                          [limit_time](const TransactionRecord& r) { return r.time < limit_time; }),
                      records.end());
    }
    if (records.empty()) {
        return;
    }

    // Sort defensively by the ordering key. decomposeTransaction already
    // produces idx order (the third-level tiebreaker for same time+hash), so
    // this is normally a no-op.
    std::sort(records.begin(), records.end(), RecordOrder());

    const uint256 hash = records.front().hash;

    // Dedup: a present hash means the full record set of this tx is already in
    // the store (all parts arrive in one call), so skip — no event.
    if (m_by_hash.find(hash) != m_by_hash.end()) {
        return;
    }

    // All records share `time` and `hash` and differ only in `idx`; under
    // TxOrderLess they form a contiguous range slotting in at the lower_bound
    // of the first record.
    const auto pos = std::lower_bound(m_records.begin(), m_records.end(), records.front(), RecordOrder());
    const std::size_t insertIdx = static_cast<std::size_t>(pos - m_records.begin());
    const std::size_t count = records.size();

    // Project the new cache entries and reserve capacity BEFORE mutating anything,
    // so the common allocation failure (a reallocation of one of the four
    // containers) happens while the store is still untouched.
    //
    // This does NOT make the splices allocation-free, and it would be wrong to
    // claim it does: vector::insert COPIES the elements, and TransactionRecord,
    // TxFilterFields and SortKey all hold std::strings, so each copied element
    // allocates. reserve() removes the reallocation, not the per-element
    // allocation. The splices therefore remain throwing operations, which is why
    // the recovery below has to be real rather than decorative.
    //
    // This used to interleave the two: shiftIndex() rewrote m_by_hash, then
    // m_records was spliced, then the projector caches were BUILT (allocating) and
    // spliced. A bad_alloc anywhere in that run left the store permanently
    // inconsistent -- an index shifted for records that were never inserted, or
    // caches at a different length than m_records -- which is exactly what feeds
    // the unchecked subscripts elsewhere in this file. There is no unwinding that
    // repairs it, because the damage is to the invariant between four containers.
    //
    // So: project first, reserve capacity in all four containers, and only then
    // mutate. After the reserves, the vector splices do not allocate (they move
    // existing elements within capacity) and the map has its buckets already.
    std::vector<TxFilterFields> new_fields;
    std::vector<SortKey> new_keys;
    new_fields.reserve(count);
    new_keys.reserve(count);
    for (const TransactionRecord& r : records) {
        new_fields.push_back(projectFields(r));
        new_keys.push_back(projectKeys(r));
    }

    m_records.reserve(m_records.size() + count);
    m_fields_cache.reserve(m_fields_cache.size() + count);
    m_keys_cache.reserve(m_keys_cache.size() + count);
    m_by_hash.reserve(m_by_hash.size() + count);

    // From here the sequence is treated as one transaction. A throw would still be
    // possible in principle (a map node allocation), and the containers are
    // cross-dependent, so recover by rebuilding everything derived from m_records
    // -- which is the authoritative table and is fully updated by then -- rather
    // than leaving a half-applied insert behind.
    try {
        // Shift the index BEFORE splicing the vector (the shift uses logical
        // positions), then splice, then add the new index entries — keeping the
        // index consistent at every observable point. The records are copied into
        // the store (the authoritative full-records table) and then moved into the
        // event.
        shiftIndex(insertIdx, static_cast<std::ptrdiff_t>(count));
        m_records.insert(m_records.begin() + insertIdx, records.begin(), records.end());
        // Splice the projector caches at the same position/order (PR4-fix F). Done
        // BEFORE the cursor drive, which reads the cache through applyStoreInsert.
        m_fields_cache.insert(m_fields_cache.begin() + insertIdx, new_fields.begin(), new_fields.end());
        m_keys_cache.insert(m_keys_cache.begin() + insertIdx, new_keys.begin(), new_keys.end());
        for (std::size_t k = 0; k < count; ++k) {
            m_by_hash.emplace(hash, insertIdx + k);
        }
    } catch (...) {
        // Rebuild EVERYTHING derived from m_records, cursors included.
        //
        // The cursors are the part that is easy to forget and the part that hurts:
        // their view_index holds ABSOLUTE m_records positions, so a splice that
        // threw part-way leaves every cursor entry at or after insertIdx short by
        // count. Repairing only m_by_hash and the projector caches produces a
        // self-consistent index over a record table the cursors no longer address
        // correctly -- and because the resulting indices are still IN RANGE, the
        // bounds checks in getRows()/getAllRows() never fire. The GUI would then
        // show the wrong transaction in every row from the insert point on,
        // silently and permanently, until the next prime(). That is strictly worse
        // than the std::terminate this exception handling replaced, so the
        // recovery has to match what prime() does.
        //
        // Note m_records itself may be in a valid-but-unspecified state if the
        // throw came from its own splice (libstdc++ grows the vector before
        // copying into the gap). Rebuilding from it is still the right move: it is
        // the only table we have, and a consistent view of slightly wrong rows
        // that every consumer then RESETS onto beats a torn index. The Resets
        // pushed below are what make the consumers re-read rather than trust their
        // replicas.
        LogPrintf("ERROR: %s: exception while splicing %u record(s) for hash %s; rebuilding the "
                  "index, projector caches and cursors from the record table, and resetting "
                  "every view",
                  __func__, static_cast<unsigned int>(count), hash.GetHex());
        rebuildIndex();
        rebuildCaches();
        for (auto& [viewId, cursor] : m_cursors) {
            cursor.rebuild(m_records.size());
            m_view_seqno[viewId] = m_queue.push(GRC::RowsResetPayload{
                viewId, cursor.epoch(), static_cast<int>(cursor.servedCount())});
        }
        throw;
    }
    updateVolatileForHash(hash);   // track for the per-tip status refresh (PR4-fix A)

    // Push the position-stamped event WHILE cs_store is held so that queue
    // seqno-order equals store-mutation-order across all producer threads.
    m_queue.push(GRC::RowsInsertedPayload{static_cast<int>(insertIdx), std::move(records)});

    // Drive each registered cursor: the new records now occupy
    // [insertIdx, insertIdx+count) in m_records (cursors index into m_records).
    for (auto& [viewId, cursor] : m_cursors) {
        emitCursorDeltas(viewId, cursor.epoch(), cursor.applyStoreInsert(insertIdx, count));
    }
}

void WalletTxStore::removeTransaction(const uint256& hash)
{
    LOCK(cs_store);
    removeLocked(hash);
}

bool WalletTxStore::locateHashRange(const uint256& hash, std::size_t& minPos,
                                    std::size_t& maxPos, std::size_t& count) const
{
    // Same-hash keys are contiguous under TxOrderLess (the hash tiebreaker
    // clusters them), so min/max bound a single range. The erase in removeLocked
    // depends on that; validate it at runtime rather than with an assert, since the
    // deployed build is -DNDEBUG and a de-clustered index would erase a wider
    // [minPos, maxPos] range that swallows foreign rows. The bounds check is
    // ordered first and short-circuits, so the m_records[] subscripts below it
    // never run out of range.
    auto range = m_by_hash.equal_range(hash);
    if (range.first == range.second) {
        return false;
    }
    minPos = std::numeric_limits<std::size_t>::max();
    maxPos = 0;
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < minPos) minPos = it->second;
        if (it->second > maxPos) maxPos = it->second;
    }
    count = maxPos - minPos + 1;
    const std::size_t distance =
        static_cast<std::size_t>(std::distance(range.first, range.second));
    return maxPos < m_records.size()
        && count == distance
        && m_records[minPos].hash == hash
        && m_records[maxPos].hash == hash;
}

bool WalletTxStore::removeLocked(const uint256& hash)
{
    std::size_t minPos = 0;
    std::size_t maxPos = 0;
    std::size_t count = 0;

    if (!locateHashRange(hash, minPos, maxPos, count)) {
        if (m_by_hash.find(hash) == m_by_hash.end()) {
            // Not present — filtered out at insert time, or never inserted. No-op,
            // no event. Report success: the caller's goal (this hash is gone) holds.
            return true;
        }
        // The index disagrees with m_records. m_by_hash is derived purely from
        // m_records, and m_records is kept in RecordOrder (which clusters a
        // transaction's parts), so rebuilding it from the records restores the
        // invariant by construction. Recover rather than bail: the old code
        // returned here, and updateTransaction's remove+insert fallback then hit
        // insertLocked's hash dedup on the still-present stale entries and
        // returned too, so the transaction became permanently un-updatable,
        // un-removable and un-reinsertable (#3257 review).
        LogPrintf("ERROR: %s: hash %s index non-contiguous/out-of-range "
                  "(minPos=%u maxPos=%u count=%u records=%u) — rebuilding the hash index",
                  __func__, hash.GetHex(),
                  static_cast<unsigned int>(minPos), static_cast<unsigned int>(maxPos),
                  static_cast<unsigned int>(count),
                  static_cast<unsigned int>(m_records.size()));
        rebuildIndex();
        if (!locateHashRange(hash, minPos, maxPos, count)) {
            if (m_by_hash.find(hash) == m_by_hash.end()) {
                return true;   // the rebuild showed it was never really there
            }
            LogPrintf("ERROR: %s: hash %s still inconsistent after an index rebuild "
                      "— skipping remove", __func__, hash.GetHex());
            return false;
        }
    }

    m_records.erase(m_records.begin() + minPos, m_records.begin() + maxPos + 1);
    // Erase the parallel projector caches over the same range (PR4-fix F).
    m_fields_cache.erase(m_fields_cache.begin() + minPos, m_fields_cache.begin() + maxPos + 1);
    m_keys_cache.erase(m_keys_cache.begin() + minPos, m_keys_cache.begin() + maxPos + 1);
    m_by_hash.erase(hash);
    m_volatile.erase(hash);   // no longer present -> not in the per-tip refresh set
    shiftIndex(maxPos + 1, -static_cast<std::ptrdiff_t>(count));

    m_queue.push(GRC::RowsRemovedPayload{static_cast<int>(minPos), static_cast<int>(count)});

    // Drive each registered cursor: [minPos, minPos+count) were erased from
    // m_records and later positions shifted down by count.
    for (auto& [viewId, cursor] : m_cursors) {
        emitCursorDeltas(viewId, cursor.epoch(), cursor.applyStoreRemove(minPos, count));
    }
    return true;
}

void WalletTxStore::prime(bool limit_enabled, int64_t limit_time)
{
    std::vector<TransactionRecord> built;

    // The park is released by an RAII guard, NOT by falling off the end of this
    // function. m_rebuilding is the worker's wait predicate, so if anything below
    // throws — and plenty can: decomposeTransaction calls GetCredit/GetDebit,
    // which throw std::runtime_error outside MoneyRange, and updateStatus reaches
    // GetGeneratedType, which hits the tx index and reads a block from disk — the
    // worker would stay parked FOREVER. Producers would keep enqueuing
    // successfully and m_intake would grow without bound, while the inline
    // applyChainTipRefresh kept refreshing already-stored rows: the GUI would show
    // existing transactions ripening normally and never show a new one again
    // (#3257).
    //
    // Declared before the LOCK2 below so it is destroyed AFTER those locks are
    // released: the guard takes cs_intake, and producers take cs_intake while
    // holding cs_main, so releasing in this order never nests the two.
    struct RebuildPark {
        WalletTxStore& s;
        ~RebuildPark()
        {
            LOCK(s.cs_intake);
            s.m_rebuilding = false;
            s.m_worker_parked = false;
            s.m_intake_cv.notify_all();
        }
    } park_guard{*this};

    // Quiesce the store-worker (PR2.5) BEFORE taking cs_main/cs_wallet. The worker
    // is an independent store mutator that those locks do not exclude, so it has
    // to be parked either way — but the wait must not happen underneath them.
    //
    // It used to: the LOCK2 came first and this wait ran while holding both. That
    // was sound only because the worker happens to need nothing but cs_store to
    // reach its park point, and nothing enforced it. The day any worker path
    // acquired cs_main — held here by this thread, so a recursive mutex does not
    // help another thread — the worker could never park, this wait would never
    // return, and the node would sit on cs_main forever: no RPC, no P2P, no
    // staking. Parking first removes the hazard by construction instead of
    // relying on a property a future refactor could quietly break. It also stops
    // the wait itself from counting against cs_main hold time.
    //
    // m_started guards the pre-start() first reload (no worker yet → nothing to
    // wait for).
    {
        WAIT_LOCK(cs_intake, ilock);
        m_rebuilding = true;
        m_intake_cv.notify_all();   // wake the worker so it observes m_rebuilding
        while (m_started && !m_worker_parked) {
            m_idle_cv.wait(ilock);  // wait until the worker confirms it has parked
        }
    }

    // Hold cs_main + cs_wallet across the whole rebuild AND the queue drain,
    // exactly as the old loadWallet held them for its scan. cs_main is the
    // load-bearing exclusion lock here: EVERY producer of insert/remove holds
    // cs_main — CT_NEW/CT_UPDATED under cs_main+cs_wallet, and BOTH CT_DELETED
    // sites (ReorganizeChain in main.cpp, ResendWalletTransactions in
    // wallet.cpp) under cs_main even though they do NOT hold cs_wallet at the
    // fire site. So holding cs_main across both the index rebuild and the
    // queue drain fully excludes producers: no event can be queued between the
    // two, which makes the rebuilt index, the re-armed cursors, and the emptied
    // queue mutually consistent.
    //
    // NOTE this is a full wallet rescan under cs_main, and it is reachable at
    // runtime from the GUI (WalletModel::reloadTransactionView, wired to the
    // datetime-cutoff option). In a multiprocess split that blocks the NODE's
    // cs_main for the duration. It is inherent to the current design — the cutoff
    // is applied at build time, so records below it are never stored and widening
    // it genuinely requires a rescan. Storing every record and filtering in the
    // cursors would remove the rescan entirely; that is a windowed-model change,
    // not a locking fix, and is deliberately not attempted here.
    LOCK2(cs_main, m_wallet->cs_wallet);

    // Discard intake queued between the park above and cs_main here. Producers
    // hold cs_main, so once we own it nothing further can be enqueued, and the
    // clear + rebuild below observe the same quiescent state the old ordering
    // gave (which cleared under both locks).
    WITH_LOCK(cs_intake, m_intake.clear());

    built.reserve(m_wallet->mapWallet.size() * 2);
    for (auto it = m_wallet->mapWallet.begin(); it != m_wallet->mapWallet.end(); ++it) {
        if (!TransactionRecord::showTransaction(it->second)) {
            continue;
        }
        const std::vector<TransactionRecord> decomposed =
            TransactionRecord::decomposeTransaction(m_wallet, it->second);
        for (const TransactionRecord& rec : decomposed) {
            if (limit_enabled && rec.time < limit_time) {
                continue;
            }
            // Compute status producer-side (cs_main held here) so the cursors
            // rebuilt over m_records can filter/sort by it and the served records
            // carry current status. Consumers no longer refresh status lazily on
            // read — the TransactionTablePriv::index() lazy path was removed in
            // windowed-model PR5-C — so this producer-side computation is the
            // authoritative refresh, not a head-start.
            TransactionRecord r = rec;
            r.updateStatus(it->second);
            r.populateDisplayLabel(*m_wallet);  // address-book label snapshot (PR4)
            built.push_back(std::move(r));
        }
    }
    std::sort(built.begin(), built.end(), RecordOrder());

    std::vector<GRC::RowsResetPayload> cursor_resets;
    {
        LOCK(cs_store);
        m_limit_enabled = limit_enabled;
        m_limit_time = limit_time;
        m_records = std::move(built);
        rebuildIndex();
        // Rebuild the projector caches and the volatile set parallel to m_records
        // (PR4-fix F/A) BEFORE the cursors rebuild — cursor.rebuild() reads the
        // cache through the projectors.
        rebuildCaches();
        // Rebuild each registered cursor over the new m_records. Their Reset
        // events are pushed AFTER the drain below (which discards pre-rebuild
        // events), so the windowed consumers refill against the new snapshot.
        for (auto& [viewId, cursor] : m_cursors) {
            cursor.rebuild(m_records.size());
            cursor_resets.push_back(GRC::RowsResetPayload{
                viewId, cursor.epoch(), static_cast<int>(cursor.servedCount())});
        }
    }

    // Discard any events queued before this rebuild: they were computed against
    // the old index and are superseded by the freshly scanned records now in
    // m_records. Producers are still blocked on cs_wallet, so nothing new can be
    // queued until we return; the rebuilt index and the now-empty queue are
    // consistent.
    m_queue.drain();

    // Re-publish the per-view cursor Resets AFTER the drain so they survive it;
    // the windowed consumers (e.g. OverviewTxModel) refill via getRows. Producers
    // are still blocked on cs_wallet, so these are the only queued events until
    // we return. Record each Reset's seqno as the view high-water (PR4-fix B) so a
    // consumer's reset-refetch knows exactly what it reflects; re-taking cs_store
    // here is contention-free (producers blocked, worker parked).
    {
        LOCK(cs_store);
        for (const GRC::RowsResetPayload& reset : cursor_resets) {
            m_view_seqno[reset.viewId] = m_queue.push(reset);
        }
    }

    // The store-worker is released by park_guard's destructor as this function
    // returns (PR2.5): the rebuilt index is live, so it resumes draining.
    // Producers remain blocked on cs_wallet until we return, so the worker has
    // nothing to apply until the snapshot is installed.
}

void WalletTxStore::updateTransaction(std::vector<TransactionRecord> records)
{
    LOCK(cs_store);

    if (records.empty()) {
        return;
    }
    // Capture the hash BEFORE the datetime-cutoff erase below: all parts of a tx
    // share `time`, so the cutoff is all-or-nothing, and on an empty result we
    // still need the hash to evict any rows the tx previously had.
    const uint256 hash = records.front().hash;

    if (m_limit_enabled) {
        const int64_t limit_time = m_limit_time;
        records.erase(std::remove_if(records.begin(), records.end(),
                          [limit_time](const TransactionRecord& r) { return r.time < limit_time; }),
                      records.end());
    }
    if (records.empty()) {
        // The whole tx fell behind the datetime cutoff: if it was present it must
        // go (its parts are now hidden). removeLocked is a no-op if absent, and
        // drives the cursors + emits the removal events.
        removeLocked(hash);
        return;
    }
    std::sort(records.begin(), records.end(), RecordOrder());

    auto range = m_by_hash.equal_range(hash);
    if (range.first == range.second) {
        // Not present (previously filtered, or just became visible) -> insert.
        insertLocked(std::move(records));
        return;
    }

    std::size_t minPos = std::numeric_limits<std::size_t>::max();
    std::size_t maxPos = 0;
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < minPos) minPos = it->second;
        if (it->second > maxPos) maxPos = it->second;
    }
    const std::size_t stored = maxPos - minPos + 1;

    // Structural guard (mirrors removeLocked): same-hash records must be one
    // contiguous run AND the part count must be unchanged for an in-place
    // overwrite. If the part count changed or the run is broken, fall back to
    // remove+insert rather than overwrite mismatched slots.
    if (maxPos >= m_records.size()
            || stored != records.size()
            || static_cast<std::size_t>(std::distance(range.first, range.second)) != stored
            || m_records[minPos].hash != hash
            || m_records[maxPos].hash != hash) {
        if (!removeLocked(hash)) {
            // The index could not be reconciled even after a rebuild. Do NOT fall
            // through to insertLocked: its hash dedup would see the stale entries
            // and return silently, dropping the update with no event and no
            // volatility refresh — permanently, for this hash (#3257 review).
            LogPrintf("ERROR: %s: hash %s could not be removed for the re-insert "
                      "fallback — dropping this update", __func__, hash.GetHex());
            return;
        }
        insertLocked(std::move(records));
        return;
    }

    // In-place overwrite: the status changed but the ordering key (time,hash,idx)
    // did not, so positions are unchanged. The change is propagated to each
    // registered cursor below via RowsChanged (the detailed/overview views refetch
    // the affected rows). No VIEW_FULL event is emitted, so the TransactionTableModel
    // replica is NOT refreshed for in-place status changes; post-PR5-C there is also
    // no lazy on-read refresh (TransactionTablePriv::index() no longer touches the
    // wallet). Acceptable because no view renders TTM and its only readers —
    // incomingTransaction (fresh inserts), focusTransaction (TxIDRole), indexForTxid
    // (hash) — do not depend on post-insert status freshness. Re-evaluate each cursor
    // per affected row: under a status sort a first confirmation repositions the row.
    //
    // Overwrite + re-drive ONE part at a time. applyStatusUpdate repositions a row
    // via lower_bound, which needs the rest of view_index sorted; for a multi-part
    // tx, recomputing ALL parts' keys first (then driving) would leave the
    // not-yet-repositioned siblings mis-keyed in their old slots and break that
    // precondition under a Status sort — producing a mis-sorted view_index and
    // inconsistent deltas that duplicate a row in the consumer caches. Interleave to
    // keep every untouched part at its consistent prior key/slot, exactly as
    // applyChainTipRefresh (PR4-fix A) and applyAddressBookChange (PR4-fix C) do, and
    // as the interleaved_reposition_to_equal_keys cursor test pins. Positions in
    // m_records are stable across the loop (status is not part of RecordOrder).
    // (Single-part txs are unaffected: the loop body runs once either way.)
    //
    for (std::size_t k = 0; k < records.size(); ++k) {
        m_records[minPos + k] = records[k];
        recomputeCacheAt(minPos + k);   // refresh F-cache before the cursor drive reads it
        for (auto& [viewId, cursor] : m_cursors) {
            emitCursorDeltas(viewId, cursor.epoch(), cursor.applyStatusUpdate(minPos + k));
        }
    }
    updateVolatileForHash(hash);        // status may have crossed a maturity threshold (PR4-fix A)
}

void WalletTxStore::applyAddressBookChange(const std::string& address, const std::string& label)
{
    LOCK(cs_store);
    // Re-snapshot the address-book label on every stored record for this address,
    // refresh its cached projector outputs, and re-drive the cursors — the label is
    // the Address-column sort key and an address-substring filter target, so a
    // record's filter membership and/or sort slot can change. Do this ONE record at
    // a time (recompute its cache, then reposition it in every cursor) rather than
    // recomputing all affected caches up front: applyStatusUpdate repositions via
    // lower_bound, which needs the rest of view_index sorted, and recomputing all
    // same-address keys first would leave several rows mis-keyed in their old slots
    // (transiently unsorted) and break lower_bound under an Address sort. Positions
    // in m_records are stable (the label is not part of RecordOrder). (PR4-fix C.)
    for (std::size_t i = 0; i < m_records.size(); ++i) {
        if (m_records[i].address != address) {
            continue;
        }
        m_records[i].label = label;
        recomputeCacheAt(i);
        for (auto& [viewId, cursor] : m_cursors) {
            emitCursorDeltas(viewId, cursor.epoch(), cursor.applyStatusUpdate(i));
        }
    }
}

void WalletTxStore::applyChainTipRefresh()
{
    // Caller holds cs_main (EXCLUSIVE_LOCKS_REQUIRED). Acquire cs_wallet (recursive
    // — re-entrant if SetBestChain already holds it) for mapWallet + updateStatus,
    // then cs_store. Canonical cs_main -> cs_wallet -> cs_store. cs_main mutually
    // excludes this from prime (which holds cs_main on the Qt thread),
    // and the worker never needs cs_main/cs_wallet, so there is no deadlock with
    // the rebuild park protocol.
    AssertLockHeld(cs_main);   // fail fast on misuse (this runs off boost::signals2)
    LOCK(m_wallet->cs_wallet);
    LOCK(cs_store);
    if (m_volatile.empty()) {
        return;
    }
    // Copy the set: updateVolatileForHash() mutates m_volatile as matured txs drop
    // out, and we must not iterate it while erasing.
    const std::vector<uint256> hashes(m_volatile.begin(), m_volatile.end());
    LogPrint(BCLog::LogFlags::VERBOSE,
             "applyChainTipRefresh: refreshing %u volatile transactions over %u records",
             static_cast<unsigned int>(hashes.size()),
             static_cast<unsigned int>(m_records.size()));
    for (const uint256& hash : hashes) {
        auto range = m_by_hash.equal_range(hash);
        if (range.first == range.second) {
            m_volatile.erase(hash);
            continue;
        }
        auto wit = m_wallet->mapWallet.find(hash);
        if (wit == m_wallet->mapWallet.end()) {
            // Vanished from the wallet (a CT_DELETED is in flight) — drop it; the
            // removal event will clean up the rows.
            m_volatile.erase(hash);
            continue;
        }
        const CWalletTx& wtx = wit->second;
        // A tx's parts are contiguous in m_records; collect + sort their positions.
        std::vector<std::size_t> positions;
        for (auto it = range.first; it != range.second; ++it) {
            // Bounds-check, as every other m_by_hash consumer in this file does.
            // This one runs inline on the core thread for every volatile record on
            // every block, and recomputeCacheAt below WRITES three parallel
            // vectors at the same index — an unvalidated position here is a heap
            // write, not just a bad read (#3257 review).
            if (it->second >= m_records.size()) {
                LogPrintf("ERROR: %s: hash %s maps to record %u of %u — skipping "
                          "this part's refresh", __func__, hash.GetHex(),
                          static_cast<unsigned int>(it->second),
                          static_cast<unsigned int>(m_records.size()));
                continue;
            }
            positions.push_back(it->second);
        }
        std::sort(positions.begin(), positions.end());
        // Refresh + re-drive ONE part at a time. applyStatusUpdate repositions a row
        // via lower_bound, which needs the rest of view_index sorted; recomputing ALL
        // parts' keys first would leave the not-yet-repositioned parts mis-keyed in
        // their old slots and break that precondition under a Status sort. Interleaving
        // keeps every untouched part at its consistent prior key/slot. Positions in
        // m_records are stable across the loop (status is not part of RecordOrder).
        //
        // Guarded per hash. updateStatus() reaches a long way for something running
        // inline on the core thread: IsTrusted() and GetBlocksToMaturity() walk the
        // wallet, and GetGeneratedType() goes to the tx index and reads a block from
        // disk. Any of that can throw — GetCredit/GetDebit raise std::runtime_error
        // outside MoneyRange, and the disk paths raise their own. Unguarded, one bad
        // record aborts the whole loop, so every hash AFTER it in the iteration order
        // is skipped — and not just once. m_volatile is an unordered_set, so a given
        // hash's position is fixed by its bucket for as long as the bucket count
        // holds: the same records get skipped block after block, and only a rehash
        // or a prime() reshuffles which ones.
        // Those rows would then never leave NotAccepted/Immature, and since both
        // production views mask inactive rows, every staked block behind the poison
        // record would stay invisible until a prime() rebuilt the volatile set.
        // One unrefreshable transaction must not freeze status refresh for the rest
        // of the wallet (#3257).
        try {
            for (std::size_t p : positions) {
                // Status transition + per-view delta count. Together these split the
                // "a staked block never appears" failure three ways in one line:
                //   before == after == NotAccepted  -> updateStatus is not clearing it
                //   status flips but 0 deltas       -> the cursor is not flipping it in
                //   status flips and a delta fires  -> the loss is downstream (queue,
                //                                      drain pump or the Qt consumer)
                // VERBOSE, so it costs nothing unless a user is chasing this (#3257).
                const int before = static_cast<int>(m_records[p].status.status);
                m_records[p].updateStatus(wtx);
                const int after = static_cast<int>(m_records[p].status.status);
                recomputeCacheAt(p);
                for (auto& [viewId, cursor] : m_cursors) {
                    const std::vector<CursorDelta> deltas = cursor.applyStatusUpdate(p);
                    LogPrint(BCLog::LogFlags::VERBOSE,
                             "applyChainTipRefresh: %s part %d record %u status %d->%d "
                             "view %d emitted %u deltas",
                             hash.GetHex(), m_records[p].idx,
                             static_cast<unsigned int>(p), before, after, viewId,
                             static_cast<unsigned int>(deltas.size()));
                    emitCursorDeltas(viewId, cursor.epoch(), deltas);
                }
            }
            updateVolatileForHash(hash);   // drops the hash once every part is terminal
        } catch (const std::exception& e) {
            // Default log level, not VERBOSE: this is the line that names the
            // offending transaction, and it must be present in a user's ordinary
            // debug.log without them having to reproduce under -debug=verbose.
            LogPrintf("ERROR: %s: refreshing hash %s threw (%s) — skipping it this "
                      "tip; the remaining %u volatile transactions still refresh",
                      __func__, hash.GetHex(), e.what(),
                      static_cast<unsigned int>(m_volatile.size()));
        } catch (...) {
            LogPrintf("ERROR: %s: refreshing hash %s threw a non-standard exception "
                      "— skipping it this tip", __func__, hash.GetHex());
        }
    }
}

const TxFilterFields& WalletTxStore::projectFieldsAt(std::size_t i) const
{
    return m_fields_cache[i];
}

const SortKey& WalletTxStore::projectKeysAt(std::size_t i) const
{
    return m_keys_cache[i];
}

void WalletTxStore::makeCursorProjectors(Cursor::FieldsFn& fields, Cursor::KeysFn& keys)
{
    // Explicit reference return types: without them the lambda would deduce a
    // by-value return and copy the cached key on every comparison, defeating F.
    fields = [this](std::size_t i) -> const TxFilterFields& { return projectFieldsAt(i); };
    keys = [this](std::size_t i) -> const SortKey& { return projectKeysAt(i); };
}

void WalletTxStore::recomputeCacheAt(std::size_t i)
{
    // The three vectors are maintained in lockstep, so this should be
    // unreachable — but it WRITES, and every caller derives `i` from m_by_hash or
    // from a cursor position, so validate rather than trust (#3257 review).
    if (i >= m_records.size() || i >= m_fields_cache.size() || i >= m_keys_cache.size()) {
        LogPrintf("ERROR: %s: index %u out of range (records=%u fields=%u keys=%u)",
                  __func__, static_cast<unsigned int>(i),
                  static_cast<unsigned int>(m_records.size()),
                  static_cast<unsigned int>(m_fields_cache.size()),
                  static_cast<unsigned int>(m_keys_cache.size()));
        return;
    }
    m_fields_cache[i] = projectFields(m_records[i]);
    m_keys_cache[i] = projectKeys(m_records[i]);
}

bool WalletTxStore::isVolatile(const TransactionRecord& r)
{
    return recordStatusIsVolatile(r);
}

void WalletTxStore::updateVolatileForHash(const uint256& hash)
{
    auto range = m_by_hash.equal_range(hash);
    bool volatile_now = false;
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second >= m_records.size()) {
            continue;   // stale index entry; removeLocked logs and repairs it
        }
        if (isVolatile(m_records[it->second])) {
            volatile_now = true;
            break;
        }
    }
    if (volatile_now) {
        m_volatile.insert(hash);
    } else {
        m_volatile.erase(hash);
    }
}

void WalletTxStore::registerView(int viewId, FilterSpec filter, int sort_column, int sort_order)
{
    LOCK(cs_store);
    Cursor::FieldsFn fields;
    Cursor::KeysFn keys;
    makeCursorProjectors(fields, keys);
    Cursor cursor(viewId, std::move(filter), sort_column, sort_order,
                  std::move(fields), std::move(keys));
    const auto deltas = cursor.rebuild(m_records.size());
    const uint64_t epoch = cursor.epoch();
    m_cursors.insert_or_assign(viewId, std::move(cursor));
    emitCursorDeltas(viewId, epoch, deltas);
}

void WalletTxStore::unregisterView(int viewId)
{
    // Drop the view's cursor. Called when a consumer view tears down (Phase
    // 1c-ii-c) so its per-view index no longer tracks store mutations. No event
    // is emitted: the consumer is going away and will not drain again.
    // Idempotent — erasing an absent viewId is a no-op, so a double teardown or
    // a never-registered view is harmless.
    LOCK(cs_store);
    m_cursors.erase(viewId);
}

void WalletTxStore::setViewSort(int viewId, int sort_column, int sort_order)
{
    LOCK(cs_store);
    auto it = m_cursors.find(viewId);
    if (it == m_cursors.end()) return;
    // Sequence the mutation before reading epoch(): setSort() bumps the cursor
    // epoch, and C++ leaves function-argument evaluation order unspecified, so
    // computing the deltas first guarantees the emitted events carry the NEW epoch.
    const auto deltas = it->second.setSort(sort_column, sort_order);
    emitCursorDeltas(viewId, it->second.epoch(), deltas);
}

void WalletTxStore::setViewFilter(int viewId, FilterSpec filter)
{
    LOCK(cs_store);
    auto it = m_cursors.find(viewId);
    if (it == m_cursors.end()) return;
    // setFilter() bumps the epoch (Reset); sequence before reading epoch().
    const auto deltas = it->second.setFilter(filter, m_records.size());
    emitCursorDeltas(viewId, it->second.epoch(), deltas);
}

void WalletTxStore::setViewLimit(int viewId, int limit)
{
    LOCK(cs_store);
    auto it = m_cursors.find(viewId);
    if (it == m_cursors.end()) return;
    // setLimit() does not bump the epoch, but sequence for consistency/safety.
    const auto deltas = it->second.setLimit(limit);
    emitCursorDeltas(viewId, it->second.epoch(), deltas);
}

RowsResult WalletTxStore::getRows(int viewId, int first, int count)
{
    LOCK(cs_store);
    RowsResult res;
    auto it = m_cursors.find(viewId);
    if (it == m_cursors.end()) {
        return res;   // unknown view: empty slice, zero metadata
    }
    const Cursor& cur = it->second;
    // Sample ALL of the view's metadata under THIS single cs_store hold, together
    // with the row copy below — total_accepted (virtual rowCount), epoch (sort/
    // filter generation) and high_water (last emitted seqno) are mutually
    // consistent with the returned rows. A caller must NOT re-sample any of these
    // via a separate locked call, or a worker insert/remove landing between the
    // two locks could misalign the slice and drop or double-count a row that the
    // seqno skip would then make permanent (PR4-fix B, generalized to PR5 scroll).
    // Clamp to INT_MAX: total_accepted feeds Qt's int-based rowCount downstream. A
    // wallet can never hold 2^31 rows, but the saturating cast keeps the count
    // well-defined rather than wrapping negative if it ever did (Copilot PR5-A).
    res.total_accepted = static_cast<int>(
        std::min<std::size_t>(cur.totalAccepted(),
                              static_cast<std::size_t>(std::numeric_limits<int>::max())));
    res.epoch = cur.epoch();
    auto sit = m_view_seqno.find(viewId);
    res.high_water = (sit == m_view_seqno.end()) ? 0 : sit->second;

    if (first < 0 || count == 0) {
        return res;   // metadata valid, slice empty by request
    }
    const std::size_t served = cur.servedCount();
    const std::size_t begin = static_cast<std::size_t>(first);
    if (begin >= served) {
        return res;   // off the served window: metadata valid, slice empty
    }
    // count < 0 means "all served rows from `first`".
    const std::size_t end = (count < 0)
        ? served
        : std::min(served, begin + static_cast<std::size_t>(count));
    res.records.reserve(end - begin);
    for (std::size_t i = begin; i < end; ++i) {
        const std::size_t absidx = cur.rowAt(i);
        if (absidx >= m_records.size()) {
            LogPrintf("ERROR: %s: view %d served row %u maps to record %u of %u — skipping",
                      __func__, viewId, static_cast<unsigned int>(i),
                      static_cast<unsigned int>(absidx),
                      static_cast<unsigned int>(m_records.size()));
            continue;
        }
        res.records.push_back(m_records[absidx]);
    }
    return res;
}

RowsResult WalletTxStore::getAllRows(int viewId)
{
    LOCK(cs_store);
    RowsResult res;
    auto it = m_cursors.find(viewId);
    if (it == m_cursors.end()) {
        return res;   // unknown view: empty, zero metadata (matches getRows)
    }
    const Cursor& cur = it->second;
    // Same atomic metadata sampling as getRows (one cs_store hold).
    res.total_accepted = static_cast<int>(
        std::min<std::size_t>(cur.totalAccepted(),
                              static_cast<std::size_t>(std::numeric_limits<int>::max())));
    res.epoch = cur.epoch();
    auto sit = m_view_seqno.find(viewId);
    res.high_water = (sit == m_view_seqno.end()) ? 0 : sit->second;
    // CAP-INDEPENDENT: iterate the full accepted set, not the served window, so a
    // CSV export is never silently truncated by a finite cap (windowed-model PR5-B).
    // Bound the row count to the same INT_MAX clamp as total_accepted: a wallet can
    // never hold 2^31 rows, but keeping records.size() == total_accepted avoids a huge
    // over-allocation and an int-unrepresentable count if it ever did (Copilot PR5-B).
    const std::size_t n = static_cast<std::size_t>(res.total_accepted);
    res.records.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t absidx = cur.rowAt(i);
        if (absidx >= m_records.size()) {
            LogPrintf("ERROR: %s: view %d accepted row %u maps to record %u of %u — skipping",
                      __func__, viewId, static_cast<unsigned int>(i),
                      static_cast<unsigned int>(absidx),
                      static_cast<unsigned int>(m_records.size()));
            continue;
        }
        res.records.push_back(m_records[absidx]);
    }
    return res;
}

int WalletTxStore::rowForKey(int viewId, const uint256& hash, int idx)
{
    LOCK(cs_store);
    auto cit = m_cursors.find(viewId);
    if (cit == m_cursors.end()) {
        return -1;
    }
    const Cursor& cur = cit->second;
    constexpr std::size_t NPOS = static_cast<std::size_t>(-1);
    // Resolve hash -> absolute record index(es) via m_by_hash, then the accepted
    // row via Cursor::positionOf. idx < 0 -> first part: the MIN accepted row across
    // all parts of the tx (old indexForTxid hash-only semantics). positionOf is a
    // value scan over view_index, so this tolerates a (defensive) de-clustered index
    // and triggers no projector evaluation.
    std::size_t best = NPOS;
    auto range = m_by_hash.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it) {
        const std::size_t absidx = it->second;
        if (idx >= 0) {
            if (absidx >= m_records.size() || m_records[absidx].idx != idx) continue;
        }
        const std::size_t pos = cur.positionOf(absidx);
        if (pos != NPOS && pos < best) best = pos;
    }
    // Guard the size_t->int cast: a position beyond INT_MAX is not representable as a
    // Qt row, so treat it as not-found (Copilot PR5-B). Unreachable at real wallet sizes.
    if (best == NPOS || best > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return -1;
    }
    return static_cast<int>(best);
}

WalletTxDetail WalletTxStore::getRowDetail(const uint256& hash, int idx)
{
    // Resolve (hash, idx) -> the clicked part's vout under cs_store ONLY, then
    // RELEASE cs_store before taking the wallet locks: the class invariant
    // (header threading note) is that cs_store is NEVER held while acquiring
    // cs_main / cs_wallet. mapWallet is authoritative for the formatted detail,
    // so a worker reordering m_records after we release cs_store is irrelevant —
    // we already captured the part's identity (hash, vout).
    unsigned int vout = 0;
    bool found = false;
    {
        LOCK(cs_store);
        auto range = m_by_hash.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
            const std::size_t absidx = it->second;
            if (absidx >= m_records.size()) continue;   // defensive
            const TransactionRecord& rec = m_records[absidx];
            if (idx < 0 || rec.idx == idx) {
                vout = rec.vout;
                found = true;
                if (idx >= 0) break;   // exact part match
            }
        }
    }
    if (!found || !m_wallet) {
        // !found: no such (hash, idx). !m_wallet: defensive — the live GUI always
        // constructs the store with a wallet, but the nullptr-wallet unit harness
        // must not dereference it once a match is found (Copilot review, PR5-C).
        return WalletTxDetail{};
    }
    // Heavy detail fill under the canonical wallet locks, OFF the store
    // leaf. A future edit MUST NOT hoist this under the cs_store scope above —
    // that would invert cs_store -> cs_main/cs_wallet and violate the lock order.
    LOCK2(cs_main, m_wallet->cs_wallet);
    auto mi = m_wallet->mapWallet.find(hash);
    if (mi == m_wallet->mapWallet.end()) {
        return WalletTxDetail{};
    }
    return FillWalletTxDetail(m_wallet, mi->second, vout);
}

void WalletTxStore::emitCursorDeltas(int viewId, uint64_t epoch,
                                     const std::vector<CursorDelta>& deltas)
{
    for (const CursorDelta& d : deltas) {
        // Record the seqno of every emitted event as the view's high-water (the
        // last one wins) so getRows can tell a consumer exactly what its refetch
        // already reflects (PR4-fix B). The push and this update happen under the
        // caller's cs_store, in lockstep with the cursor mutation.
        switch (d.type) {
        case CursorDelta::Reset:
            m_view_seqno[viewId] = m_queue.push(GRC::RowsResetPayload{viewId, epoch, d.count});
            break;
        case CursorDelta::Insert: {
            // Read the records the CURSOR stamped on the delta, NOT a re-resolution
            // of d.first against the cursor's current view_index. A single apply*
            // call emits its deltas against successive intermediate states, so by
            // the time we get here d.first can name a different row — or none at
            // all, when a later delta shrank the served window past it. That was a
            // wrong-record payload on capped Status-sorted views and an
            // out-of-bounds read on the promotion path (#3257 review, and the
            // Overview half of #3101).
            std::vector<TransactionRecord> recs;
            recs.reserve(d.rows.size());
            for (const std::size_t absidx : d.rows) {
                if (absidx >= m_records.size()) {
                    // Unreachable by construction: rows are captured from a live
                    // view_index under the same cs_store hold that mutated
                    // m_records. Log rather than assert — the deployed build is
                    // -DNDEBUG — and skip, so a stale index degrades to a missing
                    // row instead of a heap read.
                    LogPrintf("ERROR: %s: view %d Insert delta at %d references record "
                              "%u of %u — skipping",
                              __func__, viewId, d.first,
                              static_cast<unsigned int>(absidx),
                              static_cast<unsigned int>(m_records.size()));
                    continue;
                }
                recs.push_back(m_records[absidx]);
            }
            if (recs.size() != static_cast<std::size_t>(d.count)) {
                LogPrintf("ERROR: %s: view %d Insert delta at %d carries %u of %d records "
                          "— consumer replicas will diverge until the next reset",
                          __func__, viewId, d.first,
                          static_cast<unsigned int>(recs.size()), d.count);
            }
            m_view_seqno[viewId] = m_queue.push(GRC::RowsInsertedPayload{d.first, std::move(recs), viewId, epoch});
            break;
        }
        case CursorDelta::Remove:
            m_view_seqno[viewId] = m_queue.push(GRC::RowsRemovedPayload{d.first, d.count, viewId, epoch});
            break;
        case CursorDelta::Change: {
            // Sample the changed rows HERE, under the same cs_store hold that
            // mutated them, from the absolute indices the cursor stamped on the
            // delta -- exactly as the Insert case does, and for the same reason:
            // d.first is an intermediate-state coordinate, so re-resolving it later
            // can name a different row.
            //
            // The consumer used to call getRows() when it applied the event
            // instead. That sampled the cursor's state at apply time, which within
            // a drained batch can be newer than the structural position applied so
            // far, and it cost one synchronous IPC round trip per change under
            // multiprocess -- each taking cs_store, which this refresh is holding,
            // so they serialized against the producer rather than merely being slow.
            std::vector<TransactionRecord> recs;
            recs.reserve(d.rows.size());
            for (const std::size_t absidx : d.rows) {
                if (absidx >= m_records.size()) {
                    LogPrintf("ERROR: %s: view %d Change delta at %d references record "
                              "%u of %u — skipping",
                              __func__, viewId, d.first,
                              static_cast<unsigned int>(absidx),
                              static_cast<unsigned int>(m_records.size()));
                    continue;
                }
                recs.push_back(m_records[absidx]);
            }
            m_view_seqno[viewId] =
                m_queue.push(GRC::RowsChangedPayload{viewId, epoch, d.first, d.count, std::move(recs)});
            break;
        }
        }
    }
}

} // namespace GRC
