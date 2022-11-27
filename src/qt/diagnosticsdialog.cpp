// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "qt/forms/ui_diagnosticsdialog.h"
#include "qt/decoration.h"
#include "qt/researcher/researchermodel.h"

#include <numeric>
#include <type_traits>

extern std::atomic<int64_t> g_nTimeBestReceived;

DiagnosticsDialog::DiagnosticsDialog(QWidget *parent, ResearcherModel* researcher_model) :
    QDialog(parent),
    ui(new Ui::DiagnosticsDialog),
    m_researcher_model(researcher_model)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    GRC::ScaleFontPointSize(ui->diagnosticsLabel, 14);
    GRC::ScaleFontPointSize(ui->overallResultLabel, 12);
    GRC::ScaleFontPointSize(ui->overallResultResultLabel, 12);

    // Construct the tests needed.
    // If need to add a test m just add it to the below set.
    // Check Diagnose.h for the base class to create tests.
    diagnoseTestInsertInSet(ui->checkConnectionCountResultLabel,
                            std::make_unique<DiagnoseLib::CheckConnectionCount>());
    diagnoseTestInsertInSet(ui->checkOutboundConnectionCountResultLabel,
                            std::make_unique<DiagnoseLib::CheckOutboundConnectionCount>());
    diagnoseTestInsertInSet(ui->verifyWalletIsSyncedResultLabel,
                            std::make_unique<DiagnoseLib::VerifyWalletIsSynced>());
    diagnoseTestInsertInSet(ui->verifyClockResultLabel,
                            std::make_unique<DiagnoseLib::VerifyClock>());
    diagnoseTestInsertInSet(ui->checkClientVersionResultLabel,
                            std::make_unique<DiagnoseLib::CheckClientVersion>());
    diagnoseTestInsertInSet(ui->verifyBoincPathResultLabel,
                            std::make_unique<DiagnoseLib::VerifyBoincPath>());
    diagnoseTestInsertInSet(ui->verifyCPIDValidResultLabel,
                            std::make_unique<DiagnoseLib::VerifyCPIDValid>());
    diagnoseTestInsertInSet(ui->verifyCPIDHasRACResultLabel,
                            std::make_unique<DiagnoseLib::VerifyCPIDHasRAC>());
    diagnoseTestInsertInSet(ui->verifyCPIDIsActiveResultLabel,
                            std::make_unique<DiagnoseLib::VerifyCPIDIsActive>());
    diagnoseTestInsertInSet(ui->verifyTCPPortResultLabel,
                            std::make_unique<DiagnoseLib::VerifyTCPPort>());
    diagnoseTestInsertInSet(ui->checkDifficultyResultLabel,
                            std::make_unique<DiagnoseLib::CheckDifficulty>());
    diagnoseTestInsertInSet(ui->checkETTSResultLabel,
                            std::make_unique<DiagnoseLib::CheckETTS>());
}

DiagnosticsDialog::~DiagnosticsDialog()
{
    delete ui;
}

void DiagnosticsDialog::SetResearcherModel(ResearcherModel *researcherModel)
{
    m_researcher_model = researcherModel;
}

void DiagnosticsDialog::SetResultLabel(QLabel *label, DiagnosticTestStatus test_status,
                                       DiagnosticResult test_result, QString override_text, QString tooltip_text)
{
    switch (test_status)
    {
    case DiagnosticTestStatus::unknown:
        label->setText(tr(""));
        label->setStyleSheet("");
        break;
    case DiagnosticTestStatus::pending:
        label->setText(tr("Testing..."));
        label->setStyleSheet("");
        break;
    case DiagnosticTestStatus::completed:
        switch (test_result)
        {
        case DiagnosticResult::NA:
            label->setText(tr("N/A"));
            label->setStyleSheet("color:black;background-color:grey");
            break;
        case DiagnosticResult::passed:
            label->setText(tr("Passed"));
            label->setStyleSheet("color:white;background-color:green");
            break;
        case DiagnosticResult::warning:
            label->setText(tr("Warning"));
            label->setStyleSheet("color:black;background-color:yellow");
            break;
        case DiagnosticResult::failed:
            label->setText(tr("Failed"));
            label->setStyleSheet("color:white;background-color:red");
        }
    }

    if (override_text.size()) label->setText(override_text);
    label->setToolTip(tooltip_text);

    this->repaint();
}

unsigned int DiagnosticsDialog::GetNumberOfTestsPending()
{
    LOCK(cs_diagnostictests);

    unsigned int pending_count = 0;

    for (const auto& entry : m_test_status_map)
    {
        if (entry.second == pending) pending_count++;
    }

    return pending_count;
}

DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetTestStatus(DiagnoseLib::Diagnose::TestNames test_name)
{
    LOCK(cs_diagnostictests);

    auto entry = m_test_status_map.find(test_name);

    if (entry != m_test_status_map.end())
    {
        return entry->second;
    }
    else
    {
        return unknown;
    }
}

unsigned int DiagnosticsDialog::UpdateTestStatus(DiagnoseLib::Diagnose::TestNames test_name, QLabel *label,
                                                 DiagnosticTestStatus test_status, DiagnosticResult test_result,
                                                 QString override_text, QString tooltip_text)
{
    LOCK(cs_diagnostictests);

    m_test_status_map[test_name] = test_status;

    SetResultLabel(label, test_status, test_result, override_text, tooltip_text);

    UpdateTestResult(test_name, test_result);

    UpdateOverallDiagnosticResult(test_result);

    return m_test_status_map.size();
}

void DiagnosticsDialog::UpdateTestResult(DiagnoseLib::Diagnose::TestNames test_name, DiagnosticResult test_result)
{
    LOCK(cs_diagnostictests);

    m_test_result_map[test_name] = test_result;
}

DiagnosticsDialog::DiagnosticResult DiagnosticsDialog::GetTestResult(DiagnoseLib::Diagnose::TestNames test_name)
{
    LOCK(cs_diagnostictests);

    DiagnosticResult result;

    auto iter = m_test_result_map.find(test_name);

    if (iter == m_test_result_map.end()) {
        result = NA;
    } else {
        result = iter->second;
    }

    return result;
}

void DiagnosticsDialog::ResetOverallDiagnosticResult()
{
    LOCK(cs_diagnostictests);

    m_test_status_map.clear();
    m_test_result_map.clear();

    m_overall_diagnostic_result_status = pending;

    m_overall_diagnostic_result = NA;
}

void DiagnosticsDialog::UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in)
{
    LOCK(cs_diagnostictests);

    // Set diagnostic_result_status to completed. This is under lock, so no one can snoop.
    m_overall_diagnostic_result_status = completed;

    // If the total number of registered tests is less than the initialized number, then
    // the overall status is pending by default.
    if (m_test_status_map.size() < m_number_of_tests)
    {
        m_overall_diagnostic_result_status = pending;
    }

    m_overall_diagnostic_result = (DiagnosticResult) std::max<int>(diagnostic_result_in,  m_overall_diagnostic_result);

    // If diagnostic_result_status is still set to completed, then at least all tests
    // are registered. Walk through the map of tests one by one and check the status.
    // The first one encountered that is pending means the overall status is pending.
    if (m_overall_diagnostic_result_status == completed)
    {
        for (const auto& entry : m_test_status_map)
        {
            if (entry.second == pending)
            {
                m_overall_diagnostic_result_status = pending;
                break;
            }
        }
    }
}

// Lock should be taken on cs_diagnostictests before calling this function.
DiagnosticsDialog::DiagnosticResult DiagnosticsDialog::GetOverallDiagnosticResult()
{
    return m_overall_diagnostic_result;
}

// Lock should be taken on cs_diagnostictests before calling this function.
DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetOverallDiagnosticStatus()
{
    return m_overall_diagnostic_result_status;
}


void DiagnosticsDialog::DisplayOverallDiagnosticResult()
{
    LOCK(cs_diagnostictests);

    DiagnosticsDialog::DiagnosticTestStatus overall_diagnostic_status = GetOverallDiagnosticStatus();
    DiagnosticsDialog::DiagnosticResult overall_diagnostic_result = GetOverallDiagnosticResult();

    QString tooltip;

    if (overall_diagnostic_result == warning)
    {
        tooltip = tr("One or more tests have generated a warning status. Wallet operation may be degraded. Please see "
                     "the individual test tooltips for details and recommended action(s).");
    }
    else if (overall_diagnostic_result == failed)
    {
        tooltip = tr("One or more tests have failed. Proper wallet operation may be significantly degraded or impossible. "
                     "Please see the individual test tooltips for details and recommended action(s).");
    }
    else if (overall_diagnostic_result == passed)
    {
        tooltip = tr("All tests passed. Your wallet operation is normal.");
    }
    else
    {
        tooltip = QString();
    }

    SetResultLabel(ui->overallResultResultLabel, overall_diagnostic_status, overall_diagnostic_result,
                   QString(), tooltip);
}

void DiagnosticsDialog::on_testButton_clicked()
{
    // Check to see if there is already a test run in progress, and if so return.
    // We do not want overlapping test runs.

    if (GetNumberOfTestsPending())
    {
        LogPrintf("INFO: DiagnosticsDialog::on_testButton_clicked: Tests still in progress from a prior run: %u",
                  GetNumberOfTestsPending());

        return;
    }

    ResetOverallDiagnosticResult();
    DisplayOverallDiagnosticResult();

    DiagnoseLib::Diagnose::setResearcherModel();

    for (auto& i : m_diagnostic_tests) {
        auto& diagnose_test = i.second;
        auto diagnoselabel = i.first;
        UpdateTestStatus(i.second->getTestName(), diagnoselabel, pending, NA);
        diagnose_test->runCheck();
        QString tooltip = tr(diagnose_test->getResultsTip().c_str());
        QString resultString = tr(diagnose_test->getResultsString().c_str());
        for (auto& j : diagnose_test->getStringArgs()) {
            resultString = resultString.arg(QString::fromStdString(j));
        }
        for (auto& j : diagnose_test->getTipArgs()) {
            tooltip = tooltip.arg(QString::fromStdString(j));
        }

        if (diagnose_test->getResults() == DiagnoseLib::Diagnose::NONE) {
            UpdateTestStatus(i.second->getTestName(), diagnoselabel, completed, NA);
        } else if (diagnose_test->getResults() == DiagnoseLib::Diagnose::FAIL) {
            UpdateTestStatus(i.second->getTestName(), diagnoselabel, completed, failed,
                             resultString, tooltip);
        } else if (diagnose_test->getResults() == DiagnoseLib::Diagnose::WARNING) {
            UpdateTestStatus(i.second->getTestName(), diagnoselabel, completed, warning,
                             resultString, tooltip);
        } else {
            UpdateTestStatus(i.second->getTestName(), diagnoselabel, completed, passed,
                             resultString);
        }
    }

    DisplayOverallDiagnosticResult();
}
