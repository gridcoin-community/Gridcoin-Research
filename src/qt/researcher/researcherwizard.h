// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
        PageMode,
        PageModeDetail,
        PageEmail,
        PageProjects,
        PageBeacon,
        PageAuth,
        PageSummary,
        PageInvestor,
        PagePool,
        PagePoolSummary,
    };

    //!
    //! \brief Determines how to configure the wallet for participation.
    //!
    enum Modes
    {
        ModeUnknown = -1,
        ModeSolo = 0,
        ModePool = 1,
        ModeInvestor = 2,
    };

    explicit ResearcherWizard(
        QWidget *parent = nullptr,
        ResearcherModel *researcher_model = nullptr,
        WalletModel *wallet_model = nullptr);

    ~ResearcherWizard();

    static int GetNextIdByMode(const int mode);

private:
    Ui::ResearcherWizard *ui;
    ResearcherModel *m_researcher_model;

    void configureStartOverButton();

private slots:
    void setStartOverButtonVisibility(int page_id);
    void onCustomButtonClicked(int which);
    void onDetailLinkButtonClicked();
    void onReviewBeaconAuthButtonClicked();
    void onRenewBeaconButtonClicked();
};

#endif // RESEARCHERWIZARD_H
