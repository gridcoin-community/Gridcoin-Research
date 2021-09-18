// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDMODEDETAILPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDMODEDETAILPAGE_H

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

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDMODEDETAILPAGE_H
