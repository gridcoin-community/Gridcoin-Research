// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardbeaconpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"
#include "qt/researcher/researcherwizardbeaconpage.h"
#include "qt/walletmodel.h"

// -----------------------------------------------------------------------------
// Class: ResearcherWizardBeaconPage
// -----------------------------------------------------------------------------

ResearcherWizardBeaconPage::ResearcherWizardBeaconPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardBeaconPage)
    , m_researcher_model(nullptr)
    , m_wallet_model(nullptr)
{
    ui->setupUi(this);
}

ResearcherWizardBeaconPage::~ResearcherWizardBeaconPage()
{
    delete ui;
}

void ResearcherWizardBeaconPage::setModel(
    ResearcherModel* researcher_model,
    WalletModel* wallet_model)
{
    this->m_researcher_model = researcher_model;
    this->m_wallet_model = wallet_model;

    if (!m_researcher_model || !m_wallet_model) {
        return;
    }

    connect(m_researcher_model, SIGNAL(researcherChanged()), this, SLOT(refresh()));
    connect(m_researcher_model, SIGNAL(beaconChanged()), this, SLOT(refresh()));
    connect(ui->sendBeaconButton, SIGNAL(clicked()), this, SLOT(advertiseBeacon()));
}

void ResearcherWizardBeaconPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    refresh();
    updateBeaconIcon(m_researcher_model->getBeaconStatusIcon());
    updateBeaconStatus(m_researcher_model->formatBeaconStatus());
}

bool ResearcherWizardBeaconPage::isComplete() const
{
    return m_researcher_model->hasActiveBeacon()
        || m_researcher_model->hasPendingBeacon();
}

int ResearcherWizardBeaconPage::nextId() const
{
    if (m_researcher_model->needsBeaconAuth()) {
        return ResearcherWizard::PageAuth;
    }

    return ResearcherWizard::PageSummary;
}

bool ResearcherWizardBeaconPage::isEnabled() const
{
    return !isComplete() || m_researcher_model->hasRenewableBeacon();
}

void ResearcherWizardBeaconPage::refresh()
{
    if (!m_researcher_model) {
        return;
    }

    ui->cpidLabel->setText(m_researcher_model->formatCpid());
    ui->sendBeaconButton->setVisible(isEnabled());
    ui->continuePromptWrapper->setVisible(!isEnabled());

    emit completeChanged();
}

void ResearcherWizardBeaconPage::advertiseBeacon()
{
    if (!m_researcher_model || !m_wallet_model) {
        return;
    }

    const WalletModel::UnlockContext unlock_context(m_wallet_model->requestUnlock());

    if (!unlock_context.isValid()) {
        // Unlock wallet was cancelled
        return;
    }

    BeaconStatus status = m_researcher_model->advertiseBeacon();

    if (status == BeaconStatus::ACTIVE) {
        status = BeaconStatus::PENDING;
    }

    updateBeaconStatus(ResearcherModel::mapBeaconStatus(status));
    updateBeaconIcon(ResearcherModel::mapBeaconStatusIcon(status));
}

void ResearcherWizardBeaconPage::updateBeaconStatus(const QString& status)
{
    ui->beaconStatusLabel->setText(status);
}

void ResearcherWizardBeaconPage::updateBeaconIcon(const QIcon& icon)
{
    const int icon_size = ui->beaconIconLabel->width();

    ui->beaconIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
}
