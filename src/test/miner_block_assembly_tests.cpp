// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! Block assembly: the mempool selection loop in CreateRestOfTheBlock.
//!
//! Until issue #3290 these lines had no unit coverage at all. The two existing
//! drivers of CreateRestOfTheBlock -- coinstake_construction_tests.cpp and
//! gridcoin/mrc_tests.cpp -- run against a MOCK chain with an EMPTY mempool, so
//! the loop body never executes a single iteration; they cover the prologue and
//! the epilogue around it. Everything about which transactions reach a block was
//! therefore reachable only from test/functional/, where observing it costs a
//! real staked block.
//!
//! What makes this testable is RegtestChainSetup (test/chain_setup.h): a real
//! regtest chain with the genesis premine spendable. Note that the assertions
//! here never mine -- they populate the mempool and call CreateRestOfTheBlock
//! directly -- so none of them inherits the regtest stake lottery that put
//! mining_fee_policy.py in EXTENDED_SCRIPTS.
//!
//! FEE RULE FOR EVERY CANDIDATE HERE. Any transaction that is meant to be
//! evaluated past the size guard is funded at >= 100 satoshi per serialized
//! byte. On this branch the miner's only floor is ABSOLUTE (GetMinFee with
//! nBytes = 0 is size-independent, so a flat 100,000), but PR #3291 adds a
//! fee-RATE floor defaulting to the same 100,000 per KILOBYTE. The two coincide
//! at exactly 1000 bytes and diverge above it, and AddToMempool uses
//! addUnchecked, which bypasses the size-scaled relay floor that makes the rate
//! floor inert for real traffic. Funding by rate keeps these cases valid across
//! that merge instead of quietly emptying the block.
//!

#include "amount.h"
#include "chain.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "gridcoin/cpid.h"
#include "gridcoin/mrc.h"
#include "init.h"
#include "miner.h"
#include "policy/fees.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "scheduler.h"
#include "script/script.h"
#include "test/chain_setup.h"
#include "txmempool.h"
#include "validation.h"
#include "validationinterface.h"
#include "wallet/wallet.h"

#include <boost/signals2/connection.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <string>

#include <map>
#include <vector>

using grc_test::AddToMempool;
using grc_test::CreateSpend;
using grc_test::MakeBlockAndCoinstake;
using grc_test::PremineCoinbase;
using grc_test::SpendablePremineOutputs;

namespace {

using MrcMap = std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>;

//! Serialized size the miner measures a candidate by.
unsigned int TxSize(const CTransaction& tx)
{
    return ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
}

//! The miner's dFeePerKb for a transaction paying `fee`.
double FeePerKb(const CTransaction& tx, CAmount fee)
{
    return (double)fee / (double(TxSize(tx)) / 1000.0);
}

//! Fund a candidate at `sat_per_byte`, which is how every case here stays above
//! both the absolute floor on this branch and the fee-rate floor #3291 adds.
CAmount FeeAtRate(const CTransaction& probe, CAmount sat_per_byte)
{
    return (CAmount)TxSize(probe) * sat_per_byte;
}

//! Build one candidate spending premine output `index`, sized by `n_outputs`
//! and funded at `sat_per_byte`. The fee is derived from the transaction's real
//! serialized size rather than an estimate: a one-byte DER drift would
//! otherwise move the rate.
CTransaction MakeCandidate(size_t index, int n_outputs, CAmount sat_per_byte, CAmount& fee_out)
{
    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_MESSAGE(index < coins.size(), "not enough spendable premine outputs");

    // Size first, with a placeholder fee, then re-derive the fee from the
    // measured size and rebuild. The size does not depend on the fee.
    const CTransaction probe = CreateSpend(PremineCoinbase(), coins[index].n, COIN, n_outputs);
    fee_out = FeeAtRate(probe, sat_per_byte);

    const CTransaction tx = CreateSpend(PremineCoinbase(), coins[index].n, fee_out, n_outputs);
    BOOST_REQUIRE_EQUAL(TxSize(tx), TxSize(probe));

    // The rate floor is not the only one. The miner also compares the ABSOLUTE
    // fee against GetMinFee(tx, nBlockSize, GMF_BLOCK), which passes nBytes = 0
    // and is therefore a flat MIN_TX_FEE * 10 regardless of size, and
    // ConnectInputs applies the same flat floor a second time. A candidate that
    // misses it is not merely excluded -- on this branch it aborts the whole
    // selection loop -- so a case that funds too thinly must fail here as bad
    // setup rather than silently assert on an empty block.
    BOOST_REQUIRE_MESSAGE(fee_out >= GetMinFee(tx),
        "candidate underfunded for the miner's absolute floor: a small transaction "
        "needs roughly 520 satoshi per byte to clear a flat 100000");

    return tx;
}

//! As MakeCandidate, but every output carries `script`. Used to reach the sigop
//! guard, which counts signature operations in the output scripts.
CTransaction MakeCandidateToScript(size_t index, const CScript& script, int n_outputs,
                                   CAmount sat_per_byte, CAmount& fee_out)
{
    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_MESSAGE(index < coins.size(), "not enough spendable premine outputs");

    const CTransaction probe =
        grc_test::CreateSpendToScript(PremineCoinbase(), coins[index].n, COIN, script, n_outputs);
    fee_out = FeeAtRate(probe, sat_per_byte);

    const CTransaction tx =
        grc_test::CreateSpendToScript(PremineCoinbase(), coins[index].n, fee_out, script, n_outputs);
    BOOST_REQUIRE_EQUAL(TxSize(tx), TxSize(probe));

    return tx;
}

//! Run block assembly over whatever is in the mempool, returning the selected
//! transactions in selection order. block.vtx is filled by push_back after the
//! coinbase/coinstake pair, so vtx[2:] IS the order the loop chose.
std::vector<CTransaction> AssembleBlock(CAmount& fees_out)
{
    CBlock block;
    CMutableTransaction coinbase;
    CMutableTransaction coinstake;
    MakeBlockAndCoinstake(block, coinbase, coinstake);

    MrcMap mrc_map;

    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        BOOST_REQUIRE(CreateRestOfTheBlock(block, coinbase, coinstake, pindexBest, mrc_map));
    }

    // The epilogue parks the accumulated fees on the coinbase's only output.
    fees_out = coinbase.vout[0].nValue;

    BOOST_REQUIRE(block.vtx.size() >= 2);

    return std::vector<CTransaction>(block.vtx.begin() + 2, block.vtx.end());
}

bool Contains(const std::vector<CTransaction>& txs, const CTransaction& tx)
{
    const uint256 hash = tx.GetHash();

    for (const CTransaction& candidate : txs) {
        if (candidate.GetHash() == hash) return true;
    }

    return false;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(miner_block_assembly_tests,
                      *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

//!
//! The fixture's own precondition: a real chain, and premine coins that the
//! transaction index can resolve. Everything below is meaningless without it,
//! so it is asserted separately rather than left implicit in a failure
//! elsewhere.
//!
BOOST_AUTO_TEST_CASE(the_fixture_provides_spendable_coins)
{
    LOCK(cs_main);

    BOOST_CHECK(pindexGenesisBlock != nullptr);
    BOOST_CHECK(pindexBest != nullptr);
    BOOST_CHECK_EQUAL(nBestHeight, 0);

    BOOST_CHECK_EQUAL(PremineCoinbase().vout.size(), 10u);
    BOOST_CHECK_EQUAL(SpendablePremineOutputs().size(), 10u);
}

//!
//! The loop executes. This is the assertion that has never held: with a mock
//! chain the candidates cannot resolve their inputs, so they are diverted into
//! vOrphan and vecPriority is empty when std::make_heap runs.
//!
BOOST_AUTO_TEST_CASE(mempool_transactions_are_selected_into_the_block)
{
    mempool.clear();

    CAmount fee_a = 0, fee_b = 0, fee_c = 0;
    const CTransaction a = MakeCandidate(0, 1, 900, fee_a);
    const CTransaction b = MakeCandidate(1, 1, 800, fee_b);
    const CTransaction c = MakeCandidate(2, 1, 700, fee_c);

    AddToMempool(a, fee_a);
    AddToMempool(b, fee_b);
    AddToMempool(c, fee_c);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_CHECK_EQUAL(selected.size(), 3u);
    BOOST_CHECK(Contains(selected, a));
    BOOST_CHECK(Contains(selected, b));
    BOOST_CHECK(Contains(selected, c));

    mempool.clear();
}

//!
//! Ordering is by fee RATE, not absolute fee.
//!
//! The heap is keyed on dFeePerKb (miner.cpp), so a large transaction paying
//! the largest absolute fee in the block still sorts below smaller ones paying
//! a higher rate. Gridcoin's relay schedule is (1 + bytes/1000) * 0.001 GRC, so
//! the effective rate FALLS with size and the two orders genuinely differ.
//!
//! The rate relationship is asserted on the built transactions before the block
//! is assembled: if a size or fee change ever inverted it, this would fail as a
//! bad setup rather than silently testing nothing.
//!
BOOST_AUTO_TEST_CASE(selection_order_is_by_fee_rate_not_absolute_fee)
{
    mempool.clear();

    CAmount small_fee = 0, big_fee = 0;
    const CTransaction small = MakeCandidate(0, 1, 2000, small_fee);
    const CTransaction big = MakeCandidate(1, 60, 300, big_fee);

    // The premise of the case: the big transaction pays MORE in total and LESS
    // per kilobyte.
    BOOST_REQUIRE_GT(big_fee, small_fee);
    BOOST_REQUIRE_GT(FeePerKb(small, small_fee), FeePerKb(big, big_fee));

    // Added in the "wrong" order, so a loop that merely preserved insertion
    // order would fail.
    AddToMempool(big, big_fee);
    AddToMempool(small, small_fee);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_REQUIRE_EQUAL(selected.size(), 2u);
    BOOST_CHECK_EQUAL(selected[0].GetHash().ToString(), small.GetHash().ToString());
    BOOST_CHECK_EQUAL(selected[1].GetHash().ToString(), big.GetHash().ToString());

    mempool.clear();
}

//!
//! The fees the loop accumulates reach the coinbase, and they agree with what
//! the mempool entries already recorded.
//!
//! The agreement is worth pinning because the loop recomputes everything: it
//! re-reads the inputs through FetchInputs and recomputes the fee with
//! GetValueIn, ignoring CTxMemPoolEntry::GetFee() entirely. The two are
//! independent paths to the same number.
//!
BOOST_AUTO_TEST_CASE(accumulated_fees_reach_the_coinbase)
{
    mempool.clear();

    CAmount fee_a = 0, fee_b = 0;
    const CTransaction a = MakeCandidate(0, 1, 700, fee_a);
    const CTransaction b = MakeCandidate(1, 4, 700, fee_b);

    AddToMempool(a, fee_a);
    AddToMempool(b, fee_b);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_REQUIRE_EQUAL(selected.size(), 2u);
    BOOST_CHECK_EQUAL(fees, fee_a + fee_b);

    mempool.clear();
}


//!
//! An oversized candidate is SKIPPED, not treated as the end of the block.
//!
//! The guard is a continue, and the difference is observable only when the
//! oversized transaction sorts FIRST: it pays the higher rate here, so a break
//! would end selection before the affordable one is ever popped. Asserting that
//! the small transaction is still in the block is therefore the assertion that
//! distinguishes the two.
//!
//! No -blockmaxsize override is used. Setting it would mean restoring it for
//! every later suite in this binary, and ForceSetArg to the empty string does
//! not restore the default -- it parses as 0, which the miner then clamps to
//! 1000. The transaction is simply built past the real 250000 default instead.
//!
BOOST_AUTO_TEST_CASE(an_oversized_transaction_is_skipped_and_selection_continues)
{
    mempool.clear();

    // A P2PKH output serializes to 34 bytes, so this lands around 251 KB.
    CAmount big_fee = 0, small_fee = 0;
    const CTransaction oversized = MakeCandidate(0, 7400, 800, big_fee);
    const CTransaction small = MakeCandidate(1, 1, 700, small_fee);

    BOOST_REQUIRE_GE(TxSize(oversized), (unsigned int)(MAX_BLOCK_SIZE_GEN / 2));
    BOOST_REQUIRE_GT(FeePerKb(oversized, big_fee), FeePerKb(small, small_fee));

    AddToMempool(oversized, big_fee);
    AddToMempool(small, small_fee);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_CHECK(!Contains(selected, oversized));
    BOOST_CHECK(Contains(selected, small));
    BOOST_CHECK_EQUAL(fees, small_fee);

    mempool.clear();
}

//!
//! The legacy sigop guard skips its candidate and selection continues.
//!
//! Reaching it needs raw scripts rather than more outputs. Legacy sigops for a
//! P2PKH spend equal the output count, and the size guard runs first at a
//! default -blockmaxsize of 250000, which caps the reachable count near 7350 --
//! well under MAX_BLOCK_SIGOPS. A bare OP_CHECKMULTISIG counts 20 in
//! non-accurate mode, so a kilobyte of them carries about 19900, and the
//! running total starts at 100.
//!
BOOST_AUTO_TEST_CASE(a_sigop_heavy_transaction_is_skipped_and_selection_continues)
{
    mempool.clear();

    CScript sigop_script;
    for (int i = 0; i < 995; ++i) sigop_script << OP_CHECKMULTISIG;

    CAmount heavy_fee = 0, small_fee = 0;
    const CTransaction heavy = MakeCandidateToScript(0, sigop_script, 1, 800, heavy_fee);
    const CTransaction small = MakeCandidate(1, 1, 700, small_fee);

    BOOST_REQUIRE_GE(GetLegacySigOpCount(heavy) + 100u, (unsigned int)MAX_BLOCK_SIGOPS);
    BOOST_REQUIRE_LT(TxSize(heavy), (unsigned int)(MAX_BLOCK_SIZE_GEN / 2));
    BOOST_REQUIRE_GT(FeePerKb(heavy, heavy_fee), FeePerKb(small, small_fee));

    AddToMempool(heavy, heavy_fee);
    AddToMempool(small, small_fee);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_CHECK(!Contains(selected, heavy));
    BOOST_CHECK(Contains(selected, small));

    mempool.clear();
}

//!
//! A candidate stamped after the block it would go into is skipped.
//!
BOOST_AUTO_TEST_CASE(a_transaction_newer_than_the_block_is_skipped)
{
    mempool.clear();

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);

    // MakeBlockAndCoinstake stamps the block at FixtureTxTime() + 1000.
    const CTransaction late =
        CreateSpend(PremineCoinbase(), coins[0].n, 200000, 1, grc_test::FixtureTxTime() + 5000);
    const CTransaction ontime = CreateSpend(PremineCoinbase(), coins[1].n, 150000, 1);

    BOOST_REQUIRE_GT(FeePerKb(late, 200000), FeePerKb(ontime, 150000));

    AddToMempool(late, 200000);
    AddToMempool(ontime, 150000);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_CHECK(!Contains(selected, late));
    BOOST_CHECK(Contains(selected, ontime));

    mempool.clear();
}

//!
//! A coinstake sitting in the mempool is never selected.
//!
//! The candidate pays a POSITIVE fee above the miner's flat floor, which is
//! what makes the case discriminate: delete the IsCoinStake() term from the
//! enumeration guard and this transaction really does land in the block,
//! because ConnectInputs skips the value and fee tally for coinstakes. A
//! realistic minting coinstake would be excluded by the fee handler either way
//! and would prove nothing.
//!
//! Its coinbase sibling is deliberately not tested. IsCoinBase() implies a null
//! prevout, so with the guard removed the input cannot be fetched, the
//! candidate is dropped as missing inputs before it ever reaches the priority
//! heap, and no fixture state can change that -- the coinbase half of the guard
//! has no reachable behaviour to pin.
//!
namespace {

//! Registers the wallet with the validation signals for one case. The
//! signals are inert in this binary until a scheduler is registered
//! (chain_setup.cpp explains why); every signal used here is invoked
//! synchronously, so the scheduler never has to run.
struct SignalsForThisCase {
    CScheduler m_scheduler;
    SignalsForThisCase()
    {
        GetMainSignals().RegisterBackgroundSignalScheduler(m_scheduler);
        RegisterValidationInterface(pwalletMain);
    }
    ~SignalsForThisCase()
    {
        UnregisterValidationInterface(pwalletMain);
        GetMainSignals().UnregisterBackgroundSignalScheduler();
    }
};

//! Puts the -maxmempool cap back however the case exits.
struct MaxSizeRestorer {
    size_t m_saved{mempool.GetMaxSize()};
    ~MaxSizeRestorer() { mempool.SetMaxSize(m_saved); }
};

} // anonymous namespace

//!
//! A size-limit eviction inside AcceptToMemoryPool reaches the wallet.
//!
//! Lives in this suite because it needs the same chain: AcceptToMemoryPool
//! resolves inputs through the tx index, which only the fixture provides.
//! The validation signals are inert in this binary until a scheduler is
//! registered (chain_setup.cpp explains why), so the case registers one for
//! its own duration; every signal used here is invoked synchronously, so the
//! scheduler never has to run.
//!
//! Discriminates on the wallet's NotifyTransactionChanged for the EVICTED
//! transaction after the second accept. Without the emission in
//! AcceptToMemoryPool the wallet never hears about the eviction and no such
//! notification exists; depth alone would not do, since it is derived live
//! from mempool.exists() and reads -1 either way.
//!
BOOST_AUTO_TEST_CASE(a_size_limit_eviction_reaches_the_wallet)
{
    mempool.clear();

    const SignalsForThisCase signals;
    const MaxSizeRestorer max_size;

    std::vector<std::pair<uint256, ChangeType>> seen;
    boost::signals2::scoped_connection watch = pwalletMain->NotifyTransactionChanged.connect(
        [&seen](CWallet*, const uint256& hash, ChangeType status) { seen.emplace_back(hash, status); });

    CAmount fee_first = 0, fee_second = 0;
    CTransaction first = MakeCandidate(0, 1, 2000, fee_first);
    CTransaction second = MakeCandidate(1, 1, 2000, fee_second);
    const uint256 first_hash = first.GetHash();

    LOCK(cs_main);

    CValidationState state_first;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, first, state_first, nullptr),
                          "first spend rejected: " + state_first.GetRejectReason());
    BOOST_REQUIRE(mempool.exists(first_hash));
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE_MESSAGE(pwalletMain->mapWallet.count(first_hash),
                              "the wallet did not pick up its own spend from the add signal");
    }

    // Room for exactly what is in the pool now, so the next accept has to evict.
    mempool.SetMaxSize(mempool.DynamicMemoryUsage());
    const size_t seen_before = seen.size();

    CValidationState state_second;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, second, state_second, nullptr),
                          "second spend rejected: " + state_second.GetRejectReason());
    BOOST_REQUIRE(mempool.exists(second.GetHash()));
    BOOST_REQUIRE_MESSAGE(!mempool.exists(first_hash), "the size limit did not evict the first spend");

    const auto evicted_notice = std::count_if(seen.begin() + seen_before, seen.end(),
        [&](const std::pair<uint256, ChangeType>& e) { return e.first == first_hash && e.second == CT_UPDATED; });
    BOOST_CHECK_MESSAGE(evicted_notice == 1,
        "expected exactly one CT_UPDATED for the evicted tx, saw " + std::to_string(evicted_notice));

    // Eviction is not a conflict: the state is untouched and depth reads -1 live.
    {
        LOCK(pwalletMain->cs_wallet);
        const CWalletTx& wtx = pwalletMain->mapWallet.at(first_hash);
        BOOST_CHECK(wtx.isInMempool());
        BOOST_CHECK(!wtx.isInactive());
        BOOST_CHECK_EQUAL(wtx.GetDepthInMainChain(), -1);
    }

    mempool.clear();

    // The spends are deterministic, so the next case builds the same two
    // transactions; leave it a wallet that has never seen them.
    pwalletMain->EraseFromWallet(first_hash);
    pwalletMain->EraseFromWallet(second.GetHash());
}

//!
//! An evicted own spend is the input ResendWalletTransactions exists for:
//! unconfirmed, no longer in the mempool, and not conflicted. The forced
//! resend must relay exactly that spend, and nothing while it is still pooled
//! or once it is marked inactive.
//!
//! Discriminates on the relayed count. While the spend is in the pool it reads
//! depth 0 and is skipped; after the eviction it reads -1 and is revalidated
//! against the tx index and relayed; marked conflicted it is refused by
//! RelayWalletTransaction. Had the eviction handler marked it conflicted, the
//! count after the eviction would be 0. The wallet must have nothing to
//! re-announce before the case starts, or the counts would not be this case's.
//!
BOOST_AUTO_TEST_CASE(a_size_limit_evicted_own_spend_is_rebroadcast)
{
    mempool.clear();

    const SignalsForThisCase signals;
    const MaxSizeRestorer max_size;

    CAmount fee_first = 0, fee_second = 0;
    CTransaction first = MakeCandidate(0, 1, 2000, fee_first);
    CTransaction second = MakeCandidate(1, 1, 2000, fee_second);
    const uint256 first_hash = first.GetHash();

    LOCK(cs_main);

    BOOST_REQUIRE_EQUAL(pwalletMain->ResendWalletTransactions(/*fForce=*/true), 0u);

    CValidationState state_first;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, first, state_first, nullptr),
                          "first spend rejected: " + state_first.GetRejectReason());
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE(pwalletMain->mapWallet.count(first_hash));
    }

    // Still in the pool: nothing to re-announce.
    BOOST_CHECK_EQUAL(pwalletMain->ResendWalletTransactions(/*fForce=*/true), 0u);

    mempool.SetMaxSize(mempool.DynamicMemoryUsage());
    CValidationState state_second;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, second, state_second, nullptr),
                          "second spend rejected: " + state_second.GetRejectReason());
    BOOST_REQUIRE_MESSAGE(!mempool.exists(first_hash), "the size limit did not evict the first spend");

    // Evicted: exactly the first spend is relayed; the second is still pooled.
    BOOST_CHECK_EQUAL(pwalletMain->ResendWalletTransactions(/*fForce=*/true), 1u);
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK(pwalletMain->mapWallet.count(first_hash));
    }

    // Conflicted: refused, whatever depth says.
    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->mapWallet.at(first_hash).SetTxState(TxStateInactive{false});
    }
    BOOST_CHECK_EQUAL(pwalletMain->ResendWalletTransactions(/*fForce=*/true), 0u);

    mempool.clear();
    pwalletMain->EraseFromWallet(first_hash);
    pwalletMain->EraseFromWallet(second.GetHash());
}

BOOST_AUTO_TEST_CASE(a_coinstake_in_the_mempool_is_never_selected)
{
    mempool.clear();

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);

    const CAmount coinstake_fee = 200000;
    const CTransaction coinstake =
        grc_test::CreateCoinstakeShaped(PremineCoinbase(), coins[0].n, coinstake_fee);
    const CTransaction ordinary = CreateSpend(PremineCoinbase(), coins[1].n, 150000, 1);

    // Above the flat GMF_BLOCK floor, so the fee handler is not what excludes it.
    BOOST_REQUIRE_GE(coinstake_fee, GetMinFee(coinstake));
    BOOST_REQUIRE(coinstake.IsCoinStake());

    AddToMempool(coinstake, coinstake_fee);
    AddToMempool(ordinary, 150000);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_CHECK(!Contains(selected, coinstake));
    BOOST_CHECK(Contains(selected, ordinary));
    BOOST_CHECK_EQUAL(fees, 150000);

    mempool.clear();
}

BOOST_AUTO_TEST_SUITE_END()
