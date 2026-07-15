#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"

#include "node/ui_interface.h"
#include "wallet/wallet.h"
#include "main.h"
#include <key_io.h>
#include "util.h"

#include <QSet>
#include <QTimer>

#include <cassert>

WalletModel::WalletModel(interfaces::Wallet& wallet, CWallet* core_wallet,
                         OptionsModel* optionsModel, QObject* parent)
         : QObject(parent)
         , m_wallet(wallet)
         , m_core_wallet(core_wallet)
         , optionsModel(optionsModel)
         , addressTableModel(nullptr)
         , transactionTableModel(nullptr)
         , cachedBalance(0)
         , cachedStake(0)
         , cachedUnconfirmedBalance(0)
         , cachedImmatureBalance(0)
         , cachedNumTransactions(0)
         , cachedEncryptionStatus(Unencrypted)
         , cachedNumBlocks(0)
         , m_txStore(core_wallet, m_event_queue)
{
    addressTableModel = new AddressTableModel(core_wallet, this);
    // TransactionTableModel's ctor performs the initial load via
    // m_txStore.reloadAndSnapshot(); m_txStore is already constructed (init
    // list) and no producer can run yet — subscribeToCoreSignals() is called
    // below, after the model exists.
    transactionTableModel = new TransactionTableModel(core_wallet, this);

    // Drain the producer→GUI event queue at a steady cadence. 500ms is
    // imperceptible for transaction-list updates while still giving the
    // queue room to absorb bursts (e.g. a reorg flood) without per-event
    // round-trips to the Qt event loop. This single timer also drives the
    // balance / row-confirmation refresh that used to be done by a
    // separate 4-second pollBalanceChanged timer; refresh now fires off
    // ChainTipChanged events pushed by the producer-side subscriber to
    // uiInterface.NotifyBlocksChanged.
    eventDrainTimer = new QTimer(this);
    connect(eventDrainTimer, &QTimer::timeout, this, &WalletModel::drainEventQueue);
    eventDrainTimer->start(MODEL_EVENT_DRAIN_INTERVAL);

    // Launch the store-worker now — before producers can fire (subscribe is
    // below) — so it is ready to drain the intake queue off the core locks
    // (PR2.5). The initial reloadAndSnapshot in the TransactionTableModel ctor
    // above ran with no worker yet; that is fine, it skips the worker barrier
    // when the worker has not started.
    m_txStore.start();

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

qint64 WalletModel::getBalance() const
{
    return m_wallet.getBalance();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return m_wallet.getUnconfirmedBalance();
}

qint64 WalletModel::getStake() const
{
    return m_wallet.getStake();
}

qint64 WalletModel::getImmatureBalance() const
{
    return m_wallet.getImmatureBalance();
}

int WalletModel::getNumTransactions() const
{
    return m_wallet.getNumTransactions();
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::checkBalanceChanged()
{
    // The balance scans iterate the wallet's full mapWallet and become
    // INCREDIBLY expensive on large wallets. Two layers of protection:
    //
    //  1. A MODEL_UPDATE_DELAY (4s) stale-time gate: only actually recompute
    //     at most once per gate interval. Bursts of rapid-fire wallet events
    //     during a resync, rescan, or large consolidation collapse into a
    //     single recompute. Checked first — it is free, while the interface
    //     call below takes lock attempts even when it bows out.
    //
    //  2. tryGetBalances: bows out cleanly (returns false) if the core is
    //     holding cs_main/cs_wallet (e.g. during a wallet rescan). This is
    //     the same TRY_LOCK guard pollBalanceChanged used to apply, now on
    //     the node side of the interface boundary.
    //
    // The gate timestamp advances only on an actual recompute — NOT when a
    // busy core makes tryGetBalances bow out, and not only when a change is
    // detected. The gate exists to rate-limit the expensive scans
    // themselves; if the timestamp only advanced on a detected change, a
    // long-stable balance would leave the gate permanently open and every
    // drain tick (which can fire back-to-back when drainEventQueue re-arms
    // to clear a backlog) would run a fresh full-wallet scan.
    //
    // A call that fails either layer isn't lost: the next ChainTipChanged
    // event (or the next drain pass with events in it) re-runs this
    // function, which by then will pass.
    // Plain wall clock, not GetAdjustedTime(): this is a purely local rate
    // limiter, and a network-time offset step must not wedge or bypass it.
    int64_t current_time = GetTime();

    if (current_time - last_balance_update_time <= MODEL_UPDATE_DELAY / 1000) {
        return;
    }

    interfaces::WalletBalances balances;
    if (!m_wallet.tryGetBalances(balances)) {
        return;
    }

    last_balance_update_time = current_time;

    if (cachedBalance != balances.balance
            || cachedStake != balances.stake
            || cachedUnconfirmedBalance != balances.unconfirmed_balance
            || cachedImmatureBalance != balances.immature_balance)
    {
        cachedBalance = balances.balance;
        cachedStake = balances.stake;
        cachedUnconfirmedBalance = balances.unconfirmed_balance;
        cachedImmatureBalance = balances.immature_balance;

        emit balanceChanged(cachedBalance, cachedStake, cachedUnconfirmedBalance, cachedImmatureBalance);
    }
}

void WalletModel::drainEventQueue()
{
    // Reentrancy guard (PR5-B): a windowed consumer's fetch path calls this
    // synchronously, and applying a Reset can re-enter via viewReset ->
    // restoreAnchor -> setCurrentIndex. A nested call no-ops — the outer drain owns
    // the queue, so events are applied exactly once and in order. The RAII reset
    // also keeps an exception in any apply* from wedging the flag.
    if (m_draining) {
        return;
    }
    m_draining = true;
    struct DrainGuard { bool& f; ~DrainGuard() { f = false; } } drain_guard{m_draining};

    // Any drain (periodic, backlog re-arm, or a requestEventDrainSoon kick) satisfies
    // a pending user-requested drain, so clear the coalescing flag up front: a fresh
    // request that arrives after this point schedules a new kick (PR4-fix D).
    m_event_drain_requested = false;

    // Bound the per-tick batch so a large backlog (reorg flood, IBD catch-up)
    // cannot freeze the Qt main thread in a single apply pass. If the queue
    // still has events after this batch, re-arm immediately (see below)
    // instead of waiting MODEL_EVENT_DRAIN_INTERVAL for the periodic tick.
    auto events = m_event_queue.drain(MODEL_EVENT_DRAIN_MAX_BATCH);
    if (events.empty()) {
        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE,
             "WalletModel::drainEventQueue: applying %u events (front seqno=%llu, back seqno=%llu)",
             static_cast<unsigned int>(events.size()),
             static_cast<unsigned long long>(events.front().seqno),
             static_cast<unsigned long long>(events.back().seqno));

    // Detect a chain-tip advance in the batch so the consumer-side
    // post-processing (per-row confirmation refresh, balance recompute) runs
    // only when a block actually moved — not on every wallet-tx burst within
    // a single block.
    bool chain_tip_advanced = false;
    for (const auto& ev : events) {
        if (const auto* tip = std::get_if<GRC::ChainTipChangedPayload>(&ev.payload)) {
            chain_tip_advanced = true;
            // Cache the pushed tip height (GUI-thread-owned) so the transaction
            // table can derive live confirmation counts on read — no cs_main, no
            // reach-through to core state, wallet model self-contained for the
            // eventual process split. Last tip event in the batch wins.
            cachedNumBlocks = tip->height;
        }
    }

    if (transactionTableModel) {
        transactionTableModel->applyEventBatch(events);

        // Note this is subtly different than the below. If a resync is being
        // done on a wallet that already has transactions, the
        // numTransactionsChanged will not be emitted after the wallet is
        // loaded because the size() does not change. See the comments in the
        // header file.
        emit transactionUpdated();

        // Equivalent of the work pollBalanceChanged used to do when it
        // observed nBestHeight != cachedNumBlocks: refresh per-row
        // confirmation status. Driven by the event payload now.
        if (chain_tip_advanced) {
            transactionTableModel->updateConfirmations();
        }
    }

    // Fan the same batch out to the per-view windowed consumers (OverviewTxModel),
    // which filter to their own viewId. The queue is drained exactly once, here.
    emit walletEventsDrained(events);

    // Balance and number of transactions might have changed.
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if (cachedNumTransactions != newNumTransactions) {
        cachedNumTransactions = newNumTransactions;

        emit numTransactionsChanged(newNumTransactions);
    }

    // If this drain hit the per-tick batch cap there is still a backlog.
    // Re-arm immediately (0ms) rather than waiting for the next periodic
    // tick: this returns control to the Qt event loop — keeping the GUI
    // responsive between batches — but resumes draining straight away so a
    // burst clears in a few event-loop turns instead of one per 500ms.
    if (events.size() >= static_cast<std::size_t>(MODEL_EVENT_DRAIN_MAX_BATCH)) {
        QTimer::singleShot(0, this, &WalletModel::drainEventQueue);
    }
}

void WalletModel::requestEventDrainSoon()
{
    // Kick a drain on the next event-loop turn so a user-initiated cursor change
    // (filter/sort, which synchronously pushed a Reset to the queue) is reflected
    // immediately instead of waiting up to MODEL_EVENT_DRAIN_INTERVAL for the
    // periodic tick (windowed-model PR4-fix D).
    //
    // QTimer::singleShot does NOT deduplicate — each call schedules its own
    // callback — so coalesce explicitly with a pending flag, or a burst (e.g. every
    // keystroke in the filter box) would queue one drain per keystroke. The flag is
    // cleared at the top of drainEventQueue, so exactly one drain is in flight per
    // burst; it runs on the Qt thread, so the flag needs no synchronization.
    if (m_event_drain_requested) {
        return;
    }
    m_event_drain_requested = true;
    QTimer::singleShot(0, this, &WalletModel::drainEventQueue);
}

void WalletModel::updateAddressBook(const QString &address, const QString &label, bool isMine, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, status);

    // Re-snapshot the label on the windowed store's records for this address so
    // the detailed view's Address-column sort and label substring filter track an
    // address-book edit live — the behaviour the deleted TransactionFilterProxy
    // got from reading LabelRole on every filter pass (windowed-model PR4-C). Use
    // the authoritative current label (empty after a delete) rather than the raw
    // notification argument. The enqueue is O(1); the store-worker re-snapshots and
    // re-drives the cursors off the GUI thread.
    const QString current = addressTableModel
        ? addressTableModel->labelForAddress(address) : label;
    getTxStore().enqueueAddressBookChange(address.toStdString(), current.toStdString());
}

bool WalletModel::validateAddress(const QString &address)
{
    CTxDestination addressParsed = DecodeDestination(address.toStdString());
    return IsValidDestination(addressParsed);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients,
                                                    const CCoinControl *coinControl,
                                                    qint64 acceptedFee)
{
    QSet<QString> setAddress;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity. These are purely GUI-detectable
    // conditions (parseable address, positive amount, no duplicate
    // recipients), so they are checked here and never reach the interface.
    for (const SendCoinsRecipient& rcp : recipients) {
        if(!validateAddress(rcp.address))
        {
            return InvalidAddress;
        }
        setAddress.insert(rcp.address);

        if(rcp.amount <= 0)
        {
            return InvalidAmount;
        }
    }

    if(recipients.size() > setAddress.size())
    {
        return DuplicateAddress;
    }

    std::vector<interfaces::WalletSendRecipient> vRecipients;
    vRecipients.reserve(recipients.size());

    for (const SendCoinsRecipient& rcp : recipients) {
        interfaces::WalletSendRecipient recipient;
        recipient.address = rcp.address.toStdString();
        recipient.label = rcp.label.toStdString();
        recipient.amount = rcp.amount;
        recipient.message = rcp.Message.toStdString();
        recipient.subtract_fee_from_amount = rcp.fSubtractFeeFromAmount;
        vRecipients.push_back(std::move(recipient));
    }

    // Creation, fee convergence, fee-threshold gating, and commit all run
    // node-side; a FeeConfirmationRequired result comes back WITHOUT a
    // commit so the caller can prompt the user with no core locks held and
    // re-invoke with the accepted fee (the eliminated ThreadSafeAskFee used
    // to block inside the node's LOCK2 scope right before the commit).
    const interfaces::SendCoinsResult result =
        m_wallet.sendCoins(vRecipients, coinControl, acceptedFee);

    switch (result.status) {
    case interfaces::SendCoinsStatus::OK:
        return SendCoinsReturn(OK, 0, QString::fromStdString(result.txid_hex));
    case interfaces::SendCoinsStatus::InvalidAddress:
        return InvalidAddress;
    case interfaces::SendCoinsStatus::InvalidAmount:
        return InvalidAmount;
    case interfaces::SendCoinsStatus::AmountExceedsBalance:
        return AmountExceedsBalance;
    case interfaces::SendCoinsStatus::AmountWithFeeExceedsBalance:
        return SendCoinsReturn(AmountWithFeeExceedsBalance, result.fee);
    case interfaces::SendCoinsStatus::FeeExceedsSubtractedAmount:
        return SendCoinsReturn(FeeExceedsSubtractedAmount, result.fee);
    case interfaces::SendCoinsStatus::TransactionCreationFailed:
        return TransactionCreationFailed;
    case interfaces::SendCoinsStatus::TransactionCommitFailed:
        return TransactionCommitFailed;
    case interfaces::SendCoinsStatus::FeeConfirmationRequired:
        return SendCoinsReturn(FeeConfirmationRequired, result.fee);
    }

    // Unreachable: the switch above covers every SendCoinsStatus value.
    assert(false);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!m_wallet.isCrypted())
    {
        return Unencrypted;
    }
    else if(m_wallet.isLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(const SecureString& passphrase)
{
    return m_wallet.encryptWallet(passphrase);
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase, bool stakingOnly)
{
    if(locked)
    {
        // Lock. Does not clear the staking-only unlock preference.
        return m_wallet.lockWallet();
    }
    else
    {
        // Unlock; on success the staking-only preference is set node-side
        // (a full unlock clears a stale staking-only restriction).
        return m_wallet.unlockWallet(passPhrase, stakingOnly);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    // Locks the wallet first (node-side) before attempting the change.
    return m_wallet.changeWalletPassphrase(oldPass, newPass);
}

// Producer-side handlers for the tx-table core signals (the Phase 1c-ii
// leg; the status and address-book notifications arrive through
// interfaces::Wallet handlers registered in subscribeToCoreSignals).
static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet)
{
    LogPrint(BCLog::LogFlags::VERBOSE, "NotifyTransactionChanged %s status=%i", hash.GetHex(), status);

    // Producer-side push into the WalletEventQueue. CT_NEW and CT_UPDATED are
    // handled identically: the producer looks up the wtx, applies the
    // wtx-level visibility checks (orphan coinstake/coinbase / legacy
    // OP_RETURN — datetime filter is consumer-side), and pushes either a
    // TxAdded with decomposed records or a TxRemoved with just the hash.
    // The consumer's binary-search-by-hash insert path de-dupes a TxAdded
    // when the tx is already in cachedWallet, and the remove path no-ops
    // when the tx isn't.
    //
    // The unified CT_NEW/CT_UPDATED handling matters because the wallet
    // fires CT_UPDATED (not CT_NEW) when a tx already in mapWallet is
    // re-validated against a fresh chain — e.g. during an IBD that follows
    // a chainstate wipe but retains wallet.dat. If we only acted on CT_NEW,
    // the GUI's cachedWallet would never see those txs become visible
    // again. It also covers the steady-state case where a previously
    // filtered-out tx (e.g. an orphan coinstake) becomes valid: CT_UPDATED
    // fires, the producer's showTransaction now returns true, and the
    // consumer inserts the row. The reverse direction (tx falls out of
    // visibility) is covered by pushing TxRemoved when showTransaction
    // returns false.
    //
    // Lock state at this point:
    //   The CT_NEW / CT_UPDATED / CT_UPDATING branch needs BOTH cs_main and
    //   cs_wallet held:
    //     - cs_wallet — to look up mapWallet[hash] and run
    //       decomposeTransaction (which recursively calls IsMine()).
    //     - cs_main   — TransactionRecord::showTransaction() calls
    //       CWalletTx::IsInMainChain(), which is EXCLUSIVE_LOCKS_REQUIRED
    //       (cs_main). (The thread-safety analyzer does not flag this
    //       cross-TU because GetDepthInMainChain's annotation lives on the
    //       definition in main.cpp, not the header declaration — so the
    //       requirement is verified here by hand, not by the compiler.)
    //
    //   All four CT_NEW / CT_UPDATED callsites hold both locks, verified by
    //   audit:
    //     wallet.cpp:572  AddToWallet            — EXCLUSIVE_LOCKS_REQUIRED(cs_main); LOCK(cs_wallet)
    //     wallet.cpp:2400 CommitTransaction      — LOCK2(cs_main, cs_wallet)
    //     wallet.cpp:458  WalletUpdateSpent      — caller AddToWallet / AddToWalletIfInvolvingMe, both EXCLUSIVE_LOCKS_REQUIRED(cs_main); LOCK(cs_wallet)
    //     wallet.cpp:475  WalletUpdateSpent      — same
    //   This function is annotated EXCLUSIVE_LOCKS_REQUIRED(cs_main,
    //   wallet->cs_wallet) so the Clang thread-safety analyzer accepts the
    //   AssertLockHeld() calls and the mapWallet reads in the CT_NEW /
    //   CT_UPDATED branch. The AssertLockHeld() calls additionally enforce
    //   the requirement at runtime in DEBUG_LOCKORDER builds.
    //
    //   CT_DELETED callsites (main.cpp:1290, wallet.cpp:1349) DO NOT hold
    //   either lock — the tx has already been erased. The TxRemoved payload
    //   carries only the hash, so no wallet lookup or showTransaction call
    //   is needed; that branch touches nothing the annotation guards. The
    //   EXCLUSIVE_LOCKS_REQUIRED annotation therefore over-claims for the
    //   CT_DELETED path — harmless, because the handler is invoked only
    //   through boost::signals2, which the analyzer cannot trace, so no
    //   caller is ever checked against the annotation; its sole effect is
    //   to satisfy the analyzer inside the CT_NEW / CT_UPDATED branch.
    switch (status) {
    case CT_NEW:
    case CT_UPDATED:
    case CT_UPDATING: {
        AssertLockHeld(cs_main);
        AssertLockHeld(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(hash);
        if (it == wallet->mapWallet.end()) {
            // Tx isn't in mapWallet — only happens if the notification
            // raced with an erasure. Push TxRemoved to keep the consumer
            // in sync.
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "NotifyTransactionChanged: %s status=%d but tx not in mapWallet "
                     "— removing from store",
                     hash.GetHex(), status);
            walletmodel->getTxStore().enqueueRemove(hash);
            break;
        }
        const CWalletTx& wtx = it->second;

        bool visible = TransactionRecord::showTransaction(wtx, false, 0);

        // showTransaction() hides a generated (coinstake/coinbase) tx whose
        // block is not yet in the main chain. This handler, however, runs
        // synchronously inside block connection: the wallet is notified of a
        // block's transactions before SetBestChain advances pindexBest (see
        // main.cpp), so the block being connected — and its own coinstake —
        // transiently read as orphan. That is a false negative: the block is
        // a split-second from becoming the tip, and without this guard its
        // coinstake gets a TxRemoved and never enters the GUI model.
        //
        // Detect exactly that window: a block sitting directly on the current
        // tip but not yet the tip itself is the one being connected right
        // now. A genuine orphan has pprev != pindexBest and stays hidden, so
        // -showorphans semantics are unchanged. For a current-era coinstake
        // the orphan check is showTransaction()'s only false path, so this
        // override cannot un-hide a tx filtered for any other reason.
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
            // ENQUEUE to the store-worker (PR2.5). The worker applies the datetime
            // cutoff, de-dupes, computes positions, maintains the per-view cursors
            // and emits events off the core locks; the enqueue itself is O(1).
            // Status is computed here so the off-lock cursors can filter/sort by
            // it without re-touching the wallet.
            const QList<TransactionRecord> decomposed =
                TransactionRecord::decomposeTransaction(wallet, wtx);
            if (!decomposed.isEmpty()) {
                std::vector<TransactionRecord> recs(decomposed.begin(), decomposed.end());
                for (TransactionRecord& rec : recs) {
                    rec.updateStatus(wtx);
                    rec.populateDisplayLabel(*wallet);  // address-book label snapshot (PR4)
                }
                // CT_NEW is a fresh insert; CT_UPDATED / CT_UPDATING is an upsert
                // of an existing tx (e.g. a confirmation) — the store updates it in
                // place and repositions it in any status-sorted cursor.
                if (status == CT_NEW) {
                    walletmodel->getTxStore().enqueueInsert(std::move(recs));
                } else {
                    walletmodel->getTxStore().enqueueUpsert(std::move(recs));
                }
            }
        } else {
            // Tx is genuinely filtered out (a real orphan coinstake, or a
            // legacy non-IsFromMe OP_RETURN). Ensure the store removes the rows
            // if they were previously visible — a no-op if absent.
            walletmodel->getTxStore().enqueueRemove(hash);
        }
        break;
    }
    case CT_DELETED:
        walletmodel->getTxStore().enqueueRemove(hash);
        break;
    }
}

static void NotifyBlocksChangedForWallet(WalletModel *walletmodel,
                                         bool /*syncing*/,
                                         int height,
                                         int64_t best_time,
                                         uint32_t /*target_bits*/)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Fired from main.cpp::SetBestChain (under cs_main) after every chain
    // tip advance — connect, disconnect, or reorg. Pushes a lightweight
    // marker into the event queue. The Qt-side drain handler reacts by
    // re-running the existing (rate-limited) balance recompute path. This
    // replaces the 4-second pollBalanceChanged poll that used to compare
    // nBestHeight to a cached copy on a timer.
    walletmodel->getEventQueue().push(GRC::ChainTipChangedPayload{height, best_time});

    // Refresh per-row confirmation/maturity status for the bounded set of
    // height-volatile records and re-drive the cursors (windowed-model PR4-A).
    // Runs INLINE here — we already hold cs_main, so the store can take
    // cs_wallet (recursive) + cs_store in canonical order with no store-worker
    // involvement (the worker must stay cs_main/cs_wallet-free or it would
    // deadlock reloadAndSnapshot's park protocol). The work is O(volatile), so a
    // per-block refresh on the validation thread is bounded; the cursor
    // reposition cost is O(volatile × view_index) until PR5 windowing shrinks the
    // per-view index. Restores the per-block status advance the deleted proxy got
    // from TransactionTableModel::index()'s lazy updateStatus.
    walletmodel->getTxStore().applyChainTipRefresh();
}

void WalletModel::subscribeToCoreSignals()
{
    // Register notification handlers, retaining each so it is severed in
    // unsubscribeFromCoreSignals() (from ~WalletModel). Every callback below
    // captures `this`; a signal firing after this object is gone would
    // otherwise invoke a slot on freed memory during shutdown.
    //
    // Status and address-book changes come through the interfaces::Wallet
    // boundary as value types (Phase 1c-i). The callbacks fire on core
    // threads, possibly under core locks, so they enqueue to the Qt thread
    // and return — same discipline the raw-signal handlers applied.
    m_wallet_handlers.emplace_back(m_wallet.handleStatusChanged(
        [this]() {
            LogPrintf("NotifyKeyStoreStatusChanged");
            QMetaObject::invokeMethod(this, "updateStatus", Qt::QueuedConnection);
        }));
    m_wallet_handlers.emplace_back(m_wallet.handleAddressBookChanged(
        [this](const std::string& address, const std::string& label, bool is_mine,
               const std::string& purpose, ChangeType status) {
            // `purpose` is accepted to match the 6-arg core signal but is not
            // yet surfaced to the GUI; the updateAddressBook slot remains a
            // 4-arg interface (address, label, isMine, status).
            LogPrintf("NotifyAddressBookChanged %s %s isMine=%i purpose=%s status=%i",
                      address, label, is_mine, purpose, status);
            QMetaObject::invokeMethod(this, "updateAddressBook", Qt::QueuedConnection,
                                      Q_ARG(QString, QString::fromStdString(address)),
                                      Q_ARG(QString, QString::fromStdString(label)),
                                      Q_ARG(bool, is_mine),
                                      Q_ARG(int, status));
        }));

    // The tx-table producer wiring stays on the raw core signals until the
    // store/event-queue machinery migrates behind interfaces::WalletTxSource
    // (Phase 1c-ii) — the producer runs under the emitting thread's locks by
    // design (it decomposes transactions in place).
    m_handlers.emplace_back(m_core_wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this,
                                                         boost::placeholders::_1, boost::placeholders::_2,
                                                         boost::placeholders::_3)));
    m_handlers.emplace_back(uiInterface.NotifyBlocksChanged_connect(boost::bind(NotifyBlocksChangedForWallet, this,
                                                       boost::placeholders::_1, boost::placeholders::_2,
                                                       boost::placeholders::_3, boost::placeholders::_4)));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet: clearing the retained connections runs
    // each scoped_connection's (and interfaces::Handler's) destructor, which
    // disconnects it. This also covers the uiInterface.NotifyBlocksChanged
    // connection, which the previous hand-written disconnects missed and
    // leaked (issue #3129 follow-up).
    m_wallet_handlers.clear();
    m_handlers.clear();
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;

    // A staking-only unlock is not enough here: relock and force a full
    // unlock prompt. (isUnlockedForStakingOnly is the unlocked-AND-restricted
    // composite, so it can only be true on the !was_locked path.)
    if ((!was_locked) && m_wallet.isUnlockedForStakingOnly())
    {
       setWalletLocked(true);
       was_locked = getEncryptionStatus() == Locked;

    }
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked && !m_wallet.isUnlockedForStakingOnly());
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return m_wallet.getPubKey(address, vchPubKeyOut);
}

bool WalletModel::getKeyFromPool(CPubKey& out_public_key, const std::string& label)
{
    return m_wallet.getKeyFromPool(out_public_key, label);
}

// returns value snapshots of the given outpoints (unknown or conflicted
// outpoints are skipped)
std::vector<interfaces::WalletOutput> WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints) const
{
    return m_wallet.getOutputs(vOutpoints);
}

// AvailableCoins grouped by wallet address (change is grouped under the
// address it derives from, walked node-side)
std::map<std::string, std::vector<interfaces::WalletOutput>> WalletModel::listCoins() const
{
    return m_wallet.listCoins();
}
