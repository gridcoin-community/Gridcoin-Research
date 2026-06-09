// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF unit coverage for the Qt-free transaction ordering core
// (src/qt/txorder.{h,cpp}). This is the (time DESC, hash ASC, idx ASC)
// ordering — and the lower_bound insert-position and same-hash contiguity it
// guarantees — that the producer-side GRC::WalletTxStore relies on to compute
// the position-stamped RowsInserted / RowsRemoved events. Testing it here keeps
// the position/clustering logic covered in the ENABLE_GUI=OFF configuration CI
// exercises (TransactionRecord itself is Qt-coupled, so the store proper is
// covered by ASan-GUI + isolated-testnet validation instead).

#include <qt/txorder.h>

#include "uint256.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

using namespace GRC;

namespace {
TxOrderKey K(int64_t time, const char* hash_hex, int idx)
{
    return TxOrderKey{time, uint256S(hash_hex), idx};
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(qt_txorder_tests)

// ---- TxOrderLess: the three ordering levels --------------------------------

BOOST_AUTO_TEST_CASE(txorder_time_descending_primary)
{
    // Newer time sorts FIRST, so a newer key is "less than" an older one.
    TxOrderKey newer = K(300, "01", 0);
    TxOrderKey older = K(200, "01", 0);
    BOOST_CHECK( TxOrderLess(newer, older));
    BOOST_CHECK(!TxOrderLess(older, newer));
}

BOOST_AUTO_TEST_CASE(txorder_hash_ascending_tiebreak)
{
    // Same time -> lower hash sorts first (ascending).
    TxOrderKey a = K(100, "01", 0);
    TxOrderKey b = K(100, "02", 0);
    BOOST_CHECK( TxOrderLess(a, b));
    BOOST_CHECK(!TxOrderLess(b, a));
}

BOOST_AUTO_TEST_CASE(txorder_idx_ascending_tiebreak)
{
    // Same time + same hash -> lower idx sorts first (ascending).
    TxOrderKey a = K(100, "0a", 0);
    TxOrderKey b = K(100, "0a", 1);
    BOOST_CHECK( TxOrderLess(a, b));
    BOOST_CHECK(!TxOrderLess(b, a));
}

BOOST_AUTO_TEST_CASE(txorder_strict_weak_equal_is_false)
{
    // A true total order: equal keys are never less than each other.
    TxOrderKey a = K(100, "0a", 3);
    BOOST_CHECK(!TxOrderLess(a, a));
}

// ---- lower_bound insert position (mirrors WalletTxStore::insertTransaction) -

BOOST_AUTO_TEST_CASE(txorder_lower_bound_insert_positions)
{
    // A descending-by-time sorted view: 300, 200, 100 (all distinct hashes,
    // single idx). This is the cachedWallet/keys order.
    std::vector<TxOrderKey> keys = { K(300, "01", 0), K(200, "02", 0), K(100, "03", 0) };
    BOOST_REQUIRE(std::is_sorted(keys.begin(), keys.end(), TxOrderLess));

    auto pos = [&](const TxOrderKey& k) {
        return std::lower_bound(keys.begin(), keys.end(), k, TxOrderLess) - keys.begin();
    };

    // Newest of all -> front.
    BOOST_CHECK_EQUAL(pos(K(400, "00", 0)), 0);
    // Between 300 and 200 -> index 1.
    BOOST_CHECK_EQUAL(pos(K(250, "00", 0)), 1);
    // Between 200 and 100 -> index 2.
    BOOST_CHECK_EQUAL(pos(K(150, "00", 0)), 2);
    // Oldest of all -> end.
    BOOST_CHECK_EQUAL(pos(K(50, "00", 0)), 3);
}

// ---- same-hash clustering (the removal path's contiguity invariant) --------

BOOST_AUTO_TEST_CASE(txorder_same_hash_records_cluster_contiguously)
{
    // Two txs sharing a wall-clock time, each decomposed into 2 records. Under
    // (time, hash, idx) the parts of one tx must occupy a contiguous run, so
    // the store's equal_range min/max bound a single erase range.
    std::vector<TxOrderKey> keys = {
        K(100, "aa", 0), K(100, "aa", 1),   // tx aa, parts 0,1
        K(100, "bb", 0), K(100, "bb", 1),   // tx bb, parts 0,1
    };
    // Deliberately scramble then sort by the comparator.
    std::vector<TxOrderKey> scrambled = { keys[3], keys[0], keys[2], keys[1] };
    std::sort(scrambled.begin(), scrambled.end(), TxOrderLess);

    // aa < bb (hash ascending), idx ascending within each.
    BOOST_CHECK(scrambled[0].hash == uint256S("aa") && scrambled[0].idx == 0);
    BOOST_CHECK(scrambled[1].hash == uint256S("aa") && scrambled[1].idx == 1);
    BOOST_CHECK(scrambled[2].hash == uint256S("bb") && scrambled[2].idx == 0);
    BOOST_CHECK(scrambled[3].hash == uint256S("bb") && scrambled[3].idx == 1);

    // Each hash occupies a single contiguous [min,max] run.
    auto run = [&](const uint256& h) {
        int lo = -1, hi = -1;
        for (int i = 0; i < (int)scrambled.size(); ++i) {
            if (scrambled[i].hash == h) { if (lo < 0) lo = i; hi = i; }
        }
        return std::make_pair(lo, hi);
    };
    auto aa = run(uint256S("aa"));
    auto bb = run(uint256S("bb"));
    BOOST_CHECK_EQUAL(aa.first, 0); BOOST_CHECK_EQUAL(aa.second, 1);
    BOOST_CHECK_EQUAL(bb.first, 2); BOOST_CHECK_EQUAL(bb.second, 3);
    // Contiguity: run length == number of parts (no foreign row interleaved).
    BOOST_CHECK_EQUAL(aa.second - aa.first + 1, 2);
    BOOST_CHECK_EQUAL(bb.second - bb.first + 1, 2);
}

BOOST_AUTO_TEST_SUITE_END()
