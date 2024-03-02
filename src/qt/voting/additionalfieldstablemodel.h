// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_ADDITIONALFIELDSTABLEMODEL_H
#define GRIDCOIN_QT_VOTING_ADDITIONALFIELDSTABLEMODEL_H

#include <QWidget>
#include <memory>
#include <QSortFilterProxyModel>
#include <QMutex>

class PollItem;
class AdditionalFieldEntry;

class AdditionalFieldsTableModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum ColumnIndex
    {
        Name,
        Value,
        Required
    };

    enum Roles
    {
        SortRole = Qt::UserRole
    };

    explicit AdditionalFieldsTableModel(QObject* parent = nullptr);
    ~AdditionalFieldsTableModel();

    void setPollItem(const PollItem* poll_item = nullptr);

    int size() const;
    bool empty() const;
    QString columnName(int offset) const;
    const AdditionalFieldEntry* rowItem(int row) const;

public slots:
    void refresh();
    Qt::SortOrder custom_sort(int column);

private:
    const PollItem* m_poll_item;
    std::unique_ptr<QAbstractTableModel> m_data_model;
};

#endif // GRIDCOIN_QT_VOTING_ADDITIONALFIELDSTABLEMODEL_H
