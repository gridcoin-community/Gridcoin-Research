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
