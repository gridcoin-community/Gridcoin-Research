// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_pollwizard.h"
#include "qt/voting/pollwizard.h"
#include "qt/voting/votingmodel.h"

// -----------------------------------------------------------------------------
// Class: PollWizard
// -----------------------------------------------------------------------------

PollWizard::PollWizard(VotingModel& voting_model, QWidget* parent)
    : QWizard(parent)
    , ui(new Ui::PollWizard)
    , m_poll_types(new PollTypes())
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);
    resize(GRC::ScaleSize(this, 670, 580));

    ui->typePage->setPollTypes(m_poll_types.get());
    ui->projectPage->setModel(&voting_model);
    ui->detailsPage->setModel(&voting_model);
    ui->detailsPage->setPollTypes(m_poll_types.get());
}

PollWizard::~PollWizard()
{
    delete ui;
}
