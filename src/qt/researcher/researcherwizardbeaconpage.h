// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDBEACONPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDBEACONPAGE_H

#include <QWizardPage>

class QIcon;
class ResearcherModel;
class WalletModel;

namespace Ui {
class ResearcherWizardBeaconPage;
}

class ResearcherWizardBeaconPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardBeaconPage(QWidget *parent = nullptr);
    ~ResearcherWizardBeaconPage();

    void setModel(ResearcherModel *researcher_model, WalletModel *wallet_model);

    void initializePage() override;
    bool isComplete() const override;
    int nextId() const override;

    bool isEnabled() const;

private:
    Ui::ResearcherWizardBeaconPage *ui;
    ResearcherModel *m_researcher_model;
    WalletModel *m_wallet_model;

private slots:
    void refresh();
    void advertiseBeacon();
    void updateBeaconStatus(const QString& status);
    void updateBeaconIcon(const QIcon& icon);
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDBEACONPAGE_H
