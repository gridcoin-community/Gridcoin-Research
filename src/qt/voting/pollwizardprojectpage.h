// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTING_POLLWIZARDPROJECTPAGE_H
#define VOTING_POLLWIZARDPROJECTPAGE_H

#include <QWizardPage>

namespace Ui {
class PollWizardProjectPage;
}

class VotingModel;

class PollWizardProjectPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit PollWizardProjectPage(QWidget* parent = nullptr);
    ~PollWizardProjectPage();

    void setModel(VotingModel* voting_model);

    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

private:
    Ui::PollWizardProjectPage* ui;
    VotingModel* m_voting_model;
};

#endif // VOTING_POLLWIZARDPROJECTPAGE_H
