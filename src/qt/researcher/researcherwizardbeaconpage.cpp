// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

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
    , m_v3_selected(false)
    , m_v3_key_generated(false)
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

    connect(m_researcher_model, &ResearcherModel::researcherChanged, this, &ResearcherWizardBeaconPage::refresh);
    connect(m_researcher_model, &ResearcherModel::beaconChanged, this, &ResearcherWizardBeaconPage::refresh);
    connect(ui->sendBeaconButton, &QPushButton::clicked, this, &ResearcherWizardBeaconPage::advertiseBeacon);
    connect(ui->v2RadioButton, &QRadioButton::toggled, this, &ResearcherWizardBeaconPage::onVerificationMethodToggled);
    connect(ui->v3RadioButton, &QRadioButton::toggled, this, &ResearcherWizardBeaconPage::onVerificationMethodToggled);
}

void ResearcherWizardBeaconPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    m_v3_selected = false;
    m_v3_key_generated = false;

    const bool v14_active = m_researcher_model->isV14Enabled();
    const bool v3_projects = m_researcher_model->hasV3CapableProjects();
    const bool v3_available = v14_active && v3_projects;

    ui->v3RadioButton->setEnabled(v3_available);
    ui->v2RadioButton->setChecked(true);

    if (!v14_active) {
        ui->v3UnavailableLabel->setText(
            tr("Account ownership proof requires block version 14 (not yet active)."));
        ui->v3UnavailableLabel->setVisible(true);
    } else if (!v3_projects) {
        ui->v3UnavailableLabel->setText(
            tr("No whitelisted projects currently support account ownership proof."));
        ui->v3UnavailableLabel->setVisible(true);
    } else {
        ui->v3UnavailableLabel->setVisible(false);
    }

    refresh();
}

bool ResearcherWizardBeaconPage::isComplete() const
{
    if (m_v3_selected && m_v3_key_generated) {
        return true;
    }

    return m_researcher_model->hasActiveBeacon()
        || m_researcher_model->hasPendingBeacon();
}

int ResearcherWizardBeaconPage::nextId() const
{
    if (m_v3_selected && m_v3_key_generated) {
        return ResearcherWizard::PageOwnershipProof;
    }

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

    if (m_researcher_model->outOfSync()) {
        ui->sendBeaconButton->setVisible(false);
        ui->continuePromptWrapper->setVisible(false);
        ui->verificationMethodWrapper->setVisible(false);
    } else {
        const bool enabled = isEnabled();
        ui->sendBeaconButton->setVisible(enabled && !m_v3_key_generated);
        ui->continuePromptWrapper->setVisible(!enabled && !m_v3_key_generated);
        ui->verificationMethodWrapper->setVisible(enabled);
    }

    if (!m_v3_key_generated) {
        updateBeaconStatus(m_researcher_model->formatBeaconStatus());
        updateBeaconIcon(m_researcher_model->getBeaconStatusIcon());
    }

    emit completeChanged();
}

void ResearcherWizardBeaconPage::advertiseBeacon()
{
    if (!m_researcher_model || !m_wallet_model) {
        return;
    }

    const WalletModel::UnlockContext unlock_context(m_wallet_model->requestUnlock(CapabilityKind::Contract));

    if (!unlock_context.isValid()) {
        // Unlock wallet was cancelled
        return;
    }

    if (m_v3_selected) {
        // V3 path: generate the key only. The actual beacon send happens
        // on the ownership proof page after the user pastes the XML.
        const QString pubkey_hex = m_researcher_model->generateBeaconKeyForV3();

        if (pubkey_hex.isEmpty()) {
            updateBeaconStatus(ResearcherModel::mapBeaconStatus(BeaconStatus::ERROR_MISSING_KEY));
            updateBeaconIcon(m_researcher_model->mapBeaconStatusIcon(BeaconStatus::ERROR_MISSING_KEY));
            return;
        }

        m_v3_key_generated = true;
        updateBeaconStatus(tr("Beacon key generated. Press \"Next\" to continue."));
        refresh();
        return;
    }

    // V2 path: advertise beacon immediately.
    BeaconStatus status = m_researcher_model->advertiseBeacon();

    if (status == BeaconStatus::ACTIVE) {
        status = BeaconStatus::PENDING;
    }

    updateBeaconStatus(ResearcherModel::mapBeaconStatus(status));
    updateBeaconIcon(m_researcher_model->mapBeaconStatusIcon(status));
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

void ResearcherWizardBeaconPage::onVerificationMethodToggled()
{
    m_v3_selected = ui->v3RadioButton->isChecked();
    m_v3_key_generated = false;

    if (m_v3_selected) {
        ui->sendBeaconButton->setText(tr("&Generate Beacon Key"));
    } else {
        ui->sendBeaconButton->setText(tr("&Advertise Beacon"));
    }

    refresh();
}
