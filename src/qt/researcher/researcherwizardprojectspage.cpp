// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardprojectspage.h"
#include "qt/researcher/projecttablemodel.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardprojectspage.h"

#include <QMessageBox>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardProjectsPage
// -----------------------------------------------------------------------------

ResearcherWizardProjectsPage::ResearcherWizardProjectsPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardProjectsPage)
    , m_researcher_model(nullptr)
    , m_table_model(nullptr)
{
    ui->setupUi(this);
    ui->projectTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

ResearcherWizardProjectsPage::~ResearcherWizardProjectsPage()
{
    delete ui;
    delete m_table_model;
}

void ResearcherWizardProjectsPage::setModel(ResearcherModel *model)
{
    this->m_researcher_model = model;

    if (!m_researcher_model) {
        return;
    }

    m_table_model = new ProjectTableModel(model, false);

    if (!m_table_model) {
        return;
    }

    ui->projectTableView->setModel(m_table_model);
    ui->projectTableView->hideColumn(ProjectTableModel::Magnitude);

    connect(ui->refreshButton, SIGNAL(clicked()), this, SLOT(refresh()));
}

void ResearcherWizardProjectsPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    // Process the email address input from the previous page:
    if (!m_researcher_model->switchToSolo(field("emailAddress").toString())) {
        QMessageBox::warning(
            this,
            windowTitle(),
            tr("An error occurred while saving the email address to the "
                "configuration file. Please see debug.log for details."),
            QMessageBox::Ok,
            QMessageBox::Ok);
    }

    refresh();
}

bool ResearcherWizardProjectsPage::isComplete() const
{
    return m_researcher_model->hasEligibleProjects();
}

void ResearcherWizardProjectsPage::refresh()
{
    m_researcher_model->reload();

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

    emit completeChanged();
}
