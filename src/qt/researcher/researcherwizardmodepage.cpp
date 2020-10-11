// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardmodepage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"
#include "qt/researcher/researcherwizardmodepage.h"

// -----------------------------------------------------------------------------
// Class: ResearcherWizardModePage
// -----------------------------------------------------------------------------

ResearcherWizardModePage::ResearcherWizardModePage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardModePage)
    , m_researcher_model(nullptr)
{
    ui->setupUi(this);
}

ResearcherWizardModePage::~ResearcherWizardModePage()
{
    delete ui;
}

void ResearcherWizardModePage::setModel(ResearcherModel *model)
{
    this->m_researcher_model = model;
}

void ResearcherWizardModePage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    ui->modeButtonGroup->setId(ui->soloRadioButton, ResearcherWizard::ModeSolo);
    ui->modeButtonGroup->setId(ui->poolRadioButton, ResearcherWizard::ModePool);
    ui->modeButtonGroup->setId(ui->investorRadioButton, ResearcherWizard::ModeInvestor);

    connect(ui->soloIconLabel, SIGNAL(clicked()), this, SLOT(selectSolo()));
    connect(ui->soloRadioButton, SIGNAL(toggled(bool)), this, SLOT(selectSolo(bool)));
    connect(ui->poolIconLabel, SIGNAL(clicked()), this, SLOT(selectPool()));
    connect(ui->poolRadioButton, SIGNAL(toggled(bool)), this, SLOT(selectPool(bool)));
    connect(ui->investorIconLabel, SIGNAL(clicked()), this, SLOT(selectInvestor()));
    connect(ui->investorRadioButton, SIGNAL(toggled(bool)), this, SLOT(selectInvestor(bool)));

    if (m_researcher_model->configuredForInvestorMode()) {
        selectInvestor();
    } else if (m_researcher_model->hasEligibleProjects()) {
        selectSolo();
    } else if (m_researcher_model->hasPoolProjects()) {
        selectPool();
    }
}

bool ResearcherWizardModePage::isComplete() const
{
    return ui->modeButtonGroup->checkedId() != ResearcherWizard::ModeUnknown;
}

int ResearcherWizardModePage::nextId() const
{
    return ResearcherWizard::GetNextIdByMode(ui->modeButtonGroup->checkedId());
}

void ResearcherWizardModePage::selectSolo()
{
    ui->soloRadioButton->setChecked(true);
}

void ResearcherWizardModePage::selectSolo(bool checked)
{
    const int icon_size = ui->soloIconLabel->width();
    QIcon icon;

    if (checked) {
        icon = QIcon(":/images/ic_solo_active");
    } else {
        icon = QIcon(":/images/ic_solo_inactive");
    }

    ui->soloIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
    emit completeChanged();
}

void ResearcherWizardModePage::selectPool()
{
    ui->poolRadioButton->setChecked(true);
}

void ResearcherWizardModePage::selectPool(bool checked)
{
    const int icon_size = ui->poolIconLabel->width();
    QIcon icon;

    if (checked) {
        icon = QIcon(":/images/ic_pool_active");
    } else {
        icon = QIcon(":/images/ic_pool_inactive");
    }

    ui->poolIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
    emit completeChanged();
}

void ResearcherWizardModePage::selectInvestor()
{
    ui->investorRadioButton->setChecked(true);
}

void ResearcherWizardModePage::selectInvestor(bool checked)
{
    const int icon_size = ui->investorIconLabel->width();
    QIcon icon;

    if (checked) {
        icon = QIcon(":/images/ic_investor_active");
    } else {
        icon = QIcon(":/images/ic_investor_inactive");
    }

    ui->investorIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
    emit completeChanged();
}

void ResearcherWizardModePage::on_detailLinkButton_clicked()
{
    // This enables the page's beacon "renew" button to switch the current page
    // back to beacon page. Since a wizard page cannot set the wizard's current
    // index directly, this emits a signal that the wizard connects to the slot
    // that changes the page for us:
    //
    emit detailLinkButtonClicked();
}
