// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "consensus/tx_verify.h"
#include "main.h"
#include "primitives/transaction.h"

BOOST_AUTO_TEST_SUITE(bip68_tests)

BOOST_AUTO_TEST_CASE(sequence_lock_constants)
{
    // Verify BIP68 constant values match specification
    BOOST_CHECK_EQUAL(SEQUENCE_FINAL, 0xffffffff);
    BOOST_CHECK_EQUAL(SEQUENCE_LOCKTIME_DISABLE_FLAG, (1U << 31));
    BOOST_CHECK_EQUAL(SEQUENCE_LOCKTIME_TYPE_FLAG, (1U << 22));
    BOOST_CHECK_EQUAL(SEQUENCE_LOCKTIME_MASK, 0x0000ffff);
    BOOST_CHECK_EQUAL(SEQUENCE_LOCKTIME_GRANULARITY, 9);
}

BOOST_AUTO_TEST_CASE(sequence_disabled_flag)
{
    // A transaction with version < 2 should have sequence locks trivially satisfied
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10; // 10 blocks relative locktime

    std::vector<int> prevHeights = {100};
    CBlockIndex block;
    CBlockIndex prevBlock;
    prevBlock.nHeight = 109;
    block.pprev = &prevBlock;
    block.nHeight = 110;

    auto result = CalculateSequenceLocks(CTransaction(tx), 0, prevHeights, block);
    // v1 tx should always satisfy
    BOOST_CHECK_EQUAL(result.first, 0);
    BOOST_CHECK_EQUAL(result.second, -1);
}

BOOST_AUTO_TEST_CASE(sequence_lock_disabled_flag_on_input)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    // Set the disable flag - this input should not be constrained
    tx.vin[0].nSequence = SEQUENCE_LOCKTIME_DISABLE_FLAG | 10;

    std::vector<int> prevHeights = {100};
    CBlockIndex block;
    CBlockIndex prevBlock;
    prevBlock.nHeight = 99;
    block.pprev = &prevBlock;
    block.nHeight = 100;

    auto result = CalculateSequenceLocks(CTransaction(tx), 0, prevHeights, block);
    // With disable flag, locks should be trivially satisfied
    BOOST_CHECK_EQUAL(result.first, -1);
    BOOST_CHECK_EQUAL(result.second, -1);
}

BOOST_AUTO_TEST_CASE(sequence_lock_height_based)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10; // 10 blocks relative locktime

    std::vector<int> prevHeights = {100};
    CBlockIndex block;
    CBlockIndex prevBlock;
    prevBlock.nHeight = 109;
    block.pprev = &prevBlock;
    block.nHeight = 110;

    auto result = CalculateSequenceLocks(CTransaction(tx), 0, prevHeights, block);
    // Min height = nCoinHeight + nSequence - 1 = 100 + 10 - 1 = 109
    BOOST_CHECK_EQUAL(result.first, 109);
    BOOST_CHECK_EQUAL(result.second, -1);

    // At height 110, the lock should be satisfied (109 < 110)
    BOOST_CHECK(EvaluateSequenceLocks(block, result));
}

BOOST_AUTO_TEST_CASE(sequence_lock_height_based_not_satisfied)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10; // 10 blocks relative locktime

    std::vector<int> prevHeights = {100};
    CBlockIndex block;
    CBlockIndex prevBlock;
    prevBlock.nHeight = 108;
    block.pprev = &prevBlock;
    block.nHeight = 109;

    auto result = CalculateSequenceLocks(CTransaction(tx), 0, prevHeights, block);
    // Min height = 100 + 10 - 1 = 109
    BOOST_CHECK_EQUAL(result.first, 109);

    // At height 109, the lock should NOT be satisfied (109 >= 109)
    BOOST_CHECK(!EvaluateSequenceLocks(block, result));
}

BOOST_AUTO_TEST_CASE(sequence_lock_multiple_inputs)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(3);
    tx.vin[0].nSequence = 5;   // 5 blocks from height 100
    tx.vin[1].nSequence = 10;  // 10 blocks from height 90
    tx.vin[2].nSequence = SEQUENCE_LOCKTIME_DISABLE_FLAG; // disabled

    std::vector<int> prevHeights = {100, 90, 50};
    CBlockIndex block;
    CBlockIndex prevBlock;
    prevBlock.nHeight = 109;
    block.pprev = &prevBlock;
    block.nHeight = 110;

    auto result = CalculateSequenceLocks(CTransaction(tx), 0, prevHeights, block);
    // input 0: 100 + 5 - 1 = 104
    // input 1: 90 + 10 - 1 = 99
    // input 2: disabled
    // max = 104
    BOOST_CHECK_EQUAL(result.first, 104);
    BOOST_CHECK_EQUAL(result.second, -1);

    BOOST_CHECK(EvaluateSequenceLocks(block, result));
}

BOOST_AUTO_TEST_SUITE_END()
