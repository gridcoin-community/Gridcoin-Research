// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardpoolsummarypage.h"
#include "qt/researcher/projecttablemodel.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardpoolsummarypage.h"

#include <QMessageBox>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardPoolSummaryPage
// -----------------------------------------------------------------------------

ResearcherWizardPoolSummaryPage::ResearcherWizardPoolSummaryPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardPoolSummaryPage)
    , m_researcher_model(nullptr)
    , m_table_model(nullptr)
{
    ui->setupUi(this);
    ui->projectTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

ResearcherWizardPoolSummaryPage::~ResearcherWizardPoolSummaryPage()
{
    delete ui;
    delete m_table_model;
}

void ResearcherWizardPoolSummaryPage::setModel(ResearcherModel *model)
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

void ResearcherWizardPoolSummaryPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    refresh();
}

int ResearcherWizardPoolSummaryPage::nextId() const
{
    // Force this page to be a final page. Since the wizard has multiple final
    // pages, we need to return this value to ensure that no "back" and "next"
    // buttons appear on this page. The setFinalPage() method is not enough.
    //
    return -1;
}

void ResearcherWizardPoolSummaryPage::refresh()
{
    const int icon_size = ui->poolStatusIconLabel->width();

    m_researcher_model->reload();
    m_table_model->refresh();
    ui->boincPathLabel->setText(m_researcher_model->formatBoincPath());

    if (m_researcher_model->hasPoolProjects()) {
        ui->poolStatusLabel->setText(tr("Pool projects detected"));
        ui->poolStatusIconLabel->setPixmap(
            QIcon(":/icons/synced").pixmap(icon_size, icon_size));
    } else {
        ui->poolStatusLabel->setText(tr("No pool projects detected"));
        ui->poolStatusIconLabel->setPixmap(
            QIcon(":/icons/white_and_red_x").pixmap(icon_size, icon_size));
    }

    emit completeChanged();
}
