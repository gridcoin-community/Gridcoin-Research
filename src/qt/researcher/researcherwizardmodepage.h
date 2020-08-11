#ifndef RESEARCHERWIZARDMODEPAGE_H
#define RESEARCHERWIZARDMODEPAGE_H

#include <QWizardPage>

class ResearcherModel;

namespace Ui {
class ResearcherWizardModePage;
}

class ResearcherWizardModePage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardModePage(QWidget *parent = nullptr);
    ~ResearcherWizardModePage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    bool isComplete() const override;
    int nextId() const override;

private:
    Ui::ResearcherWizardModePage *ui;
    ResearcherModel *m_researcher_model;

signals:
    void detailLinkButtonClicked();

private slots:
    void selectSolo();
    void selectSolo(bool checked);
    void selectPool();
    void selectPool(bool checked);
    void selectInvestor();
    void selectInvestor(bool checked);
    void on_detailLinkButton_clicked();
};

#endif // RESEARCHERWIZARDMODEPAGE_H
