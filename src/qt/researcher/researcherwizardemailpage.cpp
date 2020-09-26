// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardemailpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardemailpage.h"

#include <QRegularExpressionValidator>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardEmailPage
// -----------------------------------------------------------------------------

ResearcherWizardEmailPage::ResearcherWizardEmailPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardEmailPage)
    , m_model(nullptr)
{
    ui->setupUi(this);

    // Validate email with the same pattern as the BOINC server
    QRegularExpression boincEmailRegexp("^([^@]+)@([^@\\.]+)\\.([^@]{2,})$");
    boincEmailValidator = new QRegularExpressionValidator(boincEmailRegexp, this);
    ui->emailAddressLineEdit->setValidator(boincEmailValidator);

    // The asterisk denotes a mandatory field:
    registerField("emailAddress*", ui->emailAddressLineEdit);
}

ResearcherWizardEmailPage::~ResearcherWizardEmailPage()
{
    delete boincEmailValidator;
    delete ui;
}

void ResearcherWizardEmailPage::setModel(ResearcherModel *model)
{
    this->m_model = model;
}

void ResearcherWizardEmailPage::initializePage()
{
    ui->emailAddressLineEdit->setText(m_model->email());
}
