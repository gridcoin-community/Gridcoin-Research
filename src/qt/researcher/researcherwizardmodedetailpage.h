// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
