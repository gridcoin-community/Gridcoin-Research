#ifndef RESEARCHERWIZARDPOOLSUMMARYPAGE_H
#define RESEARCHERWIZARDPOOLSUMMARYPAGE_H

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

#endif // RESEARCHERWIZARDPOOLSUMMARYPAGE_H
