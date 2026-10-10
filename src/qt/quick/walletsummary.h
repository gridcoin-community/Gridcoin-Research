// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_WALLETSUMMARY_H
#define BITCOIN_QT_QUICK_WALLETSUMMARY_H

#include <QtQml/qqmlregistration.h>

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <type_traits>
#include <vector>

class Clipboard;
class OptionsModel;
class QmlCallRunner;
class QQmlEngine;
class QJSEngine;
class WalletModel;

//! The QML singleton `WalletSummary`: the wallet's balances, the display unit,
//! the mask state and the wallet's lock state, as the Widgets overview and
//! status bar show them. It is owned by QmlProvider, which hands QML this
//! instance through create(). Amounts are halfords (qint64) with a text
//! formatted in C++ in the display unit; QML does no arithmetic on them. The
//! live values arrive as WalletModel's and OptionsModel's signals; the seed
//! reads at attach run on the call runner's serial lane.
class WalletSummary : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WalletSummary)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)
    Q_PROPERTY(qint64 balance READ balance NOTIFY balancesChanged)
    Q_PROPERTY(qint64 stake READ stake NOTIFY balancesChanged)
    Q_PROPERTY(qint64 unconfirmedBalance READ unconfirmedBalance NOTIFY balancesChanged)
    Q_PROPERTY(qint64 immatureBalance READ immatureBalance NOTIFY balancesChanged)
    Q_PROPERTY(qint64 total READ total NOTIFY balancesChanged)
    Q_PROPERTY(QString balanceText READ balanceText NOTIFY balancesChanged)
    Q_PROPERTY(QString stakeText READ stakeText NOTIFY balancesChanged)
    Q_PROPERTY(QString unconfirmedBalanceText READ unconfirmedBalanceText NOTIFY balancesChanged)
    Q_PROPERTY(QString immatureBalanceText READ immatureBalanceText NOTIFY balancesChanged)
    Q_PROPERTY(QString totalText READ totalText NOTIFY balancesChanged)
    Q_PROPERTY(QString balanceRoundedText READ balanceRoundedText NOTIFY balancesChanged)
    Q_PROPERTY(bool immatureVisible READ immatureVisible NOTIFY balancesChanged)
    Q_PROPERTY(Unit displayUnit READ displayUnit NOTIFY displayUnitChanged)
    Q_PROPERTY(QString displayUnitName READ displayUnitName NOTIFY displayUnitChanged)
    Q_PROPERTY(bool maskValues READ maskValues NOTIFY maskValuesChanged)
    Q_PROPERTY(EncryptionStatus encryptionStatus READ encryptionStatus NOTIFY encryptionStatusChanged)
    Q_PROPERTY(QString encryptionToolTip READ encryptionToolTip NOTIFY encryptionStatusChanged)
    Q_PROPERTY(bool encryptionStatusKnown READ encryptionStatusKnown NOTIFY encryptionStatusKnownChanged)

public:
    //! BitcoinUnits::Unit's BTC, mBTC and uBTC.
    enum Unit { GRC, MilliGRC, MicroGRC };
    Q_ENUM(Unit)
    //! WalletModel::EncryptionStatus.
    enum EncryptionStatus { Unencrypted, Locked, Unlocked, UnlockedForStakingOnly };
    Q_ENUM(EncryptionStatus)
    //! The amounts copyAmount() can copy.
    enum AmountField { Balance, Stake, Unconfirmed, Immature, Total };
    Q_ENUM(AmountField)

    //! `marker` identifies the provider that owns this instance; copyAmount()
    //! writes through `clipboard`, which must outlive this object's use.
    WalletSummary(int marker, Clipboard& clipboard);

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static WalletSummary* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! Follows the wallet model's and the options model's signals and posts
    //! the two seed reads. A null `options` means the unit GRC, unmasked, and
    //! no option signals. The models and the runner must outlive the matching
    //! detach().
    void attach(WalletModel& wallet, OptionsModel* options, QmlCallRunner& runner);
    //! Disconnects from the models; a read still in flight delivers nothing.
    //! The values are kept. A detach() before attach() does nothing.
    void detach();

    //! Sets the options model's mask state; unattached, nothing.
    Q_INVOKABLE void setMaskValues(bool masked);
    //! Copies the field's amount, in the display unit and without digit-group
    //! separators, to the clipboard. Refused (false) while the amounts are
    //! masked, as BalanceCopyFilter refuses, or while unattached.
    Q_INVOKABLE bool copyAmount(WalletSummary::AmountField field);

    int marker() const { return m_marker; }
    qint64 balance() const { return m_balance; }
    qint64 stake() const { return m_stake; }
    qint64 unconfirmedBalance() const { return m_unconfirmed_balance; }
    qint64 immatureBalance() const { return m_immature_balance; }
    qint64 total() const { return m_balance + m_stake + m_unconfirmed_balance + m_immature_balance; }
    QString balanceText() const { return m_balance_text; }
    QString stakeText() const { return m_stake_text; }
    QString unconfirmedBalanceText() const { return m_unconfirmed_balance_text; }
    QString immatureBalanceText() const { return m_immature_balance_text; }
    QString totalText() const { return m_total_text; }
    QString balanceRoundedText() const { return m_balance_rounded_text; }
    bool immatureVisible() const { return m_immature_balance != 0; }
    Unit displayUnit() const { return m_unit; }
    QString displayUnitName() const;
    bool maskValues() const { return m_masked; }
    EncryptionStatus encryptionStatus() const { return m_encryption_status; }
    QString encryptionToolTip() const { return m_encryption_tooltip; }
    bool encryptionStatusKnown() const { return m_encryption_status_known; }

signals:
    void balancesChanged();
    void displayUnitChanged();
    void maskValuesChanged();
    void encryptionStatusChanged();
    void encryptionStatusKnownChanged();

private:
    struct LockResult;
    struct BalanceResult;

    void onBalanceChanged(qint64 balance, qint64 stake, qint64 unconfirmed_balance, qint64 immature_balance);
    void onEncryptionStatusChanged(int status);
    void onDisplayUnitChanged(int unit);
    void onMaskValuesChanged(bool masked);

    void postHydrate();
    void deliverLock(uint64_t epoch, const LockResult& result);
    void deliverBalances(uint64_t epoch, const BalanceResult& result);

    //! Assigns the balances and re-formats every text; emits once on a change.
    void setBalances(qint64 balance, qint64 stake, qint64 unconfirmed_balance, qint64 immature_balance);
    //! Re-formats every text in the current unit and mask state.
    void formatTexts();
    //! Assigns the lock state and marks it known; each NOTIFY is emitted only
    //! on a change.
    void setEncryptionStatus(EncryptionStatus status);
    //! The display unit as a BitcoinUnits::Unit value.
    int bitcoinUnit() const;

    int m_marker;
    Clipboard& m_clipboard;
    WalletModel* m_wallet{nullptr};
    OptionsModel* m_options{nullptr};
    QmlCallRunner* m_runner{nullptr};
    std::vector<QMetaObject::Connection> m_connections;
    //! Moved by every detach(); a delivery of an earlier epoch does nothing.
    uint64_t m_epoch{0};

    qint64 m_balance{0};
    qint64 m_stake{0};
    qint64 m_unconfirmed_balance{0};
    qint64 m_immature_balance{0};
    QString m_balance_text;
    QString m_stake_text;
    QString m_unconfirmed_balance_text;
    QString m_immature_balance_text;
    QString m_total_text;
    QString m_balance_rounded_text;
    Unit m_unit{GRC};
    bool m_masked{false};
    EncryptionStatus m_encryption_status{Unencrypted};
    QString m_encryption_tooltip;
    bool m_encryption_status_known{false};

    // The fields a model signal set since the matching seed read was posted;
    // the seed read does not overwrite them.
    bool m_signal_balances{false};
    bool m_signal_lock{false};
};

static_assert(!std::is_default_constructible_v<WalletSummary>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_WALLETSUMMARY_H
