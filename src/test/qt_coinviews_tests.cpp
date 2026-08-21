// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free grouped index core
// (src/wallet/coinviews.{h,cpp}) of the windowed coin-control selection model
// (issue #3183): flat/tree index maintenance, directory aggregate arithmetic,
// per-scope delta emission, the scope-local high-water + per-view floor
// contract (exercised against the real GRC::WindowCache gate — the design's
// central novel invariant), and windowed reads within a pathological
// single-address group. Driven from synthetic record tables, never a real
// CWallet — the windowed tx-table risk-control discipline.

#include <qt/windowcache.h>
#include <wallet/coinviews.h>

#include <arith_uint256.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <util/strencodings.h>

#include <string>
#include <vector>

using GRC::ApplyResult;
using GRC::CoinRecord;
using GRC::CoinViewDelta;
using GRC::CoinViewMode;
using GRC::CoinViews;
using GRC::WindowCache;
using GRC::WindowCacheSink;

namespace {

constexpr int kView = GRC::VIEW_COIN_CONTROL;

uint256 hashOf(int n)
{
    return ArithToUint256(arith_uint256(static_cast<uint64_t>(n) + 1));
}

CoinRecord makeCoin(int seed, int64_t amount, const std::string& group,
                    bool is_change = false, int64_t time = 0, int height = 100)
{
    CoinRecord r;
    r.outpoint = COutPoint(hashOf(seed), 0);
    r.amount = amount;
    r.address = is_change ? ("change" + ToString(seed)) : group;
    r.group_address = group;
    r.label = "";
    r.time = time;
    r.block_height = height;
    r.is_change = is_change;
    return r;
}

//! Synthetic backing table + the CoinViews under test, wired the way the
//! store drives it: mutate the table first (append / compact), then call the
//! matching apply* (applyRemove is called before the compaction).
struct Harness {
    std::vector<CoinRecord> table;
    CoinViews cv;

    Harness() : cv([this](std::size_t i) -> const CoinRecord& { return table[i]; }) {}

    std::vector<CoinViewDelta> insert(const CoinRecord& r)
    {
        table.push_back(r);
        return cv.applyInsert(table.size() - 1);
    }

    std::vector<CoinViewDelta> remove(std::size_t absidx, bool was_selected = false)
    {
        auto deltas = cv.applyRemove(absidx, was_selected);
        table.erase(table.begin() + absidx);
        return deltas;
    }
};

//! No-op sink for WindowCache integration checks.
struct NullSink : WindowCacheSink {
    void beginReset() override {}
    void endReset() override {}
    void beginInsert(int, int) override {}
    void endInsert() override {}
    void beginRemove(int, int) override {}
    void endRemove() override {}
    void dataChanged(int, int) override {}
};

int countType(const std::vector<CoinViewDelta>& deltas, CoinViewDelta::Type type)
{
    int n = 0;
    for (const auto& d : deltas) {
        if (d.type == type) ++n;
    }
    return n;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_coinviews_tests)

BOOST_AUTO_TEST_CASE(flatViewSortsAndSlices)
{
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A"));
    h.table.push_back(makeCoin(2, 100, "A"));
    h.table.push_back(makeCoin(3, 200, "B"));

    // Amount descending — the dialog default.
    auto deltas = h.cv.registerView(kView, CoinViewMode::Flat,
                                    GRC::COINCOL_AMOUNT, /*desc*/ 1, h.table.size());
    BOOST_REQUIRE_EQUAL(deltas.size(), 1u);
    BOOST_CHECK(deltas[0].type == CoinViewDelta::Reset);

    int total = 0;
    auto slice = h.cv.flatSlice(kView, 0, -1, total);
    BOOST_CHECK_EQUAL(total, 3);
    BOOST_REQUIRE_EQUAL(slice.size(), 3u);
    BOOST_CHECK_EQUAL(h.table[slice[0]].amount, 300);
    BOOST_CHECK_EQUAL(h.table[slice[1]].amount, 200);
    BOOST_CHECK_EQUAL(h.table[slice[2]].amount, 100);

    // Windowed sub-slice.
    auto mid = h.cv.flatSlice(kView, 1, 1, total);
    BOOST_REQUIRE_EQUAL(mid.size(), 1u);
    BOOST_CHECK_EQUAL(h.table[mid[0]].amount, 200);
}

BOOST_AUTO_TEST_CASE(treeViewDirectoryAndAggregates)
{
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A"));
    h.table.push_back(makeCoin(2, 100, "A", /*is_change*/ true));
    h.table.push_back(makeCoin(3, 200, "B"));

    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    int total = 0;
    auto dir = h.cv.directorySlice(kView, 0, -1, total);
    BOOST_CHECK_EQUAL(total, 2);
    BOOST_REQUIRE_EQUAL(dir.size(), 2u);
    // A totals 400, B totals 200 — amount-desc directory.
    BOOST_CHECK_EQUAL(dir[0], "A");
    BOOST_CHECK_EQUAL(dir[1], "B");

    auto a = h.cv.groupInfo("A");
    BOOST_CHECK_EQUAL(a.total_amount, 400);
    BOOST_CHECK_EQUAL(a.output_count, 2);
    BOOST_CHECK_EQUAL(a.direct_output_count, 1); // the change member doesn't count
    BOOST_CHECK_EQUAL(a.selected_count, 0);

    // Group scope slice, child order amount-desc.
    auto members = h.cv.groupSlice(kView, "A", 0, -1, total);
    BOOST_CHECK_EQUAL(total, 2);
    BOOST_REQUIRE_EQUAL(members.size(), 2u);
    BOOST_CHECK_EQUAL(h.table[members[0]].amount, 300);
    BOOST_CHECK_EQUAL(h.table[members[1]].amount, 100);

    // Unknown group: empty, total 0.
    auto none = h.cv.groupSlice(kView, "Z", 0, -1, total);
    BOOST_CHECK(none.empty());
    BOOST_CHECK_EQUAL(total, 0);

    // The address-ordered picker directory.
    auto directory = h.cv.groupDirectory();
    BOOST_REQUIRE_EQUAL(directory.size(), 2u);
    BOOST_CHECK_EQUAL(directory[0].address, "A");
    BOOST_CHECK_EQUAL(directory[1].address, "B");
}

BOOST_AUTO_TEST_CASE(insertEmitsMemberAndDirectoryDeltas)
{
    Harness h;
    h.table.push_back(makeCoin(1, 500, "A"));
    h.table.push_back(makeCoin(2, 400, "B"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    // New group C (amount 100): member insert + directory GroupInsert at the end.
    auto deltas = h.insert(makeCoin(3, 100, "C"));
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::Insert), 1);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupInsert), 1);
    for (const auto& d : deltas) {
        if (d.type == CoinViewDelta::Insert) BOOST_CHECK_EQUAL(d.scope, "C");
        if (d.type == CoinViewDelta::GroupInsert) BOOST_CHECK_EQUAL(d.first, 2);
    }

    // A big coin lands in B (total 400 -> 900 > A's 500): the directory row
    // MOVES, which must go out as GroupRemove + GroupInsert — an in-place
    // GroupChange would silently diverge the consumer's positional replica.
    deltas = h.insert(makeCoin(4, 500, "B"));
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupRemove), 1);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupInsert), 1);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupChange), 0);

    int total = 0;
    auto dir = h.cv.directorySlice(kView, 0, -1, total);
    BOOST_REQUIRE_EQUAL(dir.size(), 3u);
    BOOST_CHECK_EQUAL(dir[0], "B");
    BOOST_CHECK_EQUAL(dir[1], "A");

    // A small coin into A (500 -> 600, still second): in place — GroupChange.
    deltas = h.insert(makeCoin(5, 100, "A"));
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupChange), 1);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupRemove), 0);
}

BOOST_AUTO_TEST_CASE(removeCompactsAndKeepsMapping)
{
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A"));
    h.table.push_back(makeCoin(2, 200, "A"));
    h.table.push_back(makeCoin(3, 100, "B"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    // Remove the middle record (absidx 1): its group survives; the absolute
    // index of the old record 2 shifts down and every index must keep
    // resolving to the right records afterwards.
    auto deltas = h.remove(1);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::Remove), 1);
    for (const auto& d : deltas) {
        if (d.type == CoinViewDelta::Remove) {
            BOOST_CHECK_EQUAL(d.scope, "A");
            BOOST_CHECK_EQUAL(d.first, 1); // second row of A's amount-desc children
        }
    }

    auto a = h.cv.groupInfo("A");
    BOOST_CHECK_EQUAL(a.output_count, 1);
    BOOST_CHECK_EQUAL(a.total_amount, 300);

    int total = 0;
    auto members = h.cv.groupSlice(kView, "B", 0, -1, total);
    BOOST_REQUIRE_EQUAL(members.size(), 1u);
    BOOST_CHECK_EQUAL(h.table[members[0]].amount, 100); // shifted index still maps

    // Removing B's last coin kills the group.
    deltas = h.remove(members[0]);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupRemove), 1);
    BOOST_CHECK(h.cv.groupInfo("B").address.empty());
    auto dir = h.cv.directorySlice(kView, 0, -1, total);
    BOOST_CHECK_EQUAL(total, 1);
}

BOOST_AUTO_TEST_CASE(updateReslotsAndRegroups)
{
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A", false, 0, -1)); // unconfirmed
    h.table.push_back(makeCoin(2, 200, "A", false, 0, 50));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_CONFS, 0, h.table.size());

    // Ascending confirmations: unconfirmed first. Record 0 confirms at a low
    // height (many confs) — it moves behind record 1: Remove + Insert.
    h.table[0].block_height = 10;
    auto deltas = h.cv.applyUpdate(0, "A", false, false);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::Remove), 1);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::Insert), 1);

    int total = 0;
    auto members = h.cv.groupSlice(kView, "A", 0, -1, total);
    BOOST_REQUIRE_EQUAL(members.size(), 2u);
    BOOST_CHECK_EQUAL(h.table[members[0]].block_height, 50); // fewer confs first
    BOOST_CHECK_EQUAL(h.table[members[1]].block_height, 10);

    // Regroup (the address-book change-walk flip): record 1 moves A -> B,
    // selected. Member Remove from A, Insert into B, new-group GroupInsert,
    // and the selection aggregate moves with it.
    h.cv.applySelection(1, true);
    h.table[1].group_address = "B";
    h.table[1].is_change = false;
    deltas = h.cv.applyUpdate(1, "A", false, /*was_selected*/ true);

    bool removed_from_a = false, inserted_into_b = false;
    for (const auto& d : deltas) {
        if (d.type == CoinViewDelta::Remove && d.scope == "A") removed_from_a = true;
        if (d.type == CoinViewDelta::Insert && d.scope == "B") inserted_into_b = true;
    }
    BOOST_CHECK(removed_from_a);
    BOOST_CHECK(inserted_into_b);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupInsert), 1);

    BOOST_CHECK_EQUAL(h.cv.groupInfo("A").selected_count, 0);
    BOOST_CHECK_EQUAL(h.cv.groupInfo("B").selected_count, 1);
    BOOST_CHECK_EQUAL(h.cv.groupInfo("B").selected_amount, 200);
}

BOOST_AUTO_TEST_CASE(selectionAggregatesAndDeltas)
{
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A"));
    h.table.push_back(makeCoin(2, 200, "A"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    auto deltas = h.cv.applySelection(0, true);
    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupChange), 1);

    auto a = h.cv.groupInfo("A");
    BOOST_CHECK_EQUAL(a.selected_count, 1);
    BOOST_CHECK_EQUAL(a.selected_amount, 300);

    h.cv.applySelection(1, true);
    BOOST_CHECK_EQUAL(h.cv.groupInfo("A").selected_count, 2); // == output_count: parent checked

    // Removing a selected coin pulls the aggregates down with it.
    h.remove(0, /*was_selected*/ true);
    a = h.cv.groupInfo("A");
    BOOST_CHECK_EQUAL(a.selected_count, 1);
    BOOST_CHECK_EQUAL(a.selected_amount, 200);
}

BOOST_AUTO_TEST_CASE(highWaterScopeAndFloorArithmetic)
{
    Harness h;
    h.table.push_back(makeCoin(1, 100, "A"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "A"), 0u);

    h.cv.noteScopeEvent(kView, "A", 5);
    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "A"), 5u);
    // Another scope's event does NOT advance A's high-water (the gap-tolerant
    // per-scope stream).
    h.cv.noteScopeEvent(kView, "B", 6);
    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "A"), 5u);

    // A broadcast (Reset/resync) floors EVERY scope of the view — including
    // scopes that have never seen an event of their own.
    h.cv.noteBroadcast(kView, 9);
    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "A"), 9u);
    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "never-seen"), 9u);
    BOOST_CHECK_EQUAL(h.cv.directoryHighWater(kView), 9u);

    // Scope events past the floor take over again.
    h.cv.noteScopeEvent(kView, "A", 12);
    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "A"), 12u);
    h.cv.noteDirectoryEvent(kView, 13);
    BOOST_CHECK_EQUAL(h.cv.directoryHighWater(kView), 13u);
}

BOOST_AUTO_TEST_CASE(broadcastFloorPreventsWindowCacheGateLivelock)
{
    // The adversarial-review livelock scenario, against the REAL WindowCache
    // gate: a sort change arrives as one global-seqno Reset; a floorless
    // high-water would leave every subsequent fetch permanently rejected on a
    // quiescent wallet (fetch high_water < the cache's reseeded baseline).
    Harness h;
    for (int i = 0; i < 4; ++i) h.table.push_back(makeCoin(i, 100 * (i + 1), "A"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    NullSink sink;
    WindowCache<CoinRecord> cache;

    // Steady state: scope A saw its last event at seqno 42.
    h.cv.noteScopeEvent(kView, "A", 42);

    // User flips the sort. The store emits CoinReset with queue seqno 60 and
    // records it as a broadcast; the consumer destroys and reseeds the cache
    // from a fresh fetch, whose high-water reflects the floor.
    h.cv.setViewSort(kView, GRC::COINCOL_AMOUNT, 0, h.table.size());
    h.cv.noteBroadcast(kView, 60);

    int total = 0;
    auto slice = h.cv.groupSlice(kView, "A", 0, -1, total);
    std::vector<CoinRecord> rows;
    for (auto idx : slice) rows.push_back(h.table[idx]);
    cache.seedInitial(rows, 0, total, h.cv.epoch(kView), h.cv.highWater(kView, "A"));

    // Quiescent wallet, user scrolls: the fetch carries the SAME high-water
    // and must be adopted.
    BOOST_CHECK(cache.fillContent(sink, 0, rows, h.cv.epoch(kView),
                                  h.cv.highWater(kView, "A")));

    // A floorless implementation would have returned the stale scope seqno
    // (42) for the reseed-time and fetch-time high-water of a scope the Reset
    // never touched... but the cache baseline would then have to come from
    // the Reset event's own seqno (60, the tx-channel applyReset pattern),
    // and 42 != 60 rejects EVERY fetch forever. Pin exactly that mismatch:
    cache.seedInitial(rows, 0, total, h.cv.epoch(kView), /*applyReset baseline*/ 60);
    BOOST_CHECK(!cache.fillContent(sink, 0, rows, h.cv.epoch(kView), /*stale scope*/ 42));

    // With the floor, later scope events resume normally: an insert for A at
    // seqno 61 advances the scope past the floor and the gate tracks it.
    h.table.push_back(makeCoin(99, 50, "A"));
    auto deltas = h.cv.applyInsert(h.table.size() - 1);
    BOOST_REQUIRE(!deltas.empty());
    h.cv.noteScopeEvent(kView, "A", 61);
    BOOST_CHECK_EQUAL(h.cv.highWater(kView, "A"), 61u);
}

BOOST_AUTO_TEST_CASE(multiScopeInterleaveGateAcceptsAndRejects)
{
    // Two scope caches; an event for one scope must not invalidate the
    // other's content fetches (the reason high-water is scope-local at all),
    // and the affected scope's fetch is rejected until its cache applies the
    // structural delta.
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A"));
    h.table.push_back(makeCoin(2, 200, "A"));
    h.table.push_back(makeCoin(3, 100, "B"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    NullSink sink;
    WindowCache<CoinRecord> cacheA, cacheB;

    auto seed = [&](WindowCache<CoinRecord>& cache, const std::string& scope) {
        int total = 0;
        auto slice = h.cv.groupSlice(kView, scope, 0, -1, total);
        std::vector<CoinRecord> rows;
        for (auto idx : slice) rows.push_back(h.table[idx]);
        cache.seedInitial(std::move(rows), 0, total, h.cv.epoch(kView),
                          h.cv.highWater(kView, scope));
    };
    seed(cacheA, "A");
    seed(cacheB, "B");

    // A coin arrives in B: global seqno 7, scope-B event.
    h.table.push_back(makeCoin(4, 400, "B"));
    auto deltas = h.cv.applyInsert(h.table.size() - 1);
    h.cv.noteScopeEvent(kView, "B", 7);

    // Scope A's fetch still gates against A's unchanged high-water: adopted.
    {
        int total = 0;
        auto slice = h.cv.groupSlice(kView, "A", 0, -1, total);
        std::vector<CoinRecord> rows;
        for (auto idx : slice) rows.push_back(h.table[idx]);
        BOOST_CHECK(cacheA.fillContent(sink, 0, std::move(rows), h.cv.epoch(kView),
                                       h.cv.highWater(kView, "A")));
    }

    // Scope B's fetch now reports high-water 7, but the cache's structural
    // seqno is still the seed value: rejected until the delta is applied.
    {
        int total = 0;
        auto slice = h.cv.groupSlice(kView, "B", 0, -1, total);
        std::vector<CoinRecord> rows;
        for (auto idx : slice) rows.push_back(h.table[idx]);
        BOOST_CHECK(!cacheB.fillContent(sink, 0, rows, h.cv.epoch(kView),
                                        h.cv.highWater(kView, "B")));

        // Apply the structural insert (the drained event), then the refetch
        // is coherent and adopted.
        int mpos = -1;
        for (const auto& d : deltas) {
            if (d.type == CoinViewDelta::Insert && d.scope == "B") mpos = d.first;
        }
        BOOST_REQUIRE(mpos >= 0);
        BOOST_CHECK(cacheB.applyInsert(sink, 7, mpos,
                                       {h.table[h.table.size() - 1]})
                    == ApplyResult::Applied);
        BOOST_CHECK(cacheB.fillContent(sink, 0, std::move(rows), h.cv.epoch(kView),
                                       h.cv.highWater(kView, "B")));
    }
}

BOOST_AUTO_TEST_CASE(pathologicalSingleGroupWindowedReads)
{
    // The #3183 pathology: one address with a very large child set. Bulk-load
    // the table, register (one O(n log n) build), then windowed reads are
    // slice copies.
    constexpr int kCoins = 100000;

    Harness h;
    h.table.reserve(kCoins);
    for (int i = 0; i < kCoins; ++i) {
        h.table.push_back(makeCoin(i, (i * 7919) % 1000000 + 1, "whale"));
    }
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    auto info = h.cv.groupInfo("whale");
    BOOST_CHECK_EQUAL(info.output_count, kCoins);

    int total = 0;
    auto window = h.cv.groupSlice(kView, "whale", 50000, 200, total);
    BOOST_CHECK_EQUAL(total, kCoins);
    BOOST_REQUIRE_EQUAL(window.size(), 200u);
    // Amount-desc order holds across the window boundary.
    for (std::size_t i = 1; i < window.size(); ++i) {
        BOOST_CHECK(h.table[window[i - 1]].amount >= h.table[window[i]].amount);
    }

    // Tail clamp.
    auto tail = h.cv.groupSlice(kView, "whale", kCoins - 50, 200, total);
    BOOST_CHECK_EQUAL(tail.size(), 50u);
}

BOOST_AUTO_TEST_CASE(rebuildResetsEpochAndSelection)
{
    Harness h;
    h.table.push_back(makeCoin(1, 100, "A"));
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());
    h.cv.applySelection(0, true);
    BOOST_CHECK_EQUAL(h.cv.groupInfo("A").selected_count, 1);

    const uint64_t epoch_before = h.cv.epoch(kView);
    auto deltas = h.cv.rebuild(h.table.size());
    BOOST_REQUIRE_EQUAL(deltas.size(), 1u);
    BOOST_CHECK(deltas[0].type == CoinViewDelta::Reset);
    BOOST_CHECK_EQUAL(h.cv.epoch(kView), epoch_before + 1);

    // Selection aggregates are cleared on rebuild; the store re-applies its
    // mirror afterwards (the reconcile contract).
    BOOST_CHECK_EQUAL(h.cv.groupInfo("A").selected_count, 0);
}

BOOST_AUTO_TEST_CASE(viewLifecycleAndModeSwitch)
{
    Harness h;
    h.table.push_back(makeCoin(1, 100, "A"));
    h.table.push_back(makeCoin(2, 200, "B"));

    BOOST_CHECK(!h.cv.hasViews());
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());
    BOOST_CHECK(h.cv.hasView(kView));

    // Mode switch: epoch bump + Reset; flat slice becomes available.
    const uint64_t epoch_before = h.cv.epoch(kView);
    auto deltas = h.cv.setViewMode(kView, CoinViewMode::Flat, h.table.size());
    BOOST_REQUIRE_EQUAL(deltas.size(), 1u);
    BOOST_CHECK(deltas[0].type == CoinViewDelta::Reset);
    BOOST_CHECK_EQUAL(h.cv.epoch(kView), epoch_before + 1);

    int total = 0;
    auto slice = h.cv.flatSlice(kView, 0, -1, total);
    BOOST_CHECK_EQUAL(total, 2);

    // Two views with different sorts coexist (the wizard flow).
    h.cv.registerView(GRC::VIEW_COIN_WIZARD, CoinViewMode::Tree,
                      GRC::COINCOL_ADDRESS, 0, h.table.size());
    auto dir = h.cv.directorySlice(GRC::VIEW_COIN_WIZARD, 0, -1, total);
    BOOST_REQUIRE_EQUAL(dir.size(), 2u);
    BOOST_CHECK_EQUAL(dir[0], "A");

    h.cv.unregisterView(kView);
    BOOST_CHECK(!h.cv.hasView(kView));
    BOOST_CHECK(h.cv.hasViews());
    h.cv.unregisterView(GRC::VIEW_COIN_WIZARD);
    BOOST_CHECK(!h.cv.hasViews());
    h.cv.unregisterView(GRC::VIEW_COIN_WIZARD); // idempotent
}

BOOST_AUTO_TEST_CASE(regroupKillsTheEmptiedOldGroup)
{
    // The regroup case updateReslotsAndRegroups does not reach: the record
    // leaving is its old group's LAST member, so the old group must die in
    // the same call that inserts into the new one. This is the only path to
    // the deferred m_groups erase-by-key in applyUpdate, and it combines
    // three teardowns (directory row, per-view member index, shared
    // aggregates) with the new group's directory reslot still to come.
    Harness h;
    h.table.push_back(makeCoin(1, 300, "A"));
    h.table.push_back(makeCoin(2, 200, "B")); // sole member of B
    h.cv.registerView(kView, CoinViewMode::Tree, GRC::COINCOL_AMOUNT, 1, h.table.size());

    int total = 0;
    BOOST_REQUIRE_EQUAL(h.cv.directorySlice(kView, 0, -1, total).size(), 2u);

    h.table[1].group_address = "A";
    auto deltas = h.cv.applyUpdate(1, "B", false, /*was_selected*/ false);

    BOOST_CHECK_EQUAL(countType(deltas, CoinViewDelta::GroupRemove), 1);

    // The emptied group is gone from the directory, the shared aggregates
    // and the view's member index alike.
    auto dir = h.cv.directorySlice(kView, 0, -1, total);
    BOOST_REQUIRE_EQUAL(dir.size(), 1u);
    BOOST_CHECK_EQUAL(dir[0], "A");
    BOOST_CHECK_EQUAL(total, 1);
    BOOST_CHECK_EQUAL(h.cv.groupInfo("B").address, "");
    BOOST_CHECK_EQUAL(h.cv.groupSlice(kView, "B", 0, -1, total).size(), 0u);

    // ...and the survivor absorbed the member and its amount.
    BOOST_CHECK_EQUAL(h.cv.groupInfo("A").output_count, 2);
    BOOST_CHECK_EQUAL(h.cv.groupInfo("A").total_amount, 500);
    BOOST_CHECK_EQUAL(h.cv.groupSlice(kView, "A", 0, -1, total).size(), 2u);
    BOOST_CHECK_EQUAL(total, 2);
}

BOOST_AUTO_TEST_SUITE_END()
