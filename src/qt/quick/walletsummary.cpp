// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/walletsummary.h>

#include <interfaces/wallet.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/quick/clipboard.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <qt/walletmodel.h>

#include <QCoreApplication>
#include <QJSEngine>

#include <memory>

namespace {

//! WalletModel::getEncryptionStatus's mapping of the lock state.
WalletSummary::EncryptionStatus MapLockState(const interfaces::WalletLockState& lock_state)
{
    if (!lock_state.crypted) return WalletSummary::Unencrypted;
    if (lock_state.locked) return WalletSummary::Locked;
    if (lock_state.unlocked_for_staking_only) return WalletSummary::UnlockedForStakingOnly;
    return WalletSummary::Unlocked;
}

//! WalletModel::EncryptionStatus, as encryptionStatusChanged carries it.
WalletSummary::EncryptionStatus MapModelStatus(int status)
{
    switch (status) {
    case WalletModel::Locked: return WalletSummary::Locked;
    case WalletModel::Unlocked: return WalletSummary::Unlocked;
    case WalletModel::UnlockedForStakingOnly: return WalletSummary::UnlockedForStakingOnly;
    default: return WalletSummary::Unencrypted;
    }
}

//! BitcoinGUI::setEncryptionStatus's tooltips, with their texts and context.
QString EncryptionToolTip(WalletSummary::EncryptionStatus status)
{
    switch (status) {
    case WalletSummary::Unencrypted:
        return QCoreApplication::translate("BitcoinGUI", "Wallet is <b>not encrypted</b>!");
    case WalletSummary::Locked:
        return QCoreApplication::translate("BitcoinGUI", "Wallet is <b>encrypted</b> and currently <b>locked</b>");
    case WalletSummary::Unlocked:
    case WalletSummary::UnlockedForStakingOnly:
        return QCoreApplication::translate("BitcoinGUI", "Wallet is <b>encrypted</b> and currently %1 ")
            .arg(status == WalletSummary::UnlockedForStakingOnly
                     ? QCoreApplication::translate("BitcoinGUI", "<b>unlocked for staking only</b>")
                     : QCoreApplication::translate("BitcoinGUI", "<b>unlocked</b>"));
    }
    return QString();
}

//! The adapter's unit for a BitcoinUnits::Unit value; anything else is GRC.
WalletSummary::Unit MapUnit(int unit)
{
    switch (unit) {
    case BitcoinUnits::mBTC: return WalletSummary::MilliGRC;
    case BitcoinUnits::uBTC: return WalletSummary::MicroGRC;
    default: return WalletSummary::GRC;
    }
}

} // namespace

//! What the lock-state seed read reads. A plain value.
struct WalletSummary::LockResult {
    interfaces::WalletLockState lock_state;
};

//! What the balance seed read reads. A plain value.
struct WalletSummary::BalanceResult {
    int64_t balance{0};
    int64_t stake{0};
    int64_t unconfirmed_balance{0};
    int64_t immature_balance{0};
};

WalletSummary::WalletSummary(int marker, Clipboard& clipboard)
    : QObject(nullptr), m_marker(marker), m_clipboard(clipboard)
{
    formatTexts();
    m_encryption_tooltip = EncryptionToolTip(m_encryption_status);
}

WalletSummary* WalletSummary::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("WalletSummary::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->walletSummary(), QJSEngine::CppOwnership);
    return &provider->walletSummary();
}

void WalletSummary::attach(WalletModel& wallet, OptionsModel* options, QmlCallRunner& runner)
{
    m_wallet = &wallet;
    m_options = options;
    m_runner = &runner;

    m_connections.push_back(connect(&wallet, &WalletModel::balanceChanged, this, &WalletSummary::onBalanceChanged));
    m_connections.push_back(
        connect(&wallet, &WalletModel::encryptionStatusChanged, this, &WalletSummary::onEncryptionStatusChanged));
    if (options) {
        m_connections.push_back(
            connect(options, &OptionsModel::displayUnitChanged, this, &WalletSummary::onDisplayUnitChanged));
        m_connections.push_back(
            connect(options, &OptionsModel::MaskValuesChanged, this, &WalletSummary::onMaskValuesChanged));
        onDisplayUnitChanged(options->getDisplayUnit());
        onMaskValuesChanged(options->getMaskValues());
    }

    postHydrate();
}

void WalletSummary::detach()
{
    for (const QMetaObject::Connection& connection : m_connections) disconnect(connection);
    m_connections.clear();
    ++m_epoch;
    m_wallet = nullptr;
    m_options = nullptr;
    m_runner = nullptr;
}

void WalletSummary::setMaskValues(bool masked)
{
    if (!m_wallet || !m_options) return;
    m_options->setMaskValues(masked);
}

bool WalletSummary::copyAmount(WalletSummary::AmountField field)
{
    if (!m_wallet || m_masked) return false;

    qint64 amount = 0;
    switch (field) {
    case Balance: amount = m_balance; break;
    case Stake: amount = m_stake; break;
    case Unconfirmed: amount = m_unconfirmed_balance; break;
    case Immature: amount = m_immature_balance; break;
    case Total: amount = total(); break;
    }
    // Clipboard::setText strips the digit-group separators format() inserts.
    m_clipboard.setText(BitcoinUnits::format(bitcoinUnit(), amount));
    return true;
}

QString WalletSummary::displayUnitName() const
{
    return BitcoinUnits::name(bitcoinUnit());
}

void WalletSummary::onBalanceChanged(qint64 balance, qint64 stake, qint64 unconfirmed_balance,
                                     qint64 immature_balance)
{
    m_signal_balances = true;
    setBalances(balance, stake, unconfirmed_balance, immature_balance);
}

void WalletSummary::onEncryptionStatusChanged(int status)
{
    m_signal_lock = true;
    setEncryptionStatus(MapModelStatus(status));
}

void WalletSummary::onDisplayUnitChanged(int unit)
{
    const Unit mapped = MapUnit(unit);
    if (mapped == m_unit) return;
    m_unit = mapped;
    emit displayUnitChanged();
    formatTexts();
    emit balancesChanged();
}

void WalletSummary::onMaskValuesChanged(bool masked)
{
    if (masked == m_masked) return;
    m_masked = masked;
    emit maskValuesChanged();
    formatTexts();
    emit balancesChanged();
}

void WalletSummary::postHydrate()
{
    if (!m_wallet || !m_runner) return;

    interfaces::Wallet& wallet = m_wallet->wallet();
    const uint64_t epoch = m_epoch;

    // (1) The lock state, as its own read, so it is known before any balance
    // scan.
    auto lock = std::make_shared<LockResult>();
    m_signal_lock = false;
    (void)m_runner->postSerial([&wallet, lock] { lock->lock_state = wallet.getLockState(); },
                               [this, lock, epoch] { deliverLock(epoch, *lock); });

    // (2) The balances: one coherent snapshot, or the four blocking getters
    // when the snapshot bows out (then the total can mix two tips until the
    // next balanceChanged).
    auto balances = std::make_shared<BalanceResult>();
    m_signal_balances = false;
    (void)m_runner->postSerial(
        [&wallet, balances] {
            interfaces::WalletBalances snapshot;
            if (wallet.tryGetBalances(snapshot)) {
                balances->balance = snapshot.balance;
                balances->stake = snapshot.stake;
                balances->unconfirmed_balance = snapshot.unconfirmed_balance;
                balances->immature_balance = snapshot.immature_balance;
            } else {
                balances->balance = wallet.getBalance();
                balances->stake = wallet.getStake();
                balances->unconfirmed_balance = wallet.getUnconfirmedBalance();
                balances->immature_balance = wallet.getImmatureBalance();
            }
        },
        [this, balances, epoch] { deliverBalances(epoch, *balances); });
}

void WalletSummary::deliverLock(uint64_t epoch, const LockResult& result)
{
    if (epoch != m_epoch) return;
    // A model signal received since the read was posted is newer.
    if (m_signal_lock) return;
    setEncryptionStatus(MapLockState(result.lock_state));
}

void WalletSummary::deliverBalances(uint64_t epoch, const BalanceResult& result)
{
    if (epoch != m_epoch) return;
    // A balanceChanged received since the read was posted is newer.
    if (m_signal_balances) return;
    setBalances(result.balance, result.stake, result.unconfirmed_balance, result.immature_balance);
}

void WalletSummary::setBalances(qint64 balance, qint64 stake, qint64 unconfirmed_balance, qint64 immature_balance)
{
    if (balance == m_balance && stake == m_stake && unconfirmed_balance == m_unconfirmed_balance &&
        immature_balance == m_immature_balance) {
        return;
    }
    m_balance = balance;
    m_stake = stake;
    m_unconfirmed_balance = unconfirmed_balance;
    m_immature_balance = immature_balance;
    formatTexts();
    emit balancesChanged();
}

void WalletSummary::formatTexts()
{
    // OverviewPage::setBalance's formatting.
    const int unit = bitcoinUnit();
    m_balance_text = BitcoinUnits::formatWithPrivacy(unit, m_balance, m_masked);
    m_stake_text = BitcoinUnits::formatWithPrivacy(unit, m_stake, m_masked);
    m_unconfirmed_balance_text = BitcoinUnits::formatWithPrivacy(unit, m_unconfirmed_balance, m_masked);
    m_immature_balance_text = BitcoinUnits::formatWithPrivacy(unit, m_immature_balance, m_masked);
    m_total_text = BitcoinUnits::formatWithPrivacy(unit, total(), m_masked);
    m_balance_rounded_text = BitcoinUnits::formatOverviewRounded(m_balance, m_masked);
}

void WalletSummary::setEncryptionStatus(EncryptionStatus status)
{
    if (status != m_encryption_status) {
        m_encryption_status = status;
        m_encryption_tooltip = EncryptionToolTip(status);
        emit encryptionStatusChanged();
    }
    if (!m_encryption_status_known) {
        m_encryption_status_known = true;
        emit encryptionStatusKnownChanged();
    }
}

int WalletSummary::bitcoinUnit() const
{
    switch (m_unit) {
    case MilliGRC: return BitcoinUnits::mBTC;
    case MicroGRC: return BitcoinUnits::uBTC;
    case GRC: break;
    }
    return BitcoinUnits::BTC;
}
