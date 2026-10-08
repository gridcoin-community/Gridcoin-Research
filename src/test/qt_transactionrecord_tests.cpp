// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF coverage for the wallet-transaction DTOs
// (src/qt/transactionrecord.{h,cpp}), Qt-scrubbed in Phase 1c-ii of the
// multiprocess program. Compiling this translation unit into the GUI-OFF
// test binary is itself the load-bearing assertion: the records are the
// wallet-transaction-channel value types a node-side source must be able to
// fill, so they must build without Qt. The cases below cover the value-type
// semantics the marshalability static_asserts promise and the decompose
// entry point's degenerate path.

#include <interfaces/wallet_tx_record.h>

#include "sync.h"
#include "uint256.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

extern CWallet* pwalletMain;

BOOST_AUTO_TEST_SUITE(qt_transactionrecord_tests)

BOOST_AUTO_TEST_CASE(record_is_a_copyable_value_type)
{
    TransactionRecord rec(uint256S("0xabcd"), 1234567,
                          TransactionRecord::SendToAddress, "some-address",
                          -5, 10, 2);
    rec.idx = 3;
    rec.label = "some-label";
    rec.status.sortKey = "sort-key";
    rec.status.depth = 7;

    // Copy round-trip: the copy is independent of the original.
    TransactionRecord copy = rec;
    BOOST_CHECK_EQUAL(copy.getTxID(), rec.getTxID());
    BOOST_CHECK_EQUAL(copy.time, rec.time);
    BOOST_CHECK_EQUAL(copy.address, rec.address);
    BOOST_CHECK_EQUAL(copy.label, rec.label);
    BOOST_CHECK_EQUAL(copy.status.sortKey, rec.status.sortKey);

    copy.address = "mutated";
    copy.status.depth = 99;
    BOOST_CHECK_EQUAL(rec.address, "some-address");
    BOOST_CHECK_EQUAL(rec.status.depth, 7);
}

BOOST_AUTO_TEST_CASE(get_tx_id_is_the_hash_string)
{
    const uint256 hash = uint256S("0xdeadbeef");
    TransactionRecord rec(hash, 0);
    BOOST_CHECK_EQUAL(rec.getTxID(), hash.ToString());
}

BOOST_AUTO_TEST_CASE(decompose_empty_transaction_degenerate_path)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    // An empty wallet transaction (no inputs, no outputs, no contracts)
    // exercises the degenerate decompose path: with zero inputs and outputs
    // the "all from me / all to me" scans are vacuously true, so the
    // transaction classifies as a single zero-amount payment-to-self record.
    // The point of the pin is that the path must not crash and must produce
    // exactly this classification. Locks held per the producer-side contract.
    CWalletTx wtx(pwalletMain);

    LOCK2(cs_main, pwalletMain->cs_wallet);
    const std::vector<TransactionRecord> parts =
        TransactionRecord::decomposeTransaction(pwalletMain, wtx);
    BOOST_REQUIRE_EQUAL(parts.size(), 1U);
    BOOST_CHECK(parts[0].type == TransactionRecord::SendToSelf);
    BOOST_CHECK_EQUAL(parts[0].debit, 0);
    BOOST_CHECK_EQUAL(parts[0].credit, 0);
    BOOST_CHECK(parts[0].hash == wtx.GetHash());
}

// ---- GRC::ProjectTxStatus / TxStatusEquals / DisplayTxStatus (#3059) ----------------------
//
// The producer suppresses a status Change when the fresh status is the projection of the
// previous one, and the GUI projects the snapshot it holds to the pushed tip height. These
// pin the projection itself.

namespace {
TransactionStatus MakeStatus(TransactionStatus::Status st, int64_t depth, int matures_in,
                             int cur_num_blocks)
{
    TransactionStatus s;
    s.status = st;
    s.depth = depth;
    s.matures_in = matures_in;
    s.cur_num_blocks = cur_num_blocks;
    s.countsForBalance = (st == TransactionStatus::Confirmed);
    s.sortKey = "0000001000-1-0000000000-000";
    s.open_for = 7;
    return s;
}
} // namespace

BOOST_AUTO_TEST_CASE(project_tx_status_moves_only_the_height_driven_fields)
{
    const TransactionStatus imm = MakeStatus(TransactionStatus::Immature, 5, 105, 1000);
    const TransactionStatus p = GRC::ProjectTxStatus(imm, 1003);
    BOOST_CHECK_EQUAL(p.cur_num_blocks, 1003);
    BOOST_CHECK_EQUAL(p.depth, 8);
    BOOST_CHECK_EQUAL(p.matures_in, 102);
    // Nothing else moves: the enum, countsForBalance, sortKey, generated_type, open_for.
    BOOST_CHECK(p.status == imm.status);
    BOOST_CHECK_EQUAL(p.countsForBalance, imm.countsForBalance);
    BOOST_CHECK_EQUAL(p.sortKey, imm.sortKey);
    BOOST_CHECK(p.generated_type == imm.generated_type);
    BOOST_CHECK_EQUAL(p.open_for, imm.open_for);

    // Confirming / Confirmed: depth only; matures_in is not touched outside Immature.
    const TransactionStatus conf = GRC::ProjectTxStatus(MakeStatus(TransactionStatus::Confirming, 3, 0, 500), 502);
    BOOST_CHECK_EQUAL(conf.depth, 5);
    BOOST_CHECK_EQUAL(conf.matures_in, 0);
    BOOST_CHECK_EQUAL(GRC::ProjectTxStatus(MakeStatus(TransactionStatus::Confirmed, 12, 0, 500), 505).depth, 17);

    // depth <= 0 is not height-driven: the mempool (0) and conflicted / off-chain (< 0).
    BOOST_CHECK_EQUAL(GRC::ProjectTxStatus(MakeStatus(TransactionStatus::Unconfirmed, 0, 0, 500), 509).depth, 0);
    BOOST_CHECK_EQUAL(GRC::ProjectTxStatus(MakeStatus(TransactionStatus::Conflicted, -1, 0, 500), 509).depth, -1);

    // open_for is never projected (a non-final tx is never refreshed).
    BOOST_CHECK_EQUAL(GRC::ProjectTxStatus(MakeStatus(TransactionStatus::OpenUntilBlock, 0, 0, 500), 504).open_for, 7);
}

BOOST_AUTO_TEST_CASE(project_tx_status_is_the_identity_without_a_forward_height)
{
    const TransactionStatus s = MakeStatus(TransactionStatus::Immature, 5, 105, 1000);
    // Tip below the snapshot: a reorg, or a fetched snapshot newer than the GUI's height.
    BOOST_CHECK(GRC::TxStatusEquals(GRC::ProjectTxStatus(s, 999), s));
    // Same height.
    BOOST_CHECK(GRC::TxStatusEquals(GRC::ProjectTxStatus(s, 1000), s));
    // A snapshot with no height at all.
    TransactionStatus unset = s;
    unset.cur_num_blocks = -1;
    BOOST_CHECK(GRC::TxStatusEquals(GRC::ProjectTxStatus(unset, 5000), unset));
}

BOOST_AUTO_TEST_CASE(project_tx_status_composes_over_forward_steps)
{
    // project(project(S, a), b) == project(S, b) for S.cur <= a <= b: what lets the producer
    // stay silent for many tips in a row while the GUI projects straight from the snapshot.
    for (const auto st : {TransactionStatus::Immature, TransactionStatus::Confirming,
                          TransactionStatus::Confirmed, TransactionStatus::Unconfirmed,
                          TransactionStatus::MaturesWarning, TransactionStatus::OpenUntilBlock}) {
        const TransactionStatus s = MakeStatus(st, st == TransactionStatus::Unconfirmed ? 0 : 4, 106, 200);
        for (int a = 200; a <= 230; a += 3) {
            for (int b = a; b <= 240; b += 5) {
                BOOST_CHECK(GRC::TxStatusEquals(GRC::ProjectTxStatus(GRC::ProjectTxStatus(s, a), b),
                                              GRC::ProjectTxStatus(s, b)));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(tx_status_equals_compares_every_field)
{
    const TransactionStatus base = MakeStatus(TransactionStatus::Immature, 5, 105, 1000);
    BOOST_CHECK(GRC::TxStatusEquals(base, base));
    auto differs = [&](auto mutate) {
        TransactionStatus m = base;
        mutate(m);
        return !GRC::TxStatusEquals(base, m);
    };
    BOOST_CHECK(differs([](TransactionStatus& m) { m.countsForBalance = !m.countsForBalance; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.sortKey += "x"; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.matures_in += 1; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.status = TransactionStatus::Confirmed; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.generated_type = GRC::MinedType::POS; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.depth += 1; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.open_for += 1; }));
    BOOST_CHECK(differs([](TransactionStatus& m) { m.cur_num_blocks += 1; }));
}

BOOST_AUTO_TEST_CASE(display_tx_status_stays_inside_the_snapshot_category)
{
    const int rec = TransactionRecord::RecommendedNumConfirmations;

    // Confirming advances with the tip but never reads "n of n" before the flip arrives.
    const TransactionStatus confirming = MakeStatus(TransactionStatus::Confirming, 3, 0, 100);
    BOOST_CHECK_EQUAL(GRC::DisplayTxStatus(confirming, 102).depth, 5);
    BOOST_CHECK_EQUAL(GRC::DisplayTxStatus(confirming, 100 + 50).depth, rec - 1);

    // Immature keeps at least one block left, and depth + matures_in constant.
    const TransactionStatus immature = MakeStatus(TransactionStatus::Immature, 100, 10, 2000);
    const TransactionStatus d1 = GRC::DisplayTxStatus(immature, 2004);
    BOOST_CHECK_EQUAL(d1.depth, 104);
    BOOST_CHECK_EQUAL(d1.matures_in, 6);
    const TransactionStatus d2 = GRC::DisplayTxStatus(immature, 2050);
    BOOST_CHECK_EQUAL(d2.matures_in, 1);
    BOOST_CHECK_EQUAL(d2.depth + d2.matures_in, immature.depth + immature.matures_in);

    // Confirmed is terminal: unbounded.
    BOOST_CHECK_EQUAL(GRC::DisplayTxStatus(MakeStatus(TransactionStatus::Confirmed, 10, 0, 100), 1100).depth, 1010);

    // Everything else is shown as-is.
    for (const auto st : {TransactionStatus::MaturesWarning, TransactionStatus::NotAccepted,
                          TransactionStatus::Offline, TransactionStatus::Unconfirmed,
                          TransactionStatus::Conflicted, TransactionStatus::OpenUntilBlock,
                          TransactionStatus::OpenUntilDate}) {
        const TransactionStatus s = MakeStatus(st, 4, 50, 300);
        BOOST_CHECK(GRC::TxStatusEquals(GRC::DisplayTxStatus(s, 320), s));
    }

    // Before the first tip event the GUI's cached height is 0, and a snapshot can be newer
    // than the cached height: both show the snapshot unchanged.
    BOOST_CHECK(GRC::TxStatusEquals(GRC::DisplayTxStatus(immature, 0), immature));
    BOOST_CHECK(GRC::TxStatusEquals(GRC::DisplayTxStatus(immature, 1999), immature));
}

BOOST_AUTO_TEST_SUITE_END()
