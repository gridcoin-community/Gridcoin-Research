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
    void renewBeaconButtonClicked();

private slots:
    void onTabChanged(int index);
    void refreshSummary();
    void refreshOverallStatus();
    void refreshProjects();
    void reloadProjects();
    void on_renewBeaconButton_clicked();
};

#endif // RESEARCHERWIZARDSUMMARYPAGE_H
