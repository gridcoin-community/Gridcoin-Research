#ifndef RESEARCHERWIZARD_H
#define RESEARCHERWIZARD_H

#include <QWizard>

class ResearcherModel;
class WalletModel;

namespace Ui {
class ResearcherWizard;
}

class ResearcherWizard : public QWizard
{
    Q_OBJECT

public:
    enum Pages
    {
        PageEmail,
        PageProjects,
        PageBeacon,
        PageAuth,
        PageSummary,
    };

    explicit ResearcherWizard(
        QWidget *parent = nullptr,
        ResearcherModel *researcher_model = nullptr,
        WalletModel *wallet_model = nullptr);

    ~ResearcherWizard();

private:
    Ui::ResearcherWizard *ui;
    ResearcherModel *m_researcher_model;

    void configureStartOverButton();

private slots:
    void onCustomButtonClicked(int which);
    void onRenewBeaconButtonClicked();
};

#endif // RESEARCHERWIZARD_H
