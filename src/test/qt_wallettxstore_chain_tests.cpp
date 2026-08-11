// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// GUI-OFF coverage for the WALLET-BACKED half of GRC::WalletTxStore — the half
// qt_wallettxstore_tests.cpp cannot reach, because every case there constructs
// the store with a nullptr wallet. That leaves applyChainTipRefresh() and
// prime(), the two functions the entire coinstake lifecycle rests on, with no
// coverage at all (#3257).
//
// Why that gap matters, and why simulating it is not good enough: with the
// default filters a freshly staked coinstake is INVISIBLE at insert. The wallet
// is notified of a block's transactions from inside ConnectBlock, before
// SetBestChain advances pindexBest, so the block holding the coinstake is not
// yet in the main chain and TransactionRecord::updateStatus takes the Generated
// branch with !IsInMainChain() -> NotAccepted. GRC::Accepts masks
// Conflicted/NotAccepted in BOTH production views. The row's only route onto the
// screen is the later applyChainTipRefresh() -> Cursor::applyStatusUpdate()
// flip-in. That single path carries every stake row in the GUI.
//
// The sibling suite approximates that flip with enqueueUpsert() and a re-stamped
// record, which drives applyStatusUpdate() but bypasses applyChainTipRefresh()
// entirely — so it passes whether or not the real path works. These cases build
// a real CWallet transaction on a real (mock) chain and advance the tip, so the
// refresh runs for real.
//
// Note also that no -regtest test can cover this: CMerkleTx::GetBlocksToMaturity()
// short-circuits to 0 on a mockable chain (wallet.cpp), so a regtest stake is
// stamped Confirmed on arrival and never enters the NotAccepted window.

#include "key.h"
#include "key_io.h"
#include "main.h"
#include "primitives/transaction.h"
#include "rpc/blockchain.h"
#include "test/test_gridcoin.h"
#include "wallet/wallet.h"

#include <interfaces/wallet_tx_filter.h>
#include <interfaces/wallet_tx_record.h>
#include <wallet/wallet_event_queue.h>
#include <wallet/wallettxstore.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <thread>
#include <vector>

using GRC::WalletEventQueue;
using GRC::WalletTxStore;

extern CWallet* pwalletMain;

namespace {

//! Overview's cap: OverviewTxModel sets limit_rows = numItems * 3.
constexpr int kOverviewCap = 9;

//! Register the two cursors exactly as the Qt consumers do — DetailedTxModel a
//! default FilterSpec (show_orphans=false, limit_rows=-1) sorted Date DESC,
//! OverviewTxModel show_inactive=false plus a finite cap sorted Status DESC. Both
//! therefore mask Conflicted/NotAccepted.
void registerProductionViews(WalletTxStore& store)
{
    GRC::FilterSpec detail;
    store.registerView(GRC::VIEW_DETAILED, detail, GRC::TXCOL_DATE, GRC::TXSORT_DESC);

    GRC::FilterSpec overview;
    overview.show_inactive = false;
    overview.limit_rows = kOverviewCap;
    store.registerView(GRC::VIEW_OVERVIEW, overview, GRC::TXCOL_STATUS, GRC::TXSORT_DESC);
}

//! Wait until the intake worker stops producing, so a test can assert on the
//! ABSENCE of per-view events without racing it.
void settle(WalletEventQueue& q)
{
    std::size_t last = q.size();
    int stable = 0;
    for (int i = 0; i < 400 && stable < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const std::size_t now = q.size();
        stable = (now == last) ? stable + 1 : 0;
        last = now;
    }
}

//! A mock chain holding a block that is BUILT but not yet CONNECTED — exactly the
//! window applyChainTipRefresh exists to close.
//!
//! `fresh->pprev == prev` and `pindexBest == prev`, but `prev->pnext` is left null,
//! which is what CBlockIndex::IsInMainChain() tests. So a transaction confirmed in
//! `fresh` reads as not-in-main-chain, precisely as it does inside ConnectBlock
//! before SetBestChain runs. connect() then does what node/chainman.cpp does after
//! ConnectBlock returns: links the block in and advances the tip.
struct PendingTipChain
{
    CBlockIndex* saved_best;
    uint256 hash_prev;
    uint256 hash_fresh;
    CBlockIndex* prev;
    CBlockIndex* fresh;

    PendingTipChain()
    {
        LOCK(cs_main);
        saved_best = pindexBest;
        hash_prev = InsecureRand256();
        hash_fresh = InsecureRand256();
        // InsertBlockIndex points phashBlock at the mapBlockIndex key, so teardown
        // is a plain erase — the index comes from BlockIndexPool and is never deleted.
        prev = GRC::MockBlockIndex::InsertBlockIndex(hash_prev);
        fresh = GRC::MockBlockIndex::InsertBlockIndex(hash_fresh);
        // High enough that a tx in `prev` is mature, so only `fresh` is immature.
        prev->nHeight = nCoinbaseMaturity + 50;
        fresh->nHeight = prev->nHeight + 1;
        fresh->pprev = prev;
        // prev->pnext deliberately NOT set yet: that is what makes `fresh` read as
        // not-in-main-chain and stamps the coinstake NotAccepted.
        pindexBest = prev;
    }

    void connect()
    {
        LOCK(cs_main);
        prev->pnext = fresh;
        pindexBest = fresh;
    }

    ~PendingTipChain()
    {
        LOCK(cs_main);
        pindexBest = saved_best;
        prev->pnext = nullptr;
        fresh->pprev = nullptr;
        mapBlockIndex.erase(hash_prev);
        mapBlockIndex.erase(hash_fresh);
    }
};

//! An owned key plus its destination — decomposeTransaction only emits a coinstake
//! part when wallet->IsMine(vout[1]) != ISMINE_NO.
struct OwnedKey
{
    CKey key;
    CTxDestination dest;

    OwnedKey()
    {
        key.MakeNewKey(false);
        BOOST_REQUIRE(pwalletMain->AddKey(key));
        dest = CTxDestination(key.GetPubKey().GetID());
    }
};

CScript P2PKH(const CTxDestination& dest)
{
    CScript script;
    script.SetDestination(dest);
    return script;
}

//! A coinstake as CTransaction::IsCoinStake() defines it: a non-null prevout, an
//! empty vout[0] marker, and at least two outputs. vout[1] is the stake return to
//! `mine` (what decomposeTransaction keys the Generated part off); any further
//! outputs are sidestakes.
CTransaction MakeCoinstake(const CTxDestination& mine, int64_t stake_value,
                           const CTxDestination& sidestake_to, int64_t sidestake_value)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(InsecureRand256(), 0);

    CTxOut empty;
    empty.SetEmpty();
    mtx.vout.push_back(empty);

    CTxOut stake_return;
    stake_return.nValue = stake_value;
    stake_return.scriptPubKey = P2PKH(mine);
    mtx.vout.push_back(stake_return);

    CTxOut sidestake;
    sidestake.nValue = sidestake_value;
    sidestake.scriptPubKey = P2PKH(sidestake_to);
    mtx.vout.push_back(sidestake);

    CTransaction tx(mtx);
    BOOST_REQUIRE(tx.IsCoinStake());
    return tx;
}

void InjectConfirmedTx(const CTransaction& tx, const uint256& block_hash)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    CWalletTx wtx(pwalletMain, tx);
    wtx.SetTxState(TxStateConfirmed{block_hash, 0});
    pwalletMain->mapWallet[tx.GetHash()] = wtx;
}

void EraseWalletTx(const uint256& hash)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->mapWallet.erase(hash);
}

//! Reproduce exactly what WalletTxSourceImpl::onTransactionChanged does on CT_NEW:
//! decompose under the core locks and stamp each part's status producer-side.
std::vector<TransactionRecord> producerRecordsFor(const uint256& hash)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    auto it = pwalletMain->mapWallet.find(hash);
    BOOST_REQUIRE(it != pwalletMain->mapWallet.end());
    const CWalletTx& wtx = it->second;

    std::vector<TransactionRecord> recs =
        TransactionRecord::decomposeTransaction(pwalletMain, wtx);
    for (TransactionRecord& rec : recs) {
        rec.updateStatus(wtx);
        rec.populateDisplayLabel(*pwalletMain);
    }
    return recs;
}

std::size_t viewRowCount(WalletTxStore& store, int viewId)
{
    return store.getRows(viewId, 0, -1).records.size();
}

//! A wallet transaction that cannot be processed without throwing.
//!
//! Funds two MAX_MONEY outputs to `mine` in a mature block (each individually in
//! range), then spends BOTH with one coinstake. CWallet::GetDebit sums per input
//! and range-checks the running total, so the second input takes it past
//! MAX_MONEY and it throws std::runtime_error. Both of the paths under test reach
//! that: decomposeTransaction calls GetDebit() directly, and updateStatus reaches
//! it via IsTrusted() -> IsFromMe() once the tx is at depth 1.
//!
//! \return the coinstake's hash. `funding_out` receives the funding tx hash so the
//! caller can erase both.
uint256 injectPoisonCoinstake(const CTxDestination& mine, const uint256& mature_block,
                              const uint256& tip_block, uint256& funding_out)
{
    CMutableTransaction fund_mtx;
    fund_mtx.vout.resize(2);
    for (int i = 0; i < 2; ++i) {
        fund_mtx.vout[i].nValue = MAX_MONEY;
        fund_mtx.vout[i].scriptPubKey = P2PKH(mine);
    }
    const CTransaction fund(fund_mtx);
    InjectConfirmedTx(fund, mature_block);
    funding_out = fund.GetHash();

    CMutableTransaction mtx;
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(fund.GetHash(), 0);
    mtx.vin[1].prevout = COutPoint(fund.GetHash(), 1);
    CTxOut empty;
    empty.SetEmpty();
    mtx.vout.push_back(empty);
    CTxOut ret;
    ret.nValue = 50 * COIN;
    ret.scriptPubKey = P2PKH(mine);
    mtx.vout.push_back(ret);
    mtx.vout.push_back(ret);

    const CTransaction poison(mtx);
    BOOST_REQUIRE(poison.IsCoinStake());
    InjectConfirmedTx(poison, tip_block);
    return poison.GetHash();
}

} // namespace

BOOST_AUTO_TEST_SUITE(qt_wallettxstore_chain_tests)

//! THE #3257 CASE. A coinstake arrives in the pre-tip window (NotAccepted, masked
//! in both views), the block is then connected, and the per-tip refresh must flip
//! it into both views. This drives the REAL applyChainTipRefresh against a real
//! CWallet — the path with no prior coverage — rather than simulating it with an
//! upsert.
//! Deliver what production delivers when the block confirming \p hash is connected.
//!
//! PendingTipChain::connect() only moves pindexBest — it is a mock chain, so it
//! runs none of ConnectBlock. Production connects a block through
//! ConnectBlock -> GetMainSignals().BlockConnected -> CWallet::BlockConnected ->
//! SyncTransaction -> NotifyTransactionChanged(hash, CT_UPDATED) ->
//! WalletTxSourceImpl::onTransactionChanged, which re-decomposes the tx, calls
//! updateStatus() under cs_main and enqueueUpserts the rows. producerRecordsFor()
//! is that same producer-side derivation, so this is the event, not a shortcut
//! around it.
//!
//! Delivering it matters because a record inserted BEFORE its block is connected
//! sits at depth -1, which recordStatusIsVolatile() correctly excludes from the
//! volatile set: nothing a new tip does can change such a record, only its own
//! block arriving can, and that arrival is this event. Skipping it here left the
//! rows permanently outside m_volatile and asserted that the per-tip poll must
//! discover the transition on its own — a contract production does not rely on,
//! and whose cost was O(blocks x wallet transactions) during a sync from zero.
void deliverBlockConnected(WalletTxStore& store, WalletEventQueue& q, const uint256& hash)
{
    store.enqueueUpsert(producerRecordsFor(hash));
    settle(q);
}

BOOST_AUTO_TEST_CASE(coinstakeFlipsIntoBothViewsOnRealChainTipRefresh)
{
    PendingTipChain chain;
    OwnedKey mine;
    OwnedKey foreign_dest;   // a distinct destination for the sidestake part

    const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, foreign_dest.dest, 1 * COIN);
    const uint256 cs_hash = cs.GetHash();
    InjectConfirmedTx(cs, chain.hash_fresh);

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    const std::vector<TransactionRecord> recs = producerRecordsFor(cs_hash);
    BOOST_REQUIRE(!recs.empty());

    // Precondition — the pre-tip window really does stamp NotAccepted. If this
    // ever stops holding, the rest of the case is vacuous, so assert it.
    BOOST_CHECK_EQUAL(static_cast<int>(recs[0].status.status),
                      static_cast<int>(TransactionStatus::NotAccepted));

    store.enqueueInsert(recs);
    settle(q);

    // Masked in both production views while NotAccepted — the invariant that makes
    // the flip-in load-bearing.
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), 0u);
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_OVERVIEW), 0u);

    // Connect the block, deliver the notification production emits for it, then
    // drive the per-tip refresh as onBlocksChanged does. Both happen in production
    // on this path: the upsert re-evaluates volatility with a real depth, and the
    // tip refresh then ripens the row while it stays inside the maturity window.
    chain.connect();
    deliverBlockConnected(store, q, cs_hash);
    {
        LOCK(cs_main);
        store.applyChainTipRefresh();
    }

    // The whole point of the issue: the rows must now be visible in BOTH views.
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), recs.size());
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_OVERVIEW), recs.size());

    EraseWalletTx(cs_hash);
}

//! prime() against a real wallet: the rebuild must pick the coinstake up and, once
//! the block is connected, present it in both views. This is the path that has been
//! masking the bug in the field — every stake a user sees may have arrived via a
//! restart's prime() rather than a live flip-in — so pin it explicitly.
BOOST_AUTO_TEST_CASE(primeRebuildsCoinstakeRowsFromTheWallet)
{
    PendingTipChain chain;
    OwnedKey mine;
    OwnedKey foreign_dest;

    const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, foreign_dest.dest, 1 * COIN);
    const uint256 cs_hash = cs.GetHash();
    InjectConfirmedTx(cs, chain.hash_fresh);

    // Connect first, so the rescan sees a settled, in-main-chain coinstake.
    chain.connect();

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    store.prime(false, 0);
    settle(q);

    const std::size_t parts = producerRecordsFor(cs_hash).size();
    BOOST_REQUIRE(parts > 0);
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), parts);

    EraseWalletTx(cs_hash);
}

//! One transaction that cannot be refreshed must not stop the refresh of every
//! other volatile transaction in the wallet.
//!
//! updateStatus() reaches IsTrusted() -> IsFromMe() -> GetDebit(), which throws
//! std::runtime_error once the summed debit leaves MoneyRange. At depth 1 — which
//! is exactly where a freshly staked block sits — that path is always taken, so a
//! wallet holding such a transaction throws on every single chain-tip refresh.
//!
//! Unguarded, that exception unwinds the whole m_volatile loop, so every hash
//! after it in iteration order is skipped. Those rows never leave NotAccepted,
//! both production views mask inactive rows, and the staked blocks behind the
//! poison record stay invisible until a prime() rebuilds the set — the #3257
//! fingerprint. m_volatile is an unordered_set, so position is by bucket; this
//! case seeds enough good transactions that some are guaranteed to sort behind
//! the poison one whatever the bucket layout.
BOOST_AUTO_TEST_CASE(oneUnrefreshableTxDoesNotFreezeTheRestOfTheWallet)
{
    constexpr int kGood = 20;

    PendingTipChain chain;
    OwnedKey mine;
    OwnedKey sidestake_dest;

    uint256 fund_hash;
    const uint256 poison_hash =
        injectPoisonCoinstake(mine.dest, chain.hash_prev, chain.hash_fresh, fund_hash);

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    // The poison record goes in hand-built: decomposeTransaction would itself throw
    // on this wallet tx, and what is under test is the REFRESH loop, not decompose.
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        TransactionRecord rec(poison_hash, pwalletMain->mapWallet[poison_hash].GetTxTime());
        rec.type = TransactionRecord::Generated;
        rec.idx = 0;
        rec.vout = 1;
        rec.status.status = TransactionStatus::NotAccepted;   // volatile, so it is refreshed
        store.enqueueInsert({rec});
    }

    std::vector<uint256> good_hashes;
    for (int i = 0; i < kGood; ++i) {
        const CTransaction cs =
            MakeCoinstake(mine.dest, (10 + i) * COIN, sidestake_dest.dest, 1 * COIN);
        InjectConfirmedTx(cs, chain.hash_fresh);
        good_hashes.push_back(cs.GetHash());
        store.enqueueInsert(producerRecordsFor(cs.GetHash()));
    }
    settle(q);

    // All masked while NotAccepted.
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), 0u);

    chain.connect();

    // Deliver the per-transaction notification production emits when the block is
    // connected (see deliverBlockConnected). The GOOD transactions only -- the
    // poison one is deliberately left to the per-tip refresh, because that is the
    // path whose try/catch this case exists to test. Note that in production the
    // poison's own CT_UPDATED would throw inside the notification handler too;
    // that is a separate gap in the event path, not something to encode here.
    for (const uint256& h : good_hashes) {
        deliverBlockConnected(store, q, h);
    }

    // Confirm the premise rather than assuming it: with the block connected the
    // poison tx sits at depth 1, so IsTrusted() takes the IsFromMe() -> GetDebit()
    // path and really does throw. Without this the whole case could pass vacuously.
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        const CWalletTx& pwtx = pwalletMain->mapWallet[poison_hash];
        TransactionRecord probe(poison_hash, pwtx.GetTxTime());
        probe.type = TransactionRecord::Generated;
        probe.vout = 1;
        BOOST_CHECK_THROW(probe.updateStatus(pwtx), std::runtime_error);
    }

    {
        LOCK(cs_main);
        store.applyChainTipRefresh();   // must not propagate
    }

    // Every good transaction must have flipped in, wherever the poison record
    // landed in the bucket order. Two parts each (stake return + sidestake).
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED),
                      static_cast<std::size_t>(kGood) * 2u);
    for (const uint256& h : good_hashes) {
        BOOST_CHECK_MESSAGE(store.rowForKey(GRC::VIEW_DETAILED, h, 0) >= 0,
                            "good coinstake " + h.GetHex() + " never reached the view");
    }

    // And the poison record is still masked — it threw, was skipped, and did not
    // silently succeed. This is what keeps the assertions above meaningful.
    BOOST_CHECK_EQUAL(store.rowForKey(GRC::VIEW_DETAILED, poison_hash, 0), -1);

    EraseWalletTx(poison_hash);
    EraseWalletTx(fund_hash);
    for (const uint256& h : good_hashes) EraseWalletTx(h);
}

//! prime()'s rescan can throw, and the intake worker must survive it.
//!
//! prime() quiesces the single intake worker (m_rebuilding = true) before
//! rebuilding the store from mapWallet, and the worker's wait predicate is that
//! flag. The rescan in between is not safe: decomposeTransaction calls GetCredit
//! and GetDebit, which throw std::runtime_error outside MoneyRange, and
//! updateStatus reaches GetGeneratedType, which hits the tx index and reads a
//! block from disk.
//!
//! If the flag is cleared by falling off the end of the function, a throw leaves
//! the worker parked FOREVER. Producers keep enqueuing successfully and m_intake
//! grows without bound, while the inline applyChainTipRefresh keeps ripening rows
//! already in the store — so the GUI shows existing transactions updating
//! normally and never shows a new one again. Releasing the park from an RAII
//! guard is what makes that unwind-safe, and this pins it (#3257).
BOOST_AUTO_TEST_CASE(primeThatThrowsMidRescanStillReleasesTheIntakeWorker)
{
    PendingTipChain chain;
    OwnedKey mine;
    OwnedKey sidestake_dest;

    uint256 fund_hash;
    const uint256 poison_hash =
        injectPoisonCoinstake(mine.dest, chain.hash_prev, chain.hash_fresh, fund_hash);
    chain.connect();

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    // Premise: the rescan really does throw on this wallet. prime() has no
    // try/catch by design — the guard exists to make the unwind safe, not to
    // swallow it — so the exception is expected to reach the caller.
    BOOST_CHECK_THROW(store.prime(false, 0), std::runtime_error);

    // The worker must have been released on the way out. Drop the poison so the
    // producer side is clean, then prove liveness the only way that counts: a new
    // transaction enqueued AFTER the failed prime still has to reach the view.
    EraseWalletTx(poison_hash);
    EraseWalletTx(fund_hash);

    const CTransaction good =
        MakeCoinstake(mine.dest, 42 * COIN, sidestake_dest.dest, 1 * COIN);
    const uint256 good_hash = good.GetHash();
    InjectConfirmedTx(good, chain.hash_fresh);

    store.enqueueInsert(producerRecordsFor(good_hash));
    settle(q);

    BOOST_CHECK_MESSAGE(store.rowForKey(GRC::VIEW_DETAILED, good_hash, 0) >= 0,
                        "the intake worker never resumed after prime() threw — "
                        "every later transaction would be invisible");

    EraseWalletTx(good_hash);
}

BOOST_AUTO_TEST_SUITE_END()
