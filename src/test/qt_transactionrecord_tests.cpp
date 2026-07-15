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

#include <qt/transactionrecord.h>

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

BOOST_AUTO_TEST_SUITE_END()
