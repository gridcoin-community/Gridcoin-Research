// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <validation.h>
#include <chainparams.h>
#include <policy/policy.h>
#include <gridcoin/superblock.h>
#include <gridcoin/contract/contract.h>
#include <gridcoin/claim.h>
#include <gridcoin/tx_message.h>
#include <consensus/consensus.h>
#include <limits>
#include <string>
#include <tinyformat.h>
#include <util/system.h>
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
//! A CONSENSUS script failure scores the relayer 100. This is the half of the
//! policy/consensus split that is writable today, and it is the half that has to
//! keep working: the point of the split is that policy failures stop scoring
//! peers, not that consensus failures stop doing so.
//!
//! CLEANSTACK is the cheapest v15 flag to violate deliberately and needs no
//! signature at all. `OP_1` as the output script, spent by `OP_1`, leaves two
//! items on the stack: the top is true, so the spend is valid under the pre-v15
//! flags, and invalid once CLEANSTACK is in the consensus set. Same transaction,
//! same inputs -- only the activation height moves, which is what makes the DoS
//! score attributable to the flag rather than to anything else about the input.
//!
//! `policy_flags` is still the empty literal at validation.cpp:467, so the
//! nDoS == 0 half cannot be written yet: nothing can fail policy without also
//! failing consensus. That needs a seam in the flag set first, and is the other
//! half of this row.
//!
BOOST_AUTO_TEST_CASE(a_consensus_script_failure_scores_the_relayer)
{
    LOCK(cs_main);

    // Restored before leaving: three suites in this binary assert that v15 is
    // unscheduled, and pool_tests forces this same argument in the other
    // direction. Leaving it set would make this file's own earlier cases order
    // dependent.
    struct V15HeightGuard {
        const bool was_set{gArgs.IsArgSet("-blockv15height")};
        const std::string previous{gArgs.GetArg("-blockv15height", "")};
        ~V15HeightGuard()
        {
            if (was_set) {
                gArgs.ForceSetArg("-blockv15height", previous);
            } else {
                gArgs.ForceSetArg("-blockv15height", strprintf("%d", std::numeric_limits<int>::max()));
            }
        }
    } guard;

    CBlockIndex index;
    index.nHeight = nGrandfather + 1;

    CMutableTransaction prev;
    prev.vout.resize(1);
    prev.vout[0].nValue = 1 * COIN;
    prev.vout[0].scriptPubKey = CScript() << OP_1;   // anyone-can-spend, leaves one item

    const CTransaction prev_tx(prev);
    const uint256 prev_hash = prev_tx.GetHash();

    CTxIndex txindex;
    txindex.vSpent.resize(1);          // unspent
    MapPrevTx inputs;
    inputs[prev_hash] = {txindex, prev_tx};

    CMutableTransaction spender;
    spender.vin.emplace_back();
    spender.vin.back().prevout = COutPoint(prev_hash, 0);
    spender.vin.back().scriptSig = CScript() << OP_1;  // one item too many at the end
    spender.vout.resize(1);
    // Leaves half a coin as fee. A zero-fee spend fails ConnectInputs' own
    // GetMinFee() check first -- and on the !fBlock path that returns false with
    // NO DoS score, which would make the control below pass for the wrong reason.
    spender.vout[0].nValue = COIN / 2;

    CTransaction tx(spender);   // ConnectInputs takes a non-const reference
    CTxDB txdb("r");

    // Control: v15 inert. CLEANSTACK is not in the flag set, the top of the stack
    // is true, and the spend is accepted. Without this the assertion below would
    // pass for a transaction that was invalid for some unrelated reason.
    {
        gArgs.ForceSetArg("-blockv15height", strprintf("%d", std::numeric_limits<int>::max()));
        BOOST_REQUIRE_EQUAL(GetBlockV15Height(), std::numeric_limits<int>::max());

        CValidationState state;
        std::map<uint256, CTxIndex> test_pool;

        BOOST_CHECK(ConnectInputs(tx, state, txdb, inputs, test_pool,
                                  CDiskTxPos(1, 1, 1), &index, /*fBlock=*/false, /*fMiner=*/false));
        BOOST_CHECK_EQUAL(state.GetDoS(), 0);
    }

    // Activated: the same transaction now fails a consensus rule, and the peer
    // that relayed it is scored the full 100.
    {
        gArgs.ForceSetArg("-blockv15height", "0");
        BOOST_REQUIRE_EQUAL(GetBlockV15Height(), 0);

        CValidationState state;
        std::map<uint256, CTxIndex> test_pool;

        BOOST_CHECK(!ConnectInputs(tx, state, txdb, inputs, test_pool,
                                   CDiskTxPos(1, 1, 1), &index, /*fBlock=*/false, /*fMiner=*/false));
        BOOST_CHECK(state.IsInvalid());
        BOOST_CHECK_EQUAL(state.GetDoS(), 100);
    }
}


namespace {
//! A superblock whose serialized size is at least `target` bytes.
GRC::Superblock SuperblockOfAtLeast(const size_t target)
{
    GRC::Superblock superblock;
    superblock.m_version = GRC::Superblock::CURRENT_VERSION;
    superblock.m_projects.Add("project", GRC::Superblock::ProjectStats());

    std::vector<unsigned char> bytes(16, 0);

    for (uint32_t i = 0; ; i++) {
        // Distinct CPIDs; duplicates would collapse in the index and loop forever.
        bytes[0] = i         & 0xff;
        bytes[1] = (i >>  8) & 0xff;
        bytes[2] = (i >> 16) & 0xff;
        bytes[3] = (i >> 24) & 0xff;

        superblock.m_cpids.Add(GRC::Cpid(bytes), GRC::Magnitude::RoundFrom(1));

        // Checking every entry would dominate the runtime of this file.
        if ((i & 0x3ff) == 0
            && ::GetSerializeSize(superblock, SER_NETWORK, PROTOCOL_VERSION) >= target)
        {
            break;
        }
    }

    return superblock;
}

//! A coinbase carrying a claim, optionally with a superblock attached.
CTransaction CoinbaseWithClaim(const GRC::Superblock* const superblock)
{
    GRC::Claim claim;
    claim.m_version = GRC::Claim::CURRENT_VERSION;
    claim.m_client_version = "test";

    // Enough for WellFormed(): a valid mining id and a subsidy. Noncruncher
    // deliberately, so the claim needs no research subsidy or signature -- the
    // rules under test are about size and position, not about rewards.
    claim.m_mining_id = GRC::MiningId::ForNoncruncher();
    claim.m_block_subsidy = 1;

    if (superblock != nullptr) {
        claim.m_superblock.Replace(GRC::Superblock(*superblock));
        claim.m_quorum_hash = claim.m_superblock->GetHash();
    }

    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1000;  // before the block time set in BlockOf()
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vin[0].scriptSig = CScript() << OP_11 << OP_11;  // within the 2..100 coinbase bound
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;
    tx.vContracts.emplace_back(GRC::MakeContract<GRC::Claim>(GRC::ContractAction::ADD, claim));

    return CTransaction(tx);
}

//! An ordinary transaction, optionally carrying a claim of its own.
CTransaction OrdinaryTx(const uint32_t nonce, const GRC::Superblock* const superblock)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1000;  // before the block time set in BlockOf()
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256(nonce + 1), 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;

    if (superblock != nullptr) {
        GRC::Claim claim;
        claim.m_version = GRC::Claim::CURRENT_VERSION;
        claim.m_client_version = "test";
        claim.m_mining_id = GRC::MiningId::ForNoncruncher();
        claim.m_block_subsidy = 1;
        claim.m_superblock.Replace(GRC::Superblock(*superblock));
        claim.m_quorum_hash = claim.m_superblock->GetHash();

        tx.vContracts.emplace_back(GRC::MakeContract<GRC::Claim>(GRC::ContractAction::ADD, claim));
    }

    return CTransaction(tx);
}

CBlock BlockOf(const int32_t version, std::vector<CTransaction> vtx)
{
    CBlock block;
    block.nVersion = version;
    block.nTime = 2000;  // at or after every transaction's nTime
    block.hashPrevBlock = uint256{1};  // not the genesis short-circuit
    block.vtx = std::move(vtx);

    return block;
}

//! CheckBlock with the expensive contextual checks off. Those are not what any
//! of this is about, and leaving them on would mean forging a proof and a
//! merkle root to reach the size accounting.
bool CheckBlockSizeOnly(CBlock& block)
{
    CValidationState state;

    // Height below nGrandfather, so the difficulty check does not fire; the
    // rules under test key on the block's version, not on this.
    return CheckBlock(block, state, 1000, false, false, false);
}
} // namespace

//!
//! Under v15 rules the block size bounds are additive and independent:
//! block-excluding-superblock <= MAX_BLOCK_SIZE, and superblock <=
//! Superblock::MAX_SIZE.
//!
//! Independent on purpose. A single combined ceiling would make each side's
//! headroom depend on the other -- a large superblock eating the room for
//! ordinary transactions, a full block capping the superblock -- and it also
//! means the superblock's own limit finally binds.
//!
//! The pair below differs only in the block's version, which is what selects
//! the rule, so it fails if the envelope is removed, keyed on the wrong
//! version, or serialized with the wrong flags.
//!
BOOST_AUTO_TEST_CASE(the_superblock_leaves_the_block_bound_only_under_v15_rules)
{
    LOCK(cs_main);

    // Inert until v15 is scheduled.
    BOOST_REQUIRE_EQUAL(GetBlockV15Height(), std::numeric_limits<int>::max());

    // Over the block bound on its own, but inside its own bound.
    const GRC::Superblock superblock = SuperblockOfAtLeast(MAX_BLOCK_SIZE + 1);
    BOOST_REQUIRE_GT(::GetSerializeSize(superblock, SER_NETWORK, PROTOCOL_VERSION), MAX_BLOCK_SIZE);
    BOOST_REQUIRE_LT(::GetSerializeSize(superblock, SER_NETWORK, PROTOCOL_VERSION), GRC::Superblock::MAX_SIZE);

    // v15: measured without the superblock, so the block is small and passes.
    CBlock v15 = BlockOf(15, {CoinbaseWithClaim(&superblock), OrdinaryTx(1, nullptr)});
    BOOST_CHECK(CheckBlockSizeOnly(v15));

    // The same block one version earlier: the superblock counts against the
    // block bound, and it alone is over.
    CBlock v14 = BlockOf(14, {CoinbaseWithClaim(&superblock), OrdinaryTx(1, nullptr)});
    BOOST_CHECK(!CheckBlockSizeOnly(v14));
}

//!
//! The superblock's own bound still binds under v15 -- excluding it from the
//! block measurement is not the same as leaving it unmeasured.
//!
BOOST_AUTO_TEST_CASE(a_superblock_over_its_own_bound_is_refused_under_v15_rules)
{
    LOCK(cs_main);

    const GRC::Superblock oversize = SuperblockOfAtLeast(GRC::Superblock::MAX_SIZE + 1);
    BOOST_REQUIRE_GT(::GetSerializeSize(oversize, SER_NETWORK, PROTOCOL_VERSION), GRC::Superblock::MAX_SIZE);

    CBlock block = BlockOf(15, {CoinbaseWithClaim(&oversize), OrdinaryTx(1, nullptr)});
    BOOST_CHECK(!CheckBlockSizeOnly(block));
}

//!
//! A claim belongs in the coinbase and nowhere else.
//!
//! The accounting takes that as given: the block is measured with
//! SER_SKIPSUPERBLOCK, which drops the superblock from every claim the
//! serializer walks, while the separate superblock measurement resolves through
//! CBlock::GetClaim() and reads vtx[0]. A claim carried anywhere else is
//! subtracted by the first and invisible to the second, so nothing bounds it.
//!
//! Reaching that gap needs BOTH halves. The coinbase has to carry a well-formed
//! superblock of its own, since that is what turns the skip flag on; without it
//! the block is measured whole and refused on plain size, which would make this
//! pass for a reason that has nothing to do with the rule under test.
//!
BOOST_AUTO_TEST_CASE(a_claim_outside_the_coinbase_is_refused_under_v15_rules)
{
    LOCK(cs_main);

    const GRC::Superblock small = SuperblockOfAtLeast(1);
    const GRC::Superblock large = SuperblockOfAtLeast(MAX_BLOCK_SIZE + 1);

    CBlock stray = BlockOf(15, {CoinbaseWithClaim(&small), OrdinaryTx(1, &large)});

    // Measured whole, the block is over the bound.
    BOOST_CHECK_GT(::GetSerializeSize(stray, SER_NETWORK, PROTOCOL_VERSION), MAX_BLOCK_SIZE);

    // Measured the way the v15 rule measures it, the bytes are gone.
    BOOST_CHECK_LT(::GetSerializeSize(stray, SER_NETWORK | SER_SKIPSUPERBLOCK, PROTOCOL_VERSION),
                   MAX_BLOCK_SIZE);

    // And the superblock bound cannot see them either: GetSuperblock() reads
    // vtx[0], which carries only the small one.
    BOOST_CHECK_LT(::GetSerializeSize(stray.GetSuperblock(), SER_NETWORK, PROTOCOL_VERSION),
                   GRC::Superblock::MAX_SIZE);

    // Both bounds satisfied, so nothing but the position of the claim refuses it.
    BOOST_CHECK(!CheckBlockSizeOnly(stray));

    // The same block with the stray claim removed is accepted, which is what
    // makes the case above discriminating rather than a block that fails anyway.
    CBlock clean = BlockOf(15, {CoinbaseWithClaim(&small), OrdinaryTx(1, nullptr)});
    BOOST_CHECK(CheckBlockSizeOnly(clean));
}

//!
//! A coinbase carrying no contracts at all.
//!
//! CBlock::GetClaim() indexes vtx[0].vContracts[0] on the strength of the block
//! version alone, and the size accounting reaches it through GetSuperblock()
//! before anything has established that the vector holds a claim. Blocks are
//! deserialized from untrusted input, so an empty vContracts is a shape that has
//! to be handled rather than assumed away.
//!
//! Note this case does not fail by returning the wrong answer if the guard is
//! removed -- it fails by faulting.
//!
BOOST_AUTO_TEST_CASE(a_coinbase_with_no_contracts_is_refused_before_the_claim_is_read)
{
    LOCK(cs_main);

    CMutableTransaction cb;
    cb.nVersion = 2;
    cb.nTime = 1000;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << OP_11 << OP_11;
    cb.vout.resize(1);
    cb.vout[0].nValue = 1;
    // vContracts deliberately left empty.

    CBlock block = BlockOf(14, {CTransaction(cb)});
    BOOST_REQUIRE(block.vtx[0].vContracts.empty());
    BOOST_REQUIRE(block.nVersion >= 11);

    BOOST_CHECK(!CheckBlockSizeOnly(block));
}

//!
//! A coinbase whose first contract is not a claim.
//!
//! The same read as the case above, reached a different way. GetClaim() used to
//! decide on the block version alone, so a coinbase carrying some other kind of
//! contract took the claim branch and handed a payload of one type to a cast
//! expecting another. The message payload here is genuinely not a Claim, which
//! is what makes this discriminating: a version-keyed accessor casts it, a
//! shape-keyed one does not.
//!
BOOST_AUTO_TEST_CASE(a_coinbase_carrying_another_contract_type_is_refused)
{
    LOCK(cs_main);

    CMutableTransaction cb;
    cb.nVersion = 2;
    cb.nTime = 1000;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << OP_11 << OP_11;
    cb.vout.resize(1);
    cb.vout[0].nValue = 1;
    cb.vContracts.emplace_back(
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "not a claim"));

    CBlock block = BlockOf(14, {CTransaction(cb)});
    BOOST_REQUIRE(block.vtx[0].vContracts[0].m_type != GRC::ContractType::CLAIM);

    BOOST_CHECK(!CheckBlockSizeOnly(block));
}

//!
//! The same shape below version 11, where the coinbase contract checks do not
//! run at all.
//!
//! Nothing rejects this on the contract, so the block is measured and the claim
//! is read. The accessor has to stay defined without a check in front of it --
//! reaching the fallback and reporting no superblock, rather than casting.
//!
//! No verdict is asserted here. Whether a block this old is acceptable is not
//! what is under test, and pinning it would invent a rule; what is under test is
//! that asking the question is safe.
//!
BOOST_AUTO_TEST_CASE(a_pre_v11_coinbase_contract_is_read_without_casting_it)
{
    LOCK(cs_main);

    CMutableTransaction cb;
    cb.nVersion = 2;
    cb.nTime = 1000;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << OP_11 << OP_11;
    cb.vout.resize(1);
    cb.vout[0].nValue = 1;
    cb.vContracts.emplace_back(
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "not a claim"));

    CBlock block = BlockOf(10, {CTransaction(cb)});
    BOOST_REQUIRE(block.nVersion < 11);
    BOOST_REQUIRE(!block.vtx[0].vContracts.empty());

    // The read itself, which is the part that used to be undefined.
    BOOST_CHECK(!block.GetSuperblock()->WellFormed());

    CValidationState state;
    (void)CheckBlock(block, state, 1000, false, false, false);
}

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
