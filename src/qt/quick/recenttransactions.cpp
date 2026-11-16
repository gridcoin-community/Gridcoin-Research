// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/recenttransactions.h>

#include <interfaces/wallet_tx_channel.h>
#include <interfaces/wallet_tx_record.h>
#include <interfaces/wallet_tx_source.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/overviewtxmodel.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <QDateTime>
#include <QJSEngine>

namespace {

//! The served-row limit OverviewTxModel is built with.
constexpr int INITIAL_LIMIT = 21;

//! The icon for a record's type and mined type: Ziggy Schulz's table from his
//! TransactionTableModel's GetIconForTransaction, with its default mined case
//! pointing at the help icon the carried tree has.
QString IconSource(int type, int mined_type)
{
    switch (type) {
    case TransactionRecord::Generated:
        switch (mined_type) {
        case GRC::MinedType::POS: return QStringLiteral("qrc:/icons/events/ic_event_yellow.svg");
        case GRC::MinedType::POR: return QStringLiteral("qrc:/icons/events/ic_event_purple.svg");
        case GRC::MinedType::ORPHANED: return QStringLiteral("qrc:/icons/generic/ic_alert.svg");
        case GRC::MinedType::POS_SIDE_STAKE_RCV: return QStringLiteral("qrc:/icons/events/ic_event_yellow_green.svg");
        case GRC::MinedType::POR_SIDE_STAKE_RCV: return QStringLiteral("qrc:/icons/events/ic_event_purple_green.svg");
        case GRC::MinedType::POS_SIDE_STAKE_SEND: return QStringLiteral("qrc:/icons/events/ic_event_yellow_red.svg");
        case GRC::MinedType::POR_SIDE_STAKE_SEND: return QStringLiteral("qrc:/icons/events/ic_event_purple_red.svg");
        case GRC::MinedType::MRC_RCV: return QStringLiteral("qrc:/icons/events/ic_event_purple_green.svg");
        case GRC::MinedType::MRC_SEND: return QStringLiteral("qrc:/icons/events/ic_event_purple_red.svg");
        case GRC::MinedType::SUPERBLOCK: return QStringLiteral("qrc:/icons/events/superblock.svg");
        default: return QStringLiteral("qrc:/icons/generic/ic_help.svg");
        }
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::RecvFromOther:
        return QStringLiteral("qrc:/icons/events/ic_event_green.svg");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return QStringLiteral("qrc:/icons/events/ic_event_red.svg");
    case TransactionRecord::BeaconAdvertisement:
        return QStringLiteral("qrc:/icons/events/ic_event_beacon.svg");
    case TransactionRecord::Poll:
    case TransactionRecord::Vote:
        return QStringLiteral("qrc:/icons/events/ic_event_voting.svg");
    case TransactionRecord::MRC:
        return QStringLiteral("qrc:/icons/events/ic_event_mrc.svg");
    case TransactionRecord::Message:
        return QStringLiteral("qrc:/icons/events/ic_event_message.svg");
    default:
        return QStringLiteral("qrc:/icons/events/ic_event_green_red.svg");
    }
}

} // namespace

RecentTransactions::RecentTransactions(int marker)
    : QIdentityProxyModel(nullptr), m_marker(marker), m_unit(BitcoinUnits::BTC)
{
}

RecentTransactions::~RecentTransactions()
{
    setSourceModel(nullptr);
    m_source.reset();
}

RecentTransactions* RecentTransactions::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("RecentTransactions::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->recentTransactions(), QJSEngine::CppOwnership);
    return &provider->recentTransactions();
}

void RecentTransactions::attach(WalletModel& wallet, OptionsModel* options, QmlCallRunner& runner)
{
    m_wallet = &wallet;
    m_options = options;
    m_runner = &runner;
    m_limit = INITIAL_LIMIT;
    m_posted_limit = INITIAL_LIMIT;
    // A limit posted by an earlier attach never delivers.
    m_limit_in_flight = false;
    m_limit_dirty = false;

    if (options) {
        m_connections.push_back(
            connect(options, &OptionsModel::MaskValuesChanged, this, &RecentTransactions::onMaskValuesChanged));
        m_connections.push_back(
            connect(options, &OptionsModel::displayUnitChanged, this, &RecentTransactions::onDisplayUnitChanged));
        m_unit = options->getDisplayUnit();
        onMaskValuesChanged(options->getMaskValues());
    }

    // Registers VIEW_OVERVIEW and reads its first rows, on the GUI thread.
    m_source = std::make_unique<OverviewTxModel>(&wallet, INITIAL_LIMIT);
    setSourceModel(m_source.get());
}

void RecentTransactions::detach()
{
    for (const QMetaObject::Connection& connection : m_connections) disconnect(connection);
    m_connections.clear();
    ++m_epoch;
    // Stop serving before the model goes, so no view reads a dead source; the
    // OverviewTxModel's destructor unregisters VIEW_OVERVIEW.
    setSourceModel(nullptr);
    m_source.reset();
    m_wallet = nullptr;
    m_options = nullptr;
    m_runner = nullptr;
}

void RecentTransactions::setVisibleRows(int n)
{
    m_visible_rows = n;
    if (!m_wallet || !m_runner) return;

    // OverviewPage::updateTransactions' stairstep: most resizes leave the
    // limit alone.
    if (m_limit < n || m_limit > n * 6) {
        m_limit = n * 3;
        postLimit();
    }
}

QVariant RecentTransactions::data(const QModelIndex& index, int role) const
{
    if (!m_source || !index.isValid()) return QVariant();
    const QModelIndex source = mapToSource(index);
    if (!source.isValid()) return QVariant();

    switch (role) {
    case IconSourceRole:
        return IconSource(source.data(TransactionTableModel::TypeRole).toInt(),
                          source.data(TransactionTableModel::GeneratedTypeRole).toInt());
    case AmountRole:
        return source.data(TransactionTableModel::AmountRole);
    case AmountTextRole:
        return source.data(TransactionTableModel::FormattedAmountRole).toString() + QStringLiteral(" ") +
               BitcoinUnits::name(m_unit);
    case NegativeRole:
        return source.data(TransactionTableModel::AmountRole).toLongLong() < 0;
    case AddressRole:
        // The record's address, not the address-book label: no wallet call
        // per row.
        return source.data(TransactionTableModel::AddressRole);
    case DateRole:
        return source.data(TransactionTableModel::DateRole);
    case ConfirmedRole:
        return source.data(TransactionTableModel::ConfirmedRole);
    }
    return QVariant();
}

QHash<int, QByteArray> RecentTransactions::roleNames() const
{
    // Exactly these: the inherited set is Qt's defaults, and display and edit
    // on the address column reach the wallet for an address-book label.
    return {
        {IconSourceRole, QByteArrayLiteral("iconSource")},
        {AmountRole, QByteArrayLiteral("amount")},
        {AmountTextRole, QByteArrayLiteral("amountText")},
        {NegativeRole, QByteArrayLiteral("negative")},
        {AddressRole, QByteArrayLiteral("address")},
        {DateRole, QByteArrayLiteral("date")},
        {ConfirmedRole, QByteArrayLiteral("confirmed")},
    };
}

void RecentTransactions::onMaskValuesChanged(bool masked)
{
    if (masked == m_masked) return;
    m_masked = masked;
    emit maskedChanged();
}

void RecentTransactions::onDisplayUnitChanged(int unit)
{
    m_unit = unit;
    const int rows = rowCount();
    if (rows > 0) emit dataChanged(index(0, 0), index(rows - 1, 0), {AmountTextRole});
}

void RecentTransactions::postLimit()
{
    // Attached only: setVisibleRows checks, and a delivery runs only in the
    // epoch of the attach that posted it.
    if (m_limit_in_flight) {
        m_limit_dirty = true;
        return;
    }
    if (m_limit == m_posted_limit) return;

    interfaces::WalletTxSource& source = m_wallet->txSource();
    const int limit = m_limit;
    const uint64_t epoch = m_epoch;
    // The store pushes the boundary events and the view's high-water mark in
    // one hold of its lock, and getRows reads that mark under the same lock,
    // so OverviewTxModel's seqno skip orders this change against the GUI
    // thread's reads.
    if (m_runner->postSerial([&source, limit] { source.setViewLimit(GRC::VIEW_OVERVIEW, limit); },
                             [this, epoch] { deliverLimit(epoch); })) {
        m_limit_in_flight = true;
        m_limit_dirty = false;
        m_posted_limit = limit;
    }
}

void RecentTransactions::deliverLimit(uint64_t epoch)
{
    if (epoch != m_epoch) return;
    m_limit_in_flight = false;
    if (m_limit_dirty) {
        m_limit_dirty = false;
        postLimit();
    }
}
