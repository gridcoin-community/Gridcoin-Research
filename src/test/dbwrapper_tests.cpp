// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <utility>

// CTxDB reads inside an open batch are documented as consistent with the
// batch: Read() consults the pending writes first and answers "absent" for a
// key the batch deletes. Exists() has the same contract, and these cases hold
// it to that in the one shape that matters: a key whose committed copy is
// still on disk while the batch holds a Delete for it. That is exactly the
// state of a transaction's index entry midway through DisconnectBlocksBatch.

BOOST_AUTO_TEST_SUITE(dbwrapper_tests)

BOOST_AUTO_TEST_CASE(exists_honours_a_batched_delete_of_a_committed_key)
{
    CTxDB txdb("r+");
    std::pair<std::string, int> key("dbwrapper_tests", 1);
    const int value = 42;

    // Committed copy on disk.
    BOOST_REQUIRE(txdb.WriteGenericSerializable(key, value));
    BOOST_REQUIRE(txdb.ExistsGenericSerializable(key));

    // Delete it inside a batch: the disk copy is still there, and Exists()
    // used to fall through to it and report the key present.
    BOOST_REQUIRE(txdb.TxnBegin());
    BOOST_REQUIRE(txdb.EraseGenericSerializable(key));
    BOOST_CHECK(!txdb.ExistsGenericSerializable(key));

    int read_back = 0;
    BOOST_CHECK(!txdb.ReadGenericSerializable(key, read_back));

    // Abandoning the batch brings the committed copy back into view.
    BOOST_REQUIRE(txdb.TxnAbort());
    BOOST_CHECK(txdb.ExistsGenericSerializable(key));

    BOOST_REQUIRE(txdb.EraseGenericSerializable(key));
    BOOST_CHECK(!txdb.ExistsGenericSerializable(key));
}

// Pins the scanner's last-entry-wins order rather than the fix above: the old
// fall-through also answered "absent" here, because nothing was on disk.
BOOST_AUTO_TEST_CASE(exists_follows_a_key_written_then_deleted_in_one_batch)
{
    CTxDB txdb("r+");
    std::pair<std::string, int> key("dbwrapper_tests", 2);
    const int value = 7;

    BOOST_REQUIRE(!txdb.ExistsGenericSerializable(key));

    BOOST_REQUIRE(txdb.TxnBegin());
    BOOST_REQUIRE(txdb.WriteGenericSerializable(key, value));
    BOOST_CHECK(txdb.ExistsGenericSerializable(key));

    // The scanner walks the whole batch, so the later Delete wins.
    BOOST_REQUIRE(txdb.EraseGenericSerializable(key));
    BOOST_CHECK(!txdb.ExistsGenericSerializable(key));
    BOOST_REQUIRE(txdb.TxnAbort());

    BOOST_CHECK(!txdb.ExistsGenericSerializable(key));
}

BOOST_AUTO_TEST_SUITE_END()
