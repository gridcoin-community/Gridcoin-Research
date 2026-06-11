// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/wallettxstore.h"

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
    return GRC::TxFilterFields{
        r.time, r.credit + r.debit, static_cast<int>(r.type),
        static_cast<int>(r.status.status), r.address, std::string()};
}

//! Project a stored record to the Qt-free sort inputs. type_string /
//! address_string are left empty: those are the localized Qt formatter strings,
//! used only when sorting by Type/Address — which the Overview view does not (it
//! sorts by date/status). PR4 supplies them when the detailed table migrates.
GRC::SortKey projectKeys(const TransactionRecord& r)
{
    return GRC::SortKey{
        r.time, r.credit + r.debit, r.status.sortKey, std::string(), std::string()};
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
    m_worker = std::thread([this] { workerLoop(); });
}

void WalletTxStore::enqueueInsert(std::vector<TransactionRecord> records)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Insert, std::move(records), uint256()});
    }
    m_intake_cv.notify_one();
}

void WalletTxStore::enqueueRemove(const uint256& hash)
{
    {
        LOCK(cs_intake);
        m_intake.push_back(IntakeItem{IntakeItem::Remove, {}, hash});
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
        m_intake.push_back(IntakeItem{IntakeItem::Update, std::move(records), hash});
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
            // While a rebuild is pending, park and tell reloadAndSnapshot we are
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
            applyIntake(std::move(item));
        }
    }
}

void WalletTxStore::applyIntake(IntakeItem item)
{
    // No lock held here; insert/remove/updateTransaction take cs_store internally.
    if (item.kind == IntakeItem::Insert) {
        insertTransaction(std::move(item.records));
    } else if (item.kind == IntakeItem::Update) {
        updateTransaction(std::move(item.records));
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

void WalletTxStore::insertTransaction(std::vector<TransactionRecord> records)
{
    LOCK(cs_store);
    insertLocked(std::move(records));
}

void WalletTxStore::insertLocked(std::vector<TransactionRecord> records)
{
    // Datetime-display cutoff (cached from the last reloadAndSnapshot). All
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

    // Shift the index BEFORE splicing the vector (the shift uses logical
    // positions), then splice, then add the new index entries — keeping the
    // index consistent at every observable point. The records are copied into the
    // store (the authoritative full-records table) and then moved into the event.
    shiftIndex(insertIdx, static_cast<std::ptrdiff_t>(count));
    m_records.insert(m_records.begin() + insertIdx, records.begin(), records.end());
    for (std::size_t k = 0; k < count; ++k) {
        m_by_hash.emplace(hash, insertIdx + k);
    }

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

void WalletTxStore::removeLocked(const uint256& hash)
{
    auto range = m_by_hash.equal_range(hash);
    if (range.first == range.second) {
        // Not present — filtered out at insert time, or never inserted. No-op,
        // no event.
        return;
    }

    // Same-hash keys are contiguous under TxOrderLess (the hash tiebreaker
    // clusters them), so min/max bound a single range.
    std::size_t minPos = std::numeric_limits<std::size_t>::max();
    std::size_t maxPos = 0;
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < minPos) minPos = it->second;
        if (it->second > maxPos) maxPos = it->second;
    }
    const std::size_t count = maxPos - minPos + 1;
    const std::size_t distance = static_cast<std::size_t>(std::distance(range.first, range.second));

    // The erase below relies on same-hash keys being a single contiguous run
    // (guaranteed by TxOrderLess's hash tiebreaker). Validate at runtime, not
    // via assert: the deployed build is -DNDEBUG, so an assert would vanish and
    // a de-clustered index would erase a wider [minPos, maxPos] range that
    // swallows foreign rows (heap/structure corruption). The bounds check is
    // ordered first and short-circuits, so the m_records[] subscripts below it
    // never run out of range. If the invariant is ever violated, bail without
    // erasing or emitting — a stale row is a safe degradation; a wrong erase is
    // not.
    if (maxPos >= m_records.size()
            || count != distance
            || m_records[minPos].hash != hash
            || m_records[maxPos].hash != hash) {
        LogPrintf("ERROR: %s: hash %s index non-contiguous/out-of-range "
                  "(minPos=%u maxPos=%u count=%u distance=%u keys=%u) — skipping remove",
                  __func__, hash.GetHex(),
                  static_cast<unsigned int>(minPos), static_cast<unsigned int>(maxPos),
                  static_cast<unsigned int>(count), static_cast<unsigned int>(distance),
                  static_cast<unsigned int>(m_records.size()));
        return;
    }

    m_records.erase(m_records.begin() + minPos, m_records.begin() + maxPos + 1);
    m_by_hash.erase(hash);
    shiftIndex(maxPos + 1, -static_cast<std::ptrdiff_t>(count));

    m_queue.push(GRC::RowsRemovedPayload{static_cast<int>(minPos), static_cast<int>(count)});

    // Drive each registered cursor: [minPos, minPos+count) were erased from
    // m_records and later positions shifted down by count.
    for (auto& [viewId, cursor] : m_cursors) {
        emitCursorDeltas(viewId, cursor.epoch(), cursor.applyStoreRemove(minPos, count));
    }
}

std::vector<TransactionRecord> WalletTxStore::reloadAndSnapshot(bool limit_enabled, int64_t limit_time)
{
    std::vector<TransactionRecord> built;

    // Hold cs_main + cs_wallet across the whole rebuild AND the queue drain,
    // exactly as the old loadWallet held them for its scan. cs_main is the
    // load-bearing exclusion lock here: EVERY producer of insert/remove holds
    // cs_main — CT_NEW/CT_UPDATED under cs_main+cs_wallet, and BOTH CT_DELETED
    // sites (ReorganizeChain in main.cpp, ResendWalletTransactions in
    // wallet.cpp) under cs_main even though they do NOT hold cs_wallet at the
    // fire site. So holding cs_main across both the index rebuild and the
    // queue drain fully excludes producers: no event can be queued between the
    // two, which makes the returned snapshot, the rebuilt index, and the
    // emptied queue mutually consistent.
    LOCK2(cs_main, m_wallet->cs_wallet);

    // Quiesce the store-worker (PR2.5) before clearing/rebuilding. The worker is
    // an independent store mutator that cs_main/cs_wallet does NOT exclude, so
    // signal a rebuild and wait for it to park. It never needs cs_main/cs_wallet
    // (insert/removeTransaction take only cs_store), so waiting for it here while
    // we hold both cannot deadlock. m_started guards the pre-start() first reload
    // (no worker yet → nothing to wait for).
    {
        WAIT_LOCK(cs_intake, ilock);
        m_rebuilding = true;
        m_intake_cv.notify_all();   // wake the worker so it observes m_rebuilding
        while (m_started && !m_worker_parked) {
            m_idle_cv.wait(ilock);  // wait until the worker confirms it has parked
        }
        m_intake.clear();           // pre-rebuild intake is superseded by the scan below
    }

    built.reserve(m_wallet->mapWallet.size() * 2);
    for (auto it = m_wallet->mapWallet.begin(); it != m_wallet->mapWallet.end(); ++it) {
        if (!TransactionRecord::showTransaction(it->second)) {
            continue;
        }
        const QList<TransactionRecord> decomposed =
            TransactionRecord::decomposeTransaction(m_wallet, it->second);
        for (const TransactionRecord& rec : decomposed) {
            if (limit_enabled && rec.time < limit_time) {
                continue;
            }
            // Compute status producer-side (cs_main held here) so the cursors
            // rebuilt over m_records can filter/sort by it. The VIEW_FULL consumer
            // still refreshes status lazily on read; this is a harmless head-start.
            TransactionRecord r = rec;
            r.updateStatus(it->second);
            built.push_back(std::move(r));
        }
    }
    std::sort(built.begin(), built.end(), RecordOrder());

    std::vector<GRC::RowsResetPayload> cursor_resets;
    {
        LOCK(cs_store);
        m_limit_enabled = limit_enabled;
        m_limit_time = limit_time;
        m_records = built;
        rebuildIndex();
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
    // the old index and are superseded by `built`. Producers are still blocked
    // on cs_wallet, so nothing new can be queued until we return; the returned
    // snapshot, the rebuilt index, and the now-empty queue are consistent.
    m_queue.drain();

    // Re-publish the per-view cursor Resets AFTER the drain so they survive it;
    // the windowed consumers (e.g. OverviewTxModel) refill via getRows. Producers
    // are still blocked on cs_wallet, so these are the only queued events until
    // we return.
    for (const GRC::RowsResetPayload& reset : cursor_resets) {
        m_queue.push(reset);
    }

    // Release the store-worker (PR2.5): the rebuilt index is live, so resume
    // draining. Producers remain blocked on cs_wallet until we return, so the
    // worker has nothing to apply until the snapshot is installed.
    {
        LOCK(cs_intake);
        m_rebuilding = false;
        m_worker_parked = false;
        m_intake_cv.notify_all();
    }

    return built;
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
        removeLocked(hash);
        insertLocked(std::move(records));
        return;
    }

    // In-place overwrite: the status changed but the ordering key (time,hash,idx)
    // did not, so positions are unchanged. No VIEW_FULL event is emitted — the
    // detailed table refreshes confirmation status lazily on read (unchanged
    // behaviour). Re-evaluate each cursor per affected row: under a status sort a
    // first confirmation repositions the row.
    for (std::size_t k = 0; k < records.size(); ++k) {
        m_records[minPos + k] = records[k];
    }
    for (auto& [viewId, cursor] : m_cursors) {
        for (std::size_t p = minPos; p <= maxPos; ++p) {
            emitCursorDeltas(viewId, cursor.epoch(), cursor.applyStatusUpdate(p));
        }
    }
}

TxFilterFields WalletTxStore::projectFieldsAt(std::size_t i) const
{
    return projectFields(m_records[i]);
}

SortKey WalletTxStore::projectKeysAt(std::size_t i) const
{
    return projectKeys(m_records[i]);
}

void WalletTxStore::makeCursorProjectors(Cursor::FieldsFn& fields, Cursor::KeysFn& keys)
{
    fields = [this](std::size_t i) { return projectFieldsAt(i); };
    keys = [this](std::size_t i) { return projectKeysAt(i); };
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

std::vector<TransactionRecord> WalletTxStore::getRows(int viewId, int first, int count)
{
    LOCK(cs_store);
    std::vector<TransactionRecord> out;
    auto it = m_cursors.find(viewId);
    if (it == m_cursors.end() || first < 0 || count <= 0) {
        return out;
    }
    const std::size_t served = it->second.servedCount();
    const std::size_t begin = static_cast<std::size_t>(first);
    if (begin >= served) {
        return out;
    }
    const std::size_t end = std::min(served, begin + static_cast<std::size_t>(count));
    out.reserve(end - begin);
    for (std::size_t i = begin; i < end; ++i) {
        out.push_back(m_records[it->second.rowAt(i)]);
    }
    return out;
}

int WalletTxStore::totalAccepted(int viewId)
{
    LOCK(cs_store);
    auto it = m_cursors.find(viewId);
    return it == m_cursors.end() ? 0 : static_cast<int>(it->second.totalAccepted());
}

void WalletTxStore::emitCursorDeltas(int viewId, uint64_t epoch,
                                     const std::vector<CursorDelta>& deltas)
{
    auto it = m_cursors.find(viewId);
    for (const CursorDelta& d : deltas) {
        switch (d.type) {
        case CursorDelta::Reset:
            m_queue.push(GRC::RowsResetPayload{viewId, epoch, d.count});
            break;
        case CursorDelta::Insert: {
            std::vector<TransactionRecord> recs;
            if (it != m_cursors.end()) {
                recs.reserve(static_cast<std::size_t>(d.count));
                for (int j = 0; j < d.count; ++j) {
                    recs.push_back(m_records[it->second.rowAt(static_cast<std::size_t>(d.first + j))]);
                }
            }
            m_queue.push(GRC::RowsInsertedPayload{d.first, std::move(recs), viewId, epoch});
            break;
        }
        case CursorDelta::Remove:
            m_queue.push(GRC::RowsRemovedPayload{d.first, d.count, viewId, epoch});
            break;
        case CursorDelta::Change:
            m_queue.push(GRC::RowsChangedPayload{viewId, epoch, d.first, d.count});
            break;
        }
    }
}

} // namespace GRC
