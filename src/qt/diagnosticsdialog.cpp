// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "diagnosticsdialog.h"
#include "interfaces/node.h"
#include "qt/clientmodel.h"
#include "qt/guilog.h"
#include "qt/forms/ui_diagnosticsdialog.h"
#include "qt/decoration.h"
#include "qt/researcher/researchermodel.h"

#include <QCoreApplication>

#include <algorithm>

namespace {
//! Map a node-side diagnostic status (interfaces::DiagnosticStatus raw value) to
//! the dialog's display result. The node runs the tests; the dialog only renders
//! the outcome.
DiagnosticsDialog::DiagnosticResult MapStatus(int status)
{
    switch (static_cast<interfaces::DiagnosticStatus>(status)) {
    case interfaces::DiagnosticStatus::FAIL:    return DiagnosticsDialog::failed;
    case interfaces::DiagnosticStatus::WARNING: return DiagnosticsDialog::warning;
    case interfaces::DiagnosticStatus::PASS:    return DiagnosticsDialog::passed;
    case interfaces::DiagnosticStatus::NONE:    return DiagnosticsDialog::NA;
    }

    return DiagnosticsDialog::NA;
}
} // anonymous namespace

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

    // Associate each result-row label with the test whose outcome it displays.
    // The tests themselves run in the node (interfaces::Node::runDiagnostics);
    // the dialog keys on the interfaces::DiagnosticTest id carried back in each
    // interfaces::DiagnosticResult. If a test is added, add its row here and bump
    // m_number_of_tests.
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::CheckConnectionCount)] =
        ui->checkConnectionCountResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::CheckOutboundConnectionCount)] =
        ui->checkOutboundConnectionCountResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyWalletIsSynced)] =
        ui->verifyWalletIsSyncedResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyClock)] =
        ui->verifyClockResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::CheckClientVersion)] =
        ui->checkClientVersionResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyBoincPath)] =
        ui->verifyBoincPathResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyCPIDValid)] =
        ui->verifyCPIDValidResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyCPIDHasRAC)] =
        ui->verifyCPIDHasRACResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyCPIDIsActive)] =
        ui->verifyCPIDIsActiveResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::VerifyTCPPort)] =
        ui->verifyTCPPortResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::CheckDifficulty)] =
        ui->checkDifficultyResultLabel;
    m_test_labels[static_cast<int>(interfaces::DiagnosticTest::CheckETTS)] =
        ui->checkETTSResultLabel;
}

DiagnosticsDialog::~DiagnosticsDialog()
{
    delete ui;
}

void DiagnosticsDialog::setClientModel(ClientModel* client_model)
{
    m_node = client_model ? &client_model->node() : nullptr;
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
    unsigned int pending_count = 0;

    for (const auto& entry : m_test_status_map)
    {
        if (entry.second == pending) pending_count++;
    }

    return pending_count;
}

DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetTestStatus(int test_id)
{
    auto entry = m_test_status_map.find(test_id);

    if (entry != m_test_status_map.end())
    {
        return entry->second;
    }
    else
    {
        return unknown;
    }
}

unsigned int DiagnosticsDialog::UpdateTestStatus(int test_id, QLabel *label,
                                                 DiagnosticTestStatus test_status, DiagnosticResult test_result,
                                                 QString override_text, QString tooltip_text)
{
    m_test_status_map[test_id] = test_status;

    SetResultLabel(label, test_status, test_result, override_text, tooltip_text);

    UpdateTestResult(test_id, test_result);

    UpdateOverallDiagnosticResult(test_result);

    return m_test_status_map.size();
}

void DiagnosticsDialog::UpdateTestResult(int test_id, DiagnosticResult test_result)
{
    m_test_result_map[test_id] = test_result;
}

DiagnosticsDialog::DiagnosticResult DiagnosticsDialog::GetTestResult(int test_id)
{
    DiagnosticResult result;

    auto iter = m_test_result_map.find(test_id);

    if (iter == m_test_result_map.end()) {
        result = NA;
    } else {
        result = iter->second;
    }

    return result;
}

void DiagnosticsDialog::ResetOverallDiagnosticResult()
{
    m_test_status_map.clear();
    m_test_result_map.clear();

    m_overall_diagnostic_result_status = pending;

    m_overall_diagnostic_result = NA;
}

void DiagnosticsDialog::UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in)
{
    // Set diagnostic_result_status to completed.
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

DiagnosticsDialog::DiagnosticResult DiagnosticsDialog::GetOverallDiagnosticResult()
{
    return m_overall_diagnostic_result;
}

DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetOverallDiagnosticStatus()
{
    return m_overall_diagnostic_result_status;
}


void DiagnosticsDialog::DisplayOverallDiagnosticResult()
{
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
        GUILogPrintf("INFO: DiagnosticsDialog::on_testButton_clicked: Tests still in progress from a prior run: %u",
                  GetNumberOfTestsPending());

        return;
    }

    if (!m_node) return;

    ResetOverallDiagnosticResult();

    // Show every row as pending, then let the event loop paint before the
    // blocking node call. runDiagnostics() runs all tests (including the NTP and
    // TCP-port network probes) and can take several seconds.
    for (const auto& [test_id, label] : m_test_labels) {
        UpdateTestStatus(test_id, label, pending, NA);
    }
    DisplayOverallDiagnosticResult();
    QCoreApplication::processEvents();

    for (const interfaces::DiagnosticResult& result : m_node->runDiagnostics()) {
        auto entry = m_test_labels.find(result.test_id);
        if (entry == m_test_labels.end()) continue;

        UpdateTestStatus(result.test_id, entry->second, completed, MapStatus(result.status),
                         QString::fromStdString(result.result_string),
                         QString::fromStdString(result.tip_string));
    }

    DisplayOverallDiagnosticResult();
}
