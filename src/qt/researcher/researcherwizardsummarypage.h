// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RESEARCHERWIZARDSUMMARYPAGE_H
#define RESEARCHERWIZARDSUMMARYPAGE_H

#include <QWizardPage>

class ProjectTableModel;
class ResearcherModel;

namespace Ui {
class ResearcherWizardSummaryPage;
}

class ResearcherWizardSummaryPage : public QWizardPage
{
    Q_OBJECT

    enum Tab
    {
        TabSummary,
        TabProjects,
    };

public:
    explicit ResearcherWizardSummaryPage(QWidget *parent = nullptr);
    ~ResearcherWizardSummaryPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    int nextId() const override;

private:
    Ui::ResearcherWizardSummaryPage *ui;
    ResearcherModel *m_researcher_model;
    ProjectTableModel *m_table_model;

signals:
    void reviewBeaconAuthButtonClicked();
    void renewBeaconButtonClicked();

private slots:
    void onTabChanged(int index);
    void refreshSummary();
    void refreshOverallStatus();
    void refreshProjects();
    void reloadProjects();
    void on_reviewBeaconAuthButton_clicked();
    void on_renewBeaconButton_clicked();
};

#endif // RESEARCHERWIZARDSUMMARYPAGE_H
