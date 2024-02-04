// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H
#define GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H

#include "uint256.h"
#include "gridcoin/voting/filter.h"

#include <memory>
#include <QSortFilterProxyModel>
#include <QMutex>

class PollItem;
class VotingModel;

class PollTableDataModel : public QAbstractTableModel
{
public:
    PollTableDataModel();

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void reload(std::vector<PollItem> rows);
    void handlePollStaleFlag(QString poll_txid_string);

private:
    QStringList m_columns;
    std::vector<PollItem> m_rows;

};

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
        StaleResults
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

signals:
    void newVoteReceivedAndPollMarkedDirty();

public slots:
    void refresh();
    void changeTitleFilter(const QString& pattern);
    Qt::SortOrder sort(int column);

    void handlePollStaleFlag(QString poll_txid_string);

private:
    VotingModel* m_voting_model;
    std::unique_ptr<PollTableDataModel> m_data_model;
    GRC::PollFilterFlag m_filter_flags;
    QMutex m_refresh_mutex;
};

#endif // GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H
