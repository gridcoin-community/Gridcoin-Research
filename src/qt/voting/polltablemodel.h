// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H
#define GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H

#include "uint256.h"
#include "gridcoin/voting/filter.h"
#include "qt/voting/votingmodel.h"

#include <atomic>
#include <memory>
#include <QFuture>
#include <QSortFilterProxyModel>

class PollTableDataModel : public QAbstractTableModel
{
    Q_OBJECT

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
    Qt::SortOrder custom_sort(int column);

    void handlePollStaleFlag(QString poll_txid_string);

private:
    // Initialized in-class so the early-return checks in setModel() and the
    // truthy guard in refresh() never read uninitialized memory before the
    // first explicit setModel(VotingModel*) call. The previous setModel()
    // body wrote unconditionally, so reads-before-write never happened
    // before; the new lifetime-aware setModel() reads first to handle
    // detach (setModel(nullptr)), which made the lack of init reachable.
    VotingModel* m_voting_model{nullptr};
    std::unique_ptr<PollTableDataModel> m_data_model;
    GRC::PollFilterFlag m_filter_flags;
    // Debounce flag for refresh(). Set on the GUI thread when a refresh
    // worker is launched, cleared inside the GUI continuation lambda once
    // reload() has actually run. Covering the full build + queued reload
    // (not just the build) prevents redundant background builds stacking
    // up while a reload sits in the GUI event queue. Was historically a
    // QMutex (tryLock on the GUI thread, unlock from the worker) but Qt's
    // non-recursive QMutex requires same-thread unlock -- a std::atomic<bool>
    // here is the right primitive for "is there a refresh in flight," and
    // can be flipped from either thread.
    std::atomic<bool> m_refresh_in_flight{false};

    // Handle to the in-flight QtConcurrent refresh worker (default-constructed
    // QFuture is in the canceled/finished state, so the destructor's
    // waitForFinished() is a no-op when no refresh has ever been launched).
    // Held so the destructor can block until the worker has finished
    // dereferencing `this` -- without it, the worker can touch m_voting_model
    // / m_filter_flags / m_data_model after PollTableModel is destroyed.
    QFuture<void> m_refresh_future;
};

#endif // GRIDCOIN_QT_VOTING_POLLTABLEMODEL_H
