#ifndef RESEARCHERWIZARDPOOLPAGE_H
#define RESEARCHERWIZARDPOOLPAGE_H

#include <QWizardPage>

class QIcon;
class ResearcherModel;
class WalletModel;

namespace Ui {
class ResearcherWizardPoolPage;
}

class ResearcherWizardPoolPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardPoolPage(QWidget *parent = nullptr);
    ~ResearcherWizardPoolPage();

    void setModel(ResearcherModel* researcher_model, WalletModel* wallet_model);

    void initializePage() override;

private:
    Ui::ResearcherWizardPoolPage *ui;
    ResearcherModel* m_researcher_model;
    WalletModel* m_wallet_model;

private slots:
    void openLink(int row, int column) const;
    void getNewAddress();
    void on_copyToClipboardButton_clicked();
};

#endif // RESEARCHERWIZARDPOOLPAGE_H
