// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free per-view cursor (src/qt/cursor.{h,cpp}),
// the windowed-model PR3 core. Exercises the view_index-maintenance algorithm
// (doc/transaction_table_windowed_model.md addendum), in particular the four
// arithmetic hazards: identity-locate (not sort-key), erase-then-lower_bound
// reposition, off-window maintenance with gated emission, and the store
// reindex/splice — plus the served-window eviction/promotion boundary. The
// cursor itself is Qt-free, so it compiles into test_gridcoin with ENABLE_GUI=OFF
// where CI actually runs (the blind spot that hid #2944).

#include <qt/cursor.h>
#include <qt/txfilter.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace GRC;

namespace {

//! A synthetic record carrying both the filter inputs and the sort inputs, so
//! the projectors index a plain vector (no wallet, no Qt) — the cursor never
//! sees this type.
struct Rec {
    int64_t time = 0;
    int64_t amount = 0;
    int type = 0;
    int status = 0;             // 0 = active; 6/9 = Conflicted/NotAccepted (inactive)
    std::string addr;
    std::string label;
    std::string status_sort;    // sort key for TXCOL_STATUS
};

//! A backing table + the two projectors bound to it. apply* must be called only
//! after the table is mutated (the production contract).
//!
//! The cursor's FieldsFn/KeysFn return by const reference (the production store
//! returns a ref into its per-record cache — PR4-fix F). The test mirrors that by
//! recomputing the requested slot from the live row on each call and returning a
//! reference into stable per-index storage: always current (no separate sync
//! step), and reentrant-safe — the first call sizes the buffer, so within a single
//! comparison the two reads never reallocate out from under each other.
struct Table {
    std::vector<Rec> rows;
    std::vector<TxFilterFields> fcache;
    std::vector<SortKey> kcache;
    Cursor::FieldsFn fields() {
        return [this](std::size_t i) -> const TxFilterFields& {
            fcache.resize(rows.size());
            const Rec& r = rows[i];
            fcache[i] = TxFilterFields{r.time, r.amount, r.type, r.status, r.addr, r.label};
            return fcache[i];
        };
    }
    Cursor::KeysFn keys() {
        return [this](std::size_t i) -> const SortKey& {
            kcache.resize(rows.size());
            const Rec& r = rows[i];
            // SortKey field order: time, net_amount, status_sort_key, type_string,
            // label_string, address_string (PR4-fix G splits label/address).
            kcache[i] = SortKey{r.time, r.amount, r.status_sort, "", r.label, r.addr};
            return kcache[i];
        };
    }
};

Rec active(int64_t time, const std::string& ssort = "") { Rec r; r.time = time; r.status = 0; r.status_sort = ssort; return r; }

// Count deltas of a given type.
int countType(const std::vector<CursorDelta>& d, CursorDelta::Type t) {
    int n = 0; for (const auto& x : d) if (x.type == t) ++n; return n;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_cursor_tests)

// ---- rebuild: filter membership + sort order --------------------------------

BOOST_AUTO_TEST_CASE(rebuild_filters_inactive_and_sorts_date_desc)
{
    Table t;
    t.rows = { active(100), active(300), /*inactive*/ Rec{200,0,0,6,"","",""}, active(150) };
    Cursor c(1, FilterSpec{}, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    auto d = c.rebuild(t.rows.size());

    // Default spec masks inactive (show_orphans=false): row 2 (status 6) excluded.
    // Sorted time-DESC: 300(idx1), 150(idx3), 100(idx0).
    BOOST_CHECK_EQUAL(c.totalAccepted(), 3u);
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 3u);
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 3u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 0u);
    BOOST_CHECK_EQUAL(d.size(), 1u);
    BOOST_CHECK_EQUAL(d[0].type, CursorDelta::Reset);
    BOOST_CHECK_EQUAL(d[0].count, 3);
}

// ---- store insert: absolute-index shift + sorted placement (Item 4) ---------

BOOST_AUTO_TEST_CASE(store_insert_shifts_absidx_and_places_in_sort_order)
{
    Table t;
    t.rows = { active(100), active(300), active(150) };  // view_index DESC: [1,2,0]
    Cursor c(1, FilterSpec{}, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());

    // Insert a record at absolute position 0 (everything shifts +1). New row time=200.
    t.rows.insert(t.rows.begin(), active(200));
    auto d = c.applyStoreInsert(0, 1);

    // Old indices 1,2,0 became 2,3,1; new row at absidx 0 (time 200) slots between
    // 300 and 150 -> view_index DESC by time: 300(2),200(0),150(3),100(1).
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 4u);
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 2u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 0u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 3u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 1u);
    BOOST_CHECK_EQUAL(countType(d, CursorDelta::Insert), 1);
    BOOST_CHECK_EQUAL(d[0].first, 1);  // inserted at served slot 1
}

// ---- store remove: identity erase + shift (Item 4) --------------------------

BOOST_AUTO_TEST_CASE(store_remove_erases_by_identity_and_shifts)
{
    Table t;
    t.rows = { active(100), active(300), active(150) };  // DESC: [1,2,0]
    Cursor c(1, FilterSpec{}, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());

    // Remove absolute index 1 (time 300). Survivors 0,2 -> 2 stays >1 so shifts to 1.
    t.rows.erase(t.rows.begin() + 1);
    auto d = c.applyStoreRemove(1, 1);

    // Remaining: 100(was 0, still 0), 150(was 2, now 1). DESC: 150(1),100(0).
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 2u);
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 0u);
    BOOST_CHECK_EQUAL(countType(d, CursorDelta::Remove), 1);
    BOOST_CHECK_EQUAL(d[0].first, 0);  // 300 was the first served row
}

// ---- status update: membership flip in / out --------------------------------

BOOST_AUTO_TEST_CASE(status_update_flip_out_then_in)
{
    Table t;
    t.rows = { active(100), active(200), active(300) };
    Cursor c(1, FilterSpec{}, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());                 // [2,1,0]
    BOOST_CHECK_EQUAL(c.totalAccepted(), 3u);

    // Row 1 goes Conflicted -> masked (default show_orphans=false) -> flip-out.
    t.rows[1].status = TXSTATUS_CONFLICTED;
    auto d1 = c.applyStatusUpdate(1);
    BOOST_CHECK_EQUAL(c.totalAccepted(), 2u);
    BOOST_CHECK_EQUAL(countType(d1, CursorDelta::Remove), 1);
    BOOST_CHECK_EQUAL(d1[0].first, 1);        // 200 was served slot 1

    // Row 1 goes active again -> flip-in, re-slots by time (200 -> slot 1).
    t.rows[1].status = 0;
    auto d2 = c.applyStatusUpdate(1);
    BOOST_CHECK_EQUAL(c.totalAccepted(), 3u);
    BOOST_CHECK_EQUAL(countType(d2, CursorDelta::Insert), 1);
    BOOST_CHECK_EQUAL(d2[0].first, 1);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 1u);
}

// ---- ⚠ Item 1: locate by identity when sort-keys collide --------------------

BOOST_AUTO_TEST_CASE(identity_locate_with_equal_sort_keys)
{
    // Three rows sharing the SAME status_sort ("u") — all equal under TXCOL_STATUS
    // Less. A sort-key lower_bound to locate would hit the band's start (a sibling);
    // identity-locate must hit the exact row.
    Table t;
    t.rows = { active(10, "u"), active(20, "u"), active(30, "u") };  // all "u"
    Cursor c(1, FilterSpec{}, TXCOL_STATUS, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 3u);

    // Flip-out the MIDDLE record by absolute identity (idx 1). The siblings (0,2)
    // must remain; the survivor that stays must be 0 and 2, not a mis-located one.
    t.rows[1].status = TXSTATUS_CONFLICTED;
    c.applyStatusUpdate(1);
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 2u);
    std::vector<std::size_t> remaining(c.viewIndex().begin(), c.viewIndex().end());
    std::sort(remaining.begin(), remaining.end());
    BOOST_CHECK_EQUAL(remaining[0], 0u);
    BOOST_CHECK_EQUAL(remaining[1], 2u);   // idx 1 removed, NOT a sibling
}

// ---- ⚠ Item 2: reposition recomputes insert slot AFTER erase ----------------

BOOST_AUTO_TEST_CASE(reposition_moves_down_no_off_by_one)
{
    // Sort by TXCOL_STATUS DESC over distinct keys d>c>b>a. view_index: [d,c,b,a].
    Table t;
    t.rows = { active(0,"d"), active(0,"c"), active(0,"b"), active(0,"a") };
    Cursor c(1, FilterSpec{}, TXCOL_STATUS, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());               // [0,1,2,3]  (d,c,b,a)
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 0u);

    // Row 0's key drops "d" -> "a.5" (between b and a). old_pos 0 < new_pos 3:
    // a pre-erase new_pos would be off-by-one. Final order must be c,b,a.5,a.
    t.rows[0].status_sort = "a5";           // "a5" sorts after "a", before "b"
    c.applyStatusUpdate(0);
    // DESC: c(1) > b(2) > a5(0) > a(3)
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 4u);
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 2u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 0u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 3u);
}

// ---- served-window: eviction on insert into a full window -------------------

BOOST_AUTO_TEST_CASE(served_window_eviction_on_insert_when_full)
{
    Table t;
    t.rows = { active(100), active(200), active(300) };
    FilterSpec spec; spec.limit_rows = 2;                 // cap 2
    Cursor c(1, spec, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());                              // accepted [2,1,0], served [2,1]
    BOOST_CHECK_EQUAL(c.servedCount(), 2u);

    // Insert time=250 at absidx 3 -> slots between 300 and 200 (served slot 1).
    t.rows.push_back(active(250));
    auto d = c.applyStoreInsert(3, 1);
    // Visible: Insert at 1, then evict the row pushed off the bottom (Remove at cap=2).
    BOOST_CHECK_EQUAL(c.servedCount(), 2u);
    BOOST_CHECK_EQUAL(c.totalAccepted(), 4u);
    BOOST_REQUIRE_EQUAL(d.size(), 2u);
    BOOST_CHECK_EQUAL(d[0].type, CursorDelta::Insert);
    BOOST_CHECK_EQUAL(d[0].first, 1);
    BOOST_CHECK_EQUAL(d[1].type, CursorDelta::Remove);
    BOOST_CHECK_EQUAL(d[1].first, 2);
}

// ---- served-window: promotion on remove with overflow -----------------------

BOOST_AUTO_TEST_CASE(served_window_promotion_on_remove_with_overflow)
{
    Table t;
    t.rows = { active(100), active(200), active(300), active(400) };
    FilterSpec spec; spec.limit_rows = 2;
    Cursor c(1, spec, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());                  // accepted [3,2,1,0], served [400,300]
    BOOST_CHECK_EQUAL(c.servedCount(), 2u);

    // Remove the top visible row (400, absidx 3). 200 should promote into view.
    t.rows.erase(t.rows.begin() + 3);
    auto d = c.applyStoreRemove(3, 1);
    BOOST_CHECK_EQUAL(c.servedCount(), 2u);
    BOOST_REQUIRE_EQUAL(d.size(), 2u);
    BOOST_CHECK_EQUAL(d[0].type, CursorDelta::Remove);
    BOOST_CHECK_EQUAL(d[0].first, 0);
    BOOST_CHECK_EQUAL(d[1].type, CursorDelta::Insert);
    BOOST_CHECK_EQUAL(d[1].first, 1);          // promoted into the last visible slot
}

// ---- ⚠ Item 3: off-window change maintains view_index, emits nothing --------

BOOST_AUTO_TEST_CASE(off_window_update_maintained_without_emission)
{
    Table t;
    t.rows = { active(100,"a"), active(200,"b"), active(300,"c"), active(400,"d") };
    FilterSpec spec; spec.limit_rows = 2;
    Cursor c(1, spec, TXCOL_STATUS, TXSORT_DESC, t.fields(), t.keys());  // DESC: d,c,b,a = [3,2,1,0]
    c.rebuild(t.rows.size());                  // served = [3,2]

    // Change an OFF-window row (idx 0, "a", at view slot 3) to "bb" (sorts between
    // c and b, i.e. to view slot 2 — still off-window). It repositions among the
    // off-window rows: no emission (both old slot 3 and new slot 2 are >= served),
    // but view_index must still reflect the new order (Item 3).
    t.rows[0].status_sort = "bb";
    auto d = c.applyStatusUpdate(0);
    BOOST_CHECK_EQUAL(d.size(), 0u);           // off-window move: silent
    // New DESC order: d(3) > c(2) > bb(0) > b(1).
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 3u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 2u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 0u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 1u);

    // Growing the window now reveals the off-window rows in the correct order.
    auto g = c.setLimit(4);
    BOOST_CHECK_EQUAL(countType(g, CursorDelta::Insert), 1);
    BOOST_CHECK_EQUAL(g[0].first, 2);          // promoted rows start at old served=2
    BOOST_CHECK_EQUAL(g[0].count, 2);
}

// ---- setLimit grow / shrink -------------------------------------------------

BOOST_AUTO_TEST_CASE(setlimit_grow_then_shrink)
{
    Table t;
    t.rows = { active(100), active(200), active(300), active(400), active(500) };
    FilterSpec spec; spec.limit_rows = 2;
    Cursor c(1, spec, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());
    BOOST_CHECK_EQUAL(c.servedCount(), 2u);

    auto grow = c.setLimit(4);
    BOOST_CHECK_EQUAL(c.servedCount(), 4u);
    BOOST_REQUIRE_EQUAL(grow.size(), 1u);
    BOOST_CHECK_EQUAL(grow[0].type, CursorDelta::Insert);
    BOOST_CHECK_EQUAL(grow[0].first, 2);
    BOOST_CHECK_EQUAL(grow[0].count, 2);

    auto shrink = c.setLimit(1);
    BOOST_CHECK_EQUAL(c.servedCount(), 1u);
    BOOST_REQUIRE_EQUAL(shrink.size(), 1u);
    BOOST_CHECK_EQUAL(shrink[0].type, CursorDelta::Remove);
    BOOST_CHECK_EQUAL(shrink[0].first, 1);
    BOOST_CHECK_EQUAL(shrink[0].count, 3);
}

// ---- CT_UPDATED first-confirmation repositions across the served boundary ----

BOOST_AUTO_TEST_CASE(ct_updated_first_confirmation_moves_out_of_window)
{
    // Status-DESC like OverviewPage. Unconfirmed sorts FIRST ("z..." high key).
    // A first confirmation drops the key so the row falls past the served window;
    // an off-window row promotes. This path runs constantly on a live Overview.
    Table t;
    t.rows = { active(0,"z_unconf"), active(0,"m_conf"), active(0,"l_conf"), active(0,"k_conf") };
    FilterSpec spec; spec.limit_rows = 2;
    Cursor c(1, spec, TXCOL_STATUS, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());                  // DESC: z(0),m(1),l(2),k(3); served [0,1]
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 0u);

    // Row 0 confirms: key z_unconf -> "a_conf" (now sorts LAST). Moves out of window.
    t.rows[0].status_sort = "a_conf";
    auto d = c.applyStatusUpdate(0);
    // Moved out of the served window (was slot 0, now last) -> Remove(0) + promote l(2) into slot 1.
    BOOST_REQUIRE_EQUAL(d.size(), 2u);
    BOOST_CHECK_EQUAL(d[0].type, CursorDelta::Remove);
    BOOST_CHECK_EQUAL(d[0].first, 0);
    BOOST_CHECK_EQUAL(d[1].type, CursorDelta::Insert);
    BOOST_CHECK_EQUAL(d[1].first, 1);
    // Full order DESC: m(1),l(2),k(3),a_conf(0).
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 0u);
}

// ---- ⚠ PR4-fix E: equal primary keys break by native record index, reproducibly ----

BOOST_AUTO_TEST_CASE(equal_keys_tiebreak_by_native_index)
{
    // Four rows sharing the SAME date (the TXCOL_DATE primary key). The producer
    // keeps m_records in native (time, hash, idx) order, so the cursor breaks the
    // tie by absolute index — a deterministic, reproducible order. The old std::sort
    // with no tie-breaker left ties arbitrary; the proxy's QSortFilterProxyModel was
    // a stable_sort over the native source, which this reproduces.
    Table t;
    t.rows = { active(500), active(500), active(500), active(500) };
    Cursor c(1, FilterSpec{}, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 4u);
    // Ascending absidx even under a DESC primary (ties are not flipped by order).
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 0u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 2u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 3u);

    // Toggling the sort order yields the IDENTICAL tie order — no arbitrariness.
    c.setSort(TXCOL_DATE, TXSORT_ASC);
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 4u);
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 0u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 2u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 3u);
}

// ---- ⚠ PR4-fix C/A review follow-up: interleaved reposition to a shared new key ----

BOOST_AUTO_TEST_CASE(interleaved_reposition_to_equal_keys)
{
    // The store's applyAddressBookChange / applyChainTipRefresh move several rows to a
    // NEW (often shared) key. applyStatusUpdate repositions via lower_bound, which needs
    // the rest of view_index sorted, so the store must recompute+drive ONE row at a time
    // (NOT recompute-all-then-drive, which transiently de-sorts the index). This verifies
    // the cursor lands them correctly under that interleaved pattern, including an
    // intervening row that the relabel crosses. Sort by ADDRESS asc (label is the primary
    // Address key); the self-syncing test projectors read the live row on each call, so
    // changing one label then driving it mirrors the store's interleave exactly.
    Table t;
    t.rows = { active(0), active(0), active(0), active(0) };
    t.rows[0].label = "C"; t.rows[1].label = "C"; t.rows[2].label = "C"; t.rows[3].label = "F";
    Cursor c(1, FilterSpec{}, TXCOL_ADDRESS, TXSORT_ASC, t.fields(), t.keys());
    c.rebuild(t.rows.size());           // labels C,C,C,F asc -> [0,1,2,3]
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 4u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 3u);

    // Rename the "C" group to "K" (crosses "F"), interleaved: relabel one, drive one.
    for (std::size_t i : { std::size_t{0}, std::size_t{1}, std::size_t{2} }) {
        t.rows[i].label = "K";
        c.applyStatusUpdate(i);
    }
    // Correct ADDRESS-asc order: F(3) < K(0) < K(1) < K(2) — the intervening F row is
    // NOT wedged among the K rows (the recompute-all-first bug produced [0,1,3,2]).
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 4u);
    BOOST_CHECK_EQUAL(c.viewIndex()[0], 3u);
    BOOST_CHECK_EQUAL(c.viewIndex()[1], 0u);
    BOOST_CHECK_EQUAL(c.viewIndex()[2], 1u);
    BOOST_CHECK_EQUAL(c.viewIndex()[3], 2u);
}

// ---- positionOf: public identity-locate (PR5 rowForKey backing) -------------

BOOST_AUTO_TEST_CASE(position_of_maps_absidx_to_accepted_row_or_npos)
{
    // The store's rowForKey resolves a (hash, idx) to an absolute record index,
    // then asks the cursor for its accepted row via positionOf — for click-through
    // to a row and anchor-on-resort (PR5). It must report the sorted accepted
    // position, and npos for a row the filter excludes or that is absent.
    constexpr std::size_t NPOS = static_cast<std::size_t>(-1);
    Table t;
    // abs:    0           1           2 (inactive)              3
    t.rows = { active(100), active(300), Rec{200,0,0,6,"","",""}, active(150) };
    Cursor c(1, FilterSpec{}, TXCOL_DATE, TXSORT_DESC, t.fields(), t.keys());
    c.rebuild(t.rows.size());
    // accepted, time-DESC: 300(abs1), 150(abs3), 100(abs0); abs2 inactive -> out.
    BOOST_REQUIRE_EQUAL(c.viewIndex().size(), 3u);

    BOOST_CHECK_EQUAL(c.positionOf(1), 0u);   // abs1 -> accepted row 0
    BOOST_CHECK_EQUAL(c.positionOf(3), 1u);   // abs3 -> accepted row 1
    BOOST_CHECK_EQUAL(c.positionOf(0), 2u);   // abs0 -> accepted row 2
    BOOST_CHECK(c.positionOf(2) == NPOS);     // filtered out -> npos
    BOOST_CHECK(c.positionOf(42) == NPOS);    // absent -> npos
}

BOOST_AUTO_TEST_SUITE_END()
