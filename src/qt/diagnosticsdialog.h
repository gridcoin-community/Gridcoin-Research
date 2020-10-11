// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DIAGNOSTICSDIALOG_H
#define DIAGNOSTICSDIALOG_H

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
    explicit DiagnosticsDialog(QWidget *parent = 0, ResearcherModel* researcher_model = nullptr);
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

    //typedef std::atomic<DiagnosticResultCategory> DiagnosticResult;

private:
    Ui::DiagnosticsDialog *ui;
    void GetData();
    void VerifyClock();
    void VerifyTCPPort();
    bool VerifyBoincPath();
    bool VerifyCPIDIsEligible();
    bool VerifyWalletIsSynced();
    bool VerifyIsCPIDValid();
    bool VerifyCPIDHasRAC();
    double VerifyETTSReasonable();
    int VerifyCountSeedNodes();
    int VerifyCountConnections();
    double GetTotalCPIDRAC(std::string cpid);
    double GetUserRAC(std::string cpid, int *projects);
    std::string KeyValue(std::string key);

    // Because some of the tests are "spurs", this object is multithreaded
    CCriticalSection cs_diagnostictests;

    // Holds the overall result of all diagnostic tests
    DiagnosticResult diagnostic_result;

    // Holds the status of the overall diagnostic result
    DiagnosticTestStatus diagnostic_result_status;

    // Holds the number of tests registered.
    unsigned int number_of_tests = 0;

    // Holds the test status entries
    typedef std::unordered_map<std::string, DiagnosticTestStatus> mDiagnosticTestStatus;
    mDiagnosticTestStatus test_status_map;

    ResearcherModel *m_researcher_model;

    QUdpSocket *udpSocket;
    QTcpSocket *tcpSocket;

public:
    void SetResearcherModel(ResearcherModel *researcherModel);
    unsigned int GetNumberOfTestsPending();
    unsigned int UpdateTestStatus(std::string test_name, QLabel *label,
                                  DiagnosticTestStatus test_status, DiagnosticResult test_result,
                                  QString override_text = QString());
    DiagnosticTestStatus GetTestStatus(std::string test_name);
    void ResetOverallDiagnosticResult(unsigned int& number_of_tests);
    void UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in);
    DiagnosticResult GetOverallDiagnosticResult();
    DiagnosticTestStatus GetOverallDiagnosticStatus();
    void DisplayOverallDiagnosticResult();

private:
    void SetResultLabel(QLabel *label, DiagnosticTestStatus test_status,
                        DiagnosticResult test_result, QString override_text = QString());

private slots:
    void on_testButton_clicked();
    void clkFinished();
    void clkStateChanged(QAbstractSocket::SocketState state);
    void clkSocketError();
    void TCPFinished();
    void TCPFailed(QAbstractSocket::SocketError socketError);
};

#endif // DIAGNOSTICSDIALOG_H
