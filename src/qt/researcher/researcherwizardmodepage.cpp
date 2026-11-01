// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
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

    GRC::ScaleFontPointSize(ui->titleLabel, 16);
}

ResearcherWizardModePage::~ResearcherWizardModePage()
{
    delete ui;
}

void ResearcherWizardModePage::setModel(ResearcherModel *model)
{
    this->m_researcher_model = model;

    // Connected once here, not in initializePage(), which runs on every entry
    // into the page (#3349).
    connect(ui->soloIconLabel, &ClickLabel::clicked, this, static_cast<void (ResearcherWizardModePage::*)()>(&ResearcherWizardModePage::selectSolo));
    connect(ui->soloRadioButton, &QRadioButton::toggled, this, static_cast<void (ResearcherWizardModePage::*)(bool)>(&ResearcherWizardModePage::selectSolo));
    connect(ui->poolIconLabel, &ClickLabel::clicked, this, static_cast<void (ResearcherWizardModePage::*)()>(&ResearcherWizardModePage::selectPool));
    connect(ui->poolRadioButton, &QRadioButton::toggled, this, static_cast<void (ResearcherWizardModePage::*)(bool)>(&ResearcherWizardModePage::selectPool));
    connect(ui->noncruncherIconLabel, &ClickLabel::clicked, this, static_cast<void (ResearcherWizardModePage::*)()>(&ResearcherWizardModePage::selectNoncruncher));
    connect(ui->noncruncherRadioButton, &QRadioButton::toggled, this, static_cast<void (ResearcherWizardModePage::*)(bool)>(&ResearcherWizardModePage::selectNoncruncher));
}

void ResearcherWizardModePage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    ui->modeButtonGroup->setId(ui->soloRadioButton, ResearcherWizard::ModeSolo);
    ui->modeButtonGroup->setId(ui->poolRadioButton, ResearcherWizard::ModePool);
    ui->modeButtonGroup->setId(ui->noncruncherRadioButton, ResearcherWizard::ModeNoncruncher);

    if (m_researcher_model->configuredForNoncruncherMode()) {
        selectNoncruncher();
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

void ResearcherWizardModePage::selectNoncruncher()
{
    ui->noncruncherRadioButton->setChecked(true);
}

void ResearcherWizardModePage::selectNoncruncher(bool checked)
{
    const int icon_size = ui->noncruncherIconLabel->width();
    QIcon icon;

    if (checked) {
        icon = QIcon(":/images/ic_noncruncher_active");
    } else {
        icon = QIcon(":/images/ic_noncruncher_inactive");
    }

    ui->noncruncherIconLabel->setPixmap(icon.pixmap(icon_size, icon_size));
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
