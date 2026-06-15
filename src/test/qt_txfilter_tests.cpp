// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free transaction filter/sort core
// (src/qt/txfilter.{h,cpp}). This is the logic that today lives inside
// TransactionFilterProxy::filterAcceptsRow() and the Qt::EditRole sort keys;
// extracting and testing it here lands the trickiest GUI predicate logic in
// the configuration CI actually exercises (ENABLE_GUI=OFF) — the blind spot
// that hid PR #2944's heap corruption.

#include <qt/txfilter.h>

#include <boost/test/unit_test.hpp>

#include <string>

using namespace GRC;

namespace {
// A fields projection that PASSES the default (no-op) filter, so each test can
// flip exactly one dimension and assert the consequence in isolation.
TxFilterFields DefaultPassingRow()
{
    TxFilterFields f;
    f.time = 1'600'000'000;   // mid-range timestamp
    f.net_amount = 5 * 100000000LL;
    f.type = 1;               // TransactionRecord::Generated (a normal, non-zero type)
    f.status = 0;             // TransactionStatus::Confirmed (active)
    f.address = "S1ExampleGridcoinAddress1111111111";
    f.label = "Donations";
    return f;
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_txfilter_tests)

// ---- Accepts(): default spec passes a normal row -------------------------

BOOST_AUTO_TEST_CASE(accepts_default_spec_passes)
{
    BOOST_CHECK(Accepts(DefaultPassingRow(), FilterSpec{}));
}

// ---- Accepts(): address_substr matches the address OR the label ----------
// The label arm is what PR4 newly exercises — the producer snapshots the
// address-book label onto the record so this filter works off the wallet.
BOOST_AUTO_TEST_CASE(accepts_address_substr_matches_address_or_label)
{
    const TxFilterFields row = DefaultPassingRow();  // address has "Gridcoin", label "Donations"
    FilterSpec spec;

    spec.address_substr = "Gridcoin";   // substring of the ADDRESS
    BOOST_CHECK(Accepts(row, spec));

    spec.address_substr = "onation";    // substring of the LABEL only (not the address)
    BOOST_CHECK(Accepts(row, spec));

    spec.address_substr = "DONATION";   // case-insensitive (mirrors Qt::CaseInsensitive)
    BOOST_CHECK(Accepts(row, spec));

    spec.address_substr = "zzz_nomatch"; // in neither -> rejected
    BOOST_CHECK(!Accepts(row, spec));
}

// ---- Accepts(): inactive (Conflicted / NotAccepted) gating ---------------

BOOST_AUTO_TEST_CASE(accepts_inactive_gating)
{
    FilterSpec spec; // defaults: show_inactive=true, show_orphans=false

    TxFilterFields conflicted = DefaultPassingRow();
    conflicted.status = TXSTATUS_CONFLICTED;
    TxFilterFields notaccepted = DefaultPassingRow();
    notaccepted.status = TXSTATUS_NOTACCEPTED;

    // Original logic rejects inactive rows when (!show_inactive || !show_orphans).
    // Default spec has show_orphans=false, so inactive rows are masked even
    // though show_inactive defaults true — mirroring the two-gate original.
    BOOST_CHECK(!Accepts(conflicted, spec));
    BOOST_CHECK(!Accepts(notaccepted, spec));

    // Both gates open => inactive rows pass.
    spec.show_orphans = true;
    spec.show_inactive = true;
    BOOST_CHECK(Accepts(conflicted, spec));
    BOOST_CHECK(Accepts(notaccepted, spec));

    // show_inactive closed masks them regardless of show_orphans.
    spec.show_inactive = false;
    BOOST_CHECK(!Accepts(conflicted, spec));
    BOOST_CHECK(!Accepts(notaccepted, spec));

    // show_orphans closed masks them regardless of show_inactive.
    spec.show_inactive = true;
    spec.show_orphans = false;
    BOOST_CHECK(!Accepts(conflicted, spec));
    BOOST_CHECK(!Accepts(notaccepted, spec));

    // An ACTIVE status is never masked by either gate.
    TxFilterFields active = DefaultPassingRow();
    active.status = 0; // Confirmed
    spec.show_inactive = false;
    spec.show_orphans = false;
    BOOST_CHECK(Accepts(active, spec));
}

// ---- Accepts(): type bit mask --------------------------------------------

BOOST_AUTO_TEST_CASE(accepts_type_mask)
{
    TxFilterFields row = DefaultPassingRow();

    for (int t = 0; t < 12; ++t) {
        row.type = t;
        FilterSpec only_this; only_this.type_mask = (1u << t);
        FilterSpec all_but_this; all_but_this.type_mask = ~(1u << t);

        BOOST_TEST_CONTEXT("type " << t) {
            BOOST_CHECK(Accepts(row, only_this));
            BOOST_CHECK(!Accepts(row, all_but_this));
            BOOST_CHECK(Accepts(row, FilterSpec{})); // ALL_TYPES default
        }
    }
}

// ---- Accepts(): date range inclusivity -----------------------------------

BOOST_AUTO_TEST_CASE(accepts_date_range_boundaries)
{
    TxFilterFields row = DefaultPassingRow();
    row.time = 1000;

    FilterSpec spec;
    spec.date_from = 1000;
    spec.date_to = 2000;

    // Inclusive on both ends.
    row.time = 1000; BOOST_CHECK(Accepts(row, spec));   // == from
    row.time = 2000; BOOST_CHECK(Accepts(row, spec));   // == to
    row.time = 1500; BOOST_CHECK(Accepts(row, spec));   // inside
    row.time = 999;  BOOST_CHECK(!Accepts(row, spec));  // below from
    row.time = 2001; BOOST_CHECK(!Accepts(row, spec));  // above to
}

// ---- Accepts(): address OR label substring, case-insensitive -------------

BOOST_AUTO_TEST_CASE(accepts_address_or_label_substring)
{
    TxFilterFields row = DefaultPassingRow();
    row.address = "S1AbCdEfGh";
    row.label = "Coffee Fund";

    auto with = [](const std::string& s){ FilterSpec f; f.address_substr = s; return f; };

    // Address match (case-insensitive substring, not prefix).
    BOOST_CHECK(Accepts(row, with("abcd")));
    BOOST_CHECK(Accepts(row, with("CDEF")));
    BOOST_CHECK(Accepts(row, with("S1ABCDEFGH")));
    // Label match (case-insensitive substring).
    BOOST_CHECK(Accepts(row, with("coffee")));
    BOOST_CHECK(Accepts(row, with("FUND")));
    BOOST_CHECK(Accepts(row, with("ee fu")));
    // Neither matches.
    BOOST_CHECK(!Accepts(row, with("zzz")));
    // Empty substring matches everything (mirrors QString::contains("")).
    BOOST_CHECK(Accepts(row, with("")));

    // Match must be against address OR label independently.
    TxFilterFields no_label = DefaultPassingRow();
    no_label.address = "S1OnlyAddr";
    no_label.label.clear();
    BOOST_CHECK(Accepts(no_label, with("onlyaddr")));
    BOOST_CHECK(!Accepts(no_label, with("nonexistent")));
}

// ---- Accepts(): min amount on absolute net value -------------------------

BOOST_AUTO_TEST_CASE(accepts_min_amount_uses_abs)
{
    FilterSpec spec;
    spec.min_amount = 3 * 100000000LL;

    TxFilterFields pos = DefaultPassingRow();
    pos.net_amount = 5 * 100000000LL;
    BOOST_CHECK(Accepts(pos, spec));        // 5 >= 3

    pos.net_amount = 2 * 100000000LL;
    BOOST_CHECK(!Accepts(pos, spec));       // 2 < 3

    // Negative net amounts are compared by absolute value (llabs), so a -5 GRC
    // send passes a 3 GRC minimum just like a +5 GRC receive.
    TxFilterFields neg = DefaultPassingRow();
    neg.net_amount = -5 * 100000000LL;
    BOOST_CHECK(Accepts(neg, spec));        // |-5| = 5 >= 3
    neg.net_amount = -2 * 100000000LL;
    BOOST_CHECK(!Accepts(neg, spec));       // |-2| = 2 < 3

    // Boundary: exactly equal passes (>= not >).
    TxFilterFields eq = DefaultPassingRow();
    eq.net_amount = 3 * 100000000LL;
    BOOST_CHECK(Accepts(eq, spec));
}

// ---- Accepts(): a non-trivial AND of several dimensions ------------------

BOOST_AUTO_TEST_CASE(accepts_combined_predicates)
{
    TxFilterFields row = DefaultPassingRow();
    row.type = 2;
    row.time = 1500;
    row.net_amount = 10 * 100000000LL;
    row.address = "S1Combined";
    row.label = "Rent";

    FilterSpec spec;
    spec.type_mask = (1u << 2);
    spec.date_from = 1000; spec.date_to = 2000;
    spec.min_amount = 1 * 100000000LL;
    spec.address_substr = "rent";

    BOOST_CHECK(Accepts(row, spec));

    // Flip each dimension out of range; each alone must reject.
    { auto s = spec; s.type_mask = (1u << 3); BOOST_CHECK(!Accepts(row, s)); }
    { auto s = spec; s.date_to = 1400;        BOOST_CHECK(!Accepts(row, s)); }
    { auto s = spec; s.min_amount = 100 * 100000000LL; BOOST_CHECK(!Accepts(row, s)); }
    { auto s = spec; s.address_substr = "zzz"; BOOST_CHECK(!Accepts(row, s)); }
}

// ---- Less(): numeric columns (Date, Amount) ------------------------------

BOOST_AUTO_TEST_CASE(less_numeric_columns)
{
    SortKey a; a.time = 100; a.net_amount = -50;
    SortKey b; b.time = 200; b.net_amount = 50;

    // Date ascending: a(100) < b(200)
    BOOST_CHECK( Less(a, b, TXCOL_DATE, TXSORT_ASC));
    BOOST_CHECK(!Less(b, a, TXCOL_DATE, TXSORT_ASC));
    // Date descending inverts.
    BOOST_CHECK(!Less(a, b, TXCOL_DATE, TXSORT_DESC));
    BOOST_CHECK( Less(b, a, TXCOL_DATE, TXSORT_DESC));

    // Amount is signed: -50 < 50 ascending.
    BOOST_CHECK( Less(a, b, TXCOL_AMOUNT, TXSORT_ASC));
    BOOST_CHECK( Less(b, a, TXCOL_AMOUNT, TXSORT_DESC));

    // Equal keys -> false in both orders (strict weak ordering), for both
    // numeric columns.
    SortKey c; c.time = 100; c.net_amount = 42;
    SortKey d; d.time = 100; d.net_amount = 42;
    BOOST_CHECK(!Less(c, d, TXCOL_DATE, TXSORT_ASC));
    BOOST_CHECK(!Less(c, d, TXCOL_DATE, TXSORT_DESC));
    BOOST_CHECK(!Less(c, d, TXCOL_AMOUNT, TXSORT_ASC));
    BOOST_CHECK(!Less(c, d, TXCOL_AMOUNT, TXSORT_DESC));
}

// ---- Less(): string columns (Status, Type, Address), case-insensitive ----

BOOST_AUTO_TEST_CASE(less_string_columns_case_insensitive)
{
    SortKey a, b;
    a.status_sort_key = "001"; b.status_sort_key = "002";
    BOOST_CHECK( Less(a, b, TXCOL_STATUS, TXSORT_ASC));
    BOOST_CHECK( Less(b, a, TXCOL_STATUS, TXSORT_DESC));
    // Equal status key -> false in both orders (strict weak ordering).
    SortKey sa, sb;
    sa.status_sort_key = "007"; sb.status_sort_key = "007";
    BOOST_CHECK(!Less(sa, sb, TXCOL_STATUS, TXSORT_ASC));
    BOOST_CHECK(!Less(sa, sb, TXCOL_STATUS, TXSORT_DESC));

    SortKey ta, tb;
    ta.type_string = "apple"; tb.type_string = "Banana";
    // Case-insensitive: "apple" < "banana" even though 'B' < 'a' in ASCII.
    BOOST_CHECK( Less(ta, tb, TXCOL_TYPE, TXSORT_ASC));
    BOOST_CHECK(!Less(tb, ta, TXCOL_TYPE, TXSORT_ASC));
    // Case-insensitively equal type string -> false in both orders.
    SortKey tc, td;
    tc.type_string = "Mined - PoS"; td.type_string = "mined - pos";
    BOOST_CHECK(!Less(tc, td, TXCOL_TYPE, TXSORT_ASC));
    BOOST_CHECK(!Less(tc, td, TXCOL_TYPE, TXSORT_DESC));

    SortKey aa, ab;
    aa.address_string = "ABC"; ab.address_string = "abc";
    // Case-insensitively equal -> false in both orders.
    BOOST_CHECK(!Less(aa, ab, TXCOL_ADDRESS, TXSORT_ASC));
    BOOST_CHECK(!Less(ab, aa, TXCOL_ADDRESS, TXSORT_ASC));
    BOOST_CHECK(!Less(aa, ab, TXCOL_ADDRESS, TXSORT_DESC));
}

// ---- CompareKeys(): 3-way sign, descending flip, and the tie sentinel (PR4-fix E) ----
BOOST_AUTO_TEST_CASE(compare_keys_three_way_and_tie)
{
    SortKey lo; lo.time = 100;
    SortKey hi; hi.time = 200;
    BOOST_CHECK(CompareKeys(lo, hi, TXCOL_DATE, TXSORT_ASC) < 0);   // lo before hi
    BOOST_CHECK(CompareKeys(hi, lo, TXCOL_DATE, TXSORT_ASC) > 0);   // hi after lo
    BOOST_CHECK(CompareKeys(lo, hi, TXCOL_DATE, TXSORT_DESC) > 0);  // descending flips the sign
    BOOST_CHECK(CompareKeys(hi, lo, TXCOL_DATE, TXSORT_DESC) < 0);
    // Equal keys -> 0 in BOTH orders: the sentinel the cursor breaks by native index.
    SortKey eqa; eqa.time = 100; SortKey eqb; eqb.time = 100;
    BOOST_CHECK_EQUAL(CompareKeys(eqa, eqb, TXCOL_DATE, TXSORT_ASC), 0);
    BOOST_CHECK_EQUAL(CompareKeys(eqa, eqb, TXCOL_DATE, TXSORT_DESC), 0);
}

// ---- Address column: (label, then address), no separator-byte collision (PR4-fix G) ----
BOOST_AUTO_TEST_CASE(address_sorts_label_then_address)
{
    // Label is the primary key: "Alice" sorts before "Bob" regardless of address.
    SortKey alice; alice.label_string = "Alice"; alice.address_string = "Szzz";
    SortKey bob;   bob.label_string   = "Bob";   bob.address_string   = "Saaa";
    BOOST_CHECK( Less(alice, bob, TXCOL_ADDRESS, TXSORT_ASC));
    BOOST_CHECK(!Less(bob, alice, TXCOL_ADDRESS, TXSORT_ASC));

    // Equal label -> the address breaks the tie.
    SortKey a1; a1.label_string = "Donations"; a1.address_string = "Saaa";
    SortKey a2; a2.label_string = "Donations"; a2.address_string = "Szzz";
    BOOST_CHECK( Less(a1, a2, TXCOL_ADDRESS, TXSORT_ASC));
    BOOST_CHECK(!Less(a2, a1, TXCOL_ADDRESS, TXSORT_ASC));

    // A control byte in a label cannot cross the (label, address) boundary: "Alice"
    // is a strict prefix of "Alice\x01Bob", so the shorter label always sorts first
    // regardless of the addresses. The old '\x01'-joined single key could invert this
    // (the address bytes leaked past the separator). Split-literal stops the \x01 hex
    // escape from swallowing 'B'.
    SortKey shortLabel; shortLabel.label_string = "Alice";              shortLabel.address_string = "Szzz";
    SortKey longLabel;  longLabel.label_string  = std::string("Alice\x01" "Bob"); longLabel.address_string = "Saaa";
    BOOST_CHECK( Less(shortLabel, longLabel, TXCOL_ADDRESS, TXSORT_ASC));
    BOOST_CHECK(!Less(longLabel, shortLabel, TXCOL_ADDRESS, TXSORT_ASC));
}

BOOST_AUTO_TEST_SUITE_END()
