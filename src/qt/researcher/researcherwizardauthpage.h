// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDAUTHPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDAUTHPAGE_H

#include <QWizardPage>

class ResearcherModel;

namespace Ui {
class ResearcherWizardAuthPage;
}

class ResearcherWizardAuthPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardAuthPage(QWidget *parent = nullptr);
    ~ResearcherWizardAuthPage();

    void setModel(ResearcherModel *researcher_model);

    void initializePage() override;

private:
    Ui::ResearcherWizardAuthPage *ui;
    ResearcherModel *m_researcher_model;

private slots:
    void refresh();
    void on_copyToClipboardButton_clicked();
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDAUTHPAGE_H
