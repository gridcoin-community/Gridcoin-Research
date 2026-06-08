#include "transactionfilterproxy.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "txfilter.h"
#include "util/system.h"

#include <QDateTime>

#include <algorithm>

// Guard the standalone (Qt-free) mirrors in txfilter.h against the authoritative
// Qt-side enums so they can never silently drift.
static_assert(GRC::TXSTATUS_CONFLICTED  == TransactionStatus::Conflicted,
              "TXSTATUS_CONFLICTED mirror out of sync with TransactionStatus::Conflicted");
static_assert(GRC::TXSTATUS_NOTACCEPTED == TransactionStatus::NotAccepted,
              "TXSTATUS_NOTACCEPTED mirror out of sync with TransactionStatus::NotAccepted");
static_assert(static_cast<int>(GRC::TXCOL_STATUS)  == static_cast<int>(TransactionTableModel::Status),    "TXCOL_STATUS mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_DATE)    == static_cast<int>(TransactionTableModel::Date),      "TXCOL_DATE mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_TYPE)    == static_cast<int>(TransactionTableModel::Type),      "TXCOL_TYPE mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_ADDRESS) == static_cast<int>(TransactionTableModel::ToAddress), "TXCOL_ADDRESS mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_AMOUNT)  == static_cast<int>(TransactionTableModel::Amount),    "TXCOL_AMOUNT mirror drift");

// Earliest date that can be represented (far in the past)
const QDateTime TransactionFilterProxy::MIN_DATE = QDateTime::fromSecsSinceEpoch(0);
// Last date that can be represented (far in the future)
const QDateTime TransactionFilterProxy::MAX_DATE = QDateTime::fromSecsSinceEpoch(0xFFFFFFFF);

//Halford 1-2-2015
TransactionFilterProxy::TransactionFilterProxy(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_spec()
{
    // The spec defaults already reproduce the prior initial state (all dates,
    // ALL_TYPES, no address filter, min_amount 0, unlimited, show_inactive
    // true). The one runtime input is -showorphans, read once here. The
    // original read it via gArgs on every filterAcceptsRow call; reading it
    // once at construction is behavior-identical because -showorphans is a
    // launch-time argument with no runtime mutation path (no GUI/RPC code
    // calls ForceSetArg/SoftSetArg on it), so its value is fixed for the
    // process lifetime.
    m_spec.show_orphans = gArgs.GetBoolArg("-showorphans", false);
}

bool TransactionFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    // Project the model roles into the Qt-free field set, then evaluate the
    // shared predicate. AmountRole is the SIGNED net amount; Accepts() takes
    // the absolute value itself.
    GRC::TxFilterFields fields;
    fields.type       = index.data(TransactionTableModel::TypeRole).toInt();
    fields.time       = index.data(TransactionTableModel::DateRole).toDateTime().toSecsSinceEpoch();
    fields.address    = index.data(TransactionTableModel::AddressRole).toString().toStdString();
    fields.label      = index.data(TransactionTableModel::LabelRole).toString().toStdString();
    fields.net_amount = index.data(TransactionTableModel::AmountRole).toLongLong();
    fields.status     = index.data(TransactionTableModel::StatusRole).toInt();

    return GRC::Accepts(fields, m_spec);
}

// Note that invalidateFilter() is just a deprecated alias for invalidate(), so these have been changed to invalidate()
// to silence the compiler warnings for Qt 6+.
// TODO: Redesign to narrow scope of invalidation.

void TransactionFilterProxy::setDateRange(const QDateTime &from, const QDateTime &to)
{
    m_spec.date_from = from.toSecsSinceEpoch();
    m_spec.date_to = to.toSecsSinceEpoch();
    invalidate();
}

void TransactionFilterProxy::setAddressPrefix(const QString &addrPrefix)
{
    m_spec.address_substr = addrPrefix.toStdString();
    invalidate();
}

void TransactionFilterProxy::setTypeFilter(quint32 modes)
{
    m_spec.type_mask = modes;
    invalidate();
}

void TransactionFilterProxy::setMinAmount(qint64 minimum)
{
    m_spec.min_amount = minimum;
    invalidate();
}

void TransactionFilterProxy::setLimit(int limit)
{
    m_spec.limit_rows = limit;
    invalidate();
}

int TransactionFilterProxy::getLimit()
{
    return m_spec.limit_rows;
}

void TransactionFilterProxy::setShowInactive(bool showInactive)
{
    m_spec.show_inactive = showInactive;
    invalidate();
}

int TransactionFilterProxy::rowCount(const QModelIndex &parent) const
{
    if(m_spec.limit_rows != -1)
    {
        return std::min(QSortFilterProxyModel::rowCount(parent), static_cast<int>(m_spec.limit_rows));
    }
    else
    {
        return QSortFilterProxyModel::rowCount(parent);
    }
}
