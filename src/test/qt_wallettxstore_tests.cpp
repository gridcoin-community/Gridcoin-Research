// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free PR2.5 store-worker / double-queue
// (src/wallet/wallettxstore.{h,cpp}): producers enqueue (O(1)) and a single
// worker thread drains the intake queue and runs the O(N) store maintenance off
// the core locks, pushing the position-stamped events. These exercise the
// worker's drain/ordering/concurrency and clean shutdown. The store has zero Qt
// dependencies, so it compiles into the GUI-OFF test binary directly. The
// rebuild barrier (reloadAndSnapshot quiescing the worker) needs a live
// CWallet, so it is covered by the ASan-GUI mesh soak + isolated-testnet
// validation, exactly as the PR2 store proper was.

#include <arith_uint256.h>
#include <interfaces/wallet_tx_record.h>
#include <wallet/wallet_event_queue.h>
#include <wallet/wallettxstore.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <set>
#include <thread>
#include <utility>
#include <vector>

using GRC::RowsInsertedPayload;
using GRC::RowsRemovedPayload;
using GRC::WalletEventQueue;
using GRC::WalletTxStore;

namespace {

//! Distinct, deterministic hash per integer (locale-independent — no string
//! conversion). n+1 avoids the zero hash.
uint256 hashOf(int n)
{
    return ArithToUint256(arith_uint256(static_cast<uint64_t>(n) + 1));
}

//! A single-part TransactionRecord carrying just the ordering key fields the
//! store uses (time/hash/idx); the rest default. The store's insert/remove
//! never touch the wallet, so these synthetic records are sufficient.
TransactionRecord makeRec(const uint256& hash, int64_t time, int idx)
{
    TransactionRecord r(hash, time);
    r.idx = idx;
    return r;
}

//! Poll the event queue (the worker drains asynchronously) until it holds at
//! least `expected` events or a generous timeout elapses. Each iteration sleeps
//! 5ms; the ~1.5s ceiling is far above the worker's drain latency.
void waitForQueue(WalletEventQueue& q, std::size_t expected)
{
    for (int i = 0; i < 300 && q.size() < expected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

//! True if the event's payload carries the given viewId. All payload variants
//! except ChainTipChangedPayload are per-view and stamp a viewId (C++17: no
//! `requires`, so match the concrete types explicitly).
bool eventHasViewId(const GRC::WalletEvent& ev, int viewId)
{
    if (const auto* p = std::get_if<GRC::RowsInsertedPayload>(&ev.payload))   return p->viewId == viewId;
    if (const auto* p = std::get_if<GRC::RowsRemovedPayload>(&ev.payload))    return p->viewId == viewId;
    if (const auto* p = std::get_if<GRC::RowsResetPayload>(&ev.payload))      return p->viewId == viewId;
    if (const auto* p = std::get_if<GRC::RowCountChangedPayload>(&ev.payload))return p->viewId == viewId;
    if (const auto* p = std::get_if<GRC::RowsChangedPayload>(&ev.payload))    return p->viewId == viewId;
    return false;
}

//! Does any event in the batch belong to the given view?
bool batchHasViewId(const std::vector<GRC::WalletEvent>& batch, int viewId)
{
    for (const auto& ev : batch) {
        if (eventHasViewId(ev, viewId)) return true;
    }
    return false;
}

//! A fully-permissive view filter: show_orphans (and the default show_inactive)
//! accept every row regardless of status, so a synthetic makeRec record is
//! guaranteed to pass the cursor and produce a per-view event.
GRC::FilterSpec permissiveSpec()
{
    GRC::FilterSpec spec;
    spec.show_orphans = true;
    return spec;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_wallettxstore_tests)

BOOST_AUTO_TEST_CASE(workerDrainsAllInsertsInOrder)
{
    WalletEventQueue q;
    // reloadAndSnapshot (the only consumer of m_wallet) is not exercised here,
    // so a null wallet is safe — the worker's insert/remove path is wallet-free.
    WalletTxStore store(nullptr, q);
    store.start();

    constexpr int N = 50;
    for (int i = 0; i < N; ++i) {
        store.enqueueInsert(std::vector<TransactionRecord>{makeRec(hashOf(i), 1000 + i, 0)});
    }
    waitForQueue(q, static_cast<std::size_t>(N));

    auto batch = q.drain();
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(N));
    // Each distinct-hash insert yields exactly one RowsInserted; the single
    // worker drains the intake FIFO in order, so seqnos are dense [0, N).
    for (int i = 0; i < N; ++i) {
        BOOST_CHECK(std::holds_alternative<RowsInsertedPayload>(batch[i].payload));
        BOOST_CHECK_EQUAL(batch[i].seqno, static_cast<uint64_t>(i));
    }
}

BOOST_AUTO_TEST_CASE(workerHandlesInterleavedInsertRemove)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    const uint256 h1 = hashOf(1);
    const uint256 h2 = hashOf(2);
    store.enqueueInsert(std::vector<TransactionRecord>{makeRec(h1, 2000, 0)});
    store.enqueueInsert(std::vector<TransactionRecord>{makeRec(h2, 1000, 0)});
    store.enqueueRemove(h1);
    waitForQueue(q, 3);

    auto batch = q.drain();
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(3));
    // The worker dispatches both intake kinds, in enqueue order.
    BOOST_CHECK(std::holds_alternative<RowsInsertedPayload>(batch[0].payload));
    BOOST_CHECK(std::holds_alternative<RowsInsertedPayload>(batch[1].payload));
    BOOST_CHECK(std::holds_alternative<RowsRemovedPayload>(batch[2].payload));
}

BOOST_AUTO_TEST_CASE(workerPreservesAllUnderConcurrentProducers)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    // Several producers enqueue concurrently (distinct hashes, so no dedup);
    // the single worker serializes them. Confirms every item is applied exactly
    // once — no loss, dup, or reorder across the MPSC intake.
    constexpr int kProducers = 4;
    constexpr int kPer       = 100;
    constexpr int kTotal     = kProducers * kPer;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&store, p]() {
            for (int i = 0; i < kPer; ++i) {
                store.enqueueInsert(
                    std::vector<TransactionRecord>{makeRec(hashOf(p * 1000 + i), 1000 + i, 0)});
            }
        });
    }
    for (auto& t : producers) t.join();
    waitForQueue(q, static_cast<std::size_t>(kTotal));

    auto batch = q.drain();
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(kTotal));
    std::set<uint64_t> seqnos;
    for (const auto& ev : batch) {
        BOOST_CHECK(std::holds_alternative<RowsInsertedPayload>(ev.payload));
        seqnos.insert(ev.seqno);
    }
    // Dense, unique seqnos == single-writer serialization of the concurrent intake.
    BOOST_CHECK_EQUAL(seqnos.size(), static_cast<std::size_t>(kTotal));
    BOOST_CHECK_EQUAL(*seqnos.begin(),  static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(*seqnos.rbegin(), static_cast<uint64_t>(kTotal - 1));
}

BOOST_AUTO_TEST_CASE(dtorWithPendingIntakeIsClean)
{
    WalletEventQueue q;
    {
        WalletTxStore store(nullptr, q);
        store.start();
        // Flood the intake, then destroy immediately — the worker is very likely
        // still draining. The dtor must set m_stop and join cleanly without
        // hanging, even with items still queued.
        for (int i = 0; i < 500; ++i) {
            store.enqueueInsert(std::vector<TransactionRecord>{makeRec(hashOf(i), 1000 + i, 0)});
        }
    } // store dtor here: stop + join. If it hung, this test would never return.
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(getRowDetailUnknownHashReturnsEmpty)
{
    WalletEventQueue q;
    // Null wallet is safe: an unknown hash finds no m_by_hash entry, so
    // getRowDetail returns under cs_store before reaching the LOCK2(cs_main,
    // cs_wallet) / mapWallet path that would dereference the wallet (PR5-C).
    WalletTxStore store(nullptr, q);
    store.start();

    // Empty store, query a hash that was never inserted.
    // getRowDetail returns a value DTO now; found == false marks the miss.
    BOOST_CHECK(!store.getRowDetail(hashOf(99), 0).found);
    // idx < 0 (first-part fallback) on an absent hash is equally a miss.
    BOOST_CHECK(!store.getRowDetail(hashOf(99), -1).found);
}

BOOST_AUTO_TEST_CASE(getRowDetailWrongIdxReturnsEmpty)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    // Insert a single-part record (idx 0). After the worker applies it, m_by_hash
    // holds h, but a query for a DIFFERENT part index matches no record, so
    // getRowDetail returns empty WITHOUT taking the wallet locks — null-wallet
    // safe. (A query for the real idx 0 would proceed to the DTO fill and need
    // a live wallet, so that path is GUI-soak-only.)
    const uint256 h = hashOf(7);
    store.enqueueInsert(std::vector<TransactionRecord>{makeRec(h, 1000, 0)});
    waitForQueue(q, 1);
    // The RowsInserted event is pushed AFTER the store mutation, so a queued event
    // proves h is in m_by_hash. Assert it explicitly: otherwise, if the insert were
    // never applied, getRowDetail would return empty for the WRONG reason (unknown
    // hash) and this test would still pass without exercising the wrong-idx path
    // (Copilot review, PR5-C).
    BOOST_CHECK(q.size() >= 1);

    BOOST_CHECK(!store.getRowDetail(h, 5).found);
}

BOOST_AUTO_TEST_CASE(unregisterViewStopsCursorEvents)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    // Register a permissive VIEW_OVERVIEW cursor. registerView pushes a Reset
    // SYNCHRONOUSLY on this thread; drain it so it cannot satisfy a later
    // waitForQueue or bleed into a batch we assert on (a Reset also carries
    // viewId==VIEW_OVERVIEW, which would make the insert assertion below vacuous).
    store.registerView(GRC::VIEW_OVERVIEW, permissiveSpec(), GRC::TXCOL_DATE, GRC::TXSORT_DESC);
    q.drain();

    // An insert while the view is registered drives the cursor. The worker
    // produces exactly two events — the native VIEW_FULL RowsInserted and the
    // VIEW_OVERVIEW cursor delta — under one cs_store hold. Wait for BOTH so the
    // insert is fully applied (the cursor drive complete) before we assert or
    // unregister; then the VIEW_OVERVIEW event genuinely proves the record passed
    // the filter and the cursor is live.
    store.enqueueInsert(std::vector<TransactionRecord>{makeRec(hashOf(1), 1000, 0)});
    waitForQueue(q, 2);
    auto batch1 = q.drain();
    BOOST_CHECK(batchHasViewId(batch1, GRC::VIEW_FULL));
    BOOST_CHECK(batchHasViewId(batch1, GRC::VIEW_OVERVIEW));

    // Drop the view. The first insert is fully drained above, so nothing from it
    // can bleed into the next batch. A subsequent identical insert must still
    // drive the native VIEW_FULL stream but emit NOTHING for the now-unregistered
    // VIEW_OVERVIEW.
    store.unregisterView(GRC::VIEW_OVERVIEW);
    store.enqueueInsert(std::vector<TransactionRecord>{makeRec(hashOf(2), 1001, 0)});
    waitForQueue(q, 1);
    auto batch2 = q.drain();
    BOOST_CHECK(batchHasViewId(batch2, GRC::VIEW_FULL));
    BOOST_CHECK(!batchHasViewId(batch2, GRC::VIEW_OVERVIEW));
}

BOOST_AUTO_TEST_CASE(unregisterViewIsIdempotent)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    // Unregistering a never-registered view is a harmless no-op.
    store.unregisterView(GRC::VIEW_OVERVIEW);

    // Register, then unregister twice — the second call is a no-op, not a crash.
    store.registerView(GRC::VIEW_DETAILED, permissiveSpec(), GRC::TXCOL_DATE, GRC::TXSORT_DESC);
    store.unregisterView(GRC::VIEW_DETAILED);
    store.unregisterView(GRC::VIEW_DETAILED);

    // Re-registering after an unregister works: it pushes a fresh Reset for the view.
    q.drain();
    store.registerView(GRC::VIEW_DETAILED, permissiveSpec(), GRC::TXCOL_DATE, GRC::TXSORT_DESC);
    BOOST_CHECK(batchHasViewId(q.drain(), GRC::VIEW_DETAILED));
}

BOOST_AUTO_TEST_SUITE_END()
