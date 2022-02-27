// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_pollwizardsummarypage.h"
#include "qt/voting/pollwizard.h"
#include "qt/voting/pollwizardsummarypage.h"
#include "qt/voting/votingmodel.h"

#include <QClipboard>

// -----------------------------------------------------------------------------
// Class: PollWizardSummaryPage
// -----------------------------------------------------------------------------

PollWizardSummaryPage::PollWizardSummaryPage(QWidget* parent)
    : QWizardPage(parent)
    , ui(new Ui::PollWizardSummaryPage)
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->pageTitleLabel, 14);
    GRC::ScaleFontPointSize(ui->pollTitleLabel, 12);
    GRC::ScaleFontPointSize(ui->pollIdLabel, 8);
}

PollWizardSummaryPage::~PollWizardSummaryPage()
{
    delete ui;
}

void PollWizardSummaryPage::initializePage()
{
    ui->pollTitleLabel->setText(field("title").toString());
    ui->pollIdLabel->setText(field("txid").toString());
}

void PollWizardSummaryPage::on_copyToClipboardButton_clicked() const
{
    QApplication::clipboard()->setText(field("txid").toString());
}
