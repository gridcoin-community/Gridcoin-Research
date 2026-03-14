// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/researcher/researcherwizardcpidsyncpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/forms/ui_researcherwizardcpidsyncpage.h"
#include "gridcoin/boinc_rpc.h"
#include "gridcoin/researcher.h"
#include "util.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QMessageBox>
#include <QApplication>
#include <QDateTime>
#include <QDir>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardCpidSyncPage
// -----------------------------------------------------------------------------

ResearcherWizardCpidSyncPage::ResearcherWizardCpidSyncPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardCpidSyncPage)
    , m_researcher_model(nullptr)
    , m_projectsTree(nullptr)
    , m_statusLabel(nullptr)
    , m_poolDetectionLabel(nullptr)
    , m_emailValidationLabel(nullptr)
    , m_cpidMatchLabel(nullptr)
    , m_syncButton(nullptr)
    , m_skipButton(nullptr)
    , m_progressBar(nullptr)
    , m_poolDetected(false)
    , m_emailValid(true)
    , m_cpidsMatch(true)
    , m_syncComplete(false)
{
    ui->setupUi(this);
    setupUi();
    
    setTitle(tr("CPID Synchronization"));
    setSubTitle(tr("Verify and synchronize your BOINC project CPIDs for solo crunching"));
}

ResearcherWizardCpidSyncPage::~ResearcherWizardCpidSyncPage()
{
    delete ui;
}

void ResearcherWizardCpidSyncPage::setModel(ResearcherModel *model)
{
    m_researcher_model = model;
}

void ResearcherWizardCpidSyncPage::initializePage()
{
    m_poolDetected = false;
    m_emailValid = true;
    m_cpidsMatch = true;
    m_syncComplete = false;
    m_resolvedCpid.clear();
    m_inconsistentEmails.clear();
    m_mismatchedCpids.clear();
    
    m_projectsTree->clear();
    showMessage(tr("Loading BOINC projects..."));
    
    // Load and analyze projects
    loadProjects();
    checkPoolAttachment();
    validateEmails();
    checkCpidMatch();
    updateStatusLabels();
}

bool ResearcherWizardCpidSyncPage::isComplete() const
{
    // Page is complete if:
    // 1. No pool detected OR pool was detached
    // 2. CPIDs are synchronized (or user chose to skip)
    return m_syncComplete || (m_cpidsMatch && !m_poolDetected);
}

int ResearcherWizardCpidSyncPage::nextId() const
{
    // Navigate to beacon page if sync complete
    return ResearcherWizard::PageBeacon;
}

void ResearcherWizardCpidSyncPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    
    // Pool detection section
    auto *poolFrame = new QFrame(this);
    auto *poolLayout = new QHBoxLayout(poolFrame);
    m_poolDetectionLabel = new QLabel(tr("Checking pool attachment..."), this);
    m_poolDetectionLabel->setWordWrap(true);
    poolLayout->addWidget(m_poolDetectionLabel);
    layout->addWidget(poolFrame);
    
    // Email validation section
    auto *emailFrame = new QFrame(this);
    auto *emailLayout = new QHBoxLayout(emailFrame);
    m_emailValidationLabel = new QLabel(tr("Checking email consistency..."), this);
    m_emailValidationLabel->setWordWrap(true);
    emailLayout->addWidget(m_emailValidationLabel);
    layout->addWidget(emailFrame);
    
    // CPID match section
    auto *cpidFrame = new QFrame(this);
    auto *cpidLayout = new QHBoxLayout(cpidFrame);
    m_cpidMatchLabel = new QLabel(tr("Checking CPID consistency..."), this);
    m_cpidMatchLabel->setWordWrap(true);
    cpidLayout->addWidget(m_cpidMatchLabel);
    layout->addWidget(cpidFrame);
    
    // Projects tree
    m_projectsTree = new QTreeWidget(this);
    m_projectsTree->setHeaderLabels({
        tr("Project"),
        tr("CPID"),
        tr("Email Hash"),
        tr("RAC"),
        tr("Created")
    });
    m_projectsTree->setColumnWidth(0, 200);
    m_projectsTree->setColumnWidth(1, 280);
    m_projectsTree->setColumnWidth(2, 200);
    m_projectsTree->setColumnWidth(3, 80);
    m_projectsTree->setColumnWidth(4, 120);
    m_projectsTree->setAlternatingRowColors(true);
    m_projectsTree->setMinimumHeight(200);
    layout->addWidget(m_projectsTree);
    
    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 0); // Indeterminate
    layout->addWidget(m_progressBar);
    
    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::RichText);
    layout->addWidget(m_statusLabel);
    
    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    
    m_syncButton = new QPushButton(tr("Synchronize CPIDs"), this);
    m_syncButton->setVisible(false);
    connect(m_syncButton, &QPushButton::clicked, this, &ResearcherWizardCpidSyncPage::onSyncClicked);
    buttonLayout->addWidget(m_syncButton);
    
    QPushButton *detachButton = new QPushButton(tr("Detach from Pool"), this);
    detachButton->setVisible(false);
    connect(detachButton, &QPushButton::clicked, this, &ResearcherWizardCpidSyncPage::onDetachPoolClicked);
    buttonLayout->addWidget(detachButton);
    
    m_skipButton = new QPushButton(tr("Skip (Not Recommended)"), this);
    m_skipButton->setVisible(false);
    connect(m_skipButton, &QPushButton::clicked, this, &ResearcherWizardCpidSyncPage::onSkipClicked);
    buttonLayout->addWidget(m_skipButton);
    
    QPushButton *refreshButton = new QPushButton(tr("Refresh"), this);
    connect(refreshButton, &QPushButton::clicked, this, &ResearcherWizardCpidSyncPage::onRefreshClicked);
    buttonLayout->addWidget(refreshButton);
    
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
}

void ResearcherWizardCpidSyncPage::loadProjects()
{
    m_projectsTree->clear();
    
    // Use BOINC RPC client to get projects
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        showMessage(tr("Failed to connect to BOINC client. Make sure BOINC is running."), true);
        return;
    }
    
    auto projects = boinc_rpc.GetProjectAccounts();
    boinc_rpc.Disconnect();
    
    if (projects.empty()) {
        showMessage(tr("No BOINC projects found. Please attach to at least one project."), true);
        return;
    }
    
    for (const auto& project : projects) {
        addProjectToTree(
            QString::fromStdString(project.project_name),
            QString::fromStdString(project.external_cpid),
            QString::fromStdString(project.email_hash),
            project.rac,
            project.creation_time);
    }
    
    showMessage(tr("Found %1 project(s)").arg(projects.size()));
}

void ResearcherWizardCpidSyncPage::checkPoolAttachment()
{
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        return;
    }
    
    // Known Gridcoin pool CPIDs (from researcher.h)
    std::vector<std::string> known_pool_cpids = {
        "7d0d73fe026d66fd4ab8d5d8da32a611", // grcpool.com
        "a914eba952be5dfcf73d926b508fd5fa", // grcpool.com-2
        "163f049997e8a2dee054d69a7720bf05", // grcpool.com-3
        "f1f4d4e93b5b319b0a54b09dd47f1486", // grcpool.com-5
        "326bb50c0dd0ba9d46e15fae3484af35", // grc.arikado.pool
    };
    
    GRC::CpidSynchronizer synchronizer(boinc_rpc);
    auto result = synchronizer.DetectPool(known_pool_cpids);
    
    boinc_rpc.Disconnect();
    
    m_poolDetected = result.is_pool;
    
    if (m_poolDetected) {
        m_poolDetectionLabel->setText(
            tr("<font color='red'><b>Pool Detected:</b></font> "
               "You are attached to %1 (CPID: %2). "
               "For solo crunching, you must detach from the pool.")
            .arg(QString::fromStdString(result.pool_name))
            .arg(QString::fromStdString(result.pool_cpid)));
        
        m_syncButton->setVisible(false);
    } else {
        m_poolDetectionLabel->setText(
            tr("<font color='green'><b>No Pool Detected:</b></font> "
               "You are not attached to any known Gridcoin mining pools."));
    }
}

void ResearcherWizardCpidSyncPage::validateEmails()
{
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        return;
    }
    
    auto projects = boinc_rpc.GetProjectAccounts();
    GRC::CpidSynchronizer synchronizer(boinc_rpc);
    auto result = synchronizer.ValidateEmails(projects);
    
    boinc_rpc.Disconnect();
    
    m_emailValid = result.all_match;
    m_inconsistentEmails.clear();
    
    for (const auto& inconsistency : result.inconsistencies) {
        m_inconsistentEmails.push_back(QString::fromStdString(inconsistency));
    }
    
    if (m_emailValid) {
        m_emailValidationLabel->setText(
            tr("<font color='green'><b>Email Consistent:</b></font> "
               "All projects use the same email address."));
    } else {
        m_emailValidationLabel->setText(
            tr("<font color='orange'><b>Email Inconsistency Detected:</b></font> "
               "Projects use different email addresses. This may cause CPID splits."));
    }
}

void ResearcherWizardCpidSyncPage::checkCpidMatch()
{
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        return;
    }
    
    auto projects = boinc_rpc.GetProjectAccounts();
    GRC::CpidSynchronizer synchronizer(boinc_rpc);
    auto result = synchronizer.CheckCpidMatch(projects);
    
    boinc_rpc.Disconnect();
    
    m_cpidsMatch = result.all_match;
    m_mismatchedCpids.clear();
    
    for (const auto& mismatch : result.mismatches) {
        m_mismatchedCpids.push_back(QString::fromStdString(mismatch));
    }
    
    if (m_cpidsMatch) {
        m_cpidMatchLabel->setText(
            tr("<font color='green'><b>CPIDs Match:</b></font> "
               "All projects are synchronized to the same CPID."));
        m_syncComplete = true;
        emit completeChanged();
    } else {
        m_cpidMatchLabel->setText(
            tr("<font color='red'><b>CPID Mismatch Detected:</b></font> "
               "Projects have different CPIDs. This will prevent reward accrual."));
        
        m_syncButton->setVisible(true);
        m_skipButton->setVisible(true);
    }
}

QString ResearcherWizardCpidSyncPage::findOldestCpid()
{
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        return QString();
    }
    
    auto projects = boinc_rpc.GetProjectAccounts();
    GRC::CpidSynchronizer synchronizer(boinc_rpc);
    auto oldest = synchronizer.FindOldestCpid(projects);
    
    boinc_rpc.Disconnect();
    
    m_resolvedCpid = QString::fromStdString(oldest.cpid);
    
    return m_resolvedCpid;
}

void ResearcherWizardCpidSyncPage::synchronizeCpids(const QString& targetCpid)
{
    m_progressBar->setVisible(true);
    m_statusLabel->setText(tr("Synchronizing CPIDs..."));
    QApplication::processEvents();
    
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        showMessage(tr("Failed to connect to BOINC client."), true);
        m_progressBar->setVisible(false);
        return;
    }
    
    auto projects = boinc_rpc.GetProjectAccounts();
    GRC::CpidSynchronizer synchronizer(boinc_rpc);
    
    // Get email from configuration
    QString email = m_researcher_model ? m_researcher_model->email() : QString();
    
    auto result = synchronizer.SyncCpids(
        projects,
        targetCpid.toStdString(),
        email.toStdString());
    
    boinc_rpc.Disconnect();
    
    m_progressBar->setVisible(false);
    
    if (result.success) {
        m_syncComplete = true;
        m_cpidsMatch = true;
        m_cpidMatchLabel->setText(
            tr("<font color='green'><b>CPIDs Synchronized:</b></font> ") +
            tr("All projects now use CPID %1").arg(targetCpid));
        
        showMessage(QString::fromStdString(result.message));
        
        // Refresh project list
        loadProjects();
        emit completeChanged();
    } else {
        QString errors;
        for (const auto& error : result.errors) {
            errors += QString::fromStdString(error) + "\n";
        }
        showMessage(tr("Synchronization failed:\n%1").arg(errors), true);
    }
}

void ResearcherWizardCpidSyncPage::addProjectToTree(
    const QString& name,
    const QString& cpid,
    const QString& email,
    double rac,
    qint64 creationTime)
{
    auto *item = new QTreeWidgetItem(m_projectsTree);
    item->setText(0, name);
    item->setText(1, cpid);
    item->setText(2, email);
    item->setText(3, QString::number(rac, 'f', 2));
    
    if (creationTime > 0) {
        QDateTime dt;
        dt.setSecsSinceEpoch(creationTime);
        item->setText(4, dt.toString("yyyy-MM-dd"));
    } else {
        item->setText(4, tr("Unknown"));
    }
    
    // Highlight mismatched CPIDs
    if (!m_cpidsMatch && !m_mismatchedCpids.isEmpty()) {
        bool isMismatch = false;
        for (const auto& mismatch : m_mismatchedCpids) {
            if (mismatch.contains(name)) {
                isMismatch = true;
                break;
            }
        }
        if (isMismatch) {
            item->setBackgroundColor(0, QColor(255, 200, 200));
        }
    }
}

void ResearcherWizardCpidSyncPage::updateStatusLabels()
{
    // Status is already updated in individual check methods
}

void ResearcherWizardCpidSyncPage::showMessage(const QString& message, bool error)
{
    if (error) {
        m_statusLabel->setText(tr("<font color='red'>%1</font>").arg(message));
    } else {
        m_statusLabel->setText(message);
    }
}

void ResearcherWizardCpidSyncPage::onSyncClicked()
{
    if (m_poolDetected) {
        QMessageBox::warning(
            this,
            tr("Pool Detected"),
            tr("You must detach from the mining pool before synchronizing CPIDs."));
        return;
    }
    
    QString oldestCpid = findOldestCpid();
    
    if (oldestCpid.isEmpty()) {
        showMessage(tr("Failed to find oldest CPID."), true);
        return;
    }
    
    int ret = QMessageBox::question(
        this,
        tr("Confirm CPID Synchronization"),
        tr("This will update all BOINC project XML files to use the oldest CPID found:\n\n"
           "CPID: %1\n\n"
           "This process will:\n"
           "1. Close BOINC client\n"
           "2. Backup and modify client_state.xml\n"
           "3. Restart BOINC client\n\n"
           "Continue?")
        .arg(oldestCpid),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        synchronizeCpids(oldestCpid);
    }
}

void ResearcherWizardCpidSyncPage::onSkipClicked()
{
    int ret = QMessageBox::warning(
        this,
        tr("Confirm Skip"),
        tr("Skipping CPID synchronization may result in:\n\n"
           "- Split CPIDs across projects\n"
           "- Inability to earn research rewards\n"
           "- Beacon verification failures\n\n"
           "Are you sure you want to skip?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        m_syncComplete = true;
        emit completeChanged();
    }
}

void ResearcherWizardCpidSyncPage::onDetachPoolClicked()
{
    GRC::BoincRpcClient boinc_rpc;
    
    if (!boinc_rpc.Connect()) {
        showMessage(tr("Failed to connect to BOINC client."), true);
        return;
    }
    
    auto projects = boinc_rpc.GetProjectAccounts();
    
    // Known Gridcoin pool CPIDs
    std::vector<std::string> known_pool_cpids = {
        "7d0d73fe026d66fd4ab8d5d8da32a611",
        "a914eba952be5dfcf73d926b508fd5fa",
        "163f049997e8a2dee054d69a7720bf05",
        "f1f4d4e93b5b319b0a54b09dd47f1486",
        "326bb50c0dd0ba9d46e15fae3484af35",
    };
    
    int detached_count = 0;
    
    for (const auto& project : projects) {
        for (const auto& pool_cpid : known_pool_cpids) {
            if (project.external_cpid == pool_cpid) {
                if (boinc_rpc.DetachProject(project.project_url, false)) {
                    detached_count++;
                }
            }
        }
    }
    
    boinc_rpc.Disconnect();
    
    if (detached_count > 0) {
        showMessage(tr("Successfully detached from %1 pool project(s). Please refresh to verify.")
                   .arg(detached_count));
        m_poolDetected = false;
        m_poolDetectionLabel->setText(
            tr("<font color='green'><b>Pool Detached:</b></font> "
               "Successfully detached from pool projects."));
    } else {
        showMessage(tr("Failed to detach from pool projects."), true);
    }
}

void ResearcherWizardCpidSyncPage::onRefreshClicked()
{
    initializePage();
}
