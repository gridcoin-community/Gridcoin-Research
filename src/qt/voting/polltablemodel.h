// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H
#define GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H

#include "gridcoin/voting/filter.h"

#include <memory>
#include <QSortFilterProxyModel>
#include <QMutex>

class PollItem;
class VotingModel;

class PollTableModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum ColumnIndex
    {
        Title,
        PollType,
        Duration,
        Expiration,
        WeightType,
        TotalVotes,
        TotalWeight,
        VotePercentAVW,
        Validated,
        TopAnswer,
    };

    enum Roles
    {
        SortRole = Qt::UserRole,
    };

    explicit PollTableModel(QObject* parent = nullptr);
    ~PollTableModel();

    void setModel(VotingModel* model = nullptr);
    void setPollFilterFlags(GRC::PollFilterFlag flags);
    bool includesActivePolls() const;

    int size() const;
    bool empty() const;
    QString columnName(int offset) const;
    const PollItem* rowItem(int row) const;

public slots:
    void refresh();
    void changeTitleFilter(const QString& pattern);
    Qt::SortOrder sort(int column);

private:
    VotingModel* m_model;
    std::unique_ptr<QAbstractTableModel> m_data_model;
    GRC::PollFilterFlag m_filter_flags;
    QMutex m_refresh_mutex;
};

#endif // GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H
