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
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace GRC;

// Shorthands for the structural apply* outcome. Named so the negative cases state
// WHICH kind of non-application they expect: a routine seqno-gate skip, or a
// rejection meaning the replica has diverged from the producer cursor.
constexpr ApplyResult APPLIED   = ApplyResult::Applied;
constexpr ApplyResult REFLECTED = ApplyResult::AlreadyReflected;
constexpr ApplyResult REJECTED  = ApplyResult::Rejected;

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

    BOOST_CHECK(c.applyInsert(s, /*seqno*/6, /*pos*/3, recs({100, 101})) == APPLIED);
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

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/13, recs({100, 101})) == APPLIED);
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

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/10, recs({100})) == APPLIED);
    check_slice(c, 10, {100, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19});
    BOOST_CHECK_EQUAL(c.total(), 31);
}

BOOST_AUTO_TEST_CASE(insert_at_window_end_appends)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);   // slice covers abs [10,20)

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/20, recs({200})) == APPLIED);   // pos == base+len
    check_slice(c, 10, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 200});
    BOOST_CHECK_EQUAL(c.total(), 31);
}

BOOST_AUTO_TEST_CASE(insert_after_window_changes_total_only)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(10, 10), 10, 30, 1, 5);

    BOOST_CHECK(c.applyInsert(s, 6, /*pos*/25, recs({300})) == APPLIED);   // pos > base+len
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

    BOOST_CHECK(c.applyInsert(s, 6, 5, recs({})) == REJECTED);     // empty
    BOOST_CHECK(c.applyInsert(s, 6, -1, recs({1})) == REJECTED);   // pos < 0
    BOOST_CHECK(c.applyInsert(s, 6, 31, recs({1})) == REJECTED);   // pos > total
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

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/3, /*count*/2) == APPLIED);   // [3,5) entirely before
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

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/3, /*count*/4) == APPLIED);   // [3,7) -> drops ids 5,6
    BOOST_CHECK_EQUAL(c.total(), 26);
    check_slice(c, 3, {7, 8, 9, 10, 11, 12, 13, 14});
}

BOOST_AUTO_TEST_CASE(remove_strictly_inside)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/8, /*count*/3) == APPLIED);   // [8,11) -> drops ids 8,9,10
    BOOST_CHECK_EQUAL(c.total(), 27);
    check_slice(c, 5, {5, 6, 7, 11, 12, 13, 14});
}

BOOST_AUTO_TEST_CASE(remove_back_overlap)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/12, /*count*/5) == APPLIED);   // [12,17) -> drops ids 12,13,14
    BOOST_CHECK_EQUAL(c.total(), 25);
    check_slice(c, 5, {5, 6, 7, 8, 9, 10, 11});
}

BOOST_AUTO_TEST_CASE(remove_spans_whole_cache)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/2, /*count*/20) == APPLIED);   // [2,22) -> drops all cached
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

    BOOST_CHECK(c.applyRemove(s, 6, /*pos*/20, /*count*/3) == APPLIED);   // entirely after
    BOOST_CHECK_EQUAL(c.total(), 27);
    check_slice(c, 5, {5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
}

BOOST_AUTO_TEST_CASE(remove_out_of_range_is_noop)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyRemove(s, 6, 0, 0) == REJECTED);     // count 0
    BOOST_CHECK(c.applyRemove(s, 6, -1, 2) == REJECTED);    // pos < 0
    BOOST_CHECK(c.applyRemove(s, 6, 29, 5) == REJECTED);    // pos+count > total
    BOOST_CHECK_EQUAL(c.total(), 30);
    BOOST_CHECK(s.ops.empty());
}

// ---- structural Change ------------------------------------------------------

BOOST_AUTO_TEST_CASE(change_refreshes_in_cache_overlap)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(5, 10), 5, 30, 1, 5);

    BOOST_CHECK(c.applyChange(s, 6, /*pos*/7, /*count*/2, recs({77, 88})) == APPLIED);
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
    BOOST_CHECK(c.applyChange(s, 6, /*pos*/3, /*count*/5, recs({30, 40, 50, 60, 70})) == APPLIED);
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

    BOOST_CHECK(c.applyChange(s, 6, /*pos*/20, /*count*/2, recs({1, 2})) == APPLIED);
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

    BOOST_CHECK(c.applyInsert(s, /*seqno*/10, 0, recs({99})) == REFLECTED);   // == high-water: skip
    BOOST_CHECK(c.applyInsert(s, /*seqno*/9, 0, recs({99})) == REFLECTED);    // <  high-water: skip
    BOOST_CHECK(c.applyRemove(s, 10, 0, 1) == REFLECTED);
    BOOST_CHECK(c.applyChange(s, 10, 0, 1, recs({1})) == REFLECTED);
    BOOST_CHECK_EQUAL(c.total(), 5);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 10u);
    BOOST_CHECK(s.ops.empty());

    BOOST_CHECK(c.applyInsert(s, /*seqno*/11, 0, recs({99})) == APPLIED);    // > high-water: apply
    BOOST_CHECK_EQUAL(c.total(), 6);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 11u);
}

BOOST_AUTO_TEST_CASE(reset_skips_stale_and_takes_max_baseline)
{
    WindowCache<Rec> c;
    RecSink s; s.cache = &c;
    c.seedInitial(seq(0, 5), 0, 5, 1, /*high_water*/10);

    // A Reset whose seqno is already reflected is skipped.
    BOOST_CHECK(c.applyReset(s, /*seqno*/10, seq(0, 3), 0, 3, /*epoch*/2, /*hw*/10) == REFLECTED);
    BOOST_CHECK_EQUAL(c.total(), 5);

    // A live Reset: new geometry; baseline = max(high_water, seqno).
    BOOST_CHECK(c.applyReset(s, /*seqno*/12, seq(100, 4), 0, 20, /*epoch*/2, /*hw*/15) == APPLIED);
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
    BOOST_CHECK(c.applyInsert(s, /*seqno*/11, /*pos*/40, recs({900, 901})) == APPLIED);
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

    BOOST_CHECK(c.applyChange(s, 6, /*pos*/7, /*count*/3, recs({77})) == APPLIED);   // only 1 of 3
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
    BOOST_CHECK(c.applyInsert(s, 11, /*pos*/0, recs({100})) == APPLIED);
    BOOST_CHECK_EQUAL(c.total(), 51);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 11u);
    check_slice(c, 0, {100, 0, 1, 2, 3, 4});

    // RowsRemoved off-window (below the cache): total shrinks, window unchanged.
    BOOST_CHECK(c.applyRemove(s, 12, /*pos*/40, /*count*/2) == APPLIED);
    BOOST_CHECK_EQUAL(c.total(), 49);
    check_slice(c, 0, {100, 0, 1, 2, 3, 4});

    // RowsChanged in-window: the consumer fetched `fresh`; refresh row 2.
    BOOST_CHECK(c.applyChange(s, 13, /*pos*/2, /*count*/1, recs({222})) == APPLIED);
    check_slice(c, 0, {100, 0, 222, 2, 3, 4});
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 13u);

    // Stale / duplicate deltas (seqno <= structural): skipped, no signals.
    s.clear();
    BOOST_CHECK(c.applyInsert(s, 13, 0, recs({999})) == REFLECTED);
    BOOST_CHECK(c.applyRemove(s, 5, 0, 1) == REFLECTED);
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
    BOOST_CHECK(c.applyReset(s, /*seqno*/14, seq(0, 5), 0, /*total*/49, /*epoch*/2, /*hw*/14) == APPLIED);
    BOOST_CHECK_EQUAL(c.epoch(), 2u);
    BOOST_CHECK_EQUAL(c.structuralSeqno(), 14u);
    check_slice(c, 0, {0, 1, 2, 3, 4});
    // A content fetch from the OLD epoch is now rejected.
    BOOST_CHECK(!c.fillContent(s, 20, seq(20, 6), /*epoch*/1, 14));
}

// ---- CoalescingSink (#3059) ----------------------------------------------------------
//
// One dataChanged per drained batch instead of one per Change event. On macOS with an
// accessibility client each model notification rebuilds one accessibility element per
// table row, so per-event emission cost gigabytes during a catch-up.

namespace {
//! A sink whose row count the coalescer reads at endBatch.
struct CoalesceFixture {
    RecSink target;
    int rows = 100;
    CoalescingSink sink{target, [this] { return rows; }};
};

//! The only dataChanged ops, as (first, count) pairs.
std::vector<std::pair<int, int>> changes(const RecSink& s)
{
    std::vector<std::pair<int, int>> out;
    for (const auto& op : s.ops) {
        if (op.kind == "dataChanged") out.emplace_back(op.first, op.count);
    }
    return out;
}
} // namespace

BOOST_AUTO_TEST_CASE(coalescer_is_a_pass_through_outside_a_batch)
{
    CoalesceFixture f;
    f.sink.dataChanged(3, 2);
    f.sink.dataChanged(10, 1);
    f.sink.dataChanged(4, 0);    // ignored: nothing to refresh
    f.sink.dataChanged(4, -1);   // ignored
    const auto c = changes(f.target);
    BOOST_REQUIRE_EQUAL(c.size(), 2u);
    BOOST_CHECK(c[0] == std::make_pair(3, 2));
    BOOST_CHECK(c[1] == std::make_pair(10, 1));
}

BOOST_AUTO_TEST_CASE(coalescer_emits_one_union_per_batch)
{
    CoalesceFixture f;
    f.sink.beginBatch();
    f.sink.dataChanged(20, 1);
    f.sink.dataChanged(5, 2);
    f.sink.dataChanged(30, 3);
    f.sink.dataChanged(7, 0);    // a zero-length change must not create a phantom endpoint
    BOOST_CHECK(changes(f.target).empty());   // nothing until the batch closes
    BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::Emitted);
    const auto c = changes(f.target);
    BOOST_REQUIRE_EQUAL(c.size(), 1u);
    BOOST_CHECK(c[0] == std::make_pair(5, 28));   // [5, 32]
    // A batch with nothing pending emits nothing.
    f.sink.beginBatch();
    BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::None);
    BOOST_CHECK_EQUAL(changes(f.target).size(), 1u);
}

BOOST_AUTO_TEST_CASE(coalescer_translates_the_range_through_inserts)
{
    struct Case { int pos, count, lo, hi; };
    // Pending [10, 14]; each case is one insert, then the emitted range.
    for (const Case& k : {Case{0, 3, 13, 17},     // above: shifts
                          Case{10, 2, 12, 16},    // at lo: the whole range shifts
                          Case{12, 2, 10, 16},    // inside: extends over the new rows
                          Case{14, 1, 10, 15},    // at hi: extends
                          Case{15, 4, 10, 14},    // at hi + 1: below, unchanged
                          Case{40, 4, 10, 14}}) { // well below: unchanged
        CoalesceFixture f;
        f.sink.beginBatch();
        f.sink.dataChanged(10, 5);
        f.sink.beginInsert(k.pos, k.count);
        f.sink.endInsert();
        f.rows = 200;
        BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::Emitted);
        const auto c = changes(f.target);
        BOOST_REQUIRE_EQUAL(c.size(), 1u);
        BOOST_CHECK_MESSAGE(c[0] == std::make_pair(k.lo, k.hi - k.lo + 1),
                            "insert at " << k.pos << " x" << k.count << ": got [" << c[0].first
                            << ", " << (c[0].first + c[0].second - 1) << "], want [" << k.lo
                            << ", " << k.hi << "]");
    }
}

BOOST_AUTO_TEST_CASE(coalescer_translates_the_range_through_removes)
{
    struct Case { int pos, count; bool dropped; int lo, hi; };
    // Pending [10, 14]; each case is one remove, then the emitted range (or none).
    for (const Case& k : {Case{0, 3, false, 7, 11},     // entirely above: shifts up
                          Case{9, 1, false, 9, 13},     // ends at lo - 1: shifts up
                          Case{8, 4, false, 8, 10},     // straddles lo
                          Case{11, 2, false, 10, 12},   // inside
                          Case{13, 5, false, 10, 12},   // straddles hi
                          Case{15, 3, false, 10, 14},   // starts at hi + 1: unchanged
                          Case{10, 5, true, 0, 0},      // exactly the range: dropped
                          Case{5, 20, true, 0, 0}}) {   // covers more: dropped
        CoalesceFixture f;
        f.sink.beginBatch();
        f.sink.dataChanged(10, 5);
        f.sink.beginRemove(k.pos, k.count);
        f.sink.endRemove();
        const CoalescingSink::EndResult r = f.sink.endBatch();
        const auto c = changes(f.target);
        if (k.dropped) {
            BOOST_CHECK(r == CoalescingSink::EndResult::None);
            BOOST_CHECK_MESSAGE(c.empty(), "remove at " << k.pos << " x" << k.count << " should drop the range");
        } else {
            BOOST_CHECK(r == CoalescingSink::EndResult::Emitted);
            BOOST_REQUIRE_EQUAL(c.size(), 1u);
            BOOST_CHECK_MESSAGE(c[0] == std::make_pair(k.lo, k.hi - k.lo + 1),
                                "remove at " << k.pos << " x" << k.count << ": got [" << c[0].first
                                << ", " << (c[0].first + c[0].second - 1) << "], want [" << k.lo
                                << ", " << k.hi << "]");
        }
    }
}

BOOST_AUTO_TEST_CASE(coalescer_reset_drops_earlier_changes_and_later_ones_still_coalesce)
{
    CoalesceFixture f;
    f.sink.beginBatch();
    f.sink.dataChanged(10, 5);
    f.sink.beginReset();
    f.sink.endReset();
    f.sink.dataChanged(3, 1);
    f.sink.dataChanged(6, 1);
    BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::Emitted);
    const auto c = changes(f.target);
    BOOST_REQUIRE_EQUAL(c.size(), 1u);
    BOOST_CHECK(c[0] == std::make_pair(3, 4));
}

BOOST_AUTO_TEST_CASE(coalescer_forwards_every_bracket_before_the_single_change)
{
    CoalesceFixture f;
    f.sink.beginBatch();
    f.sink.dataChanged(10, 1);
    f.sink.beginInsert(0, 1);
    f.sink.endInsert();
    f.sink.dataChanged(2, 1);
    f.sink.beginRemove(50, 2);
    f.sink.endRemove();
    f.sink.endBatch();
    const std::vector<std::string> want{"beginInsert", "endInsert", "beginRemove", "endRemove", "dataChanged"};
    BOOST_REQUIRE_EQUAL(f.target.ops.size(), want.size());
    for (std::size_t i = 0; i < want.size(); ++i) BOOST_CHECK_EQUAL(f.target.ops[i].kind, want[i]);
    BOOST_CHECK(changes(f.target)[0] == std::make_pair(2, 10));   // [2, 11]: 10 shifted to 11
}

BOOST_AUTO_TEST_CASE(coalescer_nested_batches_emit_once_at_the_outermost_close)
{
    CoalesceFixture f;
    f.sink.beginBatch();
    f.sink.dataChanged(1, 1);
    f.sink.beginBatch();
    f.sink.dataChanged(8, 1);
    BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::None);
    BOOST_CHECK(changes(f.target).empty());
    BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::Emitted);
    BOOST_REQUIRE_EQUAL(changes(f.target).size(), 1u);
    BOOST_CHECK(changes(f.target)[0] == std::make_pair(1, 8));
}

BOOST_AUTO_TEST_CASE(coalescer_abandon_emits_nothing_and_leaves_the_sink_usable)
{
    CoalesceFixture f;
    f.sink.beginBatch();
    f.sink.dataChanged(4, 2);
    f.sink.abandonBatch();
    BOOST_CHECK(!f.sink.inBatch());
    BOOST_CHECK(changes(f.target).empty());
    f.sink.dataChanged(9, 1);   // pass-through again
    BOOST_REQUIRE_EQUAL(changes(f.target).size(), 1u);
    BOOST_CHECK(changes(f.target)[0] == std::make_pair(9, 1));
}

BOOST_AUTO_TEST_CASE(coalescer_falls_back_to_the_whole_table_on_an_out_of_range_result)
{
    // An out-of-range result can only come from a translation bug; clamping would keep the
    // range valid but could miss the changed rows, so the whole table is refreshed instead.
    CoalesceFixture f;
    f.rows = 12;
    f.sink.beginBatch();
    f.sink.dataChanged(10, 5);   // [10, 14] but the table now has 12 rows
    BOOST_CHECK(f.sink.endBatch() == CoalescingSink::EndResult::Fallback);
    const auto c = changes(f.target);
    BOOST_REQUIRE_EQUAL(c.size(), 1u);
    BOOST_CHECK(c[0] == std::make_pair(0, 12));
    // An empty table: nothing to refresh, still reported.
    CoalesceFixture g;
    g.rows = 0;
    g.sink.beginBatch();
    g.sink.dataChanged(0, 1);
    BOOST_CHECK(g.sink.endBatch() == CoalescingSink::EndResult::Fallback);
    BOOST_CHECK(changes(g.target).empty());
}

namespace {
//! What a view would SHOW per row: re-queried when Qt would re-query it (rows inserted,
//! a reset, dataChanged); otherwise the row keeps what it last showed, shifting with
//! inserts and removes. -1 is a placeholder (an uncached row).
struct ViewMirror : public WindowCacheSink {
    const WindowCache<Rec>* cache = nullptr;
    std::vector<int> shown;
    int insert_first = 0, insert_count = 0;
    int emissions = 0;

    int truth(int row) const { const Rec* r = cache->at(row); return r ? r->id : -1; }
    void requery(int first, int count)
    {
        for (int i = first; i < first + count && i < static_cast<int>(shown.size()); ++i) {
            if (i >= 0) shown[static_cast<std::size_t>(i)] = truth(i);
        }
    }
    void beginReset() override {}
    void endReset() override { shown.assign(static_cast<std::size_t>(cache->total()), -1); requery(0, cache->total()); }
    void beginInsert(int f, int c) override
    {
        shown.insert(shown.begin() + f, static_cast<std::size_t>(c), -2);
        insert_first = f;
        insert_count = c;
    }
    void endInsert() override { requery(insert_first, insert_count); }
    void beginRemove(int f, int c) override { shown.erase(shown.begin() + f, shown.begin() + f + c); }
    void endRemove() override {}
    void dataChanged(int f, int c) override { ++emissions; requery(f, c); }
    bool staleAt(int i) const { return shown[static_cast<std::size_t>(i)] != truth(i); }
};

struct World {
    WindowCache<Rec> cache;
    ViewMirror view;
    std::unique_ptr<CoalescingSink> coalescer;
    WindowCacheSink& sink() { return coalescer ? static_cast<WindowCacheSink&>(*coalescer) : view; }
};
} // namespace

//! The main safeguard against an in-range translation bug no endBatch check can see:
//! random batches of insert / remove / change / reset / fill drive the REAL WindowCache in
//! two identical worlds, one notifying directly and one through the coalescer. After every
//! batch every row stale in the coalesced view must also be stale in the direct one (the two
//! caches are identical, so rows correspond), the coalescer must have emitted at most once,
//! and it must never have needed the full-table fallback. Compared row by row against the
//! direct path, not an absolute oracle: a fill that moves the cached slice leaves the rows it
//! evicted showing stale content on BOTH paths today. A count comparison would let a missed
//! row hide behind a row the union happened to cover.
BOOST_AUTO_TEST_CASE(coalescer_is_never_worse_than_direct_notification)
{
    std::mt19937 rng(0x3059);
    auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    int next_id = 1;
    auto fresh = [&](int n) { std::vector<Rec> v; for (int i = 0; i < n; ++i) v.push_back(Rec{next_id++}); return v; };

    int batches = 0, fallbacks = 0, multi = 0, worse = 0;
    for (int run = 0; run < 400; ++run) {
        World direct, coal;
        coal.coalescer = std::make_unique<CoalescingSink>(coal.view, [&coal] { return coal.cache.total(); });
        const int total0 = pick(0, 25);
        const int first0 = total0 ? pick(0, total0 - 1) : 0;
        const int size0 = total0 ? pick(0, std::min(12, total0 - first0)) : 0;
        const std::vector<Rec> rows0 = fresh(size0);
        for (World* w : {&direct, &coal}) {
            w->cache.seedInitial(rows0, first0, total0, /*epoch=*/1, /*high_water=*/0);
            w->view.cache = &w->cache;
            w->view.endReset();
        }
        uint64_t seqno = 0, epoch = 1;
        for (int b = 0; b < 25; ++b) {
            coal.coalescer->beginBatch();
            const int before = coal.view.emissions;
            const int nops = pick(1, 8);
            for (int o = 0; o < nops; ++o) {
                const int total = direct.cache.total();
                const int kind = pick(0, 99);
                if (kind < 25) {                                   // insert
                    const int pos = pick(0, total), n = pick(1, 3);
                    const std::vector<Rec> r = fresh(n);
                    ++seqno;
                    for (World* w : {&direct, &coal}) w->cache.applyInsert(w->sink(), seqno, pos, r);
                } else if (kind < 45 && total > 0) {               // remove
                    const int pos = pick(0, total - 1), n = pick(1, std::min(3, total - pos));
                    ++seqno;
                    for (World* w : {&direct, &coal}) w->cache.applyRemove(w->sink(), seqno, pos, n);
                } else if (kind < 85 && total > 0) {               // change
                    const int pos = pick(0, total - 1), n = pick(1, std::min(4, total - pos));
                    const std::vector<Rec> r = fresh(n);
                    ++seqno;
                    for (World* w : {&direct, &coal}) w->cache.applyChange(w->sink(), seqno, pos, n, r);
                } else if (kind < 90) {                            // reset (a resort / refilter)
                    const int nt = pick(0, 25);
                    const int cf = nt ? pick(0, nt - 1) : 0;
                    const std::vector<Rec> r = fresh(nt ? pick(0, std::min(12, nt - cf)) : 0);
                    ++seqno;
                    ++epoch;
                    for (World* w : {&direct, &coal}) w->cache.applyReset(w->sink(), seqno, r, cf, nt, epoch, seqno);
                } else if (total > 0) {                            // a content fill inside the batch
                    const int first = pick(0, total - 1);
                    const std::vector<Rec> r = fresh(pick(0, std::min(12, total - first)));
                    for (World* w : {&direct, &coal}) {
                        w->cache.fillContent(w->sink(), first, r, epoch, w->cache.structuralSeqno());
                    }
                }
            }
            const CoalescingSink::EndResult end = coal.coalescer->endBatch();
            ++batches;
            fallbacks += end == CoalescingSink::EndResult::Fallback;
            multi += (coal.view.emissions - before) > 1;
            BOOST_REQUIRE_EQUAL(coal.view.shown.size(), static_cast<std::size_t>(coal.cache.total()));
            BOOST_REQUIRE_EQUAL(direct.view.shown.size(), static_cast<std::size_t>(direct.cache.total()));
            for (int i = 0; i < coal.cache.total(); ++i) {
                if (coal.view.staleAt(i) && !direct.view.staleAt(i)) {
                    ++worse;
                    BOOST_ERROR("run " << run << " batch " << b << ": row " << i
                                << " is stale in the coalesced view but current in the direct one");
                    break;
                }
            }
        }
    }
    BOOST_CHECK_EQUAL(fallbacks, 0);
    BOOST_CHECK_EQUAL(multi, 0);
    BOOST_CHECK_EQUAL(worse, 0);
    BOOST_TEST_MESSAGE("coalescer differential: " << batches << " batches");
}

//! GRC::CloseBatch: what DetailedTxModel::applyEventBatch calls to close a drained batch. A batch
//! that carried a tip also refreshes the cached slice -- progress derived at render time
//! changed for every cached row -- in FINAL coordinates, unioned into the one emission.
BOOST_AUTO_TEST_CASE(close_batch_refreshes_the_slice_once_when_the_batch_carried_a_tip)
{
    auto setup = [](WindowCache<Rec>& c, RecSink& t) {
        c.seedInitial(seq(105, 10), /*cache_first=*/5, /*total=*/30, /*epoch=*/1, /*high_water=*/0);
        t.cache = &c;
    };

    // A change at row 7, then an insert above the slice: the slice moves to [6, 15] and the
    // change to row 8. With a tip, one dataChanged spans [6, 15].
    {
        WindowCache<Rec> c;
        RecSink t;
        setup(c, t);
        CoalescingSink sink(t, [&c] { return c.total(); });
        sink.beginBatch();
        BOOST_CHECK(c.applyChange(sink, 1, 7, 1, recs({900})) == APPLIED);
        BOOST_CHECK(c.applyInsert(sink, 2, 0, recs({901})) == APPLIED);
        BOOST_CHECK(CloseBatch(sink, c, /*saw_tip=*/true) == CoalescingSink::EndResult::Emitted);
        const auto ch = changes(t);
        BOOST_REQUIRE_EQUAL(ch.size(), 1u);
        BOOST_CHECK(ch[0] == std::make_pair(6, 10));
        BOOST_CHECK_EQUAL(t.ops.back().kind, "dataChanged");   // after every bracket
    }
    // The same batch without a tip: only the changed row, shifted.
    {
        WindowCache<Rec> c;
        RecSink t;
        setup(c, t);
        CoalescingSink sink(t, [&c] { return c.total(); });
        sink.beginBatch();
        c.applyChange(sink, 1, 7, 1, recs({900}));
        c.applyInsert(sink, 2, 0, recs({901}));
        BOOST_CHECK(CloseBatch(sink, c, /*saw_tip=*/false) == CoalescingSink::EndResult::Emitted);
        const auto ch = changes(t);
        BOOST_REQUIRE_EQUAL(ch.size(), 1u);
        BOOST_CHECK(ch[0] == std::make_pair(8, 1));
    }
    // A tip after a reset in the same batch refreshes the REBUILT slice.
    {
        WindowCache<Rec> c;
        RecSink t;
        setup(c, t);
        CoalescingSink sink(t, [&c] { return c.total(); });
        sink.beginBatch();
        c.applyChange(sink, 1, 7, 1, recs({900}));
        BOOST_CHECK(c.applyReset(sink, 2, seq(500, 4), /*cache_first=*/2, /*total=*/12, /*epoch=*/2, 2) == APPLIED);
        BOOST_CHECK(CloseBatch(sink, c, /*saw_tip=*/true) == CoalescingSink::EndResult::Emitted);
        const auto ch = changes(t);
        BOOST_REQUIRE_EQUAL(ch.size(), 1u);
        BOOST_CHECK(ch[0] == std::make_pair(2, 4));
    }
    // A tip with nothing cached and nothing changed emits nothing; so does a quiet batch.
    {
        WindowCache<Rec> c;
        RecSink t;
        c.seedInitial({}, 0, /*total=*/30, 1, 0);
        t.cache = &c;
        CoalescingSink sink(t, [&c] { return c.total(); });
        sink.beginBatch();
        BOOST_CHECK(CloseBatch(sink, c, /*saw_tip=*/true) == CoalescingSink::EndResult::None);
        sink.beginBatch();
        BOOST_CHECK(CloseBatch(sink, c, /*saw_tip=*/false) == CoalescingSink::EndResult::None);
        BOOST_CHECK(changes(t).empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
