// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DIAGNOSTICSDIALOG_H
#define BITCOIN_QT_DIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QtNetwork>
#include <QtWidgets/QLabel>

#include <string>
#include <unordered_map>

#include "sync.h"

class ResearcherModel;

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
    void GetData();
    void VerifyWalletIsSynced();
    int CheckConnectionCount();
    void CheckOutboundConnectionCount();
    void VerifyClock(unsigned int connections);
    void VerifyTCPPort();
    double CheckDifficulty();
    void CheckClientVersion();
    void VerifyBoincPath();
    void VerifyCPIDValid();
    void VerifyCPIDHasRAC();
    void VerifyCPIDIsActive();
    void CheckETTS(const double& diff);

    // Because some of the tests are "spurs", this object is multithreaded
    CCriticalSection cs_diagnostictests;

    // Holds the overall result of all diagnostic tests
    DiagnosticResult m_overall_diagnostic_result;

    // Holds the status of the overall diagnostic result
    DiagnosticTestStatus m_overall_diagnostic_result_status;

    // Holds the number of tests to be registered.
    // This needs to be updated if the number of tests is changed.
    unsigned int m_number_of_tests = 12;

    // Boolean to indicate researcher mode.
    bool m_researcher_mode = true;

    // Holds the test status and result entries
    typedef std::unordered_map<std::string, DiagnosticTestStatus> DiagnosticTestStatus_map;
    typedef std::unordered_map<std::string, DiagnosticResult> DiagnosticTestResult_map;
    DiagnosticTestStatus_map m_test_status_map;
    DiagnosticTestResult_map m_test_result_map;

    ResearcherModel *m_researcher_model;

    QUdpSocket *m_udpSocket;
    QTcpSocket *m_tcpSocket;

public:
    void SetResearcherModel(ResearcherModel *researcherModel);
    unsigned int GetNumberOfTestsPending();
    unsigned int UpdateTestStatus(std::string test_name, QLabel *label,
                                  DiagnosticTestStatus test_status, DiagnosticResult test_result,
                                  QString override_text = QString(), QString tooltip_text = QString());
    DiagnosticTestStatus GetTestStatus(std::string test_name);
    void UpdateTestResult(std::string test_name, DiagnosticResult test_result);
    void ResetOverallDiagnosticResult();
    void UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in);
    DiagnosticResult GetTestResult(std::string test_name);
    DiagnosticResult GetOverallDiagnosticResult();
    DiagnosticTestStatus GetOverallDiagnosticStatus();
    void DisplayOverallDiagnosticResult();

private:
    void SetResultLabel(QLabel *label, DiagnosticTestStatus test_status,
                        DiagnosticResult test_result, QString override_text = QString(),
                        QString tooltip_text = QString());

private slots:
    void on_testButton_clicked();
    void clkFinished();
    void clkStateChanged(QAbstractSocket::SocketState state);
    void clkSocketError();
    void clkReportResults(const int64_t& time_offset, const bool& timeout_during_check = false);
    void TCPFinished();
    void TCPFailed(QAbstractSocket::SocketError socket_error);
};

#endif // BITCOIN_QT_DIAGNOSTICSDIALOG_H
