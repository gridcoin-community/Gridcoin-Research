// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/wallet_coin_source.h"

#include "node/ui_interface.h"
#include "wallet/wallet.h"
#include "wallet/wallet_event_queue.h"
#include "wallet/walletcoinstore.h"

#include <boost/signals2/connection.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace interfaces {
namespace {

//! In-process WalletCoinSource: owns the node-side coin store and its event
//! queue, and subscribes the producer handlers that feed them from CWallet's
//! signals. Constructing is the attach (worker started, producers wired);
//! destroying is the detach — the WalletTxSourceImpl lifecycle, including the
//! weak_ptr-per-callback teardown discipline (see wallet_tx_source.cpp for
//! the full rationale; this class copies it verbatim).
class WalletCoinSourceImpl : public WalletCoinSource,
                             public std::enable_shared_from_this<WalletCoinSourceImpl>
{
public:
    explicit WalletCoinSourceImpl(CWallet* wallet)
        : m_wallet(wallet)
        , m_store(wallet, m_queue)
    {
        m_store.start();
    }

    //! Wire the producer subscriptions; called by MakeWalletCoinSource right
    //! after make_shared (weak_from_this() is only valid then).
    void subscribe();

    ~WalletCoinSourceImpl() override
    {
        m_handlers.clear();
    }

    WalletCoinSourceImpl(const WalletCoinSourceImpl&) = delete;
    WalletCoinSourceImpl& operator=(const WalletCoinSourceImpl&) = delete;

    void registerView(int view_id, GRC::CoinViewMode mode,
                      int sort_column, int sort_order) override
    {
        m_store.registerView(view_id, mode, sort_column, sort_order);
    }

    void unregisterView(int view_id) override { m_store.unregisterView(view_id); }

    void setViewMode(int view_id, GRC::CoinViewMode mode) override
    {
        m_store.setViewMode(view_id, mode);
    }

    void setViewSort(int view_id, int sort_column, int sort_order) override
    {
        m_store.setViewSort(view_id, sort_column, sort_order);
    }

    GRC::CoinRowsResult getRows(int view_id, int first, int count) override
    {
        return m_store.getRows(view_id, first, count);
    }

    GRC::CoinGroupsResult getGroups(int view_id, int first, int count) override
    {
        return m_store.getGroups(view_id, first, count);
    }

    GRC::CoinRowsResult getGroupRows(int view_id, const std::string& group_address,
                                     int first, int count) override
    {
        return m_store.getGroupRows(view_id, group_address, first, count);
    }

    std::vector<GRC::CoinGroupInfo> getGroupDirectory() override
    {
        return m_store.getGroupDirectory();
    }

    std::set<COutPoint> reconcileSelection(std::set<COutPoint> selection) override
    {
        return m_store.reconcileSelection(std::move(selection));
    }

    GRC::CoinSelectionUpdate setSelected(const COutPoint& outpoint, bool selected) override
    {
        return m_store.setSelected(outpoint, selected);
    }

    GRC::CoinBulkSelectionResult selectGroup(const std::string& group_address,
                                             bool selected) override
    {
        return m_store.selectGroup(group_address, selected);
    }

    GRC::CoinBulkSelectionResult selectAll(bool selected) override
    {
        return m_store.selectAll(selected);
    }

    GRC::CoinBulkSelectionResult applyValueFilter(bool less_or_equal, int64_t value,
                                                  uint32_t max_inputs) override
    {
        return m_store.applyValueFilter(less_or_equal, value, max_inputs);
    }

    GRC::CoinGroupsResult reloadAndSnapshot() override
    {
        return m_store.reloadAndSnapshot();
    }

    std::vector<GRC::WalletCoinEvent> drainEvents(std::size_t max_batch) override
    {
        return m_queue.drain(max_batch);
    }

    bool consumeNeedsResync() override { return m_store.consumeNeedsResync(); }

    void noteAddressBookChanged(const std::string& address, const std::string& label) override
    {
        // Two consequences, handled on two different cost tiers.
        //
        // The cheap, common one — the rendered label and the label sort key —
        // is an O(1) intake item the worker applies off this thread.
        m_store.enqueueAddressBookChange(address, label);

        // The expensive one: labeling an own address flips IsChange for its
        // outputs, so change chains re-walk to a different ancestor and coins
        // regroup across the whole store. Deciding that precisely means
        // re-decomposing every wallet transaction — O(wallet) under
        // cs_main + cs_wallet — and this runs on the GUI thread, where that
        // would freeze the UI and stall block processing behind cs_main on a
        // large wallet. Flag the resync instead: the drain path polls
        // consumeNeedsResync() and reloads on the one-shot load thread. The
        // flag is idempotent, so a burst of label edits collapses into one
        // rebuild.
        m_store.markNeedsResync();
    }

private:
    //! Producer: a wallet transaction appeared / changed / was deleted. Runs
    //! on the emitting core thread under the locks it already holds
    //! (CT_NEW/CT_UPDATED sites hold cs_main + cs_wallet; CT_DELETED sites
    //! hold neither and only the hash is touched). NO_THREAD_SAFETY_ANALYSIS
    //! for the signals2 dispatch, with AssertLockHeld enforcing at runtime —
    //! the WalletTxSourceImpl::onTransactionChanged discipline.
    void onTransactionChanged(CWallet* wallet, const uint256& hash, ChangeType status)
        NO_THREAD_SAFETY_ANALYSIS;

    //! Producer: the chain tip advanced. Runs the bounded pending-availability
    //! recheck inline under the emitter's cs_main and pushes the depth-refresh
    //! marker.
    void onBlocksChanged(bool syncing, int height, int64_t best_time, uint32_t target_bits)
        NO_THREAD_SAFETY_ANALYSIS;

    CWallet* const m_wallet;

    //! m_queue before m_store: the store binds a queue reference at
    //! construction (member init order == declaration order).
    GRC::WalletCoinEventQueue m_queue;
    GRC::WalletCoinStore m_store;

    std::vector<boost::signals2::scoped_connection> m_handlers;
};

void WalletCoinSourceImpl::subscribe()
{
    std::weak_ptr<WalletCoinSourceImpl> weak_self = weak_from_this();

    m_handlers.emplace_back(m_wallet->NotifyTransactionChanged.connect(
        [weak_self](CWallet* wallet, const uint256& hash, ChangeType status) {
            if (auto self = weak_self.lock()) {
                self->onTransactionChanged(wallet, hash, status);
            }
        }));
    m_handlers.emplace_back(m_wallet->NotifyTransactionsBulkChanged.connect(
        [weak_self]() {
            // Rescan / reaccept mutated wallet-tx state with no per-tx
            // signals; lock state at emission varies, so only flag — the GUI
            // drain path polls consumeNeedsResync() and schedules the reload
            // off the paint path.
            if (auto self = weak_self.lock()) {
                self->m_store.markNeedsResync();
            }
        }));
    m_handlers.emplace_back(uiInterface.NotifyBlocksChanged_connect(
        [weak_self](bool syncing, int height, int64_t best_time, uint32_t target_bits) {
            if (auto self = weak_self.lock()) {
                self->onBlocksChanged(syncing, height, best_time, target_bits);
            }
        }));
}

void WalletCoinSourceImpl::onTransactionChanged(CWallet* wallet, const uint256& hash,
                                                ChangeType status)
    NO_THREAD_SAFETY_ANALYSIS
{
    switch (status) {
    case CT_NEW:
    case CT_UPDATED:
    case CT_UPDATING: {
        AssertLockHeld(cs_main);
        AssertLockHeld(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(hash);
        if (it == wallet->mapWallet.end()) {
            // Raced an erasure — drop whatever the store holds for the hash.
            m_store.enqueueRemove(hash);
            break;
        }
        // Decompose the tx's CURRENTLY available outputs under the held
        // locks; the worker diffs against the stored outpoints, covering
        // receive, spend (WalletUpdateSpent fires CT_UPDATED for the funding
        // tx), confirmation, and reorg-restore (the parent CT_UPDATED added
        // by the notify-gap patch). An empty set removes every stored coin
        // of the hash.
        bool pending = false;
        std::vector<GRC::CoinRecord> recs =
            GRC::WalletCoinStore::DecomposeCoins(wallet, it->second, pending);
        m_store.enqueueUpsert(hash, std::move(recs), pending);
        break;
    }
    case CT_DELETED:
        m_store.enqueueRemove(hash);
        break;
    }
}

void WalletCoinSourceImpl::onBlocksChanged(bool /*syncing*/, int height, int64_t /*best_time*/,
                                           uint32_t /*target_bits*/)
    NO_THREAD_SAFETY_ANALYSIS
{
    // Fired under cs_main after every tip advance (the synchronous
    // SetBestChain -> UpdatedBlockTip -> uiInterface bridge). The store
    // updates its serve-time depth base, rechecks the bounded
    // pending-availability set inline (cs_wallet under the held cs_main,
    // then cs_store — never the worker), and pushes one CoinDepthRefresh.
    m_store.applyChainTipRefresh(height);
}

} // namespace

std::shared_ptr<WalletCoinSource> MakeWalletCoinSource(CWallet* wallet)
{
    // Two-phase like MakeWalletTxSource: shared ownership first, then
    // subscribe() (weak_from_this() validity).
    auto source = std::make_shared<WalletCoinSourceImpl>(wallet);
    source->subscribe();
    return source;
}

} // namespace interfaces
