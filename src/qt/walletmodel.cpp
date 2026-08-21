#include "walletmodel.h"
#include "qt/guilog.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include <key_io.h>
#include "util.h" /* for LogPrint / LogPrintf */

#include <QSet>
#include <QTimer>

#include <cassert>
#include <string>

WalletModel::WalletModel(interfaces::Wallet& wallet, interfaces::WalletTxSource& tx_source,
                         interfaces::WalletCoinSource& coin_source,
                         OptionsModel* optionsModel, QObject* parent)
         : QObject(parent)
         , m_wallet(wallet)
         , m_tx_source(tx_source)
         , m_coin_source(coin_source)
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
{
    addressTableModel = new AddressTableModel(this);

    // Prime the node-side transaction store from the wallet BEFORE any windowed
    // view (OverviewTxModel / DetailedTxModel) registers its cursor: those views
    // register against the store's records and fetch only the slice they show, so
    // the store must already hold the scanned wallet for them to have anything to
    // serve. The store-worker is already running (started in the WalletTxSource
    // ctor, before this model was constructed) and its producers are already
    // subscribed — prime() quiesces the worker and rebuilds from the wallet, so
    // any event enqueued in the window between source creation and this prime is
    // superseded by the rebuild. Unlike the old bootstrap, prime() returns
    // nothing: the full transaction list never crosses the interface boundary
    // (windowed-model: drop the O(wallet) snapshot; the views fetch windows).
    txSource().prime(optionsModel->getLimitTxnDisplay(),
                     optionsModel->getLimitTxnDateTime());

    // Stateless per-row formatter shared by the windowed views (holds no replica).
    transactionTableModel = new TransactionTableModel(this);

    // Re-prime when the datetime-display cutoff option changes: prime() re-scans
    // under the option and pushes each registered view a Reset, so the windowed
    // views re-filter. (The old full-replica TransactionTableModel handled this
    // via its own refreshWallet connection, which is gone with the replica.)
    connect(optionsModel, &OptionsModel::LimitTxnDisplayChanged, this,
            &WalletModel::reloadTransactionView);

    // Drain the producer→GUI event stream at a steady cadence. 500ms is
    // imperceptible for transaction-list updates while still giving the
    // queue room to absorb bursts (e.g. a reorg flood) without per-event
    // round-trips to the Qt event loop. This single timer also drives the
    // balance / row-confirmation refresh that used to be done by a
    // separate 4-second pollBalanceChanged timer; refresh now fires off
    // ChainTipChanged events the source pushes from its NotifyBlocksChanged
    // subscriber.
    eventDrainTimer = new QTimer(this);
    connect(eventDrainTimer, &QTimer::timeout, this, &WalletModel::drainEventQueue);
    eventDrainTimer->start(MODEL_EVENT_DRAIN_INTERVAL);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();

    // The one-shot coin-store load thread holds only node-side locks; join it
    // so it never outlives the model that launched it.
    if (m_coin_load_thread.joinable()) {
        m_coin_load_thread.join();
    }
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

    // Multiprocess safety net: every core-touching call in this drain — the
    // drainEvents() round-trip AND the apply path reached through
    // walletEventsDrained (each windowed view's getRows) and checkBalanceChanged
    // — is a synchronous IPC call that throws std::runtime_error("IPC client
    // method called after disconnect.") once the node process is gone. A drop can
    // land mid-drain, and a posted BitcoinGUI::requestQuit() (from the IPC
    // disconnect hook, bitcoin.cpp) cannot preempt a drain already running on this
    // thread, so an uncaught throw here would escape a Qt slot and abort the GUI.
    // Catch it, stop the periodic drain, and bail; the disconnect hook drives the
    // graceful quit. e.what() is logged so a non-disconnect exception is still
    // surfaced rather than silently swallowed.
    try {
        // Bound the per-tick batch so a large backlog (reorg flood, IBD catch-up)
        // cannot freeze the Qt main thread in a single apply pass. If the queue
        // still has events after this batch, re-arm immediately (see below)
        // instead of waiting MODEL_EVENT_DRAIN_INTERVAL for the periodic tick.
        std::vector<GRC::WalletEvent> events = m_tx_source.drainEvents(MODEL_EVENT_DRAIN_MAX_BATCH);
        if (events.empty()) {
            // An empty drain IS a clean pass, and on a quiet wallet it is the most
            // common one (this timer fires every MODEL_EVENT_DRAIN_INTERVAL). The
            // reset used to live only after a successful apply below, so it was
            // unreachable on this path and m_drain_failures was effectively
            // monotonic for the session: MODEL_EVENT_DRAIN_MAX_FAILURES unrelated
            // faults hours apart would trip the permanent stop and silence the whole
            // wallet UI -- the #3257 shape the counter exists to avoid, reached by a
            // slower route. The budget is for CONSECUTIVE failures, so clear it here
            // too.
            m_drain_failures = 0;
            return;
        }

        GUILogPrint(GUILogCategory::VERBOSE,
                 "WalletModel::drainEventQueue: applying %u events (front seqno=%llu, back seqno=%llu)",
                 static_cast<unsigned int>(events.size()),
                 static_cast<unsigned long long>(events.front().seqno),
                 static_cast<unsigned long long>(events.back().seqno));

        // Cache the pushed tip height (GUI-thread-owned) so the windowed views can
        // derive live confirmation counts on read — no cs_main, no reach-through to
        // core state; the wallet model is self-contained for the process split.
        // Last tip event in the batch wins.
        for (const auto& ev : events) {
            if (const auto* tip = std::get_if<GRC::ChainTipChangedPayload>(&ev.payload)) {
                cachedNumBlocks = tip->height;
            }
        }

        // General "wallet transactions changed" pulse (OverviewPage refreshes its
        // recent list on it). The windowed views (OverviewTxModel / DetailedTxModel)
        // and the new-transaction balloon consume the event batch directly through
        // walletEventsDrained below; per-row confirmation refresh is likewise driven
        // by each view's own ChainTipChanged handling, not a full-replica model.
        emit transactionUpdated();

        // Fan the same batch out to the per-view windowed consumers (OverviewTxModel),
        // which filter to their own viewId. The queue is drained exactly once, here.
        emit walletEventsDrained(events);

        // The coin channel drains on the same tick (its own queue, own guard).
        drainCoinEventQueue();

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

        m_drain_failures = 0;   // a clean pass clears the consecutive-failure budget
    } catch (const std::exception& e) {
        const std::string msg{e.what()};
        // The node process going away is the case this catch was written for, and
        // the only one where stopping the pump is right: every later call would
        // throw too, and the IPC disconnect hook (bitcoin.cpp) is already driving
        // the quit. Detect it by libmultiprocess's raise text, the same two
        // substrings GridcoinApplication::notify matches.
        if (msg.find("interrupted by disconnect") != std::string::npos
                || msg.find("called after disconnect") != std::string::npos) {
            GUILogPrintf("WalletModel: node connection lost mid-drain; stopping the "
                         "periodic wallet event drain: %s", e.what());
            if (eventDrainTimer) {
                eventDrainTimer->stop();
            }
            return;
        }

        // Anything else is a bug in an apply slot, not a dead node — and this is
        // the ONLY periodic pump feeding both windowed transaction views and the
        // balance refresh, with no restart path anywhere in the tree. Stopping it
        // for a transient throw silenced the whole wallet UI for the rest of the
        // session (#3257 review). Log and keep draining; give up only if the
        // failures are persistent, so a hard loop cannot spin forever.
        //
        // Note the batch is already popped by drainEvents(), so the events being
        // processed when the throw happened are lost to any view not yet notified.
        // Those views recover on the next cursor Reset; the counter exists so a
        // repeating fault is visible in the log rather than silently eating events.
        ++m_drain_failures;
        GUILogPrintf("WalletModel: wallet event drain failed (%d consecutive): %s",
                     m_drain_failures, e.what());
        if (m_drain_failures >= MODEL_EVENT_DRAIN_MAX_FAILURES) {
            GUILogPrintf("WalletModel: %d consecutive wallet event drain failures; "
                         "stopping the periodic drain", m_drain_failures);
            if (eventDrainTimer) {
                eventDrainTimer->stop();
            }
        }
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

void WalletModel::reloadTransactionView()
{
    // The datetime-display cutoff option changed. Re-prime the node-side store
    // under the new option: prime() re-scans the wallet, re-arms every registered
    // view cursor and pushes each a Reset, so the windowed views (Overview /
    // Detailed) re-filter to the new cutoff on their next drain. Kick that drain
    // now rather than waiting up to MODEL_EVENT_DRAIN_INTERVAL for the periodic
    // tick so the change is reflected promptly.
    txSource().prime(optionsModel->getLimitTxnDisplay(),
                     optionsModel->getLimitTxnDateTime());
    requestEventDrainSoon();
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
    txSource().noteAddressBookChanged(address.toStdString(), current.toStdString());

    // The coin channel needs the same edit: the coin-control tree renders the
    // label on its group rows and sorts by it, and an own-address label edit
    // additionally regroups change outputs (see the interface contract — the
    // implementation defers that part off the GUI thread).
    coinSource().noteAddressBookChanged(address.toStdString(), current.toStdString());
}

bool WalletModel::validateAddress(const QString &address)
{
    CTxDestination addressParsed = DecodeDestination(address.toStdString());
    return IsValidDestination(addressParsed);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients,
                                                    const std::optional<interfaces::WalletCoinControl>& coinControl,
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

    // Unreachable: the switch above covers every SendCoinsStatus value (and
    // deliberately has no default, so -Wswitch flags a new enumerator). The
    // return keeps the function well-defined in NDEBUG builds, where the
    // assert compiles out and falling off the end would be UB.
    assert(false);
    return TransactionCreationFailed;
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
    // One snapshot read: crypted and locked come back together (a narrower window
    // than two separate reads) and the split build makes a single round trip.
    const interfaces::WalletLockState lock_state = m_wallet.getLockState();

    if (!lock_state.crypted)
    {
        return Unencrypted;
    }
    else if (lock_state.locked)
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

bool WalletModel::backupWallet(const std::string& dest)
{
    return m_wallet.backupWallet(dest);
}

bool WalletModel::backupConfigFile(const std::string& dest)
{
    return m_wallet.backupConfigFile(dest);
}

// The tx-table producer-side handlers (NotifyTransactionChanged /
// NotifyBlocksChangedForWallet) and their subscriptions moved node-side into
// the WalletTxSource in Phase 1c-ii, so this model no longer touches the raw
// core signals — it drives the source's up-channel and drains its event
// stream. The status and address-book notifications still arrive through
// interfaces::Wallet handlers registered in subscribeToCoreSignals().
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
    m_wallet_handlers.emplace_back(m_wallet.handleStatusChanged(m_notify_lifetime.guard(
        [this]() {
            GUILogPrintf("NotifyKeyStoreStatusChanged");
            QMetaObject::invokeMethod(this, "updateStatus", Qt::QueuedConnection);
        })));
    m_wallet_handlers.emplace_back(m_wallet.handleAddressBookChanged(m_notify_lifetime.guard(
        [this](const std::string& address, const std::string& label, bool is_mine,
               const std::string& purpose, ChangeType status) {
            // `purpose` is accepted to match the 6-arg core signal but is not
            // yet surfaced to the GUI; the updateAddressBook slot remains a
            // 4-arg interface (address, label, isMine, status).
            GUILogPrintf("NotifyAddressBookChanged %s %s isMine=%i purpose=%s status=%i",
                      address, label, is_mine, purpose, status);
            QMetaObject::invokeMethod(this, "updateAddressBook", Qt::QueuedConnection,
                                      Q_ARG(QString, QString::fromStdString(address)),
                                      Q_ARG(QString, QString::fromStdString(label)),
                                      Q_ARG(bool, is_mine),
                                      Q_ARG(int, status));
        })));

    // The tx-table producer wiring (NotifyTransactionChanged /
    // NotifyBlocksChanged) now lives node-side in the WalletTxSource, wired in
    // its constructor; this model only drains the source's event stream.
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet: clearing the retained handlers runs each
    // interfaces::Handler's destructor, which disconnects it. The tx-table
    // producer connections severed here in earlier phases now live in the
    // WalletTxSource and are severed by its destructor.
    // Retire before anything else: disconnecting does not wait for a callback
    // already running on a core thread (qt/notificationlifetime.h).
    m_notify_lifetime.retire();

    m_wallet_handlers.clear();
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
    return m_wallet.getKeyFromPool(label, out_public_key);
}

QString WalletModel::getNewReceiveAddress(const QString& label)
{
    std::string address;
    if (!m_wallet.getNewReceiveAddressWithLabel(label.toStdString(), address)) {
        return QString();
    }
    return QString::fromStdString(address);
}

// returns value snapshots of the given outpoints (unknown or conflicted
// outpoints are skipped)
std::vector<interfaces::WalletOutput> WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints) const
{
    return m_wallet.getOutputs(vOutpoints);
}

// AvailableCoins grouped by wallet address (change is grouped under the
// address it derives from, walked node-side)
void WalletModel::drainCoinEventQueue()
{
    // Single drain point for the coin channel (drainEvents is destructive and
    // the consolidate-wizard flow can have two live coin views). Reentrancy
    // guard mirrors m_draining: the coin-selection fetch/sort paths call this
    // synchronously and applying a Reset can re-enter.
    if (m_coin_draining) {
        return;
    }
    m_coin_draining = true;
    struct CoinDrainGuard { bool& f; ~CoinDrainGuard() { f = false; } } drain_guard{m_coin_draining};

    auto events = m_coin_source.drainEvents(MODEL_EVENT_DRAIN_MAX_BATCH);
    if (!events.empty()) {
        emit coinEventsDrained(events);
    }

    // A rescan/reaccept bypassed per-tx notifications: rebuild the coin store
    // off the paint path (the flag is one-shot).
    if (m_coin_source.consumeNeedsResync()) {
        ensureCoinSourceLoaded(/*force=*/true);
    }

    // Batch cap hit: there is still a backlog. Re-arm immediately (0ms) rather
    // than waiting for the next periodic tick — the drainEventQueue discipline
    // (see there). Without this a burst drains at one batch per 500ms, and a
    // Reset stranded behind it (a sort or mode switch) would apply minutes
    // late.
    if (events.size() >= static_cast<std::size_t>(MODEL_EVENT_DRAIN_MAX_BATCH)) {
        QTimer::singleShot(0, this, [this] { drainCoinEventQueue(); });
    }
}

void WalletModel::ensureCoinSourceLoaded(bool force)
{
    if (m_coin_load_started && !force) {
        return;
    }
    // A load is STILL RUNNING: never join it here. This runs on the GUI thread
    // and the scan is O(wallet) under cs_main + cs_wallet, so joining would
    // freeze the UI for the length of a full wallet walk — and the resync flag
    // that drives the force path fires on ordinary user actions (an
    // address-book label edit regroups change outputs). Record that another
    // pass is owed and start it from the completion callback instead; repeated
    // requests collapse into the single pending flag.
    if (m_coin_load_in_flight) {
        m_coin_reload_pending = force;
        return;
    }
    // Not in flight: the thread has already run its completion callback, so
    // this join only reaps a finished thread.
    if (m_coin_load_thread.joinable()) {
        m_coin_load_thread.join();
    }
    m_coin_load_started = true;
    m_coin_load_in_flight = true;

    interfaces::WalletCoinSource* source = &m_coin_source;
    m_coin_load_thread = std::thread([this, source] {
        // O(wallet) scan under cs_main + cs_wallet -- the reason this runs on
        // a one-shot thread and never the GUI thread. Completion arrives as
        // the CoinReset the reload publishes, through the normal drain.
        source->reloadAndSnapshot();

        // Hand completion back to the GUI thread: drain first so the reload's
        // Reset is APPLIED (the consumers' caches hold the scanned wallet),
        // then announce. Consumers gate their loading state on this signal —
        // not on the first Reset they happen to see, which for a cold store is
        // the registration Reset published against an empty table. Queued to
        // this QObject, so Qt drops it if the model is gone.
        QMetaObject::invokeMethod(this, [this] {
            m_coin_load_in_flight = false;
            m_coin_load_complete = true;
            drainCoinEventQueue();
            emit coinSourceLoadFinished();

            // A resync was requested while this pass was running (the wallet
            // moved under it): run one more, now that nothing is in flight.
            if (m_coin_reload_pending) {
                m_coin_reload_pending = false;
                ensureCoinSourceLoaded(/*force=*/true);
            }
        }, Qt::QueuedConnection);
    });
}
