// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardownershipproofpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"
#include "qt/researcher/researcherwizardownershipproofpage.h"
#include "qt/walletmodel.h"

#include <QClipboard>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardOwnershipProofPage
// -----------------------------------------------------------------------------

ResearcherWizardOwnershipProofPage::ResearcherWizardOwnershipProofPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardOwnershipProofPage)
    , m_researcher_model(nullptr)
    , m_wallet_model(nullptr)
    , m_beacon_sent(false)
{
    ui->setupUi(this);
}

ResearcherWizardOwnershipProofPage::~ResearcherWizardOwnershipProofPage()
{
    delete ui;
}

void ResearcherWizardOwnershipProofPage::setModel(
    ResearcherModel* researcher_model,
    WalletModel* wallet_model)
{
    this->m_researcher_model = researcher_model;
    this->m_wallet_model = wallet_model;

    if (!m_researcher_model || !m_wallet_model) {
        return;
    }

    connect(ui->copyPubKeyButton, &QPushButton::clicked,
            this, &ResearcherWizardOwnershipProofPage::copyPubKeyToClipboard);
    connect(ui->sendBeaconButton, &QPushButton::clicked,
            this, &ResearcherWizardOwnershipProofPage::submitOwnershipProof);
}

void ResearcherWizardOwnershipProofPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    m_beacon_sent = false;

    // Populate beacon public key.
    ui->pubkeyLabel->setText(m_researcher_model->cachedBeaconPubKeyHex());

    // Build the list of v3-capable projects with clickable URLs.
    const auto projects = m_researcher_model->buildV3ProjectList();

    QString project_html;
    for (const auto& [name, url] : projects) {
        project_html += QString("<a href=\"%1\">%2</a><br/>")
            .arg(url.toHtmlEscaped(), name.toHtmlEscaped());
    }

    if (project_html.isEmpty()) {
        project_html = tr("No projects currently support this method.");
    }

    ui->projectListLabel->setText(project_html);

    // Clear any previous state.
    ui->ownershipProofXmlEdit->clear();
    ui->statusLabel->clear();
    ui->statusIconLabel->clear();

    emit completeChanged();
}

bool ResearcherWizardOwnershipProofPage::isComplete() const
{
    if (m_beacon_sent) {
        return true;
    }

    if (!m_researcher_model) {
        return false;
    }

    return m_researcher_model->hasActiveBeacon()
        || m_researcher_model->hasPendingBeacon();
}

int ResearcherWizardOwnershipProofPage::nextId() const
{
    return ResearcherWizard::PageSummary;
}

void ResearcherWizardOwnershipProofPage::copyPubKeyToClipboard()
{
    QApplication::clipboard()->setText(ui->pubkeyLabel->text());
}

void ResearcherWizardOwnershipProofPage::submitOwnershipProof()
{
    if (!m_researcher_model || !m_wallet_model) {
        return;
    }

    const QString xml = ui->ownershipProofXmlEdit->toPlainText().trimmed();

    if (xml.isEmpty()) {
        ui->statusLabel->setText(tr("Please paste the ownership proof XML from the project website."));
        return;
    }

    const WalletModel::UnlockContext unlock_context(m_wallet_model->requestUnlock(CapabilityKind::Contract));

    if (!unlock_context.isValid()) {
        return;
    }

    BeaconStatus status = m_researcher_model->advertiseBeaconV3(xml);

    if (status == BeaconStatus::ACTIVE) {
        status = BeaconStatus::PENDING;
    }

    ui->statusLabel->setText(ResearcherModel::mapBeaconStatus(status));
    updateStatusIcon(m_researcher_model->mapBeaconStatusIcon(status));

    if (status == BeaconStatus::PENDING) {
        m_beacon_sent = true;
    }

    emit completeChanged();
}

void ResearcherWizardOwnershipProofPage::updateStatusIcon(const QIcon& icon)
{
    const int icon_size = ui->statusIconLabel->width();

    ui->statusIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
}
