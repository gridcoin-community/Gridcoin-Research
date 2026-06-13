// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free consumer-side reconciliation core
// (src/qt/windowcache.h), the windowed-model PR5 detailed-table window. Exercises
// the two-channel split that is the class's whole reason for existing:
//
//   - STRUCTURAL channel (Reset / Insert / Remove / Change): the sole owner of the
//     virtual row count and the begin/end{Insert,Remove} brackets. Each delta is
//     seqno-gated (the PR4-fix B skip) and advances the structural high-water.
//   - CONTENT channel (scroll fetch): epoch + high-water gated, NEVER advances the
//     structural seqno and NEVER changes the row count.
//
// The cache-base arithmetic on an insert/remove that lands before / inside / after
// / straddling the cached slice is the corruption hazard (PR5 must-fix #5); the
// seqno/epoch gates are must-fix #2. WindowCache is a template so it compiles into
// test_gridcoin with ENABLE_GUI=OFF (TransactionRecord pulls in Qt and cannot) —
// the same risk-control discipline as the Qt-free Cursor suite.

#include <qt/windowcache.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace GRC;

namespace {

//! A synthetic record carrying just an identity, so a splice/shift bug shows up
//! as a wrong id at a given row. WindowCache never inspects the contents.
struct Rec {
    int id = 0;
};

//! ids [start, start+n) as records.
std::vector<Rec> seq(int start, int n)
{
    std::vector<Rec> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) v.push_back(Rec{start + i});
    return v;
}

//! explicit id list as records.
std::vector<Rec> recs(std::initializer_list<int> ids)
{
    std::vector<Rec> v;
    v.reserve(ids.size());
    for (int id : ids) v.push_back(Rec{id});
    return v;
}

//! Recording sink: logs every call and, on each begin, probes the cache's row
//! count so a test can assert the mutation happens INSIDE the begin/end bracket
//! (the count seen at begin is the pre-mutation value).
struct RecSink : public WindowCacheSink {
    const WindowCache<Rec>* cache = nullptr;
    struct Op {
        std::string kind;
        int first = 0;
        int count = 0;
        int total_at = -1;   // cache->total() at the moment of the call
    };
    std::vector<Op> ops;

    int probe() const { return cache ? cache->total() : -999; }
    void beginReset() override { ops.push_back({"beginReset", 0, 0, probe()}); }
    void endReset() override { ops.push_back({"endReset", 0, 0, probe()}); }
    void beginInsert(int f, int c) override { ops.push_back({"beginInsert", f, c, probe()}); }
    void endInsert() override { ops.push_back({"endInsert", 0, 0, probe()}); }
    void beginRemove(int f, int c) override { ops.push_back({"beginRemove", f, c, probe()}); }
    void endRemove() override { ops.push_back({"endRemove", 0, 0, probe()}); }
    void dataChanged(int f, int c) override { ops.push_back({"dataChanged", f, c, probe()}); }
    void clear() { ops.clear(); }
};

//! Assert the cached slice equals exactly the ids [first_id, first_id+n) at
//! absolute rows [cache_first, cache_first+n), with nothing cached just outside.
void check_slice(const WindowCache<Rec>& c, int cache_first, std::initializer_list<int> ids)
{
    BOOST_CHECK_EQUAL(c.cacheFirst(), cache_first);
    BOOST_CHECK_EQUAL(c.cacheSize(), static_cast<int>(ids.size()));
    int row = cache_first;
    for (int id : ids) {
        const Rec* r = c.at(row);
        BOOST_REQUIRE_MESSAGE(r != nullptr, "expected a cached row at " << row);
        BOOST_CHECK_EQUAL(r->id, id);
        ++row;
    }
    // Just outside the slice on both sides must be a placeholder.
    BOOST_CHECK(c.at(cache_first - 1) == nullptr);
    BOOST_CHECK(c.at(cache_first + static_cast<int>(ids.size())) == nullptr);
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_windowcache_tests)

// ---- seed / query -----------------------------------------------------------

BOOST_AUTO_TEST_CASE(seed_sets_state_without_signals)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), /*cache_first*/10, /*total*/30, /*epoch*/4, /*high_water*/7);

    BOOST_CHECK_EQUAL(c.total(), 30);
    BOOST_CHECK_EQUAL(c.epoch(), 4u);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 7u);
    BOOST_CHECK(c.has(10));
    BOOST_CHECK(c.has(19));
    BOOST_CHECK(!c.has(9));
    BOOST_CHECK(!c.has(20));
    check_slice(c, 10, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19});
    BOOST_CHECK(s.ops.empty());   // seed issues no sink signals
}

// ---- structural Insert: the four position regimes ---------------------------

BOOST_AUTO_TEST_CASE(insert_before_window_shifts_base_only)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(c.applyInsert(s, /*seqno*/6, /*pos*/3, recs({100, 101})));
    BOOST_CHECK_EQUAL(c.total(), 32);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 6u);
    // slice content unchanged, base shifted +2.
    check_slice(c, 12, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19});
    // bracket: count seen at begin is the pre-mutation total.
    BOOST_REQUIRE_EQUAL(s.ops.size(), 2u);
    BOOST_CHECK_EQUAL(s.ops[0].kind, "beginInsert");
    BOOST_CHECK_EQUAL(s.ops[0].first, 3);
    BOOST_CHECK_EQUAL(s.ops[0].count, 2);
    BOOST_CHECK_EQUAL(s.ops[0].total_at, 30);
    BOOST_CHECK_EQUAL(s.ops[1].kind, "endInsert");
    BOOST_CHECK_EQUAL(s.ops[1].total_at, 32);
}

BOOST_AUTO_TEST_CASE(insert_inside_window_splices)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/13, recs({100, 101})));
    BOOST_CHECK_EQUAL(c.total(), 32);
    check_slice(c, 10, {10, 11, 12, 100, 101, 13, 14, 15, 16, 17, 18, 19});
}

BOOST_AUTO_TEST_CASE(insert_at_window_top_splices_no_flash)
{
    // pos == cacheFirst: the new rows must appear at the top of the window, NOT
    // push the window down (that would flash placeholders for a fresh tx).
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/10, recs({100})));
    check_slice(c, 10, {100, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19});
    BOOST_CHECK_EQUAL(c.total(), 31);
}

BOOST_AUTO_TEST_CASE(insert_at_window_end_appends)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);   // slice covers abs [10,20)

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/20, recs({200})));   // pos == base+len
    check_slice(c, 10, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 200});
    BOOST_CHECK_EQUAL(c.total(), 31);
}

BOOST_AUTO_TEST_CASE(insert_after_window_changes_total_only)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/25, recs({300})));   // pos > base+len
    check_slice(c, 10, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19});
    BOOST_CHECK_EQUAL(c.total(), 31);
    BOOST_CHECK_EQUAL(s.ops[0].kind, "beginInsert");
    BOOST_CHECK_EQUAL(s.ops[0].first, 25);
}

BOOST_AUTO_TEST_CASE(insert_empty_or_out_of_range_is_noop)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(!c.applyInsert(s, 6, 5, recs({})));     // empty
    BOOST_CHECK(!c.applyInsert(s, 6, -1, recs({1})));   // pos < 0
    BOOST_CHECK(!c.applyInsert(s, 6, 31, recs({1})));   // pos > total
    BOOST_CHECK_EQUAL(c.total(), 30);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 5u);
    BOOST_CHECK(s.ops.empty());
}

// ---- structural Remove: every overlap regime (cache-base hazard) ------------

BOOST_AUTO_TEST_CASE(remove_before_window)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/3, /*count*/2));   // [3,5) entirely before
    BOOST_CHECK_EQUAL(c.total(), 28);
    check_slice(c, 8, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19});
    BOOST_CHECK_EQUAL(s.ops[0].kind, "beginRemove");
    BOOST_CHECK_EQUAL(s.ops[0].total_at, 30);
}

BOOST_AUTO_TEST_CASE(remove_front_overlap)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);   // abs [5,15) ids 5..14

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/3, /*count*/4));   // [3,7) -> drops ids 5,6
    BOOST_CHECK_EQUAL(c.total(), 26);
    check_slice(c, 3, {7, 8, 9, 10, 11, 12, 13, 14});
}

BOOST_AUTO_TEST_CASE(remove_strictly_inside)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/8, /*count*/3));   // [8,11) -> drops ids 8,9,10
    BOOST_CHECK_EQUAL(c.total(), 27);
    check_slice(c, 5, {5, 6, 7, 11, 12, 13, 14});
}

BOOST_AUTO_TEST_CASE(remove_back_overlap)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/12, /*count*/5));   // [12,17) -> drops ids 12,13,14
    BOOST_CHECK_EQUAL(c.total(), 25);
    check_slice(c, 5, {5, 6, 7, 8, 9, 10, 11});
}

BOOST_AUTO_TEST_CASE(remove_spans_whole_cache)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/2, /*count*/20));   // [2,22) -> drops all cached
    BOOST_CHECK_EQUAL(c.total(), 10);
    BOOST_CHECK_EQUAL(c.cacheFirst(), 2);
    BOOST_CHECK_EQUAL(c.cacheSize(), 0);
    BOOST_CHECK(c.at(2) == nullptr);
}

BOOST_AUTO_TEST_CASE(remove_after_window)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/20, /*count*/3));   // entirely after
    BOOST_CHECK_EQUAL(c.total(), 27);
    check_slice(c, 5, {5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
}

BOOST_AUTO_TEST_CASE(remove_out_of_range_is_noop)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(!c.applyRemove(s, 6, 0, 0));     // count 0
    BOOST_CHECK(!c.applyRemove(s, 6, -1, 2));    // pos < 0
    BOOST_CHECK(!c.applyRemove(s, 6, 29, 5));    // pos+count > total
    BOOST_CHECK_EQUAL(c.total(), 30);
    BOOST_CHECK(s.ops.empty());
}

// ---- structural Change ------------------------------------------------------

BOOST_AUTO_TEST_CASE(change_refreshes_in_cache_overlap)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyChange(s, 6, /*pos*/7, /*count*/2, recs({77, 88})));
    check_slice(c, 5, {5, 6, 77, 88, 9, 10, 11, 12, 13, 14});
    BOOST_CHECK_EQUAL(c.total(), 30);   // no size change
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 6u);
    // exactly one dataChanged over the refreshed rows.
    BOOST_REQUIRE_EQUAL(s.ops.size(), 1u);
    BOOST_CHECK_EQUAL(s.ops[0].kind, "dataChanged");
    BOOST_CHECK_EQUAL(s.ops[0].first, 7);
    BOOST_CHECK_EQUAL(s.ops[0].count, 2);
}

BOOST_AUTO_TEST_CASE(change_partial_overlap_clips_to_cache)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    // Change abs [3,8): only [5,8) is in cache -> rows 5,6,7 get fresh[2..4].
    BOOST_CHECK(c.applyChange(s, 6, /*pos*/3, /*count*/5, recs({30, 40, 50, 60, 70})));
    check_slice(c, 5, {50, 60, 70, 8, 9, 10, 11, 12, 13, 14});
    BOOST_REQUIRE_EQUAL(s.ops.size(), 1u);
    BOOST_CHECK_EQUAL(s.ops[0].first, 5);
    BOOST_CHECK_EQUAL(s.ops[0].count, 3);
}

BOOST_AUTO_TEST_CASE(change_fully_off_window_no_signal)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyChange(s, 6, /*pos*/20, /*count*/2, recs({1, 2})));
    check_slice(c, 5, {5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 6u);   // seqno still advances
    BOOST_CHECK(s.ops.empty());                   // but no dataChanged (nothing visible)
}

// ---- the seqno gate (PR4-fix B, must-fix #2) --------------------------------

BOOST_AUTO_TEST_CASE(structural_seqno_gate_skips_already_reflected)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 5, 1, /*high_water*/10);

    BOOST_CHECK(!c.applyInsert(s, /*seqno*/10, 0, recs({99})));   // == high-water: skip
    BOOST_CHECK(!c.applyInsert(s, /*seqno*/9, 0, recs({99})));    // <  high-water: skip
    BOOST_CHECK(!c.applyRemove(s, 10, 0, 1));
    BOOST_CHECK(!c.applyChange(s, 10, 0, 1, recs({1})));
    BOOST_CHECK_EQUAL(c.total(), 5);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 10u);
    BOOST_CHECK(s.ops.empty());

    BOOST_CHECK(c.applyInsert(s, /*seqno*/11, 0, recs({99})));    // > high-water: apply
    BOOST_CHECK_EQUAL(c.total(), 6);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 11u);
}

BOOST_AUTO_TEST_CASE(reset_skips_stale_and_takes_max_baseline)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 5, 1, /*high_water*/10);

    // A Reset whose seqno is already reflected is skipped.
    BOOST_CHECK(!c.applyReset(s, /*seqno*/10, seq(0, 3), 0, 3, /*epoch*/2, /*hw*/10));
    BOOST_CHECK_EQUAL(c.total(), 5);

    // A live Reset: new geometry; baseline = max(high_water, seqno).
    BOOST_CHECK(c.applyReset(s, /*seqno*/12, seq(100, 4), 0, 20, /*epoch*/2, /*hw*/15));
    BOOST_CHECK_EQUAL(c.total(), 20);
    BOOST_CHECK_EQUAL(c.epoch(), 2u);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 15u);   // hw(15) > seqno(12)
    check_slice(c, 0, {100, 101, 102, 103});
    BOOST_CHECK_EQUAL(s.ops.size(), 2u);
    BOOST_CHECK_EQUAL(s.ops[0].kind, "beginReset");
    BOOST_CHECK_EQUAL(s.ops[1].kind, "endReset");
}

// ---- the content channel (must-fix #2: never advances structural state) -----

BOOST_AUTO_TEST_CASE(content_fill_adopts_on_exact_match)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 100, /*epoch*/3, /*high_water*/10);

    // user scrolled to abs 50: fetch matches epoch AND structural seqno -> adopt.
    BOOST_CHECK(c.fillContent(s, /*first*/50, seq(50, 5), /*epoch*/3, /*high_water*/10));
    check_slice(c, 50, {50, 51, 52, 53, 54});
    BOOST_CHECK_EQUAL(c.total(), 100);             // content NEVER changes the count
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 10u);   // content NEVER advances the seqno
    BOOST_REQUIRE_EQUAL(s.ops.size(), 1u);
    BOOST_CHECK_EQUAL(s.ops[0].kind, "dataChanged");
    BOOST_CHECK_EQUAL(s.ops[0].first, 50);
    BOOST_CHECK_EQUAL(s.ops[0].count, 5);
}

BOOST_AUTO_TEST_CASE(content_fill_discards_on_epoch_mismatch)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 100, /*epoch*/3, /*high_water*/10);

    // a resort happened since the fetch was requested (epoch bumped) -> discard.
    BOOST_CHECK(!c.fillContent(s, 50, seq(50, 5), /*epoch*/2, /*high_water*/10));
    check_slice(c, 0, {0, 1, 2, 3, 4});   // unchanged
    BOOST_CHECK(s.ops.empty());
}

BOOST_AUTO_TEST_CASE(content_fill_discards_on_seqno_mismatch_either_way)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 100, /*epoch*/3, /*high_water*/10);

    BOOST_CHECK(!c.fillContent(s, 50, seq(50, 5), 3, /*high_water*/9));    // fetch staler
    BOOST_CHECK(!c.fillContent(s, 50, seq(50, 5), 3, /*high_water*/11));   // fetch ahead
    check_slice(c, 0, {0, 1, 2, 3, 4});
    BOOST_CHECK(s.ops.empty());
}

BOOST_AUTO_TEST_CASE(structural_delta_applies_after_a_content_fill)
{
    // A content fill must not poison the structural coordinate system: a later
    // insert still shifts the content-filled slice exactly as the structural
    // channel dictates.
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 100, 3, 10);

    BOOST_CHECK(c.fillContent(s, 50, seq(50, 5), 3, 10));   // cache now abs [50,55)
    s.clear();

    // insert before the window: base shifts, total grows, seqno advances.
    BOOST_CHECK(c.applyInsert(s, /*seqno*/11, /*pos*/40, recs({900, 901})));
    BOOST_CHECK_EQUAL(c.total(), 102);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 11u);
    check_slice(c, 52, {50, 51, 52, 53, 54});
    BOOST_CHECK_EQUAL(s.ops[0].kind, "beginInsert");
    BOOST_CHECK_EQUAL(s.ops[0].total_at, 100);   // pre-mutation count inside the bracket
}

BOOST_AUTO_TEST_CASE(content_fill_rejects_slice_past_table_end)
{
    // Even with matching epoch + seqno, a slice that would fall outside [0,total)
    // is rejected so it cannot corrupt the [cacheFirst, cacheFirst+cacheSize)
    // invariant (has()/at() reporting rows >= total as present). PR5-A review.
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, /*total*/100, /*epoch*/3, /*high_water*/10);

    BOOST_CHECK(!c.fillContent(s, /*first*/96, seq(96, 10), 3, 10));  // [96,106) past 100
    BOOST_CHECK(!c.fillContent(s, /*first*/100, seq(100, 1), 3, 10)); // first == total
    check_slice(c, 0, {0, 1, 2, 3, 4});   // unchanged
    BOOST_CHECK(s.ops.empty());

    // a slice ending exactly at the table end is accepted.
    BOOST_CHECK(c.fillContent(s, /*first*/95, seq(95, 5), 3, 10));    // [95,100)
    check_slice(c, 95, {95, 96, 97, 98, 99});
}

BOOST_AUTO_TEST_CASE(change_short_fresh_refreshes_what_it_has_and_advances_seqno)
{
    // Contract violation (fresh shorter than count): the covered rows refresh, the
    // rest keep their cached values (no out-of-bounds read), and the seqno still
    // advances so the Change event is consumed exactly once. PR5-A review.
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyChange(s, 6, /*pos*/7, /*count*/3, recs({77})));   // only 1 of 3
    check_slice(c, 5, {5, 6, 77, 8, 9, 10, 11, 12, 13, 14});             // 8,9 unchanged
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 6u);                          // advanced regardless
    BOOST_REQUIRE_EQUAL(s.ops.size(), 1u);
    BOOST_CHECK_EQUAL(s.ops[0].kind, "dataChanged");
    BOOST_CHECK_EQUAL(s.ops[0].first, 7);
    BOOST_CHECK_EQUAL(s.ops[0].count, 1);                               // only the refreshed row
}

BOOST_AUTO_TEST_CASE(consumer_routing_scenario)
{
    // Mirror DetailedTxModel::applyEventBatch's exact call sequence against the cache
    // + sink, pinning the windowed consumer's structural/content routing end to end
    // (the model shell itself is not GUI-OFF-testable; this is its reconcilable core).
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;

    // ctor seed: bounded window [0,5) of a 50-row table, epoch 1, high-water 10.
    c.seedInitial(seq(0, 5), 0, 50, /*epoch*/1, /*high_water*/10);
    BOOST_CHECK_EQUAL(c.total(), 50);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 10u);

    // RowsInserted at the top (a fresh tx): payload carries the record.
    BOOST_CHECK(c.applyInsert(s, 11, /*pos*/0, recs({100})));
    BOOST_CHECK_EQUAL(c.total(), 51);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 11u);
    check_slice(c, 0, {100, 0, 1, 2, 3, 4});

    // RowsRemoved off-window (below the cache): total shrinks, window unchanged.
    BOOST_CHECK(c.applyRemove(s, 12, /*pos*/40, /*count*/2));
    BOOST_CHECK_EQUAL(c.total(), 49);
    check_slice(c, 0, {100, 0, 1, 2, 3, 4});

    // RowsChanged in-window: the consumer fetched `fresh`; refresh row 2.
    BOOST_CHECK(c.applyChange(s, 13, /*pos*/2, /*count*/1, recs({222})));
    check_slice(c, 0, {100, 0, 222, 2, 3, 4});
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 13u);

    // Stale / duplicate deltas (seqno <= structural): skipped, no signals.
    s.clear();
    BOOST_CHECK(!c.applyInsert(s, 13, 0, recs({999})));
    BOOST_CHECK(!c.applyRemove(s, 5, 0, 1));
    BOOST_CHECK_EQUAL(c.total(), 49);
    BOOST_CHECK(s.ops.empty());

    // CONTENT fetch that raced an insert/remove (high-water 12 != structural 13): drop.
    BOOST_CHECK(!c.fillContent(s, /*first*/20, seq(20, 6), /*epoch*/1, /*high_water*/12));
    // CONTENT fetch that raced a resort (epoch 0 != 1): drop.
    BOOST_CHECK(!c.fillContent(s, 20, seq(20, 6), /*epoch*/0, 13));
    BOOST_CHECK_EQUAL(c.cacheFirst(), 0);   // still the original window
    BOOST_CHECK(s.ops.empty());

    // CONTENT fetch that matches (epoch 1, high-water 13): adopted; structural untouched.
    BOOST_CHECK(c.fillContent(s, /*first*/20, seq(20, 6), /*epoch*/1, /*high_water*/13));
    check_slice(c, 20, {20, 21, 22, 23, 24, 25});
    BOOST_CHECK_EQUAL(c.total(), 49);            // content never changes the count
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 13u); // content never advances the seqno

    // A structural Reset (sort/filter) with a fresh epoch: rebuild + new baseline.
    BOOST_CHECK(c.applyReset(s, /*seqno*/14, seq(0, 5), 0, /*total*/49, /*epoch*/2, /*hw*/14));
    BOOST_CHECK_EQUAL(c.epoch(), 2u);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 14u);
    check_slice(c, 0, {0, 1, 2, 3, 4});
    // A content fetch from the OLD epoch is now rejected.
    BOOST_CHECK(!c.fillContent(s, 20, seq(20, 6), /*epoch*/1, 14));
}

BOOST_AUTO_TEST_SUITE_END()
