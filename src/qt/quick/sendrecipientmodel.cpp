// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/sendrecipientmodel.h>

SendRecipientModel::SendRecipientModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_rows.append(Row{});
}

int SendRecipientModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SendRecipientModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return QVariant();
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case RecipientRole: return r.recipient.address;
    case LabelRole: return r.recipient.label;
    case AmountRole: return QVariant::fromValue<qint64>(r.recipient.amount);
    case AmountTextRole: return r.amount_text;
    case MessageRole: return r.recipient.Message;
    case SubtractFeeRole: return r.recipient.fSubtractFeeFromAmount;
    case AddressValidRole: return r.address_valid;
    case AmountValidRole: return r.amount_valid;
    }
    return QVariant();
}

QHash<int, QByteArray> SendRecipientModel::roleNames() const
{
    return {
        {RecipientRole, QByteArrayLiteral("recipient")},
        {LabelRole, QByteArrayLiteral("label")},
        {AmountRole, QByteArrayLiteral("amount")},
        {AmountTextRole, QByteArrayLiteral("amountText")},
        {MessageRole, QByteArrayLiteral("message")},
        {SubtractFeeRole, QByteArrayLiteral("subtractFee")},
        {AddressValidRole, QByteArrayLiteral("addressValid")},
        {AmountValidRole, QByteArrayLiteral("amountValid")},
    };
}

void SendRecipientModel::setRow(int row, const Row& value)
{
    m_rows[row] = value;
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed);
}

void SendRecipientModel::appendRow(const Row& value)
{
    const int row = static_cast<int>(m_rows.size());
    beginInsertRows(QModelIndex(), row, row);
    m_rows.append(value);
    endInsertRows();
}

void SendRecipientModel::removeRowAt(int row)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_rows.removeAt(row);
    endRemoveRows();
}

void SendRecipientModel::reset()
{
    beginResetModel();
    m_rows.clear();
    m_rows.append(Row{});
    endResetModel();
}
