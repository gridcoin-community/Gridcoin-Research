// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DIAGNOSTICSDIALOG_H
#define BITCOIN_QT_DIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QtWidgets/QLabel>

#include <string>
#include <unordered_map>

class ClientModel;
class ResearcherModel;

namespace interfaces {
class Node;
} // namespace interfaces

namespace Ui {
class DiagnosticsDialog;
}

class DiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagnosticsDialog(QWidget* parent = nullptr, ResearcherModel* researcher_model = nullptr);
    ~DiagnosticsDialog();

    enum DiagnosticResult
    {
        NA,
        passed,
        warning,
        failed
    };

    enum DiagnosticTestStatus
    {
        unknown,
        pending,
        completed
    };

private:
    Ui::DiagnosticsDialog *ui;

    //! Maps each diagnostic test (interfaces::DiagnosticTest raw value, matching
    //! the test_id carried in interfaces::DiagnosticResult) to its result row
    //! label. Built once at construction; the node owns the tests now, so the
    //! dialog holds only display state.
    std::unordered_map<int, QLabel*> m_test_labels;

    // Holds the overall result of all diagnostic tests
    DiagnosticResult m_overall_diagnostic_result;

    // Holds the status of the overall diagnostic result
    DiagnosticTestStatus m_overall_diagnostic_result_status;

    // Holds the number of tests to be registered.
    // This needs to be updated if the number of tests is changed.
    unsigned int m_number_of_tests = 12;

    // Boolean to indicate researcher mode.
    bool m_researcher_mode = true;

    // Holds the test status and result entries, keyed by test_id.
    typedef std::unordered_map<int, DiagnosticTestStatus> DiagnosticTestStatus_map;
    typedef std::unordered_map<int, DiagnosticResult> DiagnosticTestResult_map;
    DiagnosticTestStatus_map m_test_status_map;
    DiagnosticTestResult_map m_test_result_map;

    ResearcherModel *m_researcher_model;

    //! The node interface used to run the diagnostics. Set via setClientModel()
    //! from the client model's node(); the tests execute in the node.
    interfaces::Node* m_node = nullptr;

public:
    void setClientModel(ClientModel* client_model);
    void SetResearcherModel(ResearcherModel *researcherModel);
    unsigned int GetNumberOfTestsPending();
    unsigned int UpdateTestStatus(int test_id, QLabel *label,
                                  DiagnosticTestStatus test_status, DiagnosticResult test_result,
                                  QString override_text = QString(), QString tooltip_text = QString());
    DiagnosticTestStatus GetTestStatus(int test_id);
    void UpdateTestResult(int test_id, DiagnosticResult test_result);
    void ResetOverallDiagnosticResult();
    void UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in);
    DiagnosticResult GetTestResult(int test_id);
    DiagnosticResult GetOverallDiagnosticResult();
    DiagnosticTestStatus GetOverallDiagnosticStatus();
    void DisplayOverallDiagnosticResult();

private:
    void SetResultLabel(QLabel *label, DiagnosticTestStatus test_status,
                        DiagnosticResult test_result, QString override_text = QString(),
                        QString tooltip_text = QString());

private slots:
    void on_testButton_clicked();

};

#endif // BITCOIN_QT_DIAGNOSTICSDIALOG_H
