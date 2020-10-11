// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RESEARCHERWIZARDEMAILPAGE_H
#define RESEARCHERWIZARDEMAILPAGE_H

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

#endif // RESEARCHERWIZARDEMAILPAGE_H
