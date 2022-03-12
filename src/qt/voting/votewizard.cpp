// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_votewizard.h"
#include "qt/voting/votewizard.h"
#include "qt/voting/votingmodel.h"

// -----------------------------------------------------------------------------
// Class: VoteWizard
// -----------------------------------------------------------------------------

VoteWizard::VoteWizard(const PollItem& poll_item, VotingModel& voting_model, QWidget* parent)
    : QWizard(parent)
    , ui(new Ui::VoteWizard)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);
    resize(GRC::ScaleSize(this, width(), height()));

    ui->ballotPage->setModel(&voting_model);
    ui->ballotPage->setPoll(poll_item);
    ui->summaryPage->setPoll(poll_item);
}

VoteWizard::~VoteWizard()
{
    delete ui;
}
