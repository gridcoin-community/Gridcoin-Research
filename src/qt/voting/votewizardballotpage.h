// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTING_VOTEWIZARDBALLOTPAGE_H
#define VOTING_VOTEWIZARDBALLOTPAGE_H

#include <memory>
#include <QWizardPage>

namespace Ui {
class VoteWizardBallotPage;
}

class PollItem;
class VotingModel;

QT_BEGIN_NAMESPACE
class QButtonGroup;
QT_END_NAMESPACE

class VoteWizardBallotPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit VoteWizardBallotPage(QWidget* parent = nullptr);
    ~VoteWizardBallotPage();

    void setModel(VotingModel* voting_model);
    void setPoll(const PollItem& poll_item);

    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

private:
    Ui::VoteWizardBallotPage* ui;
    VotingModel* m_voting_model;
    std::unique_ptr<QButtonGroup> m_choice_buttons;
    QString m_poll_id;
};

#endif // VOTING_VOTEWIZARDBALLOTPAGE_H
