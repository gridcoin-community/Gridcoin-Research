// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free PR2.5 store-worker / double-queue
// (src/wallet/wallettxstore.{h,cpp}): producers enqueue (O(1)) and a single
// worker thread drains the intake queue and runs the O(N) store maintenance off
// the core locks, pushing the position-stamped events. These exercise the
// worker's drain/ordering/concurrency and clean shutdown. The store has zero Qt
// dependencies, so it compiles into the GUI-OFF test binary directly. The
// rebuild barrier (prime quiescing the worker) needs a live
// CWallet, so it is covered by the ASan-GUI mesh soak + isolated-testnet
// validation, exactly as the PR2 store proper was.

#include <arith_uint256.h>
#include <interfaces/wallet_tx_filter.h>
#include <interfaces/wallet_tx_record.h>
#include <tinyformat.h>
#include <wallet/wallet_event_queue.h>
#include <wallet/wallettxstore.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>
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
//! Wait until the queue holds at least `expected` events, or give up.
//!
//! The cap is deliberately generous (20s, not the former 1.5s). This returns the
//! instant the condition holds, so a large cap costs nothing when the machine is
//! fast -- but the old budget was a fixed 1.5s for the worker to drain everything
//! the producers enqueued, which is a race against the host rather than a wait for
//! a condition. It lost on CI's ARM64 job, where the suite runs under QEMU
//! emulation roughly 15x slower than native: the worker had drained 394 of 400
//! events when the wait expired, and the caller then asserted on a partial batch.
//! Nothing was wrong with the store; the test simply stopped watching too early.
//!
//! If a caller ever does time out here, the assertion that follows will report the
//! shortfall, which is the diagnostic that matters -- so failing slow is strictly
//! better than failing fast and blaming the code under test.
void waitForQueue(WalletEventQueue& q, std::size_t expected)
{
    for (int i = 0; i < 4000 && q.size() < expected; ++i) {
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

//
// ---- coinstake-lifecycle harness (issue #3257) -----------------------------
//
// The cases above use synthetic single-part records and a permissive filter. The
// #3257 lifecycle needs the real thing: the PRODUCTION view configurations, and
// records carrying the status and status.sortKey the producer actually stamps.
//

constexpr int     kSeedRows      = 40;
constexpr int     kBlocks        = 25;
constexpr int     kOverviewCap   = 9;          //!< OverviewTxModel's resize-derived cap
constexpr int64_t kBaseTime      = 1700000000;
constexpr int     kBaseHeight    = 3200000;
constexpr int     kStakeHashBase = 100000;     //!< disjoint from the seed hashes

//! TransactionRecord::updateStatus's sort key, byte for byte
//! (transactionrecord.cpp): "%010d-%01d-%010u-%03d" over
//! (height, isCoinBase, nTimeReceived, idx) — note idx is the LAST field, so
//! under a DESC status sort the parts of one transaction come out in REVERSE
//! decompose order.
std::string statusSortKey(int height, unsigned int time_received, int idx)
{
    return strprintf("%010d-%01d-%010u-%03d", height, 0, time_received, idx);
}

//! The two records decomposeTransaction produces for a sidestaking coinstake:
//! part idx 0 is the gross stake (vout 1, credit = every output, debit = the
//! consumed input) and part idx 1 is the sidestake sent away (vout 2). Both are
//! type Generated and share hash/time, differing only in idx — which is exactly
//! what makes them a contiguous multi-part run in the store.
std::vector<TransactionRecord> makeCoinstake(const uint256& hash, int64_t time, int height,
                                             TransactionStatus::Status status)
{
    std::vector<TransactionRecord> parts;
    for (int idx = 0; idx < 2; ++idx) {
        TransactionRecord r(hash, time);
        r.idx = idx;
        r.vout = static_cast<unsigned int>(idx + 1);
        r.type = TransactionRecord::Generated;
        if (idx == 0) {
            r.address = "mStakeReturnAddress";
            r.credit  = 1000000000;   // gross: returned principal + reward
            r.debit   = -900000000;   // the staked input
        } else {
            r.address = "mSideStakeAddress";
            r.debit   = -10000000;
        }
        r.status.status = status;
        r.status.generated_type = GRC::MinedType::POR;
        r.status.sortKey = statusSortKey(height, static_cast<unsigned int>(time), idx);
        parts.push_back(r);
    }
    return parts;
}

//! A settled single-part received payment, for seeding history.
std::vector<TransactionRecord> makePayment(const uint256& hash, int64_t time, int height)
{
    TransactionRecord r(hash, time);
    r.idx = 0;
    r.type = TransactionRecord::RecvWithAddress;
    r.address = "mSeedAddress";
    r.credit = 100000000;
    r.status.status = TransactionStatus::Confirmed;
    r.status.sortKey = statusSortKey(height, static_cast<unsigned int>(time), 0);
    return {r};
}

//! Register the two cursors exactly as the Qt consumers do: DetailedTxModel uses
//! a default FilterSpec (show_orphans=false, show_inactive=true, limit_rows=-1)
//! sorted Date DESC; OverviewTxModel sets show_inactive=false and a finite cap,
//! sorted Status DESC. Both therefore mask Conflicted/NotAccepted rows.
void registerProductionViews(WalletTxStore& store)
{
    GRC::FilterSpec detail;
    store.registerView(GRC::VIEW_DETAILED, detail, GRC::TXCOL_DATE, GRC::TXSORT_DESC);

    GRC::FilterSpec overview;
    overview.show_inactive = false;
    overview.limit_rows = kOverviewCap;
    store.registerView(GRC::VIEW_OVERVIEW, overview, GRC::TXCOL_STATUS, GRC::TXSORT_DESC);
}

//! Wait until the worker stops producing: the queue depth must hold steady for
//! several polls. Unlike waitForQueue this needs no expected count, so a test can
//! assert on the ABSENCE of per-view events without racing the worker.
//! Wait until the queue stops growing. A HEURISTIC, and weaker than
//! waitForQueue() -- prefer that whenever the expected count is known.
//!
//! The weakness: this cannot distinguish "the worker finished" from "the worker
//! has not started yet". Immediately after an enqueue the queue is legitimately
//! empty, so a short stability window is satisfied before any work happens, and a
//! caller with no positive assertion then passes vacuously.
//!
//! Both numbers were raised after CI's ARM64 job (QEMU emulation, ~15x slower than
//! native) exposed how tight they were: 4 stable polls is 20ms of quiet, which that
//! host clears trivially between the enqueue and the worker waking. 20 polls
//! (100ms) plus a 20s cap makes the vacuous window much harder to hit while still
//! returning immediately on a fast machine.
//!
//! It is still a heuristic. Callers should assert something POSITIVE afterwards --
//! a seen-event flag, an expected row count -- so a premature return fails loudly
//! instead of silently skipping the case the test exists to cover.
void settle(WalletEventQueue& q)
{
    std::size_t last = q.size();
    int stable = 0;
    for (int i = 0; i < 4000 && stable < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const std::size_t now = q.size();
        stable = (now == last) ? stable + 1 : 0;
        last = now;
    }
}

//! Row identity: (hash, idx) is what uniquely names a decomposed part.
using RowKey = std::pair<uint256, int>;

std::vector<RowKey> keysOf(const std::vector<TransactionRecord>& recs)
{
    std::vector<RowKey> out;
    out.reserve(recs.size());
    for (const TransactionRecord& r : recs) out.emplace_back(r.hash, r.idx);
    return out;
}

std::string describe(const RowKey& k)
{
    // Last 8 hex digits: uint256 prints big-endian-reversed, so the leading ones
    // are all zeros for the small synthetic hashes these tests use.
    const std::string hex = k.first.GetHex();
    return hex.substr(hex.size() - 8) + "/" + strprintf("%d", k.second);
}

//! A consumer replica driven PURELY by the event stream, mirroring what
//! OverviewTxModel and DetailedTxModel do: splice a RowsInserted payload's
//! records in at the stamped position, erase a RowsRemoved range, refetch on
//! Change/Reset. The contract every consumer depends on is that this replica
//! equals the store's own served window at every quiescent point — so comparing
//! the two catches both a lost insert (the #3257 shape) and a delta whose
//! payload does not match the slot it names.
struct Replica
{
    explicit Replica(int id) : viewId(id) {}

    int viewId;
    std::vector<RowKey> rows;
    //! Set if a delta named a position outside the replica: the producer and the
    //! consumer have diverged, which the real consumers silently swallow.
    bool out_of_range = false;

    void apply(const std::vector<GRC::WalletEvent>& batch, WalletTxStore& store)
    {
        for (const GRC::WalletEvent& ev : batch) {
            if (const auto* p = std::get_if<GRC::RowsResetPayload>(&ev.payload)) {
                if (p->viewId != viewId) continue;
                rows = keysOf(store.getRows(viewId, 0, -1).records);
            } else if (const auto* p = std::get_if<GRC::RowsInsertedPayload>(&ev.payload)) {
                if (p->viewId != viewId) continue;
                if (p->position < 0
                        || static_cast<std::size_t>(p->position) > rows.size()) {
                    out_of_range = true;
                    continue;
                }
                const std::vector<RowKey> k = keysOf(p->records);
                rows.insert(rows.begin() + p->position, k.begin(), k.end());
            } else if (const auto* p = std::get_if<GRC::RowsRemovedPayload>(&ev.payload)) {
                if (p->viewId != viewId) continue;
                if (p->position < 0 || p->count <= 0
                        || static_cast<std::size_t>(p->position) + static_cast<std::size_t>(p->count)
                               > rows.size()) {
                    out_of_range = true;
                    continue;
                }
                rows.erase(rows.begin() + p->position, rows.begin() + p->position + p->count);
            } else if (const auto* p = std::get_if<GRC::RowsChangedPayload>(&ev.payload)) {
                if (p->viewId != viewId) continue;
                // Apply the records the producer stamped on the payload, exactly as
                // the real consumers do. This replica would still "work" if it
                // refetched, which is why the dedicated test below asserts the
                // payload carries them at all.
                const std::vector<RowKey> fresh = keysOf(p->records);
                for (std::size_t i = 0; i < fresh.size()
                        && static_cast<std::size_t>(p->first) + i < rows.size(); ++i) {
                    rows[static_cast<std::size_t>(p->first) + i] = fresh[i];
                }
            }
        }
    }
};

void applyBatch(const std::vector<GRC::WalletEvent>& batch, WalletTxStore& store,
                Replica& a, Replica& b)
{
    a.apply(batch, store);
    b.apply(batch, store);
}

//! The core invariant: a replica built only from the deltas must equal what the
//! store serves for that view.
void checkReplicaMatchesStore(WalletTxStore& store, const Replica& replica)
{
    BOOST_CHECK_MESSAGE(!replica.out_of_range,
                        "view " << replica.viewId << ": a delta named an out-of-range position");

    const std::vector<RowKey> served = keysOf(store.getRows(replica.viewId, 0, -1).records);
    BOOST_CHECK_MESSAGE(replica.rows.size() == served.size(),
                        "view " << replica.viewId << ": replica has " << replica.rows.size()
                                << " rows, store serves " << served.size());

    const std::size_t n = std::min(replica.rows.size(), served.size());
    for (std::size_t i = 0; i < n; ++i) {
        BOOST_CHECK_MESSAGE(replica.rows[i] == served[i],
                            "view " << replica.viewId << " row " << i << ": replica has "
                                    << describe(replica.rows[i]) << ", store serves "
                                    << describe(served[i]));
    }
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_wallettxstore_tests)

BOOST_AUTO_TEST_CASE(workerDrainsAllInsertsInOrder)
{
    WalletEventQueue q;
    // prime (the only consumer of m_wallet) is not exercised here,
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

//
// Coinstake lifecycle coverage (issue #3257).
//
// The cases below replay what actually happens to a staked block, which the
// suite above never did: a live coinstake is stamped NotAccepted at CT_NEW
// (the wallet is notified inside ConnectBlock, before pindexBest advances), both
// production view filters mask inactive rows, and the row's ONLY route into
// either view is the later applyChainTipRefresh -> applyStatusUpdate flip-in.
// They use the production view configurations rather than permissiveSpec(),
// because the masking IS the thing under test.
//

BOOST_AUTO_TEST_CASE(coinstakeNotAcceptedIsMaskedThenFlipsIntoBothViews)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    registerProductionViews(store);
    q.drain();   // the two registration Resets

    Replica detail(GRC::VIEW_DETAILED);
    Replica overview(GRC::VIEW_OVERVIEW);

    // Seed history so the views are non-trivial and the Overview cap binds.
    for (int i = 0; i < kSeedRows; ++i) {
        store.enqueueInsert(makePayment(hashOf(i), kBaseTime + i, kBaseHeight + i));
    }
    settle(q);
    applyBatch(q.drain(), store, detail, overview);
    BOOST_CHECK_EQUAL(store.getRows(GRC::VIEW_DETAILED, 0, -1).total_accepted, kSeedRows);

    // Replay blocks: each mines a two-part coinstake that arrives NotAccepted and
    // is flipped to Immature by the next tip advance.
    for (int b = 0; b < kBlocks; ++b) {
        const uint256 h = hashOf(kStakeHashBase + b);
        const int64_t t = kBaseTime + kSeedRows + b;
        const int height = kBaseHeight + kSeedRows + b;
        const int before = store.getRows(GRC::VIEW_DETAILED, 0, -1).total_accepted;

        // (a) CT_NEW inside block connection: both parts NotAccepted.
        store.enqueueInsert(makeCoinstake(h, t, height, TransactionStatus::NotAccepted));
        settle(q);
        auto masked = q.drain();

        // The record enters m_records (native VIEW_FULL stream) but neither cursor
        // accepts it: Accepts() rejects inactive rows under both production specs.
        BOOST_CHECK(batchHasViewId(masked, GRC::VIEW_FULL));
        BOOST_CHECK(!batchHasViewId(masked, GRC::VIEW_DETAILED));
        BOOST_CHECK(!batchHasViewId(masked, GRC::VIEW_OVERVIEW));
        BOOST_CHECK_EQUAL(store.getRows(GRC::VIEW_DETAILED, 0, -1).total_accepted, before);
        applyBatch(masked, store, detail, overview);

        // (b) The tip advanced and the block matured into the chain. This is the
        // wallet-free stand-in for applyChainTipRefresh: updateTransaction's
        // in-place path drives Cursor::applyStatusUpdate exactly as the per-tip
        // refresh does, one part at a time.
        store.enqueueUpsert(makeCoinstake(h, t, height, TransactionStatus::Immature));
        settle(q);
        applyBatch(q.drain(), store, detail, overview);

        // Both views must now show the pair. A regression of the #3257 shape
        // stalls this at the first iteration.
        BOOST_CHECK_EQUAL(store.getRows(GRC::VIEW_DETAILED, 0, -1).total_accepted, before + 2);
        checkReplicaMatchesStore(store, detail);
        checkReplicaMatchesStore(store, overview);
    }

    BOOST_CHECK_EQUAL(store.getRows(GRC::VIEW_DETAILED, 0, -1).total_accepted,
                      kSeedRows + 2 * kBlocks);
    // The newest stake is resolvable by identity, i.e. it really is in the view.
    BOOST_CHECK(store.rowForKey(GRC::VIEW_DETAILED, hashOf(kStakeHashBase + kBlocks - 1), 0) >= 0);
}

BOOST_AUTO_TEST_CASE(multiPartInsertDeltaPayloadsMatchServedSlots)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    registerProductionViews(store);
    q.drain();

    Replica detail(GRC::VIEW_DETAILED);
    Replica overview(GRC::VIEW_OVERVIEW);

    for (int i = 0; i < kSeedRows; ++i) {
        store.enqueueInsert(makePayment(hashOf(i), kBaseTime + i, kBaseHeight + i));
    }
    settle(q);
    applyBatch(q.drain(), store, detail, overview);

    // A coinstake that is already accepted when it reaches the store — the
    // re-add half of updateTransaction's remove+insert fallback, and what a
    // post-prime CT_UPDATED does. Both parts pass the filter in ONE
    // insertLocked, so Cursor::applyStoreInsert emits two deltas in a single
    // batch. Each delta's position is an intermediate-state coordinate, so the
    // records the store attaches to them must be captured at emission time.
    //
    // Under the Overview's Status DESC sort this is not academic: the two parts'
    // sort keys differ only in the trailing idx field, so part idx 1 sorts BEFORE
    // part idx 0 and both deltas land at the SAME slot.
    for (int b = 0; b < kBlocks; ++b) {
        const uint256 h = hashOf(kStakeHashBase + b);
        store.enqueueInsert(makeCoinstake(h, kBaseTime + kSeedRows + b,
                                          kBaseHeight + kSeedRows + b,
                                          TransactionStatus::Immature));
        settle(q);
        applyBatch(q.drain(), store, detail, overview);

        checkReplicaMatchesStore(store, detail);
        checkReplicaMatchesStore(store, overview);
    }
}

// A Change payload must CARRY the changed rows, sampled by the producer at
// emission -- not merely name a range for the consumer to fetch later.
//
// Two things went wrong with the fetch-at-apply-time form. The consumer read
// whatever the cursor held when the event was applied, which inside a drained
// batch can be a newer state than the structural position it has applied so far,
// so future content landed in rows that had not moved yet. And every Change cost
// a getRows() on the consumer's thread -- a synchronous IPC round trip in the
// multiprocess build, taking cs_store, which the refresh that produced it is
// holding.
//
// The Replica used by the other tests in this file now applies p->records, so a
// regression to empty payloads would show up there as a stale replica. This test
// pins the payload directly, so the reason is unambiguous when it fails.
BOOST_AUTO_TEST_CASE(changePayloadCarriesTheChangedRecords)
{
    WalletEventQueue q;
    WalletTxStore store(nullptr, q);
    store.start();

    registerProductionViews(store);
    q.drain();

    for (int i = 0; i < kSeedRows; ++i) {
        store.enqueueInsert(makePayment(hashOf(i), kBaseTime + i, kBaseHeight + i));
    }
    settle(q);
    q.drain();

    // Re-send one existing row with a changed credit. It keeps its time and hash,
    // so it does not move: Cursor::applyStatusUpdate takes the same-slot branch and
    // emits a Change rather than a Remove/Insert pair.
    const int target = kSeedRows / 2;
    std::vector<TransactionRecord> updated = makePayment(hashOf(target),
                                                         kBaseTime + target,
                                                         kBaseHeight + target);
    updated.front().credit = 424242;
    store.enqueueUpsert(std::move(updated));
    settle(q);

    bool saw_change = false;
    for (const GRC::WalletEvent& ev : q.drain()) {
        const auto* p = std::get_if<GRC::RowsChangedPayload>(&ev.payload);
        if (!p || p->viewId != GRC::VIEW_DETAILED) continue;

        saw_change = true;
        // The payload is self-describing: one record per changed row.
        BOOST_CHECK_EQUAL(p->records.size(), static_cast<std::size_t>(p->count));
        BOOST_REQUIRE(!p->records.empty());
        // And it is the NEW content, not a stale copy.
        BOOST_CHECK_EQUAL(p->records.front().credit, 424242);
        // The stamped record must match what the store serves at that position,
        // which is what makes the consumer's replica converge without refetching.
        const std::vector<TransactionRecord> served =
            store.getRows(GRC::VIEW_DETAILED, p->first, p->count).records;
        BOOST_REQUIRE_EQUAL(served.size(), p->records.size());
        BOOST_CHECK(served.front().hash == p->records.front().hash);
    }

    // Non-vacuous: if the upsert stopped producing a Change at all, this test
    // would otherwise pass while asserting nothing.
    BOOST_REQUIRE_MESSAGE(saw_change, "the in-place upsert emitted no Change delta for VIEW_DETAILED");
}

BOOST_AUTO_TEST_SUITE_END()
