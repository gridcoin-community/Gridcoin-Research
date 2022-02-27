// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDPOOLSUMMARYPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDPOOLSUMMARYPAGE_H

#include <QWizardPage>

class ProjectTableModel;
class ResearcherModel;

namespace Ui {
class ResearcherWizardPoolSummaryPage;
}

class ResearcherWizardPoolSummaryPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardPoolSummaryPage(QWidget *parent = nullptr);
    ~ResearcherWizardPoolSummaryPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    int nextId() const override;

private:
    Ui::ResearcherWizardPoolSummaryPage *ui;
    ResearcherModel *m_researcher_model;
    ProjectTableModel *m_table_model;

private slots:
    void refresh();
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDPOOLSUMMARYPAGE_H
