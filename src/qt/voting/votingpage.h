// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_VOTINGPAGE_H
#define GRIDCOIN_QT_VOTING_VOTINGPAGE_H

#include <array>
#include <memory>
#include <QWidget>

namespace Ui {
    class VotingPage;
}

class OptionsModel;
class PollTab;
class VotingModel;

QT_BEGIN_NAMESPACE
class QAction;
class QResizeEvent;
class QString;
QT_END_NAMESPACE

class VotingPage : public QWidget
{
    Q_OBJECT

public:
    explicit VotingPage(QWidget* parent = nullptr);
    ~VotingPage();

    void setVotingModel(VotingModel* model);
    void setOptionsModel(OptionsModel* model);

    PollTab& currentTab();

private:
    Ui::VotingPage* ui;
    VotingModel* m_voting_model;
    std::array<PollTab*, 2> m_tabs;
    std::unique_ptr<QAction> m_filter_action;
    QString m_current_filter;


private slots:
    void updateIcons(const QString& theme);
};

#endif // GRIDCOIN_QT_VOTING_VOTINGPAGE_H
