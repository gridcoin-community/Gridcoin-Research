// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLWIZARD_H
#define GRIDCOIN_QT_VOTING_POLLWIZARD_H

#include <memory>
#include <QWizard>

namespace Ui {
class PollWizard;
}

class PollTypes;
class VotingModel;

class PollWizard : public QWizard
{
    Q_OBJECT

public:
    enum Pages
    {
        PageType,
        PageProject,
        PageDetails,
        PageSummary,
    };

    explicit PollWizard(VotingModel& voting_model, QWidget* parent = nullptr);
    ~PollWizard();

private:
    Ui::PollWizard* ui;
    std::unique_ptr<PollTypes> m_poll_types;
};

#endif // GRIDCOIN_QT_VOTING_POLLWIZARD_H
