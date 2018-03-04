#ifndef DIAGNOSTICSDIALOG_H
#define DIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QtNetwork>

#include <string>

namespace Ui {
class DiagnosticsDialog;
}

class DiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagnosticsDialog(QWidget *parent = 0);
    ~DiagnosticsDialog();

private:
    Ui::DiagnosticsDialog *ui;
    void GetData();
    void VerifyClock();
    void VerifyTCPPort();
    bool VerifyBoincPath();
    bool VerifyCPIDIsInNeuralNetwork();
    bool VerifyWalletIsSynced();
    bool VerifyIsCPIDValid();
    bool FindCPID();
    bool VerifyCPIDHasRAC();
    int VerifyCountSeedNodes();
    int VerifyCountConnections();
    double GetTotalCPIDRAC(std::string cpid);
    double GetUserRAC(std::string cpid, int *projects);
    std::string KeyValue(std::string key);

    QUdpSocket *udpSocket;
    QTcpSocket *tcpSocket;
    QNetworkAccessManager *networkManager;
    std::string testnet_flag;
    std::string syncData;

private slots:
    void on_testBtn_clicked();
    void clkFinished();
    void clkStateChanged(QAbstractSocket::SocketState state);
    void clkSocketError(QAbstractSocket::SocketError error);
    void TCPFinished();
    void TCPFailed(QAbstractSocket::SocketError socketError);
    void getGithubVersionFinished(QNetworkReply *reply);
};

#endif // DIAGNOSTICSDIALOG_H
