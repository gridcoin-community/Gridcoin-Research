// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_votewizardsummarypage.h"
#include "qt/voting/votewizardsummarypage.h"
#include "qt/voting/votingmodel.h"

#include <QClipboard>

// -----------------------------------------------------------------------------
// Class: VoteWizardSummaryPage
// -----------------------------------------------------------------------------

VoteWizardSummaryPage::VoteWizardSummaryPage(QWidget* parent)
    : QWizardPage(parent)
    , ui(new Ui::VoteWizardSummaryPage)
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->pageTitleLabel, 14);
    GRC::ScaleFontPointSize(ui->pollTitleLabel, 12);
    GRC::ScaleFontPointSize(ui->voteIdLabel, 8);
}

VoteWizardSummaryPage::~VoteWizardSummaryPage()
{
    delete ui;
}

void VoteWizardSummaryPage::setPoll(const PollItem& poll_item)
{
    ui->pollTitleLabel->setText(poll_item.m_title);
}

void VoteWizardSummaryPage::initializePage()
{
    ui->responsesLabel->setText(field("responseLabels").toString());
    ui->voteIdLabel->setText(field("txid").toString());
}

void VoteWizardSummaryPage::on_copyToClipboardButton_clicked() const
{
    QApplication::clipboard()->setText(field("txid").toString());
}
