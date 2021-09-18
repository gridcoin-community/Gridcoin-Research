// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDEMAILPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDEMAILPAGE_H

#include <QWizardPage>

class ResearcherModel;
class QRegularExpressionValidator;

namespace Ui {
class ResearcherWizardEmailPage;
}

class ResearcherWizardEmailPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardEmailPage(QWidget *parent = nullptr);
    ~ResearcherWizardEmailPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;

private:
    Ui::ResearcherWizardEmailPage *ui;
    ResearcherModel *m_model;
    QRegularExpressionValidator *boincEmailValidator;
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDEMAILPAGE_H
