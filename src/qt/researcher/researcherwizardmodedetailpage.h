#ifndef RESEARCHERWIZARDMODEDETAILPAGE_H
#define RESEARCHERWIZARDMODEDETAILPAGE_H

#include <QWizardPage>

class ResearcherModel;

namespace Ui {
class ResearcherWizardModeDetailPage;
}

class ResearcherWizardModeDetailPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardModeDetailPage(QWidget *parent = nullptr);
    ~ResearcherWizardModeDetailPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    bool isComplete() const override;
    int nextId() const override;

private:
    Ui::ResearcherWizardModeDetailPage *ui;
    ResearcherModel *m_researcher_model;

private slots:
    void onModeChange();
};

#endif // RESEARCHERWIZARDMODEDETAILPAGE_H
