#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include <QObject>
#include <vector>
#include <map>

#include "qt/wallet_event_queue.h"
#include "support/allocators/secure.h" /* for SecureString */
#include "wallet/ismine.h"

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CWallet;
class CKeyID;
class CPubKey;
class COutput;
class COutPoint;
class uint256;
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
    explicit WalletModel(CWallet* wallet, OptionsModel* optionsModel, QObject* parent = nullptr);
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
        Aborted
    };

    enum EncryptionStatus
    {
        Unencrypted,  // !wallet->IsCrypted()
        Locked,       // wallet->IsCrypted() && wallet->IsLocked()
        Unlocked      // wallet->IsCrypted() && !wallet->IsLocked()
    };

    OptionsModel *getOptionsModel();
    AddressTableModel *getAddressTableModel();
    TransactionTableModel *getTransactionTableModel();

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

    // Send coins to a list of recipients
    SendCoinsReturn sendCoins(const QList<SendCoinsRecipient>& recipients, const CCoinControl* coinControl = nullptr);

    // Wallet encryption
    bool setWalletEncrypted(const SecureString& passphrase);
    // Passphrase only needed when unlocking
    bool setWalletLocked(bool locked, const SecureString& passPhrase=SecureString());
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
    void getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs);
    void listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const;
    bool isLockedCoin(uint256 hash, unsigned int n) const;
    void lockCoin(COutPoint& output);
    void unlockCoin(COutPoint& output);
    void listLockedCoins(std::vector<COutPoint>& vOutpts);

    //!
    //! \brief Producer→GUI event channel. Producers (core threads firing
    //! NotifyTransactionChanged etc.) push under the locks they already hold;
    //! the Qt-side consumer, WalletModel::drainEventQueue(), is driven by
    //! eventDrainTimer and periodically pops events and applies them to the
    //! transaction table model.
    //!
    GRC::WalletEventQueue& getEventQueue() { return m_event_queue; }

private:
    CWallet *wallet;

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

    //!
    //! \brief MPSC queue carrying producer-side wallet events to the GUI.
    //! Producers push under the locks they already hold; the consumer is
    //! drainEventQueue(), fired by eventDrainTimer every 500ms.
    //!
    GRC::WalletEventQueue m_event_queue;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void checkBalanceChanged();


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
