// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLWIZARDDETAILSPAGE_H
#define GRIDCOIN_QT_VOTING_POLLWIZARDDETAILSPAGE_H

#include <memory>
#include <QWizardPage>

namespace Ui {
class PollWizardDetailsPage;
}

class AdditionalFieldsTableModel;
class ChoicesListModel;
class PollTypes;
class VotingModel;

class PollWizardDetailsPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit PollWizardDetailsPage(QWidget* parent = nullptr);
    ~PollWizardDetailsPage();

    void setModel(VotingModel* voting_model);
    void setPollTypes(const PollTypes* const poll_types);

    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

private:
    Ui::PollWizardDetailsPage* ui;
    VotingModel* m_voting_model;
    const PollTypes* m_poll_types;
    std::unique_ptr<AdditionalFieldsTableModel> m_additional_fields_model;
    std::unique_ptr<ChoicesListModel> m_choices_model;

private slots:
    void updateIcons();
};

#endif // GRIDCOIN_QT_VOTING_POLLWIZARDDETAILSPAGE_H
