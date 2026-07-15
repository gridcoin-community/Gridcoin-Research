#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include <QObject>
#include <map>
#include <string>
#include <vector>

#include <boost/signals2/connection.hpp>

#include "interfaces/wallet.h"
#include "qt/wallet_event_queue.h"
#include "qt/wallettxstore.h"
#include "support/allocators/secure.h" /* for SecureString */

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CWallet;
class CKeyID;
class CPubKey;
class COutPoint;
class CCoinControl;

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
    //! boundary (Phase 1c-i); the raw CWallet* leg feeds only the tx-table
    //! store/event-queue machinery, which migrates behind
    //! interfaces::WalletTxSource in Phase 1c-ii — do not add new uses.
    explicit WalletModel(interfaces::Wallet& wallet, CWallet* core_wallet,
                         OptionsModel* optionsModel, QObject* parent = nullptr);
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
        TransactionCreationFailed, // Error returned when wallet is still locked
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
                              const CCoinControl* coinControl = nullptr,
                              qint64 acceptedFee = 0);

    // Wallet encryption
    bool setWalletEncrypted(const SecureString& passphrase);
    // Passphrase only needed when unlocking; stakingOnly restricts the
    // unlock to staking (persisted node-side as the unlock preference).
    bool setWalletLocked(bool locked, const SecureString& passPhrase=SecureString(),
                         bool stakingOnly = false);
    bool changePassphrase(const SecureString& oldPass, const SecureString& newPass);

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
    //! \brief Producer→GUI event channel. Producers (core threads firing
    //! NotifyTransactionChanged etc.) push under the locks they already hold;
    //! the Qt-side consumer, WalletModel::drainEventQueue(), is driven by
    //! eventDrainTimer and periodically pops events and applies them to the
    //! transaction table model.
    //!
    GRC::WalletEventQueue& getEventQueue() { return m_event_queue; }

    //!
    //! \brief Accessor for the producer-owned transaction ordering store. The
    //! producer (NotifyTransactionChanged) mutates it; TransactionTableModel
    //! reads it only at reset time (reloadAndSnapshot).
    //!
    GRC::WalletTxStore& getTxStore() { return m_txStore; }

    //! Kick an immediate (next-event-loop-turn) event-queue drain, so a
    //! user-initiated cursor change (a windowed-view filter/sort) is reflected
    //! without waiting for the periodic drain tick (windowed-model PR4-fix D).
    void requestEventDrainSoon();

private:
    //! Interface boundary for the query/command surface (Phase 1c-i).
    interfaces::Wallet& m_wallet;

    //! Raw wallet pointer feeding ONLY the tx-table store/event-queue
    //! producer leg (m_txStore, the NotifyTransactionChanged handler, and
    //! the sub-models' constructors), which migrates behind
    //! interfaces::WalletTxSource in Phase 1c-ii. Do not add new uses.
    CWallet *m_core_wallet;

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

    //!
    //! \brief MPSC queue carrying producer-side wallet events to the GUI.
    //! Producers push under the locks they already hold; the consumer is
    //! drainEventQueue(), fired by eventDrainTimer every 500ms.
    //!
    GRC::WalletEventQueue m_event_queue;

    //!
    //! \brief Producer-owned transaction ordering store. Declared AFTER
    //! m_event_queue so the WalletEventQueue& it holds is bound to a fully
    //! constructed object (member init order == declaration order).
    //!
    GRC::WalletTxStore m_txStore;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void checkBalanceChanged();

    //! Retained core-signal connections for the tx-table producer leg
    //! (Phase 1c-ii), cleared on teardown so a signal that fires after this
    //! model is destroyed cannot invoke a slot bound to freed memory.
    //! scoped_connection disconnects on destruction (issue #3129).
    std::vector<boost::signals2::scoped_connection> m_handlers;

    //! Retained interface notification handlers (status and address-book
    //! changes), cleared on teardown. interfaces::Handler disconnects on
    //! destruction.
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
