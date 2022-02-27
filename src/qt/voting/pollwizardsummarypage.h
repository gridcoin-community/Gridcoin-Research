// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLWIZARDSUMMARYPAGE_H
#define GRIDCOIN_QT_VOTING_POLLWIZARDSUMMARYPAGE_H

#include <QWizardPage>

namespace Ui {
class PollWizardSummaryPage;
}

class PollWizardSummaryPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit PollWizardSummaryPage(QWidget* parent = nullptr);
    ~PollWizardSummaryPage();

    void initializePage() override;

private:
    Ui::PollWizardSummaryPage* ui;

private slots:
    void on_copyToClipboardButton_clicked() const;
};

#endif // GRIDCOIN_QT_VOTING_POLLWIZARDSUMMARYPAGE_H
