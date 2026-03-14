// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDCPIDSYNCPAGE_H
#define GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDCPIDSYNCPAGE_H

#include <QWizardPage>
#include <QString>
#include <vector>

class ResearcherModel;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLabel;
class QProgressBar;

namespace Ui {
class ResearcherWizardCpidSyncPage;
}

//!
//! \brief Wizard page for CPID synchronization in solo crunching mode.
//!
//! This page:
//! 1. Displays user's BOINC projects and their CPIDs
//! 2. Checks if connected to a Gridcoin pool
//! 3. Validates that all accounts use the same email
//! 4. Checks if all CPIDs match
//! 5. Offers to automatically fix CPID mismatches by finding the oldest CPID
//!
class ResearcherWizardCpidSyncPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ResearcherWizardCpidSyncPage(QWidget *parent = nullptr);
    ~ResearcherWizardCpidSyncPage();

    void setModel(ResearcherModel *model);

    void initializePage() override;
    bool isComplete() const override;
    int nextId() const override;

private:
    Ui::ResearcherWizardCpidSyncPage *ui;
    ResearcherModel *m_researcher_model;
    
    QTreeWidget *m_projectsTree;
    QLabel *m_statusLabel;
    QLabel *m_poolDetectionLabel;
    QLabel *m_emailValidationLabel;
    QLabel *m_cpidMatchLabel;
    QPushButton *m_syncButton;
    QPushButton *m_skipButton;
    QProgressBar *m_progressBar;
    
    bool m_poolDetected;
    bool m_emailValid;
    bool m_cpidsMatch;
    bool m_syncComplete;
    
    QString m_resolvedCpid;
    std::vector<QString> m_inconsistentEmails;
    std::vector<QString> m_mismatchedCpids;
    
    //!
    //! \brief Initialize the UI components.
    //!
    void setupUi();
    
    //!
    //! \brief Load and display BOINC projects.
    //!
    void loadProjects();
    
    //!
    //! \brief Check for pool attachment.
    //!
    void checkPoolAttachment();
    
    //!
    //! \brief Validate email consistency across projects.
    //!
    void validateEmails();
    
    //!
    //! \brief Check CPID consistency across projects.
    //!
    void checkCpidMatch();
    
    //!
    //! \brief Find the oldest CPID by creation date.
    //!
    //! \return The oldest CPID found.
    //!
    QString findOldestCpid();
    
    //!
    //! \brief Synchronize all CPIDs to the target CPID.
    //!
    //! \param targetCpid The CPID to sync all projects to.
    //!
    void synchronizeCpids(const QString& targetCpid);
    
    //!
    //! \brief Add a project to the tree widget.
    //!
    //! \param name Project name.
    //! \param cpid Project CPID.
    //! \param email Project email hash.
    //! \param rac Project RAC.
    //! \param creationTime Account creation time.
    //!
    void addProjectToTree(
        const QString& name,
        const QString& cpid,
        const QString& email,
        double rac,
        qint64 creationTime);
    
    //!
    //! \brief Update status labels.
    //!
    void updateStatusLabels();
    
    //!
    //! \brief Show a message to the user.
    //!
    //! \param message Message to display.
    //! \param error Whether this is an error message.
    //!
    void showMessage(const QString& message, bool error = false);

private slots:
    //!
    //! \brief Handle sync button click.
    //!
    void onSyncClicked();
    
    //!
    //! \brief Handle skip button click.
    //!
    void onSkipClicked();
    
    //!
    //! \brief Handle pool detachment.
    //!
    void onDetachPoolClicked();
    
    //!
    //! \brief Refresh the project list.
    //!
    void onRefreshClicked();
};

#endif // GRIDCOIN_QT_RESEARCHER_RESEARCHERWIZARDCPIDSYNCPAGE_H
