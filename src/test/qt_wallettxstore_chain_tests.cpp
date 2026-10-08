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
#include "primitives/transaction.h"
#include "rpc/blockchain.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "tinyformat.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <interfaces/wallet_tx_filter.h>
#include <interfaces/wallet_tx_record.h>
#include <wallet/wallet_event_queue.h>
#include <wallet/wallettxstore.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <stdexcept>
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
        BOOST_REQUIRE(WITH_LOCK(pwalletMain->cs_wallet, return pwalletMain->AddKey(key)));
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

//! A mock main chain a test can extend (or shorten) one block at a time, moving pindexBest
//! AND nBestHeight together as SetBestChain does. updateStatus reads both: depth from
//! pindexBest, cur_num_blocks and finality from nBestHeight, so moving only one would make
//! every refresh look like a non-height change. The StateGuard member puts the tip globals,
//! nCoinbaseMaturity and the block index back when the case ends.
struct AdvancingChain
{
    grc_test::StateGuard guard;   // declared first: destroyed last, after the unlinking below
    std::vector<CBlockIndex*> blocks;
    std::vector<uint256> hashes;

    //! \param coinbase_maturity  nCoinbaseMaturity for the case; a coinstake matures at
    //!                           depth coinbase_maturity + 10 (CMerkleTx::GetBlocksToMaturity).
    explicit AdvancingChain(int coinbase_maturity)
    {
        LOCK(cs_main);
        nCoinbaseMaturity = coinbase_maturity;
        append(nCoinbaseMaturity + 50);
    }

    ~AdvancingChain()
    {
        LOCK(cs_main);
        for (CBlockIndex* b : blocks) {
            b->pnext = nullptr;
            b->pprev = nullptr;
        }
    }

    uint256 tipHash() const { return hashes.back(); }

    void extend()
    {
        LOCK(cs_main);
        append(blocks.back()->nHeight + 1);
    }

    //! Disconnect the tip (a one-block reorg back), as a DisconnectBlock + SetBestChain would.
    void disconnectTip()
    {
        LOCK(cs_main);
        CBlockIndex* old = blocks.back();
        blocks.pop_back();
        hashes.pop_back();
        old->pprev = nullptr;
        blocks.back()->pnext = nullptr;
        pindexBest = blocks.back();
        nBestHeight = pindexBest->nHeight;
    }

private:
    void append(int height) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        const uint256 hash = InsecureRand256();
        CBlockIndex* b = GRC::MockBlockIndex::InsertBlockIndex(hash);
        b->nHeight = height;
        if (!blocks.empty()) {
            b->pprev = blocks.back();
            blocks.back()->pnext = b;
        }
        blocks.push_back(b);
        hashes.push_back(hash);
        pindexBest = b;
        nBestHeight = height;
    }
};

//! A destination this wallet does NOT own, so a coinstake's side stake to it adds no record.
CTxDestination ForeignDest()
{
    CKey key;
    key.MakeNewKey(false);
    return CTxDestination(key.GetPubKey().GetID());
}

//! What one drain carried, per view.
struct ViewTally
{
    int inserts = 0;
    int changes = 0;
    std::vector<TransactionRecord> last;   //!< every record the view was sent, in order
};

std::map<int, ViewTally> Tally(WalletEventQueue& q)
{
    std::map<int, ViewTally> out;
    for (const GRC::WalletEvent& ev : q.drain()) {
        if (const auto* ins = std::get_if<GRC::RowsInsertedPayload>(&ev.payload)) {
            ViewTally& t = out[ins->viewId];
            ++t.inserts;
            t.last.insert(t.last.end(), ins->records.begin(), ins->records.end());
        } else if (const auto* chg = std::get_if<GRC::RowsChangedPayload>(&ev.payload)) {
            ViewTally& t = out[chg->viewId];
            ++t.changes;
            t.last.insert(t.last.end(), chg->records.begin(), chg->records.end());
        }
    }
    return out;
}

//! Drive the real per-tip refresh, as onBlocksChanged does, and let the worker settle.
void RefreshTip(WalletTxStore& store, WalletEventQueue& q)
{
    {
        LOCK(cs_main);
        store.applyChainTipRefresh();
    }
    settle(q);
}

int TipHeight() { return WITH_LOCK(cs_main, return nBestHeight); }

//! The truth: a fresh updateStatus of every part of `hash`, as of now, keyed by part idx.
std::map<int, TransactionStatus> FreshStatuses(const uint256& hash)
{
    std::map<int, TransactionStatus> out;
    for (const TransactionRecord& rec : producerRecordsFor(hash)) out[rec.idx] = rec.status;
    return out;
}

//! The last snapshot each (view, part) was sent.
using SentMap = std::map<std::pair<int, int>, TransactionStatus>;

//! Only the two production cursors (registerProductionViews) are tracked: the intake also
//! pushes a legacy VIEW_FULL insert stream, which no windowed view consumes and which the
//! per-tip refresh never drives.
void Absorb(SentMap& sent, const std::map<int, ViewTally>& tally)
{
    for (const auto& [view, t] : tally) {
        if (view != GRC::VIEW_DETAILED && view != GRC::VIEW_OVERVIEW) continue;
        for (const TransactionRecord& rec : t.last) sent[{view, rec.idx}] = rec.status;
    }
}

//! Equality over what the GUI RENDERS for a status. updateStatus leaves members its branch
//! does not assign at their previous values (a record that has turned Confirmed keeps its last
//! Immature matures_in), so a store record and a freshly built one can differ in a field that
//! is never displayed for that status; those are excluded.
bool DisplayEquals(const TransactionStatus& a, const TransactionStatus& b)
{
    if (a.status != b.status || a.depth != b.depth || a.countsForBalance != b.countsForBalance
        || a.sortKey != b.sortKey) {
        return false;
    }
    if (a.status == TransactionStatus::Immature && a.matures_in != b.matures_in) return false;
    if ((a.status == TransactionStatus::OpenUntilBlock || a.status == TransactionStatus::OpenUntilDate)
        && a.open_for != b.open_for) {
        return false;
    }
    return true;
}

//! What the GUI would show for each (view, part) -- the projection of what it was sent to the
//! current tip -- must equal a fresh updateStatus. Returns the number of mismatches.
int CheckProjection(const SentMap& sent, const std::map<int, TransactionStatus>& truth,
                    const std::string& where)
{
    int bad = 0;
    for (const auto& [key, status] : sent) {
        const auto it = truth.find(key.second);
        BOOST_REQUIRE(it != truth.end());
        if (!DisplayEquals(GRC::ProjectTxStatus(status, TipHeight()), it->second)) {
            ++bad;
            BOOST_ERROR(where << ": view " << key.first << " part " << key.second
                        << " shows status " << static_cast<int>(status.status) << " depth "
                        << GRC::ProjectTxStatus(status, TipHeight()).depth << "; truth is status "
                        << static_cast<int>(it->second.status) << " depth " << it->second.depth);
        }
    }
    return bad;
}

//! A transaction paying `mine` from an input this wallet does not own: an ordinary receive.
CTransaction MakeReceive(const CTxDestination& mine, int64_t value)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(InsecureRand256(), 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = value;
    mtx.vout[0].scriptPubKey = P2PKH(mine);
    return CTransaction(mtx);
}

//! Set up a store holding one freshly confirmed coinstake (depth 1, Immature) and drain the
//! insert. A staker's coinstake has one part for the stake return and one per side stake sent
//! elsewhere (decomposeTransaction), so `parts_out` receives the part count. Returns the
//! snapshot each (view, part) was sent.
SentMap InsertFreshCoinstake(WalletTxStore& store, WalletEventQueue& q, const CTransaction& cs,
                             std::size_t& parts_out)
{
    const std::vector<TransactionRecord> recs = producerRecordsFor(cs.GetHash());
    BOOST_REQUIRE(!recs.empty());
    for (const TransactionRecord& rec : recs) {
        BOOST_REQUIRE_EQUAL(static_cast<int>(rec.status.status), static_cast<int>(TransactionStatus::Immature));
        BOOST_REQUIRE_EQUAL(rec.status.depth, 1);
    }
    parts_out = recs.size();
    store.enqueueInsert(recs, /*block_known=*/true);
    settle(q);
    SentMap sent;
    Absorb(sent, Tally(q));
    BOOST_REQUIRE_EQUAL(sent.size(), 2 * recs.size());   // both views, every part
    return sent;
}

} // namespace

BOOST_AUTO_TEST_SUITE(qt_wallettxstore_chain_tests)

//! THE #3257 CASE. A coinstake arrives in the pre-tip window (NotAccepted, masked
//! in both views), the block is then connected, and the per-tip refresh must flip
//! it into both views. This drives the REAL applyChainTipRefresh against a real
//! CWallet — the path with no prior coverage — rather than simulating it with an
//! upsert.
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

    // block_known=true mirrors what WalletTxSourceImpl computes: this tx's
    // confirming block IS in mapBlockIndex (PendingTipChain put it there) but is not
    // yet the tip. That is the window in which the record must stay volatile so the
    // per-tip refresh below can ripen it -- the invariant this case exists to prove.
    store.enqueueInsert(recs, /*block_known=*/true);
    settle(q);

    // Masked in both production views while NotAccepted — the invariant that makes
    // the flip-in load-bearing.
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), 0u);
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_OVERVIEW), 0u);

    // Connect the block and drive the real per-tip refresh, as onBlocksChanged does.
    chain.connect();
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
        store.enqueueInsert(producerRecordsFor(cs.GetHash()), /*block_known=*/true);
    }
    settle(q);

    // All masked while NotAccepted.
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), 0u);

    chain.connect();

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

    // The refresh hands work to the intake worker, so wait for it the same way
    // the earlier assertions do. Without this the check below races the worker
    // and reads an empty view on a loaded machine -- observed as "0 != 40" while
    // the rest of the suite was running, and not reproducible in isolation.
    settle(q);

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

    store.enqueueInsert(producerRecordsFor(good_hash), /*block_known=*/true);
    settle(q);

    BOOST_CHECK_MESSAGE(store.rowForKey(GRC::VIEW_DETAILED, good_hash, 0) >= 0,
                        "the intake worker never resumed after prime() threw - "
                        "every later transaction would be invisible");

    EraseWalletTx(good_hash);
}

// ---- Height-only refreshes emit nothing (#3059) -------------------------------------
//
// The per-tip refresh re-snapshots every volatile record but sends a Change only when the
// fresh status is not the height projection of the previous one. The GUI derives depth and
// maturity progress from the pushed tip height. These drive the REAL applyChainTipRefresh
// over a mock chain advanced one block at a time.

//! A coinstake through maturity: silent on every height-only tip, exactly one Change at the
//! Immature -> Confirmed flip, and at EVERY tip the projection of the last snapshot each
//! view was sent equals a fresh updateStatus -- what the GUI shows is the truth.
BOOST_AUTO_TEST_CASE(heightOnlyRefreshesEmitNothingUntilTheMaturityFlip)
{
    AdvancingChain chain(/*coinbase_maturity=*/3);
    const int maturity_depth = nCoinbaseMaturity + 10;
    OwnedKey mine;
    const CTxDestination foreign_dest = ForeignDest();
    chain.extend();   // the confirming block, now the tip
    const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, foreign_dest, 1 * COIN);
    InjectConfirmedTx(cs, chain.tipHash());

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();
    std::size_t parts = 0;
    SentMap sent = InsertFreshCoinstake(store, q, cs, parts);

    int flip_tips = 0;
    for (int depth = 2; depth <= maturity_depth + 3; ++depth) {
        chain.extend();
        RefreshTip(store, q);
        std::map<int, ViewTally> t = Tally(q);
        const std::map<int, TransactionStatus> truth = FreshStatuses(cs.GetHash());
        BOOST_REQUIRE_EQUAL(truth.begin()->second.depth, depth);

        const int detailed = t[GRC::VIEW_DETAILED].changes + t[GRC::VIEW_DETAILED].inserts;
        const int overview = t[GRC::VIEW_OVERVIEW].changes + t[GRC::VIEW_OVERVIEW].inserts;
        if (depth < maturity_depth) {
            BOOST_CHECK_MESSAGE(detailed == 0 && overview == 0,
                                "height-only tip at depth " << depth << " emitted " << detailed
                                << " detailed / " << overview << " overview deltas");
        } else if (depth == maturity_depth) {
            for (const auto& [idx, st] : truth) {
                BOOST_CHECK_EQUAL(static_cast<int>(st.status), static_cast<int>(TransactionStatus::Confirmed));
            }
            BOOST_CHECK_EQUAL(t[GRC::VIEW_DETAILED].changes, static_cast<int>(parts));
            BOOST_CHECK_GE(overview, 1);
            ++flip_tips;
        } else {
            // Confirmed is terminal: the hash has left the volatile set; nothing more is sent.
            BOOST_CHECK_EQUAL(detailed + overview, 0);
        }

        Absorb(sent, t);
        CheckProjection(sent, truth, strprintf("depth %d", depth));
    }
    BOOST_CHECK_EQUAL(flip_tips, 1);

    EraseWalletTx(cs.GetHash());
}

//! An ordinary received transaction: a Change exactly where something other than height
//! moved -- depth 3, where IsTrusted() turns true for a tx not from this wallet and the
//! amount stops being bracketed (countsForBalance), and depth 10, Confirming -> Confirmed.
BOOST_AUTO_TEST_CASE(receivedTxEmitsOnlyAtTheTrustAndConfirmationFlips)
{
    AdvancingChain chain(/*coinbase_maturity=*/3);
    OwnedKey mine;
    chain.extend();
    const CTransaction rx = MakeReceive(mine.dest, 7 * COIN);
    InjectConfirmedTx(rx, chain.tipHash());

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    const std::vector<TransactionRecord> recs = producerRecordsFor(rx.GetHash());
    BOOST_REQUIRE_EQUAL(recs.size(), 1u);
    BOOST_REQUIRE_EQUAL(static_cast<int>(recs[0].status.status), static_cast<int>(TransactionStatus::Confirming));
    BOOST_REQUIRE(!recs[0].status.countsForBalance);
    store.enqueueInsert(recs, /*block_known=*/true);
    settle(q);
    Tally(q);

    std::vector<int> change_depths;
    for (int depth = 2; depth <= TransactionRecord::RecommendedNumConfirmations + 3; ++depth) {
        chain.extend();
        RefreshTip(store, q);
        std::map<int, ViewTally> t = Tally(q);
        if (t[GRC::VIEW_DETAILED].changes > 0) change_depths.push_back(depth);
    }
    const std::vector<int> want{3, TransactionRecord::RecommendedNumConfirmations};
    BOOST_CHECK_EQUAL_COLLECTIONS(change_depths.begin(), change_depths.end(), want.begin(), want.end());

    EraseWalletTx(rx.GetHash());
}

//! A tip that moves back is never a height projection: every volatile record is re-sent.
BOOST_AUTO_TEST_CASE(aTipThatMovesBackIsAlwaysSent)
{
    AdvancingChain chain(/*coinbase_maturity=*/3);
    OwnedKey mine;
    const CTxDestination foreign_dest = ForeignDest();
    chain.extend();
    const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, foreign_dest, 1 * COIN);
    InjectConfirmedTx(cs, chain.tipHash());

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();
    std::size_t parts = 0;
    SentMap sent = InsertFreshCoinstake(store, q, cs, parts);

    for (int i = 0; i < 4; ++i) {
        chain.extend();
        RefreshTip(store, q);
        BOOST_CHECK_EQUAL(Tally(q)[GRC::VIEW_DETAILED].changes, 0);
    }
    chain.disconnectTip();
    RefreshTip(store, q);
    std::map<int, ViewTally> t = Tally(q);
    BOOST_CHECK_EQUAL(t[GRC::VIEW_DETAILED].changes, static_cast<int>(parts));
    Absorb(sent, t);
    CheckProjection(sent, FreshStatuses(cs.GetHash()), "after the tip moved back");

    EraseWalletTx(cs.GetHash());
}

//! A refresh that throws must not let the gate strand a view. updateStatus writes in place
//! and reads the disk last, so a throw can leave a status in the store that was never sent;
//! a throw after it (cache recompute, or between two views' emissions) leaves the views
//! disagreeing. Either way the next tip must re-send the fresh status to EVERY view -- the
//! per-hash force marker -- and the tip after that must be quiet again.
BOOST_AUTO_TEST_CASE(aRefreshThatThrowsIsResentToEveryViewOnTheNextTip)
{
    using Stage = WalletTxStore::RefreshStage;
    for (const Stage stage : {Stage::AfterUpdateStatus, Stage::AfterRecompute, Stage::BeforeViewEmit}) {
        AdvancingChain chain(/*coinbase_maturity=*/3);
        const int maturity_depth = nCoinbaseMaturity + 10;
        OwnedKey mine;
        const CTxDestination foreign_dest = ForeignDest();
        chain.extend();
        const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, foreign_dest, 1 * COIN);
        InjectConfirmedTx(cs, chain.tipHash());

        WalletEventQueue q;
        WalletTxStore store(pwalletMain, q);
        registerProductionViews(store);
        store.start();
        std::size_t parts = 0;
        SentMap sent = InsertFreshCoinstake(store, q, cs, parts);

        // Height-only tips up to the one before the flip.
        for (int depth = 2; depth < maturity_depth; ++depth) {
            chain.extend();
            RefreshTip(store, q);
            Tally(q);
        }

        // The flip tip: inject one throw at `stage`. For BeforeViewEmit, throw on the SECOND
        // view, so the first view has already been sent the flip -- the views now disagree.
        int view_calls = 0;
        bool thrown = false;
        store.SetRefreshFaultHookForTests([&](Stage s, std::size_t, int) {
            if (thrown || s != stage) return;
            if (s == Stage::BeforeViewEmit && ++view_calls < 2) return;
            thrown = true;
            throw std::runtime_error("injected refresh failure");
        });
        chain.extend();
        RefreshTip(store, q);
        store.SetRefreshFaultHookForTests(nullptr);
        BOOST_REQUIRE_MESSAGE(thrown, "stage " << static_cast<int>(stage) << " was never reached");
        Absorb(sent, Tally(q));

        // The next tip: every (view, part) must end up holding the flipped status.
        chain.extend();
        RefreshTip(store, q);
        Absorb(sent, Tally(q));
        const std::map<int, TransactionStatus> truth = FreshStatuses(cs.GetHash());
        for (const auto& [idx, st] : truth) {
            BOOST_REQUIRE_EQUAL(static_cast<int>(st.status), static_cast<int>(TransactionStatus::Confirmed));
        }
        CheckProjection(sent, truth, strprintf("stage %d, tip after the throw", static_cast<int>(stage)));

        // And the tip after that is quiet: the marker was cleared and the record is terminal.
        chain.extend();
        RefreshTip(store, q);
        std::map<int, ViewTally> after = Tally(q);
        BOOST_CHECK_EQUAL(after[GRC::VIEW_DETAILED].changes + after[GRC::VIEW_OVERVIEW].changes, 0);

        EraseWalletTx(cs.GetHash());
    }
}

//! A transaction this wallet SENT is trusted from depth 1 (its own confirmation satisfies
//! AreDependenciesConfirmed), so countsForBalance never flips: the only Change is at depth 10.
BOOST_AUTO_TEST_CASE(sentTxEmitsOnlyAtTheConfirmationFlip)
{
    AdvancingChain chain(/*coinbase_maturity=*/3);
    OwnedKey mine;
    const CTxDestination elsewhere = ForeignDest();

    // Fund `mine` in the chain's first (deep) block, then spend that output elsewhere.
    CMutableTransaction fund_mtx;
    fund_mtx.vin.resize(1);
    fund_mtx.vin[0].prevout = COutPoint(InsecureRand256(), 0);
    fund_mtx.vout.resize(1);
    fund_mtx.vout[0].nValue = 20 * COIN;
    fund_mtx.vout[0].scriptPubKey = P2PKH(mine.dest);
    const CTransaction fund(fund_mtx);
    InjectConfirmedTx(fund, chain.hashes.front());

    chain.extend();
    CMutableTransaction send_mtx;
    send_mtx.vin.resize(1);
    send_mtx.vin[0].prevout = COutPoint(fund.GetHash(), 0);
    send_mtx.vout.resize(1);
    send_mtx.vout[0].nValue = 19 * COIN;
    send_mtx.vout[0].scriptPubKey = P2PKH(elsewhere);
    const CTransaction send(send_mtx);
    InjectConfirmedTx(send, chain.tipHash());

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    const std::vector<TransactionRecord> recs = producerRecordsFor(send.GetHash());
    BOOST_REQUIRE_EQUAL(recs.size(), 1u);
    BOOST_REQUIRE_EQUAL(static_cast<int>(recs[0].status.status), static_cast<int>(TransactionStatus::Confirming));
    BOOST_REQUIRE(recs[0].status.countsForBalance);
    store.enqueueInsert(recs, /*block_known=*/true);
    settle(q);
    Tally(q);

    std::vector<int> change_depths;
    for (int depth = 2; depth <= TransactionRecord::RecommendedNumConfirmations + 3; ++depth) {
        chain.extend();
        RefreshTip(store, q);
        if (Tally(q)[GRC::VIEW_DETAILED].changes > 0) change_depths.push_back(depth);
    }
    const std::vector<int> want{TransactionRecord::RecommendedNumConfirmations};
    BOOST_CHECK_EQUAL_COLLECTIONS(change_depths.begin(), change_depths.end(), want.begin(), want.end());

    EraseWalletTx(send.GetHash());
    EraseWalletTx(fund.GetHash());
}

//! The force marker lasts exactly one clean pass. A throw at a HEIGHT-ONLY tip (no flip, so
//! nothing would be sent anyway) must make the next tip re-send every part to every view even
//! though that tip is height-only too, and the tip after that must be gated again. Without
//! the clear, a hash that ever threw would lose the gate for good.
BOOST_AUTO_TEST_CASE(aForcedRefreshIsSentOnceThenTheGateReturns)
{
    using Stage = WalletTxStore::RefreshStage;
    AdvancingChain chain(/*coinbase_maturity=*/3);
    OwnedKey mine;
    const CTxDestination foreign_dest = ForeignDest();
    chain.extend();
    const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, foreign_dest, 1 * COIN);
    InjectConfirmedTx(cs, chain.tipHash());

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();
    std::size_t parts = 0;
    SentMap sent = InsertFreshCoinstake(store, q, cs, parts);

    for (int depth = 2; depth <= 4; ++depth) {
        chain.extend();
        RefreshTip(store, q);
        Absorb(sent, Tally(q));
    }

    // Depth 5, height-only: throw after the cache recompute of the first part.
    bool thrown = false;
    store.SetRefreshFaultHookForTests([&](Stage s, std::size_t, int) {
        if (thrown || s != Stage::AfterRecompute) return;
        thrown = true;
        throw std::runtime_error("injected refresh failure");
    });
    chain.extend();
    RefreshTip(store, q);
    store.SetRefreshFaultHookForTests(nullptr);
    BOOST_REQUIRE(thrown);
    std::map<int, ViewTally> at_throw = Tally(q);
    BOOST_CHECK_EQUAL(at_throw[GRC::VIEW_DETAILED].changes + at_throw[GRC::VIEW_OVERVIEW].changes, 0);
    Absorb(sent, at_throw);

    // Depth 6, height-only, but forced: every part re-sent to the detailed view.
    chain.extend();
    RefreshTip(store, q);
    std::map<int, ViewTally> forced = Tally(q);
    BOOST_CHECK_EQUAL(forced[GRC::VIEW_DETAILED].changes, static_cast<int>(parts));
    BOOST_CHECK_GE(forced[GRC::VIEW_OVERVIEW].changes + forced[GRC::VIEW_OVERVIEW].inserts, 1);
    Absorb(sent, forced);
    CheckProjection(sent, FreshStatuses(cs.GetHash()), "forced tip");

    // Depth 7: the marker was cleared by that clean pass; height-only is gated again.
    chain.extend();
    RefreshTip(store, q);
    std::map<int, ViewTally> after = Tally(q);
    BOOST_CHECK_EQUAL(after[GRC::VIEW_DETAILED].changes + after[GRC::VIEW_OVERVIEW].changes, 0);
    Absorb(sent, after);
    CheckProjection(sent, FreshStatuses(cs.GetHash()), "tip after the forced one");

    EraseWalletTx(cs.GetHash());
}

//! A throw AFTER a cursor moved a row but before its delta was queued cannot be repaired by a
//! forced resend: the cursor already holds the new slot and would emit only a Change. Here the
//! row is flipping INTO the views (NotAccepted -> Immature: the cursor emits an Insert), and the
//! refresh throws right after the first view's cursor moved it. Every view must be reset, and
//! hold the rows, so no consumer is left with a wrong row count.
BOOST_AUTO_TEST_CASE(aThrowAfterACursorMovedARowResetsEveryView)
{
    using Stage = WalletTxStore::RefreshStage;
    PendingTipChain chain;
    OwnedKey mine;
    const CTransaction cs = MakeCoinstake(mine.dest, 50 * COIN, ForeignDest(), 1 * COIN);
    InjectConfirmedTx(cs, chain.hash_fresh);

    WalletEventQueue q;
    WalletTxStore store(pwalletMain, q);
    registerProductionViews(store);
    store.start();

    const std::vector<TransactionRecord> recs = producerRecordsFor(cs.GetHash());
    BOOST_REQUIRE(!recs.empty());
    BOOST_REQUIRE_EQUAL(static_cast<int>(recs[0].status.status), static_cast<int>(TransactionStatus::NotAccepted));
    store.enqueueInsert(recs, /*block_known=*/true);
    settle(q);
    q.drain();
    BOOST_REQUIRE_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), 0u);   // masked while NotAccepted

    chain.connect();
    bool thrown = false;
    store.SetRefreshFaultHookForTests([&](Stage s, std::size_t, int) {
        if (thrown || s != Stage::AfterCursorUpdate) return;
        thrown = true;
        throw std::runtime_error("injected refresh failure");
    });
    RefreshTip(store, q);
    store.SetRefreshFaultHookForTests(nullptr);
    BOOST_REQUIRE(thrown);

    std::set<int> reset_views;
    for (const GRC::WalletEvent& ev : q.drain()) {
        if (const auto* r = std::get_if<GRC::RowsResetPayload>(&ev.payload)) reset_views.insert(r->viewId);
    }
    BOOST_CHECK_MESSAGE(reset_views.count(GRC::VIEW_DETAILED) == 1, "the detailed view was not reset");
    BOOST_CHECK_MESSAGE(reset_views.count(GRC::VIEW_OVERVIEW) == 1, "the overview was not reset");
    // The throw hit the first part, so only it flipped in this tip; the parts after it were not
    // refreshed and stay masked. A consumer re-reading after the reset gets exactly that.
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), 1u);

    // The next refresh is forced (the hash is marked): the remaining parts flip in with their
    // own Inserts, and every view ends up holding every part.
    RefreshTip(store, q);
    std::map<int, ViewTally> next = Tally(q);
    BOOST_CHECK_EQUAL(next[GRC::VIEW_DETAILED].inserts, static_cast<int>(recs.size()) - 1);
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_DETAILED), recs.size());
    BOOST_CHECK_EQUAL(viewRowCount(store, GRC::VIEW_OVERVIEW), recs.size());

    EraseWalletTx(cs.GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
