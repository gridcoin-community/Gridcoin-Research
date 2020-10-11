// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RESEARCHERWIZARDINVESTORPAGE_H
#define RESEARCHERWIZARDINVESTORPAGE_H

#include <QWizardPage>

class QIcon;
class ResearcherModel;

namespace Ui {
class ResearcherWizardInvestorPage;
}

class ResearcherWizardInvestorPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardInvestorPage(QWidget *parent = nullptr);
    ~ResearcherWizardInvestorPage();

    void setModel(ResearcherModel *researcher_model);

    void initializePage() override;
    int nextId() const override;

private:
    Ui::ResearcherWizardInvestorPage *ui;
    ResearcherModel *m_researcher_model;
};

#endif // RESEARCHERWIZARDINVESTORPAGE_H
