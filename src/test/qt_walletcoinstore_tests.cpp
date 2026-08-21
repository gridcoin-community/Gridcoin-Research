// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the coin-store worker / double-queue
// (src/wallet/walletcoinstore.{h,cpp}, #3183), driven with a NULL wallet and
// synthetic CoinRecords — the qt_wallettxstore_tests discipline. The park
// protocol and lock ordering are structurally identical to WalletTxStore
// (covered by its suite); this suite pins the coin-specific behavior: upsert
// diffing, suppress-when-unwatched, the validated selection mirror (the
// phantom-selection race), and the server-side value filter's parity
// semantics including the tie-break and both legacy call shapes.

#include <wallet/walletcoinstore.h>
#include <wallet/wallet_event_queue.h>

#include <arith_uint256.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <climits>
#include <string>
#include <thread>
#include <vector>

using GRC::CoinRecord;
using GRC::CoinViewMode;
using GRC::WalletCoinEventQueue;
using GRC::WalletCoinStore;

namespace {

constexpr int kView = GRC::VIEW_COIN_CONTROL;

uint256 hashOf(int n)
{
    return ArithToUint256(arith_uint256(static_cast<uint64_t>(n) + 1));
}

CoinRecord makeCoin(const uint256& hash, unsigned int vout, int64_t amount,
                    const std::string& group, int height = 100)
{
    CoinRecord r;
    r.outpoint = COutPoint(hash, vout);
    r.amount = amount;
    r.address = group;
    r.group_address = group;
    r.time = 0;
    r.block_height = height;
    r.is_change = false;
    return r;
}

//! Null-wallet store + queue; helpers to wait for the worker to drain.
struct Harness {
    WalletCoinEventQueue queue;
    WalletCoinStore store{nullptr, queue};

    Harness() { store.start(); }

    //! Wait until the flat total for kView reaches \p expected (bounded).
    bool waitForTotal(int expected)
    {
        for (int i = 0; i < 5000; ++i) {
            if (store.getRows(kView, 0, 0).total_accepted == expected) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

    //! Wait until the group's member total reaches \p expected (bounded).
    bool waitForGroupTotal(const std::string& group, int expected)
    {
        for (int i = 0; i < 5000; ++i) {
            if (store.getGroupRows(kView, group, 0, 0).total_accepted == expected) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

    //! Wait until the shared (view-independent) group directory holds
    //! \p expected groups — the only observable while no view is registered.
    bool waitForDirectorySize(std::size_t expected)
    {
        for (int i = 0; i < 5000; ++i) {
            if (store.getGroupDirectory().size() == expected) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }
};

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_walletcoinstore_tests)

BOOST_AUTO_TEST_CASE(workerDrainsUpsertsInOrder)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Flat, GRC::COINCOL_AMOUNT, 1);
    h.queue.drain(); // discard the registration Reset

    const uint256 h1 = hashOf(1), h2 = hashOf(2);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A"), makeCoin(h1, 1, 100, "A")}, false);
    h.store.enqueueUpsert(h2, {makeCoin(h2, 0, 200, "B")}, false);

    BOOST_REQUIRE(h.waitForTotal(3));
    auto rows = h.store.getRows(kView, 0, -1);
    BOOST_REQUIRE_EQUAL(rows.records.size(), 3u);
    BOOST_CHECK_EQUAL(rows.records[0].amount, 300);
    BOOST_CHECK_EQUAL(rows.records[1].amount, 200);
    BOOST_CHECK_EQUAL(rows.records[2].amount, 100);

    // Events arrived in mutation order with monotonic seqnos.
    auto events = h.queue.drain();
    BOOST_REQUIRE(!events.empty());
    for (std::size_t i = 1; i < events.size(); ++i) {
        BOOST_CHECK(events[i].seqno > events[i - 1].seqno);
    }
}

BOOST_AUTO_TEST_CASE(upsertDiffsSpendConfirmAndRestore)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1);
    // Two coins; one unconfirmed (height -1).
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A"),
                               makeCoin(h1, 1, 100, "A", /*height*/ -1)}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 2));

    // "Confirm" output 1 (height fill-in) and "spend" output 0 (absent from
    // the fresh decomposition) in one upsert — the single diff path.
    h.store.enqueueUpsert(h1, {makeCoin(h1, 1, 100, "A", /*height*/ 200)}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 1));

    auto rows = h.store.getGroupRows(kView, "A", 0, -1);
    BOOST_REQUIRE_EQUAL(rows.records.size(), 1u);
    BOOST_CHECK(rows.records[0].outpoint == COutPoint(h1, 1));
    BOOST_CHECK_EQUAL(rows.records[0].block_height, 200);

    // "Restore" output 0 (reorg unspend arrives as a re-decomposition that
    // includes it again).
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A"),
                               makeCoin(h1, 1, 100, "A", 200)}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 2));

    // Empty upsert removes everything for the hash (a fully-spent tx).
    h.store.enqueueUpsert(h1, {}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 0));
    BOOST_CHECK_EQUAL(h.store.getGroups(kView, 0, -1).total_groups, 0);
}

BOOST_AUTO_TEST_CASE(suppressWhenUnwatchedAndClearOnLastUnregister)
{
    Harness h;

    // No view registered: mutations maintain the store but push no events.
    // Wait for the worker to absorb the upsert BEFORE registering, so the
    // event-count assertion below cannot race the intake drain.
    const uint256 h1 = hashOf(1);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A")}, false);
    BOOST_REQUIRE(h.waitForDirectorySize(1));
    BOOST_CHECK_EQUAL(h.queue.size(), static_cast<std::size_t>(0));

    // Register and verify the records were kept warm.
    h.store.registerView(kView, CoinViewMode::Flat, GRC::COINCOL_AMOUNT, 1);
    BOOST_REQUIRE(h.waitForTotal(1));
    BOOST_CHECK_EQUAL(h.queue.size(), static_cast<std::size_t>(1)); // the Reset only

    // Live events flow while watched...
    h.queue.drain();
    const uint256 h2 = hashOf(2);
    h.store.enqueueUpsert(h2, {makeCoin(h2, 0, 200, "B")}, false);
    BOOST_REQUIRE(h.waitForTotal(2));
    BOOST_CHECK(h.queue.size() > 0);

    // ...and the last unregister discards the backlog.
    h.store.unregisterView(kView);
    BOOST_CHECK_EQUAL(h.queue.size(), static_cast<std::size_t>(0));

    // Unwatched mutations stay silent; seqnos remain monotonic across the
    // clear (pinned by re-registering and draining the fresh Reset).
    const uint256 h3 = hashOf(3);
    h.store.enqueueUpsert(h3, {makeCoin(h3, 0, 100, "C")}, false);
    h.store.registerView(kView, CoinViewMode::Flat, GRC::COINCOL_AMOUNT, 1);
    BOOST_REQUIRE(h.waitForTotal(3));
    auto events = h.queue.drain();
    BOOST_REQUIRE(!events.empty());
}

BOOST_AUTO_TEST_CASE(phantomSelectionIsRefused)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A")}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 1));

    // Select while present: applied, aggregates update synchronously.
    auto update = h.store.setSelected(COutPoint(h1, 0), true);
    BOOST_CHECK(update.applied);
    BOOST_CHECK_EQUAL(update.group.selected_count, 1);

    // The coin is spent (worker removal) before the user's next click lands:
    // the toggle must be REFUSED — accepting it would plant a mirror entry
    // whose removal event was emitted before the phantom insert, so no
    // future event could repair the divergence.
    h.store.enqueueUpsert(h1, {}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 0));

    update = h.store.setSelected(COutPoint(h1, 0), true);
    BOOST_CHECK(!update.applied);

    // And the earlier selection was pruned with the removal.
    auto reconciled = h.store.reconcileSelection({COutPoint(h1, 0)});
    BOOST_CHECK(reconciled.empty());
}

BOOST_AUTO_TEST_CASE(selectGroupAndSelectAllReturnExactDeltas)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1), h2 = hashOf(2);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A"), makeCoin(h1, 1, 100, "A")}, false);
    h.store.enqueueUpsert(h2, {makeCoin(h2, 0, 200, "B")}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 2));
    BOOST_REQUIRE(h.waitForGroupTotal("B", 1));

    auto result = h.store.selectGroup("A", true);
    BOOST_CHECK_EQUAL(result.added.size(), 2u);
    BOOST_CHECK_EQUAL(result.removed.size(), 0u);

    // Selecting an already-selected member is not re-reported.
    result = h.store.selectAll(true);
    BOOST_CHECK_EQUAL(result.added.size(), 1u); // only B's coin
    BOOST_CHECK(result.added[0] == COutPoint(h2, 0));

    // Group aggregates drive the tristate.
    auto groups = h.store.getGroups(kView, 0, -1);
    for (const auto& g : groups.groups) {
        BOOST_CHECK_EQUAL(g.selected_count, g.output_count);
    }

    result = h.store.selectAll(false);
    BOOST_CHECK_EQUAL(result.removed.size(), 3u);
}

BOOST_AUTO_TEST_CASE(bulkSelectionCoalescesOneGroupEventPerGroup)
{
    // A bulk op toggles every member of a group, but every one of those
    // toggles refreshes the SAME directory row: emitting per record puts one
    // event per coin on the queue (half a million for the #3183 gate wallet),
    // which the consumer then drains a bounded batch at a time. The bulk paths
    // must coalesce to ONE GroupChange per touched group per view.
    Harness h;
    h.store.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1), h2 = hashOf(2);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A"), makeCoin(h1, 1, 100, "A"),
                               makeCoin(h1, 2, 150, "A")}, false);
    h.store.enqueueUpsert(h2, {makeCoin(h2, 0, 200, "B")}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 3));
    BOOST_REQUIRE(h.waitForGroupTotal("B", 1));

    auto countGroupChanges = [&]() {
        std::size_t n = 0;
        for (const GRC::WalletCoinEvent& ev : h.queue.drain()) {
            if (std::get_if<GRC::CoinGroupsChangedPayload>(&ev.payload)) ++n;
        }
        return n;
    };
    countGroupChanges(); // discard the seeding events

    // Three members of A toggled -> one event, not three.
    h.store.selectGroup("A", true);
    BOOST_CHECK_EQUAL(countGroupChanges(), 1u);

    // selectAll touches both groups (A is already selected, so only B's coin
    // actually toggles) -> one event for the group that moved.
    h.store.selectAll(true);
    BOOST_CHECK_EQUAL(countGroupChanges(), 1u);

    // Deselecting everything touches both groups -> exactly two.
    h.store.selectAll(false);
    BOOST_CHECK_EQUAL(countGroupChanges(), 2u);

    // The aggregates still track the mirror exactly after the coalescing.
    h.store.selectGroup("A", true);
    countGroupChanges();
    for (const auto& g : h.store.getGroups(kView, 0, -1).groups) {
        if (g.address == "A") {
            BOOST_CHECK_EQUAL(g.selected_count, 3);
            BOOST_CHECK_EQUAL(g.selected_amount, 550);
        } else {
            BOOST_CHECK_EQUAL(g.selected_count, 0);
        }
    }

    // The value filter's prune + cap passes coalesce the same way: one event
    // for the single group whose members it deselects.
    h.store.applyValueFilter(/*less_or_equal=*/true, 120, /*max_inputs=*/1000);
    BOOST_CHECK_EQUAL(countGroupChanges(), 1u);
}

BOOST_AUTO_TEST_CASE(reconcileSelectionNotifiesOtherViews)
{
    // reconcileSelection mutates the VIEW-INDEPENDENT group aggregates. The
    // reconciling view reads them back directly, but a second registered view
    // (the consolidate wizard page) only learns through events — without them
    // its parent rows keep a stale tristate forever.
    constexpr int kSecondView = kView + 1;

    Harness h;
    h.store.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);
    h.store.registerView(kSecondView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A"), makeCoin(h1, 1, 100, "A")}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 2));
    h.queue.drain();

    h.store.reconcileSelection({COutPoint(h1, 0)});

    std::size_t second_view_events = 0;
    for (const GRC::WalletCoinEvent& ev : h.queue.drain()) {
        if (const auto* p = std::get_if<GRC::CoinGroupsChangedPayload>(&ev.payload)) {
            if (p->view_id == kSecondView) ++second_view_events;
        }
    }
    BOOST_CHECK_EQUAL(second_view_events, 1u);

    // And the aggregate the event tells it to refetch is the reconciled one.
    auto groups = h.store.getGroups(kSecondView, 0, -1);
    BOOST_REQUIRE_EQUAL(groups.groups.size(), 1u);
    BOOST_CHECK_EQUAL(groups.groups[0].selected_count, 1);
}

BOOST_AUTO_TEST_CASE(valueFilterParityAndTieBreak)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Flat, GRC::COINCOL_AMOUNT, 1);

    // Amounts: 50, 100, 100, 100, 200 — the three 100s tie.
    const uint256 h1 = hashOf(1), h2 = hashOf(2), h3 = hashOf(3);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 50, "A"), makeCoin(h1, 1, 100, "A")}, false);
    h.store.enqueueUpsert(h2, {makeCoin(h2, 0, 100, "B"), makeCoin(h2, 1, 200, "B")}, false);
    h.store.enqueueUpsert(h3, {makeCoin(h3, 0, 100, "C")}, false);
    BOOST_REQUIRE(h.waitForTotal(5));

    h.store.selectAll(true);

    // Filter-button shape: predicate only, no cap (UINT_MAX). less_or_equal
    // 100 deselects the 200.
    auto result = h.store.applyValueFilter(true, 100, UINT_MAX);
    BOOST_CHECK_EQUAL(result.removed.size(), 1u);
    BOOST_CHECK(result.removed[0] == COutPoint(h2, 1));
    BOOST_CHECK(!result.culled);

    // Consolidation shape: always-true predicate + cap 3. Prune-only keeps
    // the 3 SMALLEST; the 100s tie-break by outpoint (hash asc, then n), so
    // the survivor set is deterministic: 50, then the two lowest-outpoint
    // 100s (h1:1 and h2:0), culling h3:0.
    result = h.store.applyValueFilter(true, INT64_MAX, 3);
    BOOST_CHECK(result.culled);
    BOOST_REQUIRE_EQUAL(result.removed.size(), 1u);
    BOOST_CHECK(result.removed[0] == COutPoint(h3, 0));

    // The filter never SELECTS: re-running with a permissive predicate and a
    // generous cap adds nothing.
    result = h.store.applyValueFilter(true, INT64_MAX, UINT_MAX);
    BOOST_CHECK_EQUAL(result.added.size(), 0u);
    BOOST_CHECK_EQUAL(result.removed.size(), 0u);

    // Greater-or-equal shape keeps the LARGEST under the cap.
    h.store.selectAll(true);
    result = h.store.applyValueFilter(false, 100, 2);
    // Predicate drops the 50; the cap keeps the two largest survivors
    // (200 and the highest-outpoint 100 = h3:0).
    bool removed_50 = false;
    for (const auto& op : result.removed) {
        if (op == COutPoint(h1, 0)) removed_50 = true;
    }
    BOOST_CHECK(removed_50);
    BOOST_CHECK(result.culled);
}

BOOST_AUTO_TEST_CASE(reconcilePrunesAndRestoresAggregates)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1), h2 = hashOf(2);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A")}, false);
    BOOST_REQUIRE(h.waitForGroupTotal("A", 1));

    // The GUI hands over a set containing one live and one stale outpoint.
    auto pruned = h.store.reconcileSelection({COutPoint(h1, 0), COutPoint(h2, 5)});
    BOOST_REQUIRE_EQUAL(pruned.size(), 1u);
    BOOST_CHECK(pruned.count(COutPoint(h1, 0)) == 1);

    auto groups = h.store.getGroups(kView, 0, -1);
    BOOST_REQUIRE_EQUAL(groups.groups.size(), 1u);
    BOOST_CHECK_EQUAL(groups.groups[0].selected_count, 1);
    BOOST_CHECK_EQUAL(groups.groups[0].selected_amount, 300);

    // A second reconcile with an empty set clears the aggregates.
    pruned = h.store.reconcileSelection({});
    BOOST_CHECK(pruned.empty());
    groups = h.store.getGroups(kView, 0, -1);
    BOOST_CHECK_EQUAL(groups.groups[0].selected_count, 0);
}

BOOST_AUTO_TEST_CASE(depthIsServeTimeFromTip)
{
    Harness h;
    h.store.registerView(kView, CoinViewMode::Flat, GRC::COINCOL_AMOUNT, 1);

    const uint256 h1 = hashOf(1);
    h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A", /*height*/ 100),
                               makeCoin(h1, 1, 100, "A", /*height*/ -1)}, false);
    BOOST_REQUIRE(h.waitForTotal(2));

    // Null-wallet applyChainTipRefresh updates the serve-time base and (with
    // a view registered) pushes the depth-refresh marker. cs_main is not
    // actually needed by the null-wallet path, but the annotation demands the
    // caller hold it in production; the test drives the store directly.
    {
        LOCK(cs_main);
        h.queue.drain();
        h.store.applyChainTipRefresh(149);
    }

    auto rows = h.store.getRows(kView, 0, -1);
    BOOST_REQUIRE_EQUAL(rows.records.size(), 2u);
    BOOST_CHECK_EQUAL(rows.records[0].depth, 50); // 149 - 100 + 1
    BOOST_CHECK_EQUAL(rows.records[1].depth, 0);  // unconfirmed

    // The marker arrived.
    bool saw_refresh = false;
    for (const auto& ev : h.queue.drain()) {
        if (std::holds_alternative<GRC::CoinDepthRefreshPayload>(ev.payload)) {
            saw_refresh = true;
        }
    }
    BOOST_CHECK(saw_refresh);
}

BOOST_AUTO_TEST_CASE(cleanShutdownWithPendingIntake)
{
    // Destruction with queued intake must not hang or crash (the tx-store
    // dtor contract).
    Harness h;
    const uint256 h1 = hashOf(1);
    for (int i = 0; i < 100; ++i) {
        h.store.enqueueUpsert(h1, {makeCoin(h1, 0, 300, "A")}, false);
    }
    // ~Harness runs ~WalletCoinStore with a possibly non-empty intake.
}

BOOST_AUTO_TEST_SUITE_END()
