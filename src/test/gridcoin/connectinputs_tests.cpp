// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <validation.h>
#include <txdb.h>
#include <primitives/transaction.h>

#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE(connectinputs_tests)

//!
//! A transaction carrying an already-spent outpoint costs no signature
//! verifications, whatever position that outpoint occupies.
//!
//! This is the ordering guarantee, and it is invisible to a test that only
//! checks the return value: the transaction is rejected either way. What the
//! conflict pre-scan changes is the COST of rejecting it. With the conflict
//! check interleaved in the signature loop, every input ahead of the
//! conflicting one is verified first -- at MAX_STANDARD_TX_SIZE roughly 675
//! ECDSA operations under cs_main, for a transaction that was never going to be
//! accepted, and which earns the sender no misbehaviour score.
//!
//! The signatures here are deliberately junk. They are never reached, which is
//! the entire assertion; if a regression reaches them the count moves and this
//! fails regardless of whether they would have verified.
//!
BOOST_AUTO_TEST_CASE(a_conflicting_input_costs_no_signature_verifications)
{
    LOCK(cs_main);

    CBlockIndex index;
    index.nHeight = nGrandfather + 1;   // past the grandfathered accept path

    CMutableTransaction spender;
    MapPrevTx inputs;

    // Three inputs; the LAST one is already spent. Ahead of it sit two that a
    // pre-scan must stop us from verifying.
    for (int i = 0; i < 3; ++i) {
        CMutableTransaction prev;
        prev.vout.resize(1);

        // Distinct values so the three hash differently. Identical prevs
        // collapse to ONE entry in the map, every input then references the
        // same already-spent outpoint, and the conflict is found at index 0 --
        // where the old code bails before verifying anything too, so the test
        // would pass with or without the pre-scan and guard nothing.
        prev.vout[0].nValue = (i + 1) * COIN;

        const CTransaction prev_tx(prev);
        const uint256 prev_hash = prev_tx.GetHash();

        CTxIndex txindex;
        txindex.vSpent.resize(1);
        if (i == 2) txindex.vSpent[0] = CDiskTxPos(1, 1, 1);

        inputs[prev_hash] = {txindex, prev_tx};

        spender.vin.emplace_back();
        spender.vin.back().prevout = COutPoint(prev_hash, 0);
        spender.vin.back().scriptSig = CScript() << OP_1;   // junk, never reached
    }

    spender.vout.resize(1);
    spender.vout[0].nValue = 1 * COIN;

    BOOST_REQUIRE_EQUAL(inputs.size(), 3u);   // three DISTINCT prevouts

    CTransaction tx(spender);
    CValidationState state;
    CTxDB txdb("r");
    std::map<uint256, CTxIndex> test_pool;

    const uint64_t before = g_connectinputs_signature_checks.load();

    const bool accepted = ConnectInputs(tx, state, txdb, inputs, test_pool,
                                        CDiskTxPos(1, 1, 1), &index,
                                        /*fBlock=*/false, /*fMiner=*/false);

    const uint64_t verifications = g_connectinputs_signature_checks.load() - before;

    BOOST_CHECK(!accepted);
    BOOST_CHECK_EQUAL(verifications, 0u);
}

//!
//! The converse, so the test above cannot pass by never reaching the loop at
//! all: with no conflict, the signature loop IS entered and does verify.
//!
BOOST_AUTO_TEST_CASE(a_clean_input_set_reaches_the_signature_loop)
{
    LOCK(cs_main);

    CBlockIndex index;
    index.nHeight = nGrandfather + 1;

    CMutableTransaction spender;
    MapPrevTx inputs;

    CMutableTransaction prev;
    prev.vout.resize(1);
    prev.vout[0].nValue = 1 * COIN;

    const CTransaction prev_tx(prev);
    const uint256 prev_hash = prev_tx.GetHash();

    CTxIndex txindex;
    txindex.vSpent.resize(1);          // unspent
    inputs[prev_hash] = {txindex, prev_tx};

    spender.vin.emplace_back();
    spender.vin.back().prevout = COutPoint(prev_hash, 0);
    spender.vin.back().scriptSig = CScript() << OP_1;

    spender.vout.resize(1);
    spender.vout[0].nValue = 1 * COIN;

    CTransaction tx(spender);
    CValidationState state;
    CTxDB txdb("r");
    std::map<uint256, CTxIndex> test_pool;

    const uint64_t before = g_connectinputs_signature_checks.load();

    (void)ConnectInputs(tx, state, txdb, inputs, test_pool,
                        CDiskTxPos(1, 1, 1), &index, /*fBlock=*/false, /*fMiner=*/false);

    BOOST_CHECK_EQUAL(g_connectinputs_signature_checks.load() - before, 1u);
}

BOOST_AUTO_TEST_SUITE_END()
