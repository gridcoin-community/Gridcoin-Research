#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include <QObject>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_source.h"
#include "interfaces/wallet_tx_source.h"
#include "qt/notificationlifetime.h"
#include "support/allocators/secure.h" /* for SecureString */

class GuiStateCache;
class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CKeyID;
class CPubKey;
class COutPoint;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient
{
public:
    QString address;
    QString label;
    qint64 amount = 0;
    QString Message;
    bool fSubtractFeeFromAmount = false;
};

/** Interface to Bitcoin wallet from Qt view code. */
class WalletModel : public QObject
{
    Q_OBJECT

public:
    //! The query/command surface goes through the interfaces::Wallet
    //! boundary (Phase 1c-i); the windowed tx-table store/event-queue
    //! machinery is reached through the interfaces::WalletTxSource boundary
    //! (Phase 1c-ii) and the windowed coin-control store through
    //! interfaces::WalletCoinSource (#3183) — neither is owned by the model:
    //! both are created node-side (interfaces::Init::makeWalletTxSource /
    //! makeWalletCoinSource) and must outlive it. No raw CWallet* leg remains:
    //! every core interaction crosses one of the three interface boundaries, so
    //! the model carries nothing that would prevent it running in a separate
    //! process from the wallet (Phase 2).
    explicit WalletModel(interfaces::Wallet& wallet, interfaces::WalletTxSource& tx_source,
                         interfaces::WalletCoinSource& coin_source,
                         OptionsModel* optionsModel,
                         QObject* parent = nullptr);
    ~WalletModel();

    //! Take the balances from the state the core pushes (ClientModel::guiState()),
    //! and read the one already computed. Call once, after construction. The
    //! model asks the wallet for no balance scan; balanceChanged is emitted
    //! when a pushed figure differs from the last one shown.
    void setGuiState(GuiStateCache* cache);

    enum StatusCode // Returned by sendCoins
    {
        OK,
        InvalidAmount,
        InvalidAddress,
        AmountExceedsBalance,
        AmountWithFeeExceedsBalance,
        FeeExceedsSubtractedAmount,
        DuplicateAddress,
        //! Transaction could not be created and nothing was committed. Most
        //! commonly the wallet is still locked; also covers subtract-fee
        //! non-convergence node-side and the fee-confirmation attempt cap
        //! in SendCoinsDialog.
        TransactionCreationFailed,
        TransactionCommitFailed,
        //! The wallet is unlocked for staking only and will not build a spend.
        WalletUnlockedForStakingOnly,
        Aborted,
        //! The required fee exceeds both the configured transaction fee and
        //! the fee the caller has accepted so far; nothing was committed.
        //! The caller prompts the user (with no core locks held — this
        //! replaces the eliminated ThreadSafeAskFee modal) and re-invokes
        //! sendCoins with acceptedFee set to the returned fee.
        FeeConfirmationRequired
    };

    enum EncryptionStatus
    {
        Unencrypted,             // !wallet->IsCrypted()
        Locked,                  // crypted, scope Locked
        Unlocked,                // crypted, scope Full
        UnlockedForStakingOnly   // crypted, scope StakingOnly
    };

    //! The interface boundary for wallet queries and commands. Dialogs that
    //! need surface not wrapped by this model (e.g. the staking-only unlock
    //! preference) reach it here rather than through core globals.
    interfaces::Wallet& wallet() const { return m_wallet; }

    OptionsModel *getOptionsModel();
    AddressTableModel *getAddressTableModel();
    TransactionTableModel *getTransactionTableModel();

    //! Last chain-tip height pushed to the GUI via the wallet event stream
    //! (ChainTipChangedPayload), cached on the GUI thread. Lets the transaction
    //! table derive live confirmation counts on read without reaching into core
    //! state or taking cs_main — GUI-thread-owned, no lock, process-separation safe.
    //! 0 until the first chain-tip event drains (callers must guard, as the count
    //! derivation does); never blocks.
    int getChainHeight() const { return cachedNumBlocks; }

    qint64 getBalance() const;
    qint64 getStake() const;
    qint64 getUnconfirmedBalance() const;
    qint64 getImmatureBalance() const;
    //! The fee rate per kB a send pays (interfaces::Wallet::getTransactionFeeRate).
    CAmount getTransactionFeeRate() const;
    int getNumTransactions() const;
    EncryptionStatus getEncryptionStatus() const;

    // Check address for validity
    bool validateAddress(const QString &address);

    // Return status record for SendCoins, contains error id + information
    struct SendCoinsReturn
    {
        SendCoinsReturn(StatusCode status=Aborted,
                         qint64 fee=0,
                         QString hex=QString()):
            status(status), fee(fee), hex(hex) {}
        StatusCode status;
        qint64 fee; // is used in case status is "AmountWithFeeExceedsBalance"
        QString hex; // is filled with the transaction hash if status is "OK"
    };

    // Send coins to a list of recipients. acceptedFee is the largest fee the
    // user has already confirmed (see StatusCode::FeeConfirmationRequired).
    SendCoinsReturn sendCoins(const QList<SendCoinsRecipient>& recipients,
                              const std::optional<interfaces::WalletCoinControl>& coinControl = std::nullopt,
                              qint64 acceptedFee = 0);

    // Wallet encryption
    bool setWalletEncrypted(const SecureString& passphrase);
    // Passphrase only needed when unlocking; stakingOnly restricts the
    // unlock to staking (persisted node-side as the unlock preference).
    bool setWalletLocked(bool locked, const SecureString& passPhrase=SecureString(),
                         bool stakingOnly = false);

    //! Record that the passphrase prompt raised by requireUnlock() began a
    //! capability (interfaces::Wallet::beginElevation()), so the requestUnlock()
    //! waiting on that prompt hands back a context that ends it. GUI thread
    //! only; the prompt calls it once the check has succeeded.
    void setElevationBegun() { m_elevation_begun = true; }

    //! A passphrase prompt raised by requestUnlock() is open. A second
    //! requestUnlock() made meanwhile (from a queued event run inside the
    //! prompt's event loop) is refused. GUI thread only.
    bool isUnlockPromptOpen() const { return m_unlock_prompt_open; }

    //! Snapshot the backup items (a mask of interfaces::BackupItem) in the core
    //! and return a source that streams them: interfaces::Wallet::beginBackup().
    //! Null, with \p error_out set, when the core refuses. The GUI's backup
    //! action calls it on the modal GuiIpcWorker, within a Backup context from
    //! requestUnlock().
    std::unique_ptr<interfaces::FileSource> beginBackup(uint8_t items, std::string& error_out);

    //! RAII object for one operation's access to the wallet's keys, returned by
    //! requestUnlock(). While it lives, the interface calls for that operation
    //! run under the capability it began; it ends that capability when it goes.
    class UnlockContext
    {
    public:
        UnlockContext(WalletModel *wallet, bool valid, bool end_elevation);
        ~UnlockContext();

        bool isValid() const { return valid; }

        //! Copying transfers the context: the copy ends the capability, and
        //! the original no longer does.
        UnlockContext(const UnlockContext& obj) { CopyFrom(obj); }

        //! Not assignable. Assigning would either leave two owners of one
        //! capability or end one still in use; a context is only ever handed
        //! over by construction.
        UnlockContext& operator=(const UnlockContext&) = delete;
        UnlockContext& operator=(UnlockContext&&) = delete;
    private:
        WalletModel *wallet{nullptr};
        bool valid{false};
        //! This context began a capability and ends it when it goes. False for
        //! a wallet that needed none (unencrypted or fully unlocked) and for a
        //! context that was copied away. Mutable, as copying clears it.
        mutable bool end_elevation{false};

        void CopyFrom(const UnlockContext& rhs);
    };

    //! Get the wallet's keys for one operation of \p kind.
    //!
    //! An unencrypted or fully unlocked wallet needs nothing, and the context
    //! is valid at once. A locked wallet, or one unlocked for staking only,
    //! raises requireUnlock(): the user is asked for the passphrase, and a
    //! capability for \p kind is begun with it. The context ends the capability
    //! when it goes. Cancelled or
    //! refused, the context is invalid and nothing changed. Synchronous: it
    //! returns once the prompt has closed. While a prompt is open, a further
    //! call is refused with an invalid context and no prompt.
    UnlockContext requestUnlock(CapabilityKind kind);

    bool getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    bool getKeyFromPool(CPubKey& out_public_key, const std::string& label);
    //! Reserve a fresh receive key labeled \p label and return its encoded
    //! address (empty on failure). The key stays node-side; used by the
    //! researcher pool page so it needs no key.h / CPubKey.
    QString getNewReceiveAddress(const QString& label);
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>& vOutpoints) const;

    //!
    //! \brief The windowed transaction-table boundary (Phase 1c-ii). The GUI
    //! sub-models drive per-view cursors, pull rows/detail, and drain the
    //! producer→GUI event stream through this handle; the concrete node-side
    //! implementation owns the store, its worker thread, and the producer
    //! subscriptions to CWallet's transaction signals. The source is created
    //! and owned outside this model (interfaces::Init::makeWalletTxSource) and
    //! must outlive it.
    //!
    interfaces::WalletTxSource& txSource() { return m_tx_source; }

    //! The windowed coin-control boundary (#3183). Same ownership contract as
    //! txSource(): created node-side (interfaces::Init::makeWalletCoinSource)
    //! and outlives this model.
    interfaces::WalletCoinSource& coinSource() { return m_coin_source; }

    //! Drain the coin-channel event queue and fan the batch out via
    //! coinEventsDrained. THE single drain point for the coin channel:
    //! drainEvents is destructive, and the consolidate-wizard flow can have
    //! two live coin views — per-consumer drain timers would steal each
    //! other's events. Driven by the periodic tick; also called synchronously
    //! by CoinSelectionModel's fetch/sort paths.
    void drainCoinEventQueue();

    //! Run the coin store's initial wallet scan (reloadAndSnapshot) on the
    //! process's coin-load GuiIpcWorker — never the GUI thread: the scan holds
    //! cs_main + cs_wallet and is O(wallet). Idempotent unless \p force
    //! (the bulk-resync path). Completion is drained and then announced via
    //! coinSourceLoadFinished(); a scan that fails, or finds no worker to run
    //! on, is announced via coinSourceLoadFailed().
    void ensureCoinSourceLoaded(bool force = false);

    //! True once a coin-store scan has completed and been drained. A consumer
    //! attaching to a warm store (a dialog reopen) is not loading and needs no
    //! signal; one attaching to a cold store waits for
    //! coinSourceLoadFinished(). Distinguishing the two is what keeps the
    //! registration Reset — published against a not-yet-scanned store — from
    //! being mistaken for the load completing.
    bool isCoinSourceLoaded() const { return m_coin_load_complete; }

    //! Kick an immediate (next-event-loop-turn) event-queue drain, so a
    //! user-initiated cursor change (a windowed-view filter/sort) is reflected
    //! without waiting for the periodic drain tick (windowed-model PR4-fix D).
    void requestEventDrainSoon();

private:
    //! Interface boundary for the query/command surface (Phase 1c-i).
    interfaces::Wallet& m_wallet;

    //! The windowed transaction-table boundary (Phase 1c-ii). Not owned: the
    //! source is created node-side (interfaces::Init::makeWalletTxSource) and
    //! outlives this model; the model only drives it.
    interfaces::WalletTxSource& m_tx_source;

    //! The windowed coin-control boundary (#3183). Not owned (see coinSource()).
    interfaces::WalletCoinSource& m_coin_source;

    //! Ready once the latest coin-store scan's job has finished on the worker
    //! (see ensureCoinSourceLoaded). The destructor waits for it: the job
    //! reports to this model.
    std::future<void> m_coin_load_done;
    bool m_coin_load_started = false;
    //! Set on the GUI thread when a completed scan has been drained (see
    //! isCoinSourceLoaded).
    bool m_coin_load_complete = false;
    //! A scan is running: further requests must not join it on the GUI thread.
    bool m_coin_load_in_flight = false;
    //! A resync was requested mid-scan; run one more pass when it finishes.
    bool m_coin_reload_pending = false;
    //! Reentrancy guard for drainCoinEventQueue (mirrors m_draining).
    bool m_coin_draining = false;

    //! The coin-store scan has ended, with \p error empty if it succeeded.
    //! Runs on the GUI thread.
    void coinSourceLoadEnded(const QString& error);

    // Wallet has an options model for wallet-specific options
    // (transaction fee, for example)
    OptionsModel *optionsModel;

    AddressTableModel *addressTableModel;
    TransactionTableModel *transactionTableModel;

    // Cache some values to be able to detect changes
    qint64 cachedBalance;
    qint64 cachedStake;
    qint64 cachedUnconfirmedBalance;
    qint64 cachedImmatureBalance;
    qint64 cachedNumTransactions;
    EncryptionStatus cachedEncryptionStatus;
    int cachedNumBlocks;

    QTimer *eventDrainTimer;

    //! Coalescing guard for requestEventDrainSoon() (PR4-fix D): true while a
    //! user-requested immediate drain is already scheduled, so a burst of requests
    //! (e.g. per-keystroke filter changes) collapses to one drain. Qt-thread only.
    bool m_event_drain_requested = false;

    //! Reentrancy guard for drainEventQueue() (windowed-model PR5-B): true while a
    //! drain is applying events. A windowed consumer's synchronous fetch path
    //! (DetailedTxModel::fetchWindow) calls drainEventQueue(), and applying a Reset
    //! can synchronously re-enter (viewReset -> restoreAnchor -> setCurrentIndex ->
    //! ...). A nested call no-ops so the outer drain owns the queue and events are
    //! never double-processed. Qt-thread only.
    bool m_draining = false;

    //! Consecutive drain failures that were NOT a node disconnect. Any clean pass
    //! resets it; the periodic pump gives up at MODEL_EVENT_DRAIN_MAX_FAILURES, so
    //! a repeating fault stays bounded without a single transient throw silencing
    //! both windowed views and the balance refresh for the session. Qt-thread only.
    int m_drain_failures = 0;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void applyPushedBalances(const GuiStateCache& cache);

    //! The connection to the cache's changed(), kept so a new cache replaces it.
    QMetaObject::Connection m_gui_state_connection;

    //! Retained interface notification handlers (status and address-book
    //! changes), cleared on teardown. interfaces::Handler disconnects on
    //! destruction. The tx-table producer subscriptions moved node-side into
    //! the WalletTxSource in Phase 1c-ii, so this model no longer holds any
    //! raw core-signal connections.
    std::vector<std::unique_ptr<interfaces::Handler>> m_wallet_handlers;

    //! Closes the destruction race that disconnecting alone leaves open.
    //! See qt/notificationlifetime.h.
    NotificationLifetime m_notify_lifetime;

    //! Set by setElevationBegun() while requestUnlock() waits on its prompt,
    //! and read and cleared by it afterwards. GUI thread only.
    bool m_elevation_begun{false};

    //! requestUnlock() is waiting on its prompt. GUI thread only.
    bool m_unlock_prompt_open{false};


public slots:
    /* Wallet status might have changed */
    void updateStatus();
    /* New, updated or removed address book entry */
    void updateAddressBook(const QString &address, const QString &label, bool isMine, int status);
    /* Drain the WalletEventQueue and apply any pending events to the
     * transaction table model. Fires from eventDrainTimer on a 500ms
     * cadence. Replaces both the legacy QMetaObject::invokeMethod queued
     * connection from CWallet::NotifyTransactionChanged and the
     * 4-second pollBalanceChanged timer that used to drive balance /
     * confirmation refresh — the latter is now event-driven via
     * ChainTipChangedPayload pushed by the uiInterface.NotifyBlocksChanged
     * subscriber. */
    void drainEventQueue();

    //! Re-prime the node-side store under the current datetime-display cutoff
    //! option and kick a drain so the Reset each view cursor receives is applied
    //! promptly. Wired to OptionsModel::LimitTxnDisplayChanged; the windowed
    //! views re-filter off the Resets prime() pushes.
    void reloadTransactionView();

signals:
    // Transaction updated. This is necessary because on a resync from zero with an existing wallet.
    // the numTransactionsChanged signal will not be emitted, and therefore the overpage transaction list
    // needs this signal instead.
    void transactionUpdated();

    //! Fan-out of a drained wallet-event batch to per-view windowed consumers
    //! (PR3: OverviewTxModel). WalletModel drains the queue once and applies the
    //! VIEW_FULL stream to its TransactionTableModel; this delivers the same batch
    //! to the per-view consumers, which filter to their own viewId. Same-thread
    //! (DirectConnection), so the const-ref is passed without a copy.
    void walletEventsDrained(const std::vector<GRC::WalletEvent>& events);

    //! One drained coin-channel batch, fanned out to the coin-selection
    //! consumers (which filter by their view id). Emitted only from
    //! drainCoinEventQueue — the coin queue is drained exactly once, there.
    void coinEventsDrained(const std::vector<GRC::WalletCoinEvent>& events);

    //! The coin store's wallet scan finished and its Reset has been drained,
    //! so the consumers' caches hold the scanned wallet. Emitted on the GUI
    //! thread; consumers clear their loading state here.
    void coinSourceLoadFinished();

    //! The coin store's wallet scan failed with \p message: in the split
    //! build, most often because the core dropped the connection. Nothing was
    //! loaded, coinSourceLoadFinished() is not emitted for it, and the next
    //! ensureCoinSourceLoaded() scans again. Emitted on the GUI thread.
    void coinSourceLoadFailed(const QString& message);

    // Signal that balance in wallet changed
    void balanceChanged(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);

    // Number of transactions in wallet changed
    void numTransactionsChanged(int count);

    // Encryption status of wallet changed
    void encryptionStatusChanged(int status);

    // Signal emitted when wallet needs to be unlocked
    // It is valid behaviour for listeners to keep the wallet locked after this signal;
    // this means that the unlocking failed or was cancelled.
    //! Ask the UI for the passphrase so an operation of \p kind can proceed.
    //! The receiver begins a capability for \p kind with it and, on success,
    //! calls setElevationBegun() before it returns.
    void requireUnlock(CapabilityKind kind);

    // Asynchronous error notification
    void error(const QString &title, const QString &message, bool modal);
};

#endif // BITCOIN_QT_WALLETMODEL_H
