// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <validation.h>
#include <chainparams.h>
#include <policy/policy.h>
#include <gridcoin/superblock.h>
#include <consensus/consensus.h>
#include <limits>
#include <script/interpreter.h>
#include <script/script.h>
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


//!
//! Script flags are evaluated at the height the transaction will be validated
//! IN, which is not always the height of the index the caller happens to hold.
//!
//! Connecting a block, that index IS the block. On the mempool and miner paths
//! it is the PREVIOUS block, and the transaction is bound for the next one --
//! so both were a height low. Harmless while no activation boundary is nearby,
//! and a lost stake at the next one: a staker computing pre-fork flags builds a
//! block that validators, computing post-fork flags, reject.
//!
BOOST_AUTO_TEST_CASE(script_flags_follow_the_height_a_transaction_lands_at)
{
    const int v14 = Params().GetConsensus().BlockV14Height;

    // Straddling the activation, the flag set differs by exactly one height.
    const unsigned int before = GetBlockScriptFlags(v14 - 1);
    const unsigned int at     = GetBlockScriptFlags(v14);

    BOOST_CHECK((before & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY) == 0);
    BOOST_CHECK((at & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY) != 0);
    BOOST_CHECK((at & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY) != 0);

    // P2SH is unconditional on both sides.
    BOOST_CHECK((before & SCRIPT_VERIFY_P2SH) != 0);
    BOOST_CHECK((at & SCRIPT_VERIFY_P2SH) != 0);

    // The distinction the fix turns on: a miner or mempool holding the index at
    // v14 - 1 is building for v14, and must use v14's flags, not that index's.
    BOOST_CHECK(before != at);
}


//!
//! The v15 malleability flags are gated, and inert until v15 is scheduled.
//!
//! Landing them early is only safe if they genuinely do nothing at present.
//! BlockV15Height is numeric_limits<int>::max() until a release schedules it,
//! so every reachable height must produce the pre-v15 flag set.
//!
BOOST_AUTO_TEST_CASE(v15_script_flags_are_inert_until_v15_is_scheduled)
{
    const int v15 = GetBlockV15Height();

    // Unscheduled today. If this ever fails, v15 has been given a height and
    // the assertions below need revisiting rather than deleting.
    BOOST_REQUIRE_EQUAL(v15, std::numeric_limits<int>::max());

    for (const int h : {0, 1, 1000000, 3989999, 3990000, 4000000, 100000000}) {
        const unsigned int flags = GetBlockScriptFlags(h);
        BOOST_CHECK_MESSAGE((flags & V15_SCRIPT_VERIFY_FLAGS) == 0,
                            "v15 flags leaked at height " << h);
    }
}

//!
//! And that they are the right flags once it is.
//!
BOOST_AUTO_TEST_CASE(v15_script_flags_close_malleability_at_activation)
{
    // Below the gate: none of them. At and above: all of them, and the v14 set
    // is still present, since v15 adds rather than replaces.
    const unsigned int below = GetBlockScriptFlags(std::numeric_limits<int>::max() - 1);
    BOOST_CHECK((below & SCRIPT_VERIFY_LOW_S) == 0);

    const unsigned int at = GetBlockScriptFlags(std::numeric_limits<int>::max());
    BOOST_CHECK((at & V15_SCRIPT_VERIFY_FLAGS) == V15_SCRIPT_VERIFY_FLAGS);
    BOOST_CHECK((at & SCRIPT_VERIFY_LOW_S) != 0);
    BOOST_CHECK((at & SCRIPT_VERIFY_P2SH) != 0);
    BOOST_CHECK((at & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY) != 0);
    BOOST_CHECK((at & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY) != 0);
}


//!
//! From v15 the block size bounds are additive and independent:
//! block-excluding-superblock <= MAX_BLOCK_SIZE, and superblock <=
//! Superblock::MAX_SIZE.
//!
//! Independent on purpose. A single combined ceiling would make each side's
//! headroom depend on the other -- a large superblock eating the room for
//! ordinary transactions, a full block capping the superblock. It also means
//! the superblock's own limit finally binds: while the block measurement
//! includes the superblock's bytes and the block ceiling is the smaller of the
//! two, the Superblock::MAX_SIZE term can never be reached.
//!
//! Inert today: BlockV15Height is numeric_limits<int>::max(), so every
//! reachable height keeps the combined measurement.
//!
BOOST_AUTO_TEST_CASE(the_superblock_size_envelope_is_gated_and_additive)
{
    BOOST_REQUIRE_EQUAL(GetBlockV15Height(), std::numeric_limits<int>::max());

    // The gate is closed at every height reachable today.
    for (const int h : {0, 1000000, 3989999, 3990000, 4000000, 100000000}) {
        BOOST_CHECK_MESSAGE(!IsV15Enabled(h), "v15 gate open at height " << h);
    }

    // The flag that makes the two measurements separable. Combined with AND --
    // as a plain reading of the two constants might suggest -- it yields
    // neither flag, and the claim serializer would emit the superblock into a
    // measurement meant to exclude it.
    BOOST_CHECK_EQUAL(SER_NETWORK & SER_SKIPSUPERBLOCK, 0);
    BOOST_CHECK((( SER_NETWORK | SER_SKIPSUPERBLOCK) & SER_SKIPSUPERBLOCK) != 0);

    // And the envelope the two bounds describe once open.
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIZE, 1000000u);
    BOOST_CHECK_EQUAL(GRC::Superblock::MAX_SIZE, 4000000u);
}


//!
//! A block with no transactions is refused before anything reads vtx[0].
//!
//! CheckBlock() receives blocks straight off the wire -- ProcessMessage
//! deserializes a peer-supplied CBlock and ProcessBlock calls through to here
//! with no emptiness check of its own -- so an empty vtx is attacker-supplied
//! input. Everything in the size-limit block reaches GetSuperblock(), which
//! goes through CBlock::GetClaim() and indexes vtx[0] unconditionally, so the
//! emptiness test has to come first and has to stand alone rather than sit in
//! a || chain that a later reordering could disturb.
//!
BOOST_AUTO_TEST_CASE(an_empty_block_is_refused_before_vtx_is_read)
{
    LOCK(cs_main);

    CBlock block;
    block.nVersion = 11;
    BOOST_REQUIRE(block.vtx.empty());

    CValidationState state;

    // Must return false rather than crash. Before the guard was hoisted this
    // dereferenced vtx[0] on an empty vector.
    BOOST_CHECK(!CheckBlock(block, state, 1, false, false, false, false));
}

BOOST_AUTO_TEST_SUITE_END()
