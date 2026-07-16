// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/wallet_tx_source.h"

#include "main.h"
#include "node/ui_interface.h"
#include "wallet/wallet.h"
#include "wallet/wallet_event_queue.h"
#include "wallet/wallettxstore.h"

#include <boost/signals2/connection.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace interfaces {
namespace {

//! In-process WalletTxSource: owns the node-side windowed-table store and its
//! event queue, and subscribes the producer handlers that feed them from
//! CWallet's transaction signals. Constructing an instance is the "attach"
//! (worker started, producers wired); destroying it is the "detach" (producers
//! severed, worker joined) — so nothing runs when no GUI holds a source.
class WalletTxSourceImpl : public WalletTxSource
{
public:
    explicit WalletTxSourceImpl(CWallet* wallet)
        : m_wallet(wallet)
        , m_store(wallet, m_queue)
    {
        // Launch the store-worker before producers can fire, so it is ready to
        // drain the intake queue off the core locks (windowed-model PR2.5).
        m_store.start();
        subscribe();
    }

    ~WalletTxSourceImpl() override
    {
        // scoped_connection destructors sever the producer subscriptions; the
        // WalletTxStore destructor then stops and joins its worker thread.
        m_handlers.clear();
    }

    WalletTxSourceImpl(const WalletTxSourceImpl&) = delete;
    WalletTxSourceImpl& operator=(const WalletTxSourceImpl&) = delete;

    void registerView(int view_id, GRC::FilterSpec filter, int sort_column, int sort_order) override
    {
        m_store.registerView(view_id, std::move(filter), sort_column, sort_order);
    }

    void setViewSort(int view_id, int sort_column, int sort_order) override
    {
        m_store.setViewSort(view_id, sort_column, sort_order);
    }

    void setViewFilter(int view_id, GRC::FilterSpec filter) override
    {
        m_store.setViewFilter(view_id, std::move(filter));
    }

    void setViewLimit(int view_id, int limit) override { m_store.setViewLimit(view_id, limit); }

    GRC::RowsResult getRows(int view_id, int first, int count) override
    {
        return m_store.getRows(view_id, first, count);
    }

    GRC::RowsResult getAllRows(int view_id) override { return m_store.getAllRows(view_id); }

    int rowForKey(int view_id, const uint256& hash, int idx) override
    {
        return m_store.rowForKey(view_id, hash, idx);
    }

    GRC::WalletTxDetail getRowDetail(const uint256& hash, int idx) override
    {
        return m_store.getRowDetail(hash, idx);
    }

    std::vector<TransactionRecord> reloadAndSnapshot(bool limit_enabled, int64_t limit_time) override
    {
        return m_store.reloadAndSnapshot(limit_enabled, limit_time);
    }

    std::vector<GRC::WalletEvent> drainEvents(std::size_t max_batch) override
    {
        return m_queue.drain(max_batch);
    }

    void noteAddressBookChanged(const std::string& address, const std::string& label) override
    {
        m_store.enqueueAddressBookChange(address, label);
    }

private:
    void subscribe();

    //! Producer: a wallet transaction became visible / changed / was deleted.
    //! Runs on the emitting core thread under the locks it already holds;
    //! decomposes and status-stamps in place, then enqueues to the store worker
    //! (O(1)). Body hoisted unchanged from the former WalletModel free function.
    void onTransactionChanged(CWallet* wallet, const uint256& hash, ChangeType status)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet);

    //! Producer: the chain tip advanced (connect/disconnect/reorg). Pushes a
    //! ChainTipChanged marker and runs the bounded per-tip status refresh inline
    //! under cs_main. Body hoisted unchanged from the former WalletModel free
    //! function.
    void onBlocksChanged(bool syncing, int height, int64_t best_time, uint32_t target_bits)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    CWallet* const m_wallet;

    //! m_queue is declared BEFORE m_store: the store holds a WalletEventQueue&
    //! bound at construction, so the queue must be fully constructed first
    //! (member init order == declaration order).
    GRC::WalletEventQueue m_queue;
    GRC::WalletTxStore m_store;

    //! Retained producer subscriptions, severed on destruction (scoped_connection
    //! disconnects in its destructor — issue #3129).
    std::vector<boost::signals2::scoped_connection> m_handlers;
};

void WalletTxSourceImpl::subscribe()
{
    // The producer runs under the emitting thread's locks by design (it
    // decomposes transactions in place). The handlers are bound to this source
    // (not the WalletModel), so the source outlives the GUI models that drive
    // it. ~WalletTxSourceImpl clears m_handlers first, disconnecting the
    // subscriptions so no NEW emission is dispatched to a freed handler. This
    // does NOT block an emission already in flight on another core thread when
    // disconnect runs (boost::signals2's default mutex releases before the slot
    // body executes, and the binds carry no shared_ptr slot tracking); that
    // narrow window is bounded only by shutdown ordering — the same pre-existing
    // property the former WalletModel::unsubscribeFromCoreSignals() relied on.
    // Closing it fully (shared_ptr-tracked slots, or teardown gated on producer
    // quiescence) belongs to the attach/detach lifecycle work (Phase 1c-ii-c).
    m_handlers.emplace_back(m_wallet->NotifyTransactionChanged.connect(
        boost::bind(&WalletTxSourceImpl::onTransactionChanged, this,
                    boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3)));
    m_handlers.emplace_back(uiInterface.NotifyBlocksChanged_connect(
        boost::bind(&WalletTxSourceImpl::onBlocksChanged, this,
                    boost::placeholders::_1, boost::placeholders::_2,
                    boost::placeholders::_3, boost::placeholders::_4)));
}

void WalletTxSourceImpl::onTransactionChanged(CWallet* wallet, const uint256& hash, ChangeType status)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet)
{
    LogPrint(BCLog::LogFlags::VERBOSE, "NotifyTransactionChanged %s status=%i", hash.GetHex(), status);

    // CT_NEW and CT_UPDATED are handled identically: look up the wtx, apply the
    // wtx-level visibility checks (orphan coinstake/coinbase / legacy OP_RETURN —
    // datetime filter is consumer-side), and enqueue either an insert/upsert with
    // decomposed records or a removal by hash. The wallet fires CT_UPDATED (not
    // CT_NEW) when a tx already in mapWallet is re-validated against a fresh chain
    // (e.g. an IBD after a chainstate wipe that retained wallet.dat), and when a
    // previously filtered-out tx becomes valid; the reverse (tx leaves visibility)
    // is covered by removal when showTransaction returns false.
    //
    // The CT_NEW / CT_UPDATED / CT_UPDATING branch needs BOTH cs_main (for
    // showTransaction -> IsInMainChain) and cs_wallet (for mapWallet + the
    // recursive IsMine in decomposeTransaction). All four such callsites hold
    // both locks (AddToWallet / CommitTransaction / WalletUpdateSpent, verified
    // by audit); the annotation lets the analyzer accept the AssertLockHeld()s
    // and the DEBUG_LOCKORDER builds enforce them at runtime. CT_DELETED callsites
    // hold neither lock (the tx is already erased) and touch only the hash, so the
    // annotation over-claims harmlessly for that path (the handler is reached only
    // through boost::signals2, which the analyzer cannot trace).
    switch (status) {
    case CT_NEW:
    case CT_UPDATED:
    case CT_UPDATING: {
        AssertLockHeld(cs_main);
        AssertLockHeld(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(hash);
        if (it == wallet->mapWallet.end()) {
            // Tx isn't in mapWallet — only happens if the notification raced
            // with an erasure. Remove to keep the consumer in sync.
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "NotifyTransactionChanged: %s status=%d but tx not in mapWallet "
                     "— removing from store",
                     hash.GetHex(), status);
            m_store.enqueueRemove(hash);
            break;
        }
        const CWalletTx& wtx = it->second;

        bool visible = TransactionRecord::showTransaction(wtx, false, 0);

        // showTransaction() hides a generated (coinstake/coinbase) tx whose block
        // is not yet in the main chain. This handler runs synchronously inside
        // block connection — the wallet is notified of a block's transactions
        // before SetBestChain advances pindexBest — so the block being connected
        // and its own coinstake transiently read as orphan. Detect exactly that
        // window (a block directly on the current tip but not yet the tip) and
        // keep it visible; a genuine orphan has pprev != pindexBest and stays
        // hidden, so -showorphans semantics are unchanged.
        if (!visible && (wtx.IsCoinStake() || wtx.IsCoinBase())) {
            auto bi = mapBlockIndex.find(wtx.hashBlock);
            if (bi != mapBlockIndex.end() && bi->second != nullptr
                    && !bi->second->IsInMainChain()
                    && bi->second->pprev == pindexBest) {
                LogPrint(BCLog::LogFlags::VERBOSE,
                         "NotifyTransactionChanged: %s is in the block being "
                         "connected — keeping visible despite transient orphan state",
                         hash.GetHex());
                visible = true;
            }
        }

        if (visible) {
            // Decompose under the locks already held, compute per-row status
            // producer-side (updateStatus requires cs_main, held here), then
            // enqueue to the store-worker; the worker applies the datetime
            // cutoff, de-dupes, computes positions, maintains cursors and emits
            // events off the core locks. Status is computed here so the off-lock
            // cursors can filter/sort by it without re-touching the wallet.
            std::vector<TransactionRecord> recs =
                TransactionRecord::decomposeTransaction(wallet, wtx);
            if (!recs.empty()) {
                for (TransactionRecord& rec : recs) {
                    rec.updateStatus(wtx);
                    rec.populateDisplayLabel(*wallet);  // address-book label snapshot (PR4)
                }
                // CT_NEW is a fresh insert; CT_UPDATED / CT_UPDATING is an upsert
                // of an existing tx (e.g. a confirmation).
                if (status == CT_NEW) {
                    m_store.enqueueInsert(std::move(recs));
                } else {
                    m_store.enqueueUpsert(std::move(recs));
                }
            }
        } else {
            // Genuinely filtered out (a real orphan coinstake, or a legacy
            // non-IsFromMe OP_RETURN). Remove the rows if previously visible.
            m_store.enqueueRemove(hash);
        }
        break;
    }
    case CT_DELETED:
        m_store.enqueueRemove(hash);
        break;
    }
}

void WalletTxSourceImpl::onBlocksChanged(bool /*syncing*/, int height, int64_t best_time,
                                         uint32_t /*target_bits*/)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Fired under cs_main after every chain-tip advance, via the synchronous
    // validation-interface bridge (SetBestChain -> UpdatedBlockTip ->
    // UINotificationBridge -> uiInterface.NotifyBlocksChanged); the emission runs
    // on the calling thread, so cs_main is still held here. Push a lightweight
    // marker so the Qt-side drain re-runs the rate-limited balance recompute,
    // then refresh per-row confirmation/maturity status for the bounded
    // height-volatile set inline (we already hold cs_main,
    // so the store takes cs_wallet + cs_store in canonical order with no worker
    // involvement — the worker must stay cs_main/cs_wallet-free or it would
    // deadlock reloadAndSnapshot's park protocol).
    m_queue.push(GRC::ChainTipChangedPayload{height, best_time});
    m_store.applyChainTipRefresh();
}

} // namespace

std::unique_ptr<WalletTxSource> MakeWalletTxSource(CWallet* wallet)
{
    return std::make_unique<WalletTxSourceImpl>(wallet);
}

} // namespace interfaces
