#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include <QObject>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"
#include "support/allocators/secure.h" /* for SecureString */

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CWallet;
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
    //! (Phase 1c-ii), which the model does not own — it is created node-side
    //! (interfaces::Init::makeWalletTxSource) and must outlive the model. The
    //! raw CWallet* leg now feeds only the address-table and
    //! transaction-table sub-model constructors, which migrate in a later
    //! phase — do not add new uses.
    explicit WalletModel(interfaces::Wallet& wallet, interfaces::WalletTxSource& tx_source,
                         CWallet* core_wallet, OptionsModel* optionsModel,
                         QObject* parent = nullptr);
    ~WalletModel();

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
        Unencrypted,  // !wallet->IsCrypted()
        Locked,       // wallet->IsCrypted() && wallet->IsLocked()
        Unlocked      // wallet->IsCrypted() && !wallet->IsLocked()
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
    bool changePassphrase(const SecureString& oldPass, const SecureString& newPass);

    // Back up the wallet .dat / config file to dest (pass-throughs to the
    // wallet interface, used by the GUI backup action). Return false on
    // I/O failure.
    bool backupWallet(const std::string& dest);
    bool backupConfigFile(const std::string& dest);

    // RAI object for unlocking wallet, returned by requestUnlock()
    class UnlockContext
    {
    public:
        UnlockContext(WalletModel *wallet, bool valid, bool relock);
        ~UnlockContext();

        bool isValid() const { return valid; }

        // Copy operator and constructor transfer the context
        UnlockContext(const UnlockContext& obj) { CopyFrom(obj); }

        // Commented out as we don't use the below form and it triggers an infinite recursion
        // warning.
        // UnlockContext& operator=(const UnlockContext& rhs) { CopyFrom(rhs); return *this; }
    private:
        WalletModel *wallet;
        bool valid;
        mutable bool relock; // mutable, as it can be set to false by copying

        void CopyFrom(const UnlockContext& rhs);
    };

    UnlockContext requestUnlock();

    bool getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    bool getKeyFromPool(CPubKey& out_public_key, const std::string& label);
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>& vOutpoints) const;
    std::map<std::string, std::vector<interfaces::WalletOutput>> listCoins() const;

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

    int64_t last_balance_update_time = 0;

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

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void checkBalanceChanged();

    //! Retained interface notification handlers (status and address-book
    //! changes), cleared on teardown. interfaces::Handler disconnects on
    //! destruction. The tx-table producer subscriptions moved node-side into
    //! the WalletTxSource in Phase 1c-ii, so this model no longer holds any
    //! raw core-signal connections.
    std::vector<std::unique_ptr<interfaces::Handler>> m_wallet_handlers;


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

    // Signal that balance in wallet changed
    void balanceChanged(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);

    // Number of transactions in wallet changed
    void numTransactionsChanged(int count);

    // Encryption status of wallet changed
    void encryptionStatusChanged(int status);

    // Signal emitted when wallet needs to be unlocked
    // It is valid behaviour for listeners to keep the wallet locked after this signal;
    // this means that the unlocking failed or was cancelled.
    void requireUnlock();

    // Asynchronous error notification
    void error(const QString &title, const QString &message, bool modal);
};

#endif // BITCOIN_QT_WALLETMODEL_H
