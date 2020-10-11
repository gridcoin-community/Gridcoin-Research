// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/bitcoinunits.h"
#include "qt/forms/ui_researcherwizardsummarypage.h"
#include "qt/researcher/projecttablemodel.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardsummarypage.h"

// -----------------------------------------------------------------------------
// Class: ResearcherWizardSummaryPage
// -----------------------------------------------------------------------------

ResearcherWizardSummaryPage::ResearcherWizardSummaryPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardSummaryPage)
    , m_researcher_model(nullptr)
    , m_table_model(nullptr)
{
    ui->setupUi(this);
    ui->projectTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

ResearcherWizardSummaryPage::~ResearcherWizardSummaryPage()
{
    delete ui;
}

void ResearcherWizardSummaryPage::setModel(ResearcherModel *model)
{
    this->m_researcher_model = model;

    if (!m_researcher_model) {
        return;
    }

    m_table_model = new ProjectTableModel(model);

    if (!m_table_model) {
        return;
    }

    ui->projectTableView->setModel(m_table_model);

    connect(model, SIGNAL(researcherChanged()), this, SLOT(refreshSummary()));
    connect(ui->refreshButton, SIGNAL(clicked()), this, SLOT(reloadProjects()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));
}

void ResearcherWizardSummaryPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    refreshSummary();
}

int ResearcherWizardSummaryPage::nextId() const
{
    // Force this page to be a final page. Since the wizard has multiple final
    // pages, we need to return this value to ensure that no "back" and "next"
    // buttons appear on this page. The setFinalPage() method is not enough.
    //
    return -1;
}

void ResearcherWizardSummaryPage::onTabChanged(int index)
{
    switch (index) {
        case TabSummary:
            refreshSummary();
            break;
        case TabProjects:
            refreshProjects();
            break;
    }
}

void ResearcherWizardSummaryPage::refreshSummary()
{
    if (ui->tabWidget->currentIndex() != TabSummary) {
        return;
    }

    const BitcoinUnits::Unit unit = BitcoinUnits::BTC;
    const int icon_size = ui->beaconDetailsIconLabel->width();

    ui->cpidLabel->setText(m_researcher_model->formatCpid());
    ui->statusLabel->setText(m_researcher_model->formatStatus());
    ui->magnitudeLabel->setText(m_researcher_model->formatMagnitude());
    ui->accrualLabel->setText(m_researcher_model->formatAccrual(unit));
    ui->reviewBeaconAuthButton->setVisible(m_researcher_model->needsBeaconAuth());

    ui->beaconDetailsIconLabel->setPixmap(
        m_researcher_model->getBeaconStatusIcon().pixmap(icon_size, icon_size));
    ui->beaconStatusLabel->setText(m_researcher_model->formatBeaconStatus());
    ui->beaconAgeLabel->setText(m_researcher_model->formatBeaconAge());
    ui->beaconExpiresLabel->setText(m_researcher_model->formatTimeToBeaconExpiration());
    ui->rainAddressLabel->setText(m_researcher_model->formatBeaconAddress());
    ui->renewBeaconButton->setEnabled(m_researcher_model->hasRenewableBeacon());

    refreshOverallStatus();
}

void ResearcherWizardSummaryPage::refreshOverallStatus()
{
    const int icon_size = ui->overallStatusIconLabel->width();

    QString status;
    QIcon icon;

    if (m_researcher_model->hasPendingBeacon()) {
        status = tr("Beacon awaiting confirmation.");
        icon = QIcon(":/icons/notsynced");
    } else if (m_researcher_model->hasRenewableBeacon()) {
        status = tr("Beacon renewal available.");
        icon = QIcon(":/icons/warning");
    } else if (!m_researcher_model->hasMagnitude()) {
        status = tr("Waiting for magnitude.");
        icon = QIcon(":/icons/notsynced");
    } else {
        status = tr("Everything looks good.");
        icon = QIcon(":/icons/synced");
    }

    ui->overallStatusLabel->setText(status);
    ui->overallStatusIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
}

void ResearcherWizardSummaryPage::refreshProjects()
{
    if (ui->tabWidget->currentIndex() != TabProjects) {
        return;
    }

    ui->emailLabel->setText(m_researcher_model->email());
    ui->boincPathLabel->setText(m_researcher_model->formatBoincPath());
    ui->selectedCpidLabel->setText(m_researcher_model->formatCpid());

    const int icon_size = ui->selectedCpidIconLabel->width();

    if (m_researcher_model->hasEligibleProjects()) {
        ui->selectedCpidIconLabel->setPixmap(
            QIcon(":/icons/synced").pixmap(icon_size, icon_size));
    } else {
        ui->selectedCpidIconLabel->setPixmap(
            QIcon(":/icons/white_and_red_x").pixmap(icon_size, icon_size));
    }

    m_table_model->refresh();
}

void ResearcherWizardSummaryPage::reloadProjects()
{
    m_researcher_model->reload();
    refreshProjects();
}

void ResearcherWizardSummaryPage::on_reviewBeaconAuthButton_clicked()
{
    // This enables the "review beacon auth" button to switch the current page
    // back to verify beacon page. Since a wizard page cannot set the wizard's
    // current index directly, this emits a signal that the wizard connects to
    // the slot that changes the page for us:
    //
    emit reviewBeaconAuthButtonClicked();
}

void ResearcherWizardSummaryPage::on_renewBeaconButton_clicked()
{
    // This enables the page's beacon "renew" button to switch the current page
    // back to beacon page. Since a wizard page cannot set the wizard's current
    // index directly, this emits a signal that the wizard connects to the slot
    // that changes the page for us:
    //
    emit renewBeaconButtonClicked();
}
