// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLTAB_H
#define GRIDCOIN_QT_VOTING_POLLTAB_H

#include "gridcoin/voting/filter.h"

#include <memory>
#include <QDateTime>
#include <QWidget>

namespace Ui {
class PollTab;
}

class LoadingBar;
class NoResult;
class PollItem;
class PollTableModel;
class VotingModel;

class PollTab : public QWidget
{
    Q_OBJECT

public:
    //!
    //! \brief Represents the offsets of the main tabs on the voting page.
    //!
    enum TabId
    {
        TabActive,
        TabFinished,
    };

    //!
    //! \brief Represents the data views available in the stack for each tab.
    //!
    enum ViewId
    {
        ViewCards,
        ViewTable,
    };

    explicit PollTab(QWidget* parent = nullptr);
    ~PollTab();

    void setVotingModel(VotingModel* voting_model);
    void setPollFilterFlags(GRC::PollFilterFlag flags);

signals:
    void newVoteReceivedAndPollMarkedDirty();

public slots:
    void changeViewMode(const ViewId view_id);
    void refresh();
    void filter(const QString& needle);
    void sort(const int column);
    void updateIcons(const QString& theme);

private:
    Ui::PollTab* ui;
    VotingModel* m_voting_model;
    std::unique_ptr<PollTableModel> m_polltable_model;
    std::unique_ptr<NoResult> m_no_result;
    std::unique_ptr<LoadingBar> m_loading;
    QString m_last_filter;

    const PollItem* selectedTableItem() const;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void finishRefresh();
    void showVoteRowDialog(int row);
    void showVoteDialog(const PollItem& poll_item);
    void showDetailsRowDialog(int row);
    void showDetailsDialog(const PollItem& poll_item);
    void showPreferredDialog(const QModelIndex& index);
    void showTableContextMenu(const QPoint& pos);
};

#endif // GRIDCOIN_QT_VOTING_POLLTAB_H
