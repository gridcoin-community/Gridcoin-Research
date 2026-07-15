// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free wallet event queue
// (src/wallet/wallet_event_queue.{h,cpp}), the MPSC intake the producer-side
// WalletTxStore uses to hand position-stamped events to the consumer. The queue
// has zero Qt dependencies, so it compiles into the GUI-OFF test binary
// directly. These exercise the drain/ordering semantics and the multi-producer
// seqno density contract the eventual multiprocess consumer relies on.

#include <wallet/wallet_event_queue.h>

#include <boost/test/unit_test.hpp>

#include <set>
#include <thread>
#include <vector>

using GRC::ChainTipChangedPayload;
using GRC::RowsInsertedPayload;
using GRC::RowsRemovedPayload;
using GRC::WalletEvent;
using GRC::WalletEventPayload;
using GRC::WalletEventQueue;

BOOST_AUTO_TEST_SUITE(qt_wallet_event_queue_tests)

BOOST_AUTO_TEST_CASE(emptyDrainIsEmpty)
{
    WalletEventQueue q;
    BOOST_CHECK_EQUAL(q.size(), static_cast<std::size_t>(0));

    auto batch = q.drain();
    BOOST_CHECK(batch.empty());
    BOOST_CHECK_EQUAL(q.size(), static_cast<std::size_t>(0));
}

BOOST_AUTO_TEST_CASE(singlePushDrainsOne)
{
    WalletEventQueue q;

    q.push(RowsRemovedPayload{0, 1});
    BOOST_CHECK_EQUAL(q.size(), static_cast<std::size_t>(1));

    auto batch = q.drain();
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(1));
    BOOST_CHECK_EQUAL(batch[0].seqno, static_cast<uint64_t>(0));
    BOOST_CHECK(std::holds_alternative<RowsRemovedPayload>(batch[0].payload));

    // Queue is now empty.
    BOOST_CHECK_EQUAL(q.size(), static_cast<std::size_t>(0));
}

BOOST_AUTO_TEST_CASE(seqnoMonotonicWithinSingleProducer)
{
    WalletEventQueue q;

    constexpr int N = 100;
    for (int i = 0; i < N; ++i) {
        q.push(RowsRemovedPayload{0, 1});
    }

    auto batch = q.drain();
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        BOOST_CHECK_EQUAL(batch[i].seqno, static_cast<uint64_t>(i));
    }
}

BOOST_AUTO_TEST_CASE(allPayloadVariantsRoundTrip)
{
    WalletEventQueue q;

    q.push(RowsInsertedPayload{}); // empty record list is enough — testing the variant lane.
    q.push(RowsRemovedPayload{0, 1});
    q.push(ChainTipChangedPayload{2771000, 1779000000});

    auto batch = q.drain();
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(3));

    BOOST_CHECK(std::holds_alternative<RowsInsertedPayload>(batch[0].payload));
    BOOST_CHECK(std::holds_alternative<RowsRemovedPayload>(batch[1].payload));
    BOOST_CHECK(std::holds_alternative<ChainTipChangedPayload>(batch[2].payload));

    const auto& tip = std::get<ChainTipChangedPayload>(batch[2].payload);
    BOOST_CHECK_EQUAL(tip.height,    2771000);
    BOOST_CHECK_EQUAL(tip.best_time, static_cast<int64_t>(1779000000));
}

BOOST_AUTO_TEST_CASE(drainPartialBatch)
{
    WalletEventQueue q;

    for (int i = 0; i < 10; ++i) {
        q.push(RowsRemovedPayload{0, 1});
    }
    BOOST_CHECK_EQUAL(q.size(), static_cast<std::size_t>(10));

    // Drain a smaller batch than what's queued — confirms partial-drain
    // semantics and that the remaining events stay in order.
    auto first = q.drain(3);
    BOOST_CHECK_EQUAL(first.size(), static_cast<std::size_t>(3));
    BOOST_CHECK_EQUAL(first[0].seqno, static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(first[2].seqno, static_cast<uint64_t>(2));
    BOOST_CHECK_EQUAL(q.size(), static_cast<std::size_t>(7));

    auto rest = q.drain();
    BOOST_CHECK_EQUAL(rest.size(), static_cast<std::size_t>(7));
    BOOST_CHECK_EQUAL(rest[0].seqno, static_cast<uint64_t>(3));
    BOOST_CHECK_EQUAL(rest[6].seqno, static_cast<uint64_t>(9));
}

BOOST_AUTO_TEST_CASE(multiProducerSeqnosAreUniqueAndDense)
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
    BOOST_CHECK_EQUAL(batch.size(), static_cast<std::size_t>(kTotal));

    std::set<uint64_t> seen;
    for (const auto& ev : batch) seen.insert(ev.seqno);
    BOOST_CHECK_EQUAL(seen.size(), static_cast<std::size_t>(kTotal));
    BOOST_CHECK_EQUAL(*seen.begin(),  static_cast<uint64_t>(0));
    BOOST_CHECK_EQUAL(*seen.rbegin(), static_cast<uint64_t>(kTotal - 1));
}

BOOST_AUTO_TEST_SUITE_END()
