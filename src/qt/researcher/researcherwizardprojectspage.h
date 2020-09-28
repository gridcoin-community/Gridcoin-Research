// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RESEARCHERWIZARDPROJECTSPAGE_H
#define RESEARCHERWIZARDPROJECTSPAGE_H

#include <QWizardPage>

class ProjectTableModel;
class ResearcherModel;

namespace Ui {
class ResearcherWizardProjectsPage;
}

class ResearcherWizardProjectsPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardProjectsPage(QWidget *parent = nullptr);
    ~ResearcherWizardProjectsPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    bool isComplete() const override;

private:
    Ui::ResearcherWizardProjectsPage *ui;
    ResearcherModel *m_researcher_model;
    ProjectTableModel *m_table_model;

private slots:
    void refresh();
};

#endif // RESEARCHERWIZARDPROJECTSPAGE_H
