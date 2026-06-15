// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet_event_queue_tests.h"

#include "qt/wallet_event_queue.h"

#include <set>
#include <thread>
#include <vector>

using GRC::ChainTipChangedPayload;
using GRC::RowsInsertedPayload;
using GRC::RowsRemovedPayload;
using GRC::WalletEvent;
using GRC::WalletEventPayload;
using GRC::WalletEventQueue;

void WalletEventQueueTests::emptyDrainIsEmpty()
{
    WalletEventQueue q;
    QCOMPARE(q.size(), static_cast<std::size_t>(0));

    auto batch = q.drain();
    QVERIFY(batch.empty());
    QCOMPARE(q.size(), static_cast<std::size_t>(0));
}

void WalletEventQueueTests::singlePushDrainsOne()
{
    WalletEventQueue q;

    q.push(RowsRemovedPayload{0, 1});
    QCOMPARE(q.size(), static_cast<std::size_t>(1));

    auto batch = q.drain();
    QCOMPARE(batch.size(), static_cast<std::size_t>(1));
    QCOMPARE(batch[0].seqno, static_cast<uint64_t>(0));
    QVERIFY(std::holds_alternative<RowsRemovedPayload>(batch[0].payload));

    // Queue is now empty.
    QCOMPARE(q.size(), static_cast<std::size_t>(0));
}

void WalletEventQueueTests::seqnoMonotonicWithinSingleProducer()
{
    WalletEventQueue q;

    constexpr int N = 100;
    for (int i = 0; i < N; ++i) {
        q.push(RowsRemovedPayload{0, 1});
    }

    auto batch = q.drain();
    QCOMPARE(batch.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        QCOMPARE(batch[i].seqno, static_cast<uint64_t>(i));
    }
}

void WalletEventQueueTests::allPayloadVariantsRoundTrip()
{
    WalletEventQueue q;

    q.push(RowsInsertedPayload{}); // empty record list is enough — testing the variant lane.
    q.push(RowsRemovedPayload{0, 1});
    q.push(ChainTipChangedPayload{2771000, 1779000000});

    auto batch = q.drain();
    QCOMPARE(batch.size(), static_cast<std::size_t>(3));

    QVERIFY(std::holds_alternative<RowsInsertedPayload>(batch[0].payload));
    QVERIFY(std::holds_alternative<RowsRemovedPayload>(batch[1].payload));
    QVERIFY(std::holds_alternative<ChainTipChangedPayload>(batch[2].payload));

    const auto& tip = std::get<ChainTipChangedPayload>(batch[2].payload);
    QCOMPARE(tip.height,    2771000);
    QCOMPARE(tip.best_time, static_cast<int64_t>(1779000000));
}

void WalletEventQueueTests::drainPartialBatch()
{
    WalletEventQueue q;

    for (int i = 0; i < 10; ++i) {
        q.push(RowsRemovedPayload{0, 1});
    }
    QCOMPARE(q.size(), static_cast<std::size_t>(10));

    // Drain a smaller batch than what's queued — confirms partial-drain
    // semantics and that the remaining events stay in order.
    auto first = q.drain(3);
    QCOMPARE(first.size(), static_cast<std::size_t>(3));
    QCOMPARE(first[0].seqno, static_cast<uint64_t>(0));
    QCOMPARE(first[2].seqno, static_cast<uint64_t>(2));
    QCOMPARE(q.size(), static_cast<std::size_t>(7));

    auto rest = q.drain();
    QCOMPARE(rest.size(), static_cast<std::size_t>(7));
    QCOMPARE(rest[0].seqno, static_cast<uint64_t>(3));
    QCOMPARE(rest[6].seqno, static_cast<uint64_t>(9));
}

void WalletEventQueueTests::multiProducerSeqnosAreUniqueAndDense()
{
    WalletEventQueue q;

    // Three producers pushing concurrently; confirm every seqno [0, total)
    // appears exactly once. This is the property the consumer relies on for
    // resync-from-N+1 semantics in the eventual multiprocess form, and it is
    // the only contract the MPSC structure has to guarantee in-process.
    constexpr int kProducers       = 3;
    constexpr int kPerProducer     = 500;
    constexpr int kTotal           = kProducers * kPerProducer;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&q]() {
            for (int i = 0; i < kPerProducer; ++i) {
                q.push(RowsRemovedPayload{0, 1});
            }
        });
    }
    for (auto& t : producers) t.join();

    auto batch = q.drain();
    QCOMPARE(batch.size(), static_cast<std::size_t>(kTotal));

    std::set<uint64_t> seen;
    for (const auto& ev : batch) seen.insert(ev.seqno);
    QCOMPARE(seen.size(), static_cast<std::size_t>(kTotal));
    QCOMPARE(*seen.begin(),  static_cast<uint64_t>(0));
    QCOMPARE(*seen.rbegin(), static_cast<uint64_t>(kTotal - 1));
}
