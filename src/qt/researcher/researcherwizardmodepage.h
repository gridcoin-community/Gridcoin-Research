// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDMODEPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDMODEPAGE_H

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

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDMODEPAGE_H
