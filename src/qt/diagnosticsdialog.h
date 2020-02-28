#ifndef DIAGNOSTICSDIALOG_H
#define DIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QtNetwork>

#include <string>
#include <unordered_map>

#include "sync.h"

namespace Ui {
class DiagnosticsDialog;
}

class DiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagnosticsDialog(QWidget *parent = 0);
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
    bool VerifyCPIDIsInNeuralNetwork();
    bool VerifyWalletIsSynced();
    bool VerifyIsCPIDValid();
    bool VerifyCPIDHasRAC();
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

    QUdpSocket *udpSocket;
    QTcpSocket *tcpSocket;

public:
    unsigned int GetNumberOfTestsPending();
    unsigned int UpdateTestStatus(std::string test_name, DiagnosticTestStatus test_status);
    void ResetOverallDiagnosticResult(unsigned int& number_of_tests);
    void UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in);
    DiagnosticResult GetOverallDiagnosticResult();
    DiagnosticTestStatus GetOverallDiagnosticStatus();
    void DisplayOverallDiagnosticResult();

private slots:
    void on_testButton_clicked();
    void clkFinished();
    void clkStateChanged(QAbstractSocket::SocketState state);
    void clkSocketError(QAbstractSocket::SocketError error);
    void TCPFinished();
    void TCPFailed(QAbstractSocket::SocketError socketError);
};

#endif // DIAGNOSTICSDIALOG_H
