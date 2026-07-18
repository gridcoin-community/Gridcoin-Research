#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include "node/ui_interface.h" /* for ChangeType */
#include <key_io.h>
#include "util.h" /* for LogPrint / LogPrintf */

#include <QSet>
#include <QTimer>

#include <cassert>

WalletModel::WalletModel(interfaces::Wallet& wallet, interfaces::WalletTxSource& tx_source,
                         CWallet* core_wallet, OptionsModel* optionsModel, QObject* parent)
         : QObject(parent)
         , m_wallet(wallet)
         , m_tx_source(tx_source)
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
    addressTableModel = new AddressTableModel(core_wallet, this);
    // TransactionTableModel's ctor performs the initial load via
    // txSource().reloadAndSnapshot(). The source's store-worker is already
    // running (started in the WalletTxSource ctor, before this model was
    // constructed), and its producers are already subscribed — reloadAndSnapshot
    // quiesces the worker and rebuilds from the wallet, so any event enqueued in
    // the window between source creation and this snapshot is superseded by the
    // rebuild.
    transactionTableModel = new TransactionTableModel(core_wallet, this);

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
    auto events = m_tx_source.drainEvents(MODEL_EVENT_DRAIN_MAX_BATCH);
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
    txSource().noteAddressBookChanged(address.toStdString(), current.toStdString());
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
