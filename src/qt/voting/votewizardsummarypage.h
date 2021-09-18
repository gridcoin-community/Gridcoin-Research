// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_VOTEWIZARDSUMMARYPAGE_H
#define GRIDCOIN_QT_VOTING_VOTEWIZARDSUMMARYPAGE_H

#include <QWizardPage>

namespace Ui {
class VoteWizardSummaryPage;
}

class PollItem;

class VoteWizardSummaryPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit VoteWizardSummaryPage(QWidget* parent = nullptr);
    ~VoteWizardSummaryPage();

    void setPoll(const PollItem& poll_item);

    void initializePage() override;

private:
    Ui::VoteWizardSummaryPage* ui;

private slots:
    void on_copyToClipboardButton_clicked() const;
};

#endif // GRIDCOIN_QT_VOTING_VOTEWIZARDSUMMARYPAGE_H
