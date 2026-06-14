// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallettxstore_tests.h"

#include "arith_uint256.h"
#include "qt/transactionrecord.h"
#include "qt/wallet_event_queue.h"
#include "qt/wallettxstore.h"
#include "uint256.h"

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
//! least `expected` events or a generous timeout elapses. qWait pumps the Qt
//! loop and sleeps; ~1.5s ceiling is far above the worker's drain latency.
void waitForQueue(WalletEventQueue& q, std::size_t expected)
{
    for (int i = 0; i < 300 && q.size() < expected; ++i) {
        QTest::qWait(5);
    }
}

} // anonymous namespace

void WalletTxStoreTests::workerDrainsAllInsertsInOrder()
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
    QCOMPARE(batch.size(), static_cast<std::size_t>(N));
    // Each distinct-hash insert yields exactly one RowsInserted; the single
    // worker drains the intake FIFO in order, so seqnos are dense [0, N).
    for (int i = 0; i < N; ++i) {
        QVERIFY(std::holds_alternative<RowsInsertedPayload>(batch[i].payload));
        QCOMPARE(batch[i].seqno, static_cast<uint64_t>(i));
    }
}

void WalletTxStoreTests::workerHandlesInterleavedInsertRemove()
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
    QCOMPARE(batch.size(), static_cast<std::size_t>(3));
    // The worker dispatches both intake kinds, in enqueue order.
    QVERIFY(std::holds_alternative<RowsInsertedPayload>(batch[0].payload));
    QVERIFY(std::holds_alternative<RowsInsertedPayload>(batch[1].payload));
    QVERIFY(std::holds_alternative<RowsRemovedPayload>(batch[2].payload));
}

void WalletTxStoreTests::workerPreservesAllUnderConcurrentProducers()
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
    QCOMPARE(batch.size(), static_cast<std::size_t>(kTotal));
    std::set<uint64_t> seqnos;
    for (const auto& ev : batch) {
        QVERIFY(std::holds_alternative<RowsInsertedPayload>(ev.payload));
        seqnos.insert(ev.seqno);
    }
    // Dense, unique seqnos == single-writer serialization of the concurrent intake.
    QCOMPARE(seqnos.size(), static_cast<std::size_t>(kTotal));
    QCOMPARE(*seqnos.begin(),  static_cast<uint64_t>(0));
    QCOMPARE(*seqnos.rbegin(), static_cast<uint64_t>(kTotal - 1));
}

void WalletTxStoreTests::dtorWithPendingIntakeIsClean()
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
    QVERIFY(true);
}

void WalletTxStoreTests::getRowDetailUnknownHashReturnsEmpty()
{
    WalletEventQueue q;
    // Null wallet is safe: an unknown hash finds no m_by_hash entry, so
    // getRowDetail returns under cs_store before reaching the LOCK2(cs_main,
    // cs_wallet) / mapWallet path that would dereference the wallet (PR5-C).
    WalletTxStore store(nullptr, q);
    store.start();

    // Empty store, query a hash that was never inserted.
    QVERIFY(store.getRowDetail(hashOf(99), 0).isEmpty());
    // idx < 0 (first-part fallback) on an absent hash is equally a miss.
    QVERIFY(store.getRowDetail(hashOf(99), -1).isEmpty());
}

void WalletTxStoreTests::getRowDetailWrongIdxReturnsEmpty()
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    // Insert a single-part record (idx 0). After the worker applies it, m_by_hash
    // holds h, but a query for a DIFFERENT part index matches no record, so
    // getRowDetail returns empty WITHOUT taking the wallet locks — null-wallet
    // safe. (A query for the real idx 0 would proceed to toHTML and need a live
    // wallet, so that path is GUI-soak-only.)
    const uint256 h = hashOf(7);
    store.enqueueInsert(std::vector<TransactionRecord>{makeRec(h, 1000, 0)});
    waitForQueue(q, 1);

    QVERIFY(store.getRowDetail(h, 5).isEmpty());
}
