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
