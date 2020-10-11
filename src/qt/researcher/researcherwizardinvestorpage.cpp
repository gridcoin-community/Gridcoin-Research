// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardinvestorpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"
#include "qt/researcher/researcherwizardinvestorpage.h"

// -----------------------------------------------------------------------------
// Class: ResearcherWizardInvestorPage
// -----------------------------------------------------------------------------

ResearcherWizardInvestorPage::ResearcherWizardInvestorPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardInvestorPage)
{
    ui->setupUi(this);
}

ResearcherWizardInvestorPage::~ResearcherWizardInvestorPage()
{
    delete ui;
}

void ResearcherWizardInvestorPage::setModel(ResearcherModel* researcher_model)
{
    this->m_researcher_model = researcher_model;
}

void ResearcherWizardInvestorPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    m_researcher_model->switchToInvestor();
}

int ResearcherWizardInvestorPage::nextId() const
{
    // Force this page to be a final page. Since the wizard has multiple final
    // pages, we need to return this value to ensure that no "back" and "next"
    // buttons appear on this page. The setFinalPage() method is not enough.
    //
    return -1;
}
