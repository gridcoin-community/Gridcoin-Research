// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_SENDRECIPIENTMODEL_H
#define BITCOIN_QT_QUICK_SENDRECIPIENTMODEL_H

#include <qt/walletmodel.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QString>
#include <QVariant>

//! The send page's rows: a SendCoinsRecipient each, with the amount's text as
//! typed and whether the last send() found the address and the amount valid.
//! There is always at least one row. SendAdapter is its one writer; QML reads
//! it through the roles.
class SendRecipientModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        RecipientRole = Qt::UserRole + 1,
        LabelRole,
        //! The amount in halfords (qint64).
        AmountRole,
        AmountTextRole,
        MessageRole,
        SubtractFeeRole,
        AddressValidRole,
        AmountValidRole,
    };

    //! One row.
    struct Row {
        SendCoinsRecipient recipient;
        QString amount_text;
        bool address_valid{true};
        bool amount_valid{true};
    };

    explicit SendRecipientModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // For SendAdapter, the one writer. Each emits the model's change signals.

    const Row& row(int row) const { return m_rows.at(row); }
    const QList<Row>& rows() const { return m_rows; }
    //! Replaces the row at `row` (a valid row).
    void setRow(int row, const Row& value);
    //! Appends `value`.
    void appendRow(const Row& value);
    //! Removes the row at `row` (a valid row, not the last one left).
    void removeRowAt(int row);
    //! Leaves one empty row.
    void reset();

private:
    QList<Row> m_rows;
};

#endif // BITCOIN_QT_QUICK_SENDRECIPIENTMODEL_H
