// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizard.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"

#include <cassert>
#include <QAbstractButton>

namespace {
//!
//! \brief Alias for the "start over" custom wizard button.
//!
constexpr QWizard::WizardButton START_OVER_BUTTON = QWizard::CustomButton1;
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: ResearcherWizard
// -----------------------------------------------------------------------------

ResearcherWizard::ResearcherWizard(
    QWidget *parent,
    ResearcherModel* researcher_model,
    WalletModel* wallet_model)
    : QWizard(parent)
    , ui(new Ui::ResearcherWizard)
    , m_researcher_model(researcher_model)
{
    ui->setupUi(this);
    configureStartOverButton();

    setAttribute(Qt::WA_DeleteOnClose, true);

    ui->modePage->setModel(researcher_model);
    ui->modeDetailPage->setModel(researcher_model);
    ui->emailPage->setModel(researcher_model);
    ui->projectsPage->setModel(researcher_model);
    ui->beaconPage->setModel(researcher_model, wallet_model);
    ui->authPage->setModel(researcher_model);
    ui->summaryPage->setModel(researcher_model);
    ui->investorPage->setModel(researcher_model);
    ui->poolPage->setModel(researcher_model, wallet_model);
    ui->poolSummaryPage->setModel(researcher_model);

    // This enables the "help me choose" button on the modes page to switch the
    // current page to the details page. Because the modes page cannot navigate
    // next to a specific page directly, it emits a signal that we wire up here
    // to change the page when requested:
    //
    connect(ui->modePage, SIGNAL(detailLinkButtonClicked()),
            this, SLOT(onDetailLinkButtonClicked()));

    // This enables the "review beacon verification" button on the summary page
    // to switch the current page back to beacon authentication page. Since the
    // summary page cannot navigate back to specific pages directly, it emits a
    // signal that we wire up here to change the page when requested:
    //
    connect(ui->summaryPage, SIGNAL(reviewBeaconAuthButtonClicked()),
            this, SLOT(onReviewBeaconAuthButtonClicked()));

    // This enables the beacon "renew" button on the summary page to switch the
    // current page back to beacon page. Since the summary page cannot navigate
    // back to a specific page directly, it emits a signal that we wire up here
    // to change the page when requested:
    //
    connect(ui->summaryPage, SIGNAL(renewBeaconButtonClicked()),
            this, SLOT(onRenewBeaconButtonClicked()));
}

ResearcherWizard::~ResearcherWizard()
{
    m_researcher_model->onWizardClose();

    delete ui;
}

int ResearcherWizard::GetNextIdByMode(const int mode)
{
    switch (mode) {
        case ModeUnknown:  return PageMode;
        case ModeSolo:     return PageEmail;
        case ModePool:     return PagePool;
        case ModeInvestor: return PageInvestor;
    }

    assert(false);
}

void ResearcherWizard::configureStartOverButton()
{
    setButtonText(START_OVER_BUTTON, tr("&Start Over"));
    connect(this, SIGNAL(customButtonClicked(int)),
            this, SLOT(onCustomButtonClicked(int)));
    connect(this, SIGNAL(currentIdChanged(int)),
            this, SLOT(setStartOverButtonVisibility(int)));
}

void ResearcherWizard::setStartOverButtonVisibility(int page_id)
{
    button(START_OVER_BUTTON)->setVisible(page_id > 0);
}

void ResearcherWizard::onCustomButtonClicked(int which)
{
    if (which == START_OVER_BUTTON) {
        setStartId(PageMode);
        restart();
    }
}

void ResearcherWizard::onDetailLinkButtonClicked()
{
    // This handles the "help me choose" button on the modes page. Qt doesn't
    // give us a good way to switch directly to the detail page, so we rewind
    // and start from it instead:
    //
    setStartId(PageModeDetail);
    restart();
}

void ResearcherWizard::onReviewBeaconAuthButtonClicked()
{
    // This handles the beacon "renew" button on the summary page. Qt doesn't
    // give us a good way to switch directly to the beacon page, so we rewind
    // and start from it instead:
    //
    setStartId(PageAuth);
    restart();
}

void ResearcherWizard::onRenewBeaconButtonClicked()
{
    // This handles the beacon "renew" button on the summary page. Qt doesn't
    // give us a good way to switch directly to the beacon page, so we rewind
    // and start from it instead:
    //
    setStartId(PageBeacon);
    restart();
}
