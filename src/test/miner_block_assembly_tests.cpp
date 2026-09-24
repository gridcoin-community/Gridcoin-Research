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
//! byte. The miner applies two floors: the ABSOLUTE GetMinFee floor (nBytes = 0
//! is size-independent, a flat 100,000) and, since #3291, a fee-RATE floor
//! defaulting to the same 100,000 per KILOBYTE. The two coincide at exactly
//! 1000 bytes and diverge above it, and AddToMempool uses addUnchecked, which
//! bypasses the size-scaled relay floor that makes the rate floor inert for
//! real traffic. Funding by rate keeps every candidate above both floors.
//!

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "gridcoin/cpid.h"
#include "gridcoin/pool.h"
#include "gridcoin/mrc.h"
#include "init.h"
#include "miner.h"
#include "node/blockstorage.h"
#include "policy/fees.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "scheduler.h"
#include "script/script.h"
#include "gridcoin/tx_message.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "txmempool.h"
#include "validation.h"
#include "validationinterface.h"
#include "wallet/wallet.h"

#include <boost/signals2/connection.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using grc_test::AddToMempool;
using grc_test::CreateAndProcessBlock;
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
    // misses it is skipped (the selection loop continues past it since #3291),
    // so a case that funds too thinly would surface as a confusing assertion
    // on the block's contents; fail it here as bad setup instead.
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

// RegistryResetFor is here because the POOL cases below seed the process-global
// PoolRegistry. StateGuard deliberately does NOT restore registries (state_guard.h
// -- Reset() is destructive and some suites want it on entry only), and the
// suite-level leak detector compares registry SIZES, which those cases do not
// change: they modify an existing builtin seed rather than adding an entry. So the
// mutation is both unrestored and invisible, and the reset has to be asked for.
//
// WalletTxScope is the per-case fixture: the chain is shared, but every
// unconfirmed entry a case adds is erased at its exit, so no case hands a
// sibling a resend candidate and the cases hold in any order (--random
// shuffles them). It does not restore entries a case REMOVES -- the resend
// case's forced ResendWalletTransactions can erase pre-existing invalid
// entries -- so this is a no-leak guarantee for what cases add, not a
// restoration of the whole entry set (see chain_setup.h).
BOOST_FIXTURE_TEST_SUITE(miner_block_assembly_tests, grc_test::WalletTxScope, *boost::unit_test::fixture<grc_test::RegtestChainSetup>() *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::POOL_REGISTER>>())

//!
//! The fixture's own precondition: a real chain, and premine coins that the
//! transaction index can resolve. Everything below is meaningless without it,
//! so it is asserted separately rather than left implicit in a failure
//! elsewhere.
//!
//! Asserted as the invariant that holds for the fixture's whole lifetime, not
//! as the state at suite entry: the cases run in any order, and five of them
//! mine. The entry state (height 0, ten unspent outputs) is the fixture
//! constructor's own precondition, checked before the shuffle. What this case
//! pins is that the transaction index agrees with the chain: the premine
//! outputs it reports spendable are exactly those no transaction in a block on
//! the active chain has consumed. The two are independent sources -- vSpent in
//! the index against block contents read back from disk.
//!
BOOST_AUTO_TEST_CASE(the_fixture_provides_spendable_coins)
{
    LOCK(cs_main);

    BOOST_REQUIRE(pindexGenesisBlock != nullptr);
    BOOST_REQUIRE(pindexBest != nullptr);
    BOOST_CHECK_EQUAL(nBestHeight, pindexBest->nHeight);

    const CTransaction& coinbase = PremineCoinbase();
    BOOST_CHECK_EQUAL(coinbase.vout.size(), 10u);

    // Walk the active chain from the tip back to genesis, collecting every
    // premine output a block on it spends (a coinstake's kernel is an ordinary
    // input here). The walk is over pprev, never over mapBlockIndex: when this
    // fixture is the second one constructed in the binary, LoadBlockIndex
    // reloads the index records of blocks an earlier fixture committed and then
    // disconnected, and those side-chain spends were rightly undone in the
    // transaction index.
    std::set<uint32_t> spent_on_chain;
    const CBlockIndex* pindex = pindexBest;

    for (; pindex != nullptr && pindex != pindexGenesisBlock; pindex = pindex->pprev) {
        CBlock block;
        BOOST_REQUIRE_MESSAGE(ReadBlockFromDisk(block, pindex, Params().GetConsensus()),
                              "could not read block " << pindex->nHeight << " from disk");

        for (const CTransaction& tx : block.vtx) {
            for (const CTxIn& txin : tx.vin) {
                if (txin.prevout.hash == coinbase.GetHash()) {
                    spent_on_chain.insert(txin.prevout.n);
                }
            }
        }
    }

    BOOST_REQUIRE_MESSAGE(pindex == pindexGenesisBlock, "the tip does not descend from genesis");

    std::set<uint32_t> spendable;
    for (const COutPoint& out : SpendablePremineOutputs()) {
        BOOST_CHECK(out.hash == coinbase.GetHash());
        spendable.insert(out.n);
    }

    std::set<uint32_t> expected;
    for (uint32_t n = 0; n < coinbase.vout.size(); ++n) {
        if (!spent_on_chain.count(n)) expected.insert(n);
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(spendable.begin(), spendable.end(),
                                  expected.begin(), expected.end());
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
//!
//! A transaction carrying a MESSAGE contract is not selected once the disable
//! height is reached, and selection continues past it.
//!
//! This is not redundant with the CheckContracts gate. AcceptToMemoryPool
//! evaluates contracts at the height of the block a transaction would ENTER, so
//! one carrying a MESSAGE contract is admitted only while the tip is two blocks
//! or more below the gate -- but such a transaction can miss its last valid
//! block and still be pooled when the disable-height template is assembled.
//! Without the miner guard it is selected, and the staker loses the block to a
//! transaction its own ConnectBlock rejects.
//!
//! The candidate is funded ABOVE the on-time control so that fee-rate ordering
//! cannot be what excludes it: if the guard is deleted this transaction really
//! does land in the block, which is what makes the case discriminate rather than
//! merely pass.
//!
BOOST_AUTO_TEST_CASE(a_message_contract_transaction_is_skipped_at_the_disable_height)
{
    mempool.clear();
    grc_test::StateGuard guard;

    // The template is built at pindexBest->nHeight + 1 (miner.cpp), so pin the
    // gate exactly there rather than guessing a constant that fixture changes
    // could drift away from.
    int template_height = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        template_height = pindexBest->nHeight + 1;
    }
    gArgs.ForceSetArg("-messagecontractdisableheight", ToString(template_height));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);

    const CTransaction with_message = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[0].n, 200000,
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "stuffed"));
    const CTransaction ordinary = CreateSpend(PremineCoinbase(), coins[1].n, 150000, 1);

    BOOST_REQUIRE_GT(FeePerKb(with_message, 200000), FeePerKb(ordinary, 150000));

    AddToMempool(with_message, 200000);
    AddToMempool(ordinary, 150000);

    CAmount fees = 0;
    const std::vector<CTransaction> selected = AssembleBlock(fees);

    BOOST_CHECK_MESSAGE(!Contains(selected, with_message),
        "the miner put a MESSAGE-contract transaction in a template at the disable height");
    BOOST_CHECK_MESSAGE(Contains(selected, ordinary),
        "selection did not continue past the skipped transaction");

    mempool.clear();
}

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

//! The wallet entries a forced ResendWalletTransactions could relay right now:
//! unconfirmed, depth -1, and not inactive. Inactive entries are selected too
//! but RelayWalletTransaction refuses them, so they never reach the count; an
//! earlier suite in this binary (accounting_tests) leaves three of those in the
//! process wallet, and they are as invisible to the count as they always were.
//! Empty when nothing a sibling left could be counted as this case's.
std::string DescribeRelayableCandidates()
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string out;

    for (const auto& item : pwalletMain->mapWallet) {
        const CWalletTx& wtx = item.second;

        if (wtx.isConfirmed() || wtx.isInactive() || wtx.GetDepthInMainChain() != -1) continue;

        const char* state = wtx.state<TxStateInMempool>() ? "in-mempool tag, not pooled"
                                                         : "unrecognized";

        if (!out.empty()) out += ", ";
        out += item.first.GetHex() + " (" + state + ")";
    }

    return out;
}

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
//! The suite's per-case WalletTxScope keeps a sibling case's resend candidates
//! from leaking in, in any order; it says nothing about entries other suites
//! leave, which is why the entry check below names any relayable candidate it
//! finds rather than only counting it.
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

    // Taken BEFORE the call: a forced resend erases every candidate that fails
    // revalidation as a side effect, so a count alone cannot say what was there.
    const std::string leaked = DescribeRelayableCandidates();
    BOOST_REQUIRE_MESSAGE(leaked.empty(), "relayable candidates left by a sibling case: " << leaked);
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
}

BOOST_AUTO_TEST_CASE(a_size_limit_eviction_signals_the_victims_descendants_too)
{
    mempool.clear();

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
    } signals;

    struct MaxSizeRestorer {
        size_t m_saved{mempool.GetMaxSize()};
        ~MaxSizeRestorer() { mempool.SetMaxSize(m_saved); }
    } max_size;

    std::vector<std::pair<uint256, ChangeType>> seen;
    boost::signals2::scoped_connection watch = pwalletMain->NotifyTransactionChanged.connect(
        [&seen](CWallet*, const uint256& hash, ChangeType status) { seen.emplace_back(hash, status); });

    // The parent takes the LOWEST feerate in the pool so the trim selects it as
    // the primary victim; the child (spending the parent's in-pool output)
    // rides a higher rate and is erased only through remove()'s recursive
    // descendant branch -- the path whose signal this case pins.
    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);
    CTransaction parent = CreateSpend(PremineCoinbase(), coins[0].n, 100000, 1);
    CTransaction child = CreateSpend(parent, 0, 300000, 1);
    BOOST_REQUIRE_GE(CAmount{100000}, GetMinFee(parent));
    const uint256 parent_hash = parent.GetHash();
    const uint256 child_hash = child.GetHash();

    LOCK(cs_main);

    CValidationState state_parent, state_child;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, parent, state_parent, nullptr),
                          "parent rejected: " + state_parent.GetRejectReason());
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, child, state_child, nullptr),
                          "child rejected: " + state_child.GetRejectReason());
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE(pwalletMain->mapWallet.count(parent_hash));
        BOOST_REQUIRE(pwalletMain->mapWallet.count(child_hash));
    }

    // Room for exactly what is in the pool now, so the next accept has to evict.
    mempool.SetMaxSize(mempool.DynamicMemoryUsage());
    const size_t seen_before = seen.size();

    CAmount fee_third = 0;
    CTransaction third = MakeCandidate(1, 1, 2000, fee_third);
    CValidationState state_third;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, third, state_third, nullptr),
                          "third spend rejected: " + state_third.GetRejectReason());
    BOOST_REQUIRE_MESSAGE(!mempool.exists(parent_hash), "the size limit did not evict the parent");
    BOOST_REQUIRE_MESSAGE(!mempool.exists(child_hash),
                          "the recursive removal did not take the child with the parent");

    for (const uint256& hash : {parent_hash, child_hash}) {
        const auto notices = std::count_if(seen.begin() + seen_before, seen.end(),
            [&](const std::pair<uint256, ChangeType>& e) { return e.first == hash && e.second == CT_UPDATED; });
        BOOST_CHECK_MESSAGE(notices == 1,
            "expected exactly one CT_UPDATED for " + hash.GetHex() + ", saw " + std::to_string(notices));
    }

    mempool.clear();
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

//! Helper: is \p txid in the wallet and marked abandoned?
//!
//! Abandonment is the sweep's signature effect. Nothing else in a mined block
//! produces it, which is what makes it the discriminator between the two cases
//! below.
bool IsAbandonedInWallet(const uint256& txid)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    const auto it = pwalletMain->mapWallet.find(txid);
    if (it == pwalletMain->mapWallet.end()) return false;

    const auto* inactive = it->second.state<TxStateInactive>();
    return inactive != nullptr && inactive->m_abandoned;
}

//! Put \p tx in the wallet so the abandon half of the sweep has something to act
//! on. AddToMempool bypasses validation and the signals, so nothing tells the
//! wallet about it; add it directly, which is what accounting_tests does.
void PutInWallet(const CTransaction& tx)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    CWalletTx wtx(pwalletMain, tx);
    pwalletMain->AddToWallet(wtx, &walletdb);
}

//!
//! The activation sweep, end to end through real block connection.
//!
//! The txmempool cases cover the pieces in isolation -- the entry tag, the
//! counter, recursive remove() -- and would all still pass if the height
//! predicate in ReorganizeChain, the lookup loop, or the AbandonTransaction
//! integration stopped working. This is the case that executes that branch:
//! a MESSAGE transaction and a child of it are pooled, a real block is mined
//! that carries the chain across the configured height, and the sweep has to
//! take both out of the pool and release the wallet's inputs.
//!
BOOST_AUTO_TEST_CASE(a_pooled_message_transaction_is_swept_when_the_chain_crosses_the_height)
{
    mempool.clear();

    int mined_height = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        mined_height = pindexBest->nHeight + 1;
    }

    // ONE ABOVE the block about to be mined, which is what makes this case pin
    // the sweep's "+ 1". The sweep asks !IsMessageContractEnabled(pindex->
    // nHeight + 1), so connecting this block must fire it because the NEXT block
    // is the first that refuses MESSAGE; a predicate reading pindex->nHeight
    // would not fire here at all. An earlier version put the gate AT the mined
    // block, where BOTH predicates fire and the "+ 1" was therefore unpinned.
    //
    // ForcedArgGuard, not StateGuard: this case mines, and StateGuard would
    // restore nBestHeight and erase the block index before the suite fixture's
    // teardown can rewind the committed chain (see state_guard.h).
    grc_test::ForcedArgGuard gate("messagecontractdisableheight", ToString(mined_height + 1));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 1u);

    // The burn matters: without it CheckContracts rejects the transaction on the
    // fee rule and a block carrying it is refused, so the case would pass for the
    // wrong reason -- "not mined" would mean malformed, not gated.
    const GRC::Contract contract =
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "stuffed");

    // Stamped past the block being mined so the miner's own timestamp guard
    // (miner.cpp) leaves it out of the template. It is still perfectly VALID for
    // that block -- the gate is a block further out -- so it must survive mining
    // and then be removed by the sweep, not by confirmation. That is also the
    // real shape of the problem: a transaction admitted below the gate that
    // misses its last valid block. A real block is stamped from
    // GetAdjustedTime(), not the fixture constant, so "later" is clock-relative.
    const int64_t after_the_block = GetAdjustedTime() + 3600;

    const CTransaction message = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[0].n, 200000, contract, after_the_block,
        contract.RequiredBurnAmount());

    // A child of it, to pin the recursive half: it is just as unmineable once
    // the parent can never confirm, so leaving it pooled would keep its own
    // inputs locked for exactly the same reason.
    const CTransaction child = CreateSpend(message, 0, 50000, 1, after_the_block);

    AddToMempool(message, 200000);
    AddToMempool(child, 50000);
    PutInWallet(message);

    BOOST_REQUIRE(mempool.exists(message.GetHash()));
    BOOST_REQUIRE(mempool.exists(child.GetHash()));
    BOOST_REQUIRE(!IsAbandonedInWallet(message.GetHash()));

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CreateAndProcessBlock(block, err), "could not mine: " << err);

    // Kept out by the timestamp guard, not by the gate -- so anything below is
    // the sweep and cannot be ordinary confirmation.
    BOOST_REQUIRE_MESSAGE(!Contains(block.vtx, message),
        "the MESSAGE transaction was mined, so this case cannot distinguish the "
        "sweep from ordinary confirmation");

    BOOST_CHECK_MESSAGE(!mempool.exists(message.GetHash()),
        "the MESSAGE transaction was left in the mempool after the chain reached "
        "the block before the disable height");
    BOOST_CHECK_MESSAGE(!mempool.exists(child.GetHash()),
        "the child of the MESSAGE transaction was left in the mempool");
    BOOST_CHECK_MESSAGE(IsAbandonedInWallet(message.GetHash()),
        "the wallet transaction was not abandoned, so its inputs stay locked");

    mempool.clear();
}

//!
//! The control. Same transaction, same mining, gate out of reach: nothing is
//! swept and nothing is abandoned.
//!
//! Without this the case above would pass just as well for a sweep that fired
//! unconditionally -- which would retire MESSAGE transactions on every block
//! from the day this merges.
//!
BOOST_AUTO_TEST_CASE(a_pooled_message_transaction_is_untouched_below_the_height)
{
    mempool.clear();

    int mined_height = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        mined_height = pindexBest->nHeight + 1;
    }

    // Identical to the case above except the gate sits one block FURTHER out, so
    // the block being connected is NOT the one before the disable height and the
    // sweep must not fire. That is what forbids a sweep running a block early;
    // with the pair, the predicate is pinned from both sides.
    grc_test::ForcedArgGuard gate("messagecontractdisableheight", ToString(mined_height + 2));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);

    const int64_t after_the_block = GetAdjustedTime() + 3600;

    // A DIFFERENT premine output from the sweep case's, so the two transactions
    // never share a txid. WalletTxScope erases the sweep's abandoned entry at
    // its exit, so nothing depends on this any more; it keeps the two cases'
    // transactions distinguishable in a log.
    const GRC::Contract contract =
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "stuffed");

    const CTransaction message = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[1].n, 200000, contract, after_the_block,
        contract.RequiredBurnAmount());

    AddToMempool(message, 200000);
    PutInWallet(message);

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CreateAndProcessBlock(block, err), "could not mine: " << err);

    BOOST_REQUIRE_MESSAGE(!Contains(block.vtx, message),
        "the MESSAGE transaction was mined, so this control proves nothing about "
        "the sweep");

    // Abandonment, not pool membership, is what this asserts on. The sweep's
    // signature effect is AbandonTransaction, so an early fire is caught here
    // exactly. Pool membership is NOT safe to require: the coinstake draws from
    // the same premine outputs these fixtures spend, so the block's
    // removeConflicts() can evict this transaction for spending an outpoint the
    // kernel just took -- and which output the kernel takes depends on the
    // 16-second slot the case happens to run in. Requiring it pooled made this
    // case fail on a slower CI leg while passing here every time.
    BOOST_CHECK_MESSAGE(!IsAbandonedInWallet(message.GetHash()),
        "a MESSAGE transaction was abandoned a block before the disable height");

    mempool.clear();
}

//!
//! AcceptToMemoryPool evaluates the contract gates at the height of the block
//! the transaction would ENTER, not the tip.
//!
//! This is the one height where the two conventions disagree, so it is the only
//! place the difference can be observed: the gate is pinned at the tip's NEXT
//! height, which leaves the tip one below it. Reading the tip, the pool would
//! admit a transaction that the very next block rejects -- and the pool has no
//! way to retire it afterwards, so its inputs stay locked until a restart.
//!
//! Paired with the control below, which moves the gate one block further out and
//! requires the SAME transaction to be accepted. Without that, this case would
//! pass just as well for a transaction rejected on the fee rule, a missing burn,
//! or anything else AcceptToMemoryPool checks.
//!
BOOST_AUTO_TEST_CASE(message_contract_acceptance_is_evaluated_at_the_next_block)
{
    mempool.clear();
    grc_test::StateGuard guard;

    int next_height = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        next_height = pindexBest->nHeight + 1;
    }

    gArgs.ForceSetArg("-messagecontractdisableheight", ToString(next_height));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 1u);

    const GRC::Contract contract =
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "stuffed");
    CTransaction message = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[0].n, 200000, contract, /*tx_time=*/0,
        contract.RequiredBurnAmount());

    CValidationState state;
    LOCK(cs_main);

    BOOST_CHECK_MESSAGE(!AcceptToMemoryPool(mempool, message, state, nullptr),
        "the pool admitted a MESSAGE transaction that the next block rejects");
    BOOST_CHECK(!mempool.exists(message.GetHash()));

    mempool.clear();
}

//! The control: the same transaction, with the gate one block further out so the
//! next block still accepts MESSAGE. It must be admitted -- which is what makes
//! the rejection above attributable to the height gate and nothing else.
BOOST_AUTO_TEST_CASE(message_contract_is_accepted_while_the_next_block_still_allows_it)
{
    mempool.clear();
    grc_test::StateGuard guard;

    int next_height = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        next_height = pindexBest->nHeight + 1;
    }

    gArgs.ForceSetArg("-messagecontractdisableheight", ToString(next_height + 1));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);

    // A different premine output, so this does not collide with the txid the
    // case above built and left rejected.
    const GRC::Contract contract =
        GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "stuffed");
    CTransaction message = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[1].n, 200000, contract, /*tx_time=*/0,
        contract.RequiredBurnAmount());

    CValidationState state;
    LOCK(cs_main);

    BOOST_CHECK_MESSAGE(AcceptToMemoryPool(mempool, message, state, nullptr),
        "the pool refused a MESSAGE transaction the next block still accepts");
    BOOST_CHECK(mempool.exists(message.GetHash()));

    mempool.clear();
}

//!
//! The POOL expiry sweep, end to end through real block connection.
//!
//! IsStrandedAtHeight and the POOL_REGISTER tag/counter are covered in their own
//! suites, but nothing there connects a block: a regression in this branch's
//! height, its lookup loop, the recursive removal or the wallet abandonment would
//! still pass. Same gap the MESSAGE sweep had, and the same fix.
//!
//! Retention is forced to 1 block so the boundary is reachable on a regtest chain
//! that is a couple of blocks old, and V15 is forced on because ValidateAtHeight
//! refuses every POOL contract below it. The transaction is stamped past the block
//! being mined so the miner's timestamp guard leaves it out of the template -- it
//! has to survive mining and be removed by the sweep, not by confirmation.
//!
BOOST_AUTO_TEST_CASE(a_pooled_pool_register_is_swept_when_its_authorization_expires)
{
    mempool.clear();
    grc_test::V15HeightGuard v15(0);
    grc_test::ForcedArgGuard retention("pendingpoolretention", "1");

    int authorized_at = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        authorized_at = pindexBest->nHeight;
    }

    // An unclaimed builtin slot carrying a Foundation OPEN authorization. With
    // retention 1 the authorization covers heights up to authorized_at + 1, so
    // the block about to be mined puts the NEXT block past it.
    const GRC::Cpid cpid =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);
    CKey operator_key;
    operator_key.MakeNewKey(false);

    uint256 prev_hash;
    {
        LOCK(cs_main);
        GRC::PoolRegistry& registry = GRC::GetPoolRegistry();
        GRC::Pool_ptr seed = registry.Try(cpid);
        BOOST_REQUIRE(seed);

        GRC::Pool entry = *seed;
        entry.m_authorized_operator_key = operator_key.GetPubKey();
        entry.m_authorization_height = authorized_at;
        registry.SeedForTests(entry);
        prev_hash = entry.m_hash;
    }

    GRC::PoolRegisterPayload payload(cpid, "grcpool.com", "https://grcpool.com/",
                                     operator_key.GetPubKey());
    BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, prev_hash));
    const GRC::Contract contract = GRC::MakeContract<GRC::PoolRegisterPayload>(
        GRC::ContractAction::ADD, std::move(payload));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 1u);

    const int64_t after_the_block = GetAdjustedTime() + 3600;
    const CTransaction pool_tx = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[0].n, 200000, contract, after_the_block,
        contract.RequiredBurnAmount());

    AddToMempool(pool_tx, 200000);
    PutInWallet(pool_tx);
    BOOST_REQUIRE(mempool.exists(pool_tx.GetHash()));
    BOOST_REQUIRE(!IsAbandonedInWallet(pool_tx.GetHash()));

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CreateAndProcessBlock(block, err), "could not mine: " << err);

    BOOST_REQUIRE_MESSAGE(!Contains(block.vtx, pool_tx),
        "the POOL transaction was mined, so this case cannot distinguish the sweep "
        "from ordinary confirmation");

    BOOST_CHECK_MESSAGE(!mempool.exists(pool_tx.GetHash()),
        "a POOL_REGISTER whose authorization expired was left in the mempool");
    BOOST_CHECK_MESSAGE(IsAbandonedInWallet(pool_tx.GetHash()),
        "the wallet transaction was not abandoned, so its inputs stay locked");

    mempool.clear();
}

//! The control: identical, with retention long enough that the authorization
//! still covers the next block. Nothing is swept and nothing is abandoned, which
//! is what forbids a sweep that retires POOL contracts unconditionally.
BOOST_AUTO_TEST_CASE(a_pooled_pool_register_is_untouched_while_its_authorization_holds)
{
    mempool.clear();
    grc_test::V15HeightGuard v15(0);
    grc_test::ForcedArgGuard retention("pendingpoolretention", "100000");

    int authorized_at = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        authorized_at = pindexBest->nHeight;
    }

    const GRC::Cpid cpid =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);
    CKey operator_key;
    operator_key.MakeNewKey(false);

    uint256 prev_hash;
    {
        LOCK(cs_main);
        GRC::PoolRegistry& registry = GRC::GetPoolRegistry();
        GRC::Pool_ptr seed = registry.Try(cpid);
        BOOST_REQUIRE(seed);

        GRC::Pool entry = *seed;
        entry.m_authorized_operator_key = operator_key.GetPubKey();
        entry.m_authorization_height = authorized_at;
        registry.SeedForTests(entry);
        prev_hash = entry.m_hash;
    }

    GRC::PoolRegisterPayload payload(cpid, "grcpool.com", "https://grcpool.com/",
                                     operator_key.GetPubKey());
    BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, prev_hash));
    const GRC::Contract contract = GRC::MakeContract<GRC::PoolRegisterPayload>(
        GRC::ContractAction::ADD, std::move(payload));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 2u);

    // A different premine output from the sweep case's, so the two never share a
    // txid; WalletTxScope erases the sweep's entry at its exit, so this is only
    // for distinguishability in a log.
    const int64_t after_the_block = GetAdjustedTime() + 3600;
    const CTransaction pool_tx = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[1].n, 200000, contract, after_the_block,
        contract.RequiredBurnAmount());

    AddToMempool(pool_tx, 200000);
    PutInWallet(pool_tx);

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CreateAndProcessBlock(block, err), "could not mine: " << err);
    BOOST_REQUIRE(!Contains(block.vtx, pool_tx));

    // As in the MESSAGE control: abandonment is the sweep's signature and the
    // safe thing to assert. Pool membership races the coinstake for premine
    // outputs (see above), so requiring it would make this case fail on timing
    // rather than on behaviour.
    BOOST_CHECK_MESSAGE(!IsAbandonedInWallet(pool_tx.GetHash()),
        "a POOL_REGISTER was abandoned while its authorization still held");

    mempool.clear();
}

//!
//! The stale-MRC removal, through real block connection.
//!
//! This path predates the branch and had no end-to-end coverage --
//! getstalemrcs_selects_by_anchor_block pins the pool helper, not the
//! ReorganizeChain integration that consumes it. It is added here because this
//! branch MOVED that removal to after the chain commit, and the argument for the
//! move being behaviour-preserving rests on hashBestChain not being assigned
//! until after the enclosing scope closes, so GetStaleMRCs sees the same
//! pre-connect tip at either position. That is worth a test rather than only an
//! argument.
//!
//! An MRC anchored to something other than the current head is stale and must be
//! removed when the block connects; the control anchors to the head and must
//! survive.
//!
//! Both are built WITHOUT inputs, which matters. An earlier version funded them
//! from premine outputs, and the coinstake draws from those same outputs -- so
//! the block's removeConflicts() could take the control transaction out of the
//! pool for spending an outpoint the coinstake had just consumed. Which output
//! the kernel picks depends on timestamps, so that race resolved differently per
//! environment: it passed here every run and failed on the Debian CI leg. A
//! contract-only transaction has no outpoints to conflict on and nothing the
//! miner wants, so the only thing that can remove it is the sweep under test.
//!
BOOST_AUTO_TEST_CASE(a_stale_mrc_is_removed_when_a_block_connects)
{
    mempool.clear();

    uint256 head;
    {
        LOCK(cs_main);
        head = hashBestChain;
    }

    // Contract-only, no inputs and no outputs: nothing for removeConflicts() to
    // match and nothing for the miner to select. AddToMempool bypasses
    // acceptance, which is what makes that possible.
    auto make_mrc_tx = [](const uint256& anchor) {
        GRC::MRC mrc;
        mrc.m_mining_id = GRC::Cpid::Parse(GetRandHash().ToString().substr(0, 32));
        mrc.m_fee = 0;
        mrc.m_last_block_hash = anchor;

        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.vContracts.emplace_back(
            GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, mrc));
        return CTransaction(mtx);
    };

    const CTransaction stale_tx = make_mrc_tx(GetRandHash());  // not the head -> stale
    const CTransaction current_tx = make_mrc_tx(head);          // the head      -> not stale

    AddToMempool(stale_tx, 200000);
    AddToMempool(current_tx, 200000);
    BOOST_REQUIRE(mempool.exists(stale_tx.GetHash()));
    BOOST_REQUIRE(mempool.exists(current_tx.GetHash()));

    // Both go in the wallet as well, which no earlier version of this case did:
    // the removal calls EraseFromWallet and signals CT_DELETED, and with nothing
    // in the wallet EraseFromWallet simply returned false every run, so that half
    // of the path was never exercised at all.
    PutInWallet(stale_tx);
    PutInWallet(current_tx);
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE(pwalletMain->mapWallet.count(stale_tx.GetHash()) == 1);
        BOOST_REQUIRE(pwalletMain->mapWallet.count(current_tx.GetHash()) == 1);
    }

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CreateAndProcessBlock(block, err), "could not mine: " << err);

    // Neither can be mined or conflicted, so mempool membership is now an exact
    // read of what the sweep did.
    BOOST_REQUIRE_MESSAGE(!Contains(block.vtx, stale_tx), "the stale MRC was mined");
    BOOST_REQUIRE_MESSAGE(!Contains(block.vtx, current_tx), "the control MRC was mined");

    BOOST_CHECK_MESSAGE(!mempool.exists(stale_tx.GetHash()),
        "a stale MRC survived block connection");
    BOOST_CHECK_MESSAGE(mempool.exists(current_tx.GetHash()),
        "an MRC anchored to the head was removed as stale");

    // The wallet half, which the removal does through EraseFromWallet.
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK_MESSAGE(pwalletMain->mapWallet.count(stale_tx.GetHash()) == 0,
            "a stale MRC was removed from the mempool but left in the wallet");
        BOOST_CHECK_MESSAGE(pwalletMain->mapWallet.count(current_tx.GetHash()) == 1,
            "an MRC anchored to the head was erased from the wallet");
    }

    mempool.clear();
}

BOOST_AUTO_TEST_SUITE_END()
