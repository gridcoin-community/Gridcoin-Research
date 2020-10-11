// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardmodedetailpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"
#include "qt/researcher/researcherwizardmodedetailpage.h"

// -----------------------------------------------------------------------------
// Class: ResearcherWizardModeDetailPage
// -----------------------------------------------------------------------------

ResearcherWizardModeDetailPage::ResearcherWizardModeDetailPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardModeDetailPage)
    , m_researcher_model(nullptr)
{
    ui->setupUi(this);
}

ResearcherWizardModeDetailPage::~ResearcherWizardModeDetailPage()
{
    delete ui;
}

void ResearcherWizardModeDetailPage::setModel(ResearcherModel *model)
{
    this->m_researcher_model = model;
}

void ResearcherWizardModeDetailPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    ui->modeButtonGroup->setId(ui->soloRadioButton, ResearcherWizard::ModeSolo);
    ui->modeButtonGroup->setId(ui->poolRadioButton, ResearcherWizard::ModePool);
    ui->modeButtonGroup->setId(ui->investorRadioButton, ResearcherWizard::ModeInvestor);

    connect(ui->modeButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)),
            this, SLOT(onModeChange()));

    if (m_researcher_model->configuredForInvestorMode()) {
        ui->investorRadioButton->setChecked(true);
    } else if (m_researcher_model->hasEligibleProjects()) {
        ui->soloRadioButton->setChecked(true);
    } else if (m_researcher_model->hasPoolProjects()) {
        ui->poolRadioButton->setChecked(true);
    }
}

bool ResearcherWizardModeDetailPage::isComplete() const
{
    return ui->modeButtonGroup->checkedId() != ResearcherWizard::ModeUnknown;
}

int ResearcherWizardModeDetailPage::nextId() const
{
    return ResearcherWizard::GetNextIdByMode(ui->modeButtonGroup->checkedId());
}

void ResearcherWizardModeDetailPage::onModeChange()
{
    emit completeChanged();
}
