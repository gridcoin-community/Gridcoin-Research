#ifndef RESEARCHERWIZARDPROJECTSPAGE_H
#define RESEARCHERWIZARDPROJECTSPAGE_H

#include <QWizardPage>

class ProjectTableModel;
class ResearcherModel;

namespace Ui {
class ResearcherWizardProjectsPage;
}

class ResearcherWizardProjectsPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardProjectsPage(QWidget *parent = nullptr);
    ~ResearcherWizardProjectsPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    bool isComplete() const override;

private:
    Ui::ResearcherWizardProjectsPage *ui;
    ResearcherModel *m_researcher_model;
    ProjectTableModel *m_table_model;

private slots:
    void refresh();
};

#endif // RESEARCHERWIZARDPROJECTSPAGE_H
