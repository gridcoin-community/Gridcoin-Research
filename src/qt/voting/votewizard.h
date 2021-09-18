// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_VOTEWIZARD_H
#define GRIDCOIN_QT_VOTING_VOTEWIZARD_H

#include <QWizard>

namespace Ui {
class VoteWizard;
}

class PollItem;
class VotingModel;

class VoteWizard : public QWizard
{
    Q_OBJECT

public:
    enum Pages
    {
        PageBallot,
        PageSummary,
    };

    explicit VoteWizard(
        const PollItem& poll_item,
        VotingModel& voting_model,
        QWidget* parent = nullptr);
    ~VoteWizard();

private:
    Ui::VoteWizard* ui;
};

#endif // GRIDCOIN_QT_VOTING_VOTEWIZARD_H
