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

} // anonymous namespace

namespace GRC {

WalletTxStore::WalletTxStore(CWallet* wallet, GRC::WalletEventQueue& queue)
    : m_wallet(wallet)
    , m_queue(queue)
{
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
    m_by_hash.reserve(m_keys.size() * 2 + 1);
    for (std::size_t i = 0; i < m_keys.size(); ++i) {
        m_by_hash.emplace(m_keys[i].hash, i);
    }
}

void WalletTxStore::insertTransaction(std::vector<TransactionRecord> records)
{
    LOCK(cs_store);

    // Datetime-display cutoff (cached from the last reloadAndSnapshot). All
    // records of one tx share `time`, so this is all-or-nothing.
    if (m_limit_enabled) {
        records.erase(std::remove_if(records.begin(), records.end(),
                          [&](const TransactionRecord& r) { return r.time < m_limit_time; }),
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
    // of the first key.
    const TxOrderKey first_key{records.front().time, hash, records.front().idx};
    const auto pos = std::lower_bound(m_keys.begin(), m_keys.end(), first_key, GRC::TxOrderLess);
    const std::size_t insertIdx = static_cast<std::size_t>(pos - m_keys.begin());
    const std::size_t count = records.size();

    std::vector<TxOrderKey> new_keys;
    new_keys.reserve(count);
    for (const TransactionRecord& r : records) {
        new_keys.push_back({r.time, r.hash, r.idx});
    }

    // Shift the index BEFORE splicing the vector (the shift uses logical
    // positions), then splice, then add the new index entries — keeping the
    // index consistent at every observable point.
    shiftIndex(insertIdx, static_cast<std::ptrdiff_t>(count));
    m_keys.insert(m_keys.begin() + insertIdx, new_keys.begin(), new_keys.end());
    for (std::size_t k = 0; k < count; ++k) {
        m_by_hash.emplace(hash, insertIdx + k);
    }

    // Push the position-stamped event WHILE cs_store is held so that queue
    // seqno-order equals store-mutation-order across all producer threads.
    m_queue.push(GRC::RowsInsertedPayload{static_cast<int>(insertIdx), std::move(records)});
}

void WalletTxStore::removeTransaction(const uint256& hash)
{
    LOCK(cs_store);

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
    // ordered first and short-circuits, so the m_keys[] subscripts below it
    // never run out of range. If the invariant is ever violated, bail without
    // erasing or emitting — a stale row is a safe degradation; a wrong erase is
    // not.
    if (maxPos >= m_keys.size()
            || count != distance
            || m_keys[minPos].hash != hash
            || m_keys[maxPos].hash != hash) {
        LogPrintf("ERROR: %s: hash %s index non-contiguous/out-of-range "
                  "(minPos=%u maxPos=%u count=%u distance=%u keys=%u) — skipping remove",
                  __func__, hash.GetHex(),
                  static_cast<unsigned int>(minPos), static_cast<unsigned int>(maxPos),
                  static_cast<unsigned int>(count), static_cast<unsigned int>(distance),
                  static_cast<unsigned int>(m_keys.size()));
        return;
    }

    m_keys.erase(m_keys.begin() + minPos, m_keys.begin() + maxPos + 1);
    m_by_hash.erase(hash);
    shiftIndex(maxPos + 1, -static_cast<std::ptrdiff_t>(count));

    m_queue.push(GRC::RowsRemovedPayload{static_cast<int>(minPos), static_cast<int>(count)});
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
            built.push_back(rec);
        }
    }
    std::sort(built.begin(), built.end(), RecordOrder());

    {
        LOCK(cs_store);
        m_limit_enabled = limit_enabled;
        m_limit_time = limit_time;
        m_keys.clear();
        m_keys.reserve(built.size());
        for (const TransactionRecord& r : built) {
            m_keys.push_back({r.time, r.hash, r.idx});
        }
        rebuildIndex();
    }

    // Discard any events queued before this rebuild: they were computed against
    // the old index and are superseded by `built`. Producers are still blocked
    // on cs_wallet, so nothing new can be queued until we return; the returned
    // snapshot, the rebuilt index, and the now-empty queue are consistent.
    m_queue.drain();

    return built;
}

} // namespace GRC
