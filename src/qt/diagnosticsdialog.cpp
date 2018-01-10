#include "main.h"
#include "util.h"
#include "boinc.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"

#include <numeric>
#include <fstream>

std::string GetListOf(std::string datatype);

DiagnosticsDialog::DiagnosticsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiagnosticsDialog)
{
    ui->setupUi(this);

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(getGithubVersionFinished(QNetworkReply *)));
}

DiagnosticsDialog::~DiagnosticsDialog()
{
    delete ui;
}

int DiagnosticsDialog::VerifyBoincPath() {
    boost::filesystem::path boincPath = (boost::filesystem::path) GetBoincDataDir();
    if(boincPath.empty())
        boincPath = (boost::filesystem::path) GetArgument("boincdatadir", "");

    boincPath = boincPath / "client_state.xml";

    if(boost::filesystem::exists(boincPath))
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::FindCPID() {
    std::string cpid = GetArgument("cpid", "");

    if(cpid.length() != 0)
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::VerifyIsCPIDValid() {
    boost::filesystem::path clientStatePath = (boost::filesystem::path) GetBoincDataDir();
    if(!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";
    else
        clientStatePath = (boost::filesystem::path) GetArgument("boincdatadir", "") / "client_state.xml";

    if(clientStatePath.empty())
        return 0;

    std::ifstream clientStateStream;
    std::string clientState;

    clientStateStream.open(clientStatePath.string());
    clientState.assign((std::istreambuf_iterator<char>(clientStateStream)), std::istreambuf_iterator<char>());
    clientStateStream.close();

    size_t pos = 0;
    std::string cpid;
    if((pos = clientState.find("<external_cpid>")) != std::string::npos) {
        cpid = clientState.substr(pos + 15, clientState.length());
        pos = cpid.find("</external_cpid>");
        cpid.erase(pos, cpid.length());
    }

    if(GetArgument("cpid", "") == cpid)
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork() {
    std::string beacons = GetListOf("beacon");
    boost::algorithm::to_lower(beacons);

    if(beacons.length() < 100)
        return -1;

    std::string cpid = GetArgument("cpid", "");
    if(cpid != "" && beacons.find(cpid) != std::string::npos)
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::VerifyWalletIsSynced() {
    int64_t nwalletAge = PreviousBlockAge();

    if(nwalletAge < (60 * 60))
        return 1;
    else
        return 0;

}

int DiagnosticsDialog::VerifyCPIDHasRAC() {
    boost::filesystem::path clientStatePath = (boost::filesystem::path) GetBoincDataDir();
    if(!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";
    else
        clientStatePath = (boost::filesystem::path) GetArgument("boincdatadir", "") / "client_state.xml";

    if(clientStatePath.empty())
        return 0;

    std::ifstream clientStateStream;
    std::string clientState;
    std::vector<std::string> racStrings;
    std::vector<int> racValues;

    clientStateStream.open(clientStatePath.string());
    clientState.assign((std::istreambuf_iterator<char>(clientStateStream)), std::istreambuf_iterator<char>());
    clientStateStream.close();

    if(clientState.length() > 0) {
        size_t pos = 0;
        std::string delim = "</user_expavg_credit>";
        std::string line;
        while ((pos = clientState.find(delim)) != std::string::npos) {
            line = clientState.substr(0, pos);
            clientState.erase(0, pos + delim.length());
            pos = line.find("<user_expavg_credit>");
            racStrings.push_back(line.substr(pos + 20, line.length()));
        }
        for(std::string racString : racStrings) {
            racValues.push_back(std::stod(racString, nullptr));
        }
        return (int) std::accumulate(racValues.begin(), racValues.end(), 0);
    } else
        return 0;

}

int DiagnosticsDialog::VerifyAddNode() {
    long int peersLength = boost::filesystem::file_size(GetDataDir(true) / "peers.dat");
    std::string addNode = GetArgument("addnode", "");

    if(addNode.length() == 0 && peersLength > 100)
        return 2;
    if(addNode.length() > 4 && peersLength == 0)
        return 3;
    if(peersLength > 100)
        return 1;
    if(peersLength == 0 && addNode == "")
        return -1;
    return 0;

}

void DiagnosticsDialog::VerifyClock() {
    QHostInfo NTPHost = QHostInfo::fromName("pool.ntp.org");
    udpSocket = new QUdpSocket(this);

    connect(udpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(clkStateChanged(QAbstractSocket::SocketState)));
    connect(udpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(clkSocketError(QAbstractSocket::SocketError)));

    udpSocket->connectToHost(QHostAddress(NTPHost.addresses().first()), 123, QIODevice::ReadWrite);
}



void DiagnosticsDialog::VerifyTCPPort() {
    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(TCPFinished()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(TCPFailed(QAbstractSocket::SocketError)));

    tcpSocket->connectToHost("london.grcnode.co.uk", 32749);

}

void DiagnosticsDialog::on_testBtn_clicked() {
    int result = 0;
    //boinc path
    ui->boincPathResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyBoincPath();
    if(result == 1)
        ui->boincPathResultLbl->setText("Passed");
    else
        ui->boincPathResultLbl->setText("Failed");
    //find cpid
    ui->findCPIDReaultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::FindCPID();
    if(result == 1)
        ui->findCPIDReaultLbl->setText(QString::fromStdString("Passed CPID: " + GetArgument("cpid", "")));
    else
        ui->findCPIDReaultLbl->setText("Failed (Is CPID in gridcoinresearch.conf?)");
    //cpid valid
    ui->verifyCPIDValidResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyIsCPIDValid();
    if(result == 1)
        ui->verifyCPIDValidResultLbl->setText("Passed");
    else
        ui->verifyCPIDValidResultLbl->setText("Failed (BOINC CPID and gridcoinresearch.conf CPID do not match)");
    //cpid has rac
    ui->verifyCPIDHasRACResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyCPIDHasRAC();
    if(result > 0)
        ui->verifyCPIDHasRACResultLbl->setText(QString::fromStdString("Passed RAC: " + std::to_string(result)));
    else
        ui->verifyCPIDHasRACResultLbl->setText("Failed");
    //cpid is in nn
    ui->verifyCPIDIsInNNResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork();
    if(result == 1)
        ui->verifyCPIDIsInNNResultLbl->setText("Passed");
    else if (result == -1)
        ui->verifyCPIDIsInNNResultLbl->setText("Beacon data empty, sync wallet");
    else
        ui->verifyCPIDIsInNNResultLbl->setText("Failed");
    //wallet synced
    ui->verifyWalletIsSyncedResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyWalletIsSynced();
    if(result == 1)
        ui->verifyWalletIsSyncedResultLbl->setText("Passed");
    else
        ui->verifyWalletIsSyncedResultLbl->setText("Failed");
    //clock
    ui->verifyClkResultLbl->setText("Testing...");
    this->repaint();
    DiagnosticsDialog::VerifyClock();
    //addnode
    ui->verifyAddnodeResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyAddNode();
    if(result == 1)
        ui->verifyAddnodeResultLbl->setText("Passed");
    else if(result == 2)
        ui->verifyAddnodeResultLbl->setText("Passed (Addnode does not exist but peers are synced)");
    else if(result == 3)
        ui->verifyAddnodeResultLbl->setText("Passed (Addnode exists but peers not synced)");
    else if(result == -1)
        ui->verifyAddnodeResultLbl->setText("Failed (Please add addnode=node.gridcoin.us in conf file)");
    else
        ui->verifyAddnodeResultLbl->setText("Failed");
    //tcp port
    ui->verifyTCPPortResultLbl->setText("Testing...");
    this->repaint();
    DiagnosticsDialog::VerifyTCPPort();
    //client version
    ui->checkClientVersionResultLbl->setText("Testing...");
    networkManager->get(QNetworkRequest(QUrl("https://api.github.com/repos/gridcoin/Gridcoin-Research/releases/latest")));

}

void DiagnosticsDialog::clkStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ConnectedState)
    {
        connect(udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));

        char NTPMessage[48] = {0x1b, 0, 0, 0 ,0, 0, 0, 0, 0};

        udpSocket->writeDatagram(NTPMessage, sizeof(NTPMessage), udpSocket->peerAddress(), udpSocket->peerPort());
    }
}

void DiagnosticsDialog::clkSocketError(QAbstractSocket::SocketError error)
{
    ui->verifyClkResultLbl->setText("Failed to make connection to NTP server");

    udpSocket->close();
}

void DiagnosticsDialog::clkFinished()
{
    if (udpSocket->waitForReadyRead())
    {
        while (udpSocket->hasPendingDatagrams())
        {
            QByteArray BufferSocket = udpSocket->readAll();

            if (BufferSocket.size() == 48)
            {
                int nNTPCount = 40;
                unsigned long DateTimeIn = uchar(BufferSocket.at(nNTPCount)) + (uchar(BufferSocket.at(nNTPCount + 1)) << 8) + (uchar(BufferSocket.at(nNTPCount + 2)) << 16) + (uchar(BufferSocket.at(nNTPCount + 3)) << 24);
                long tmit = ntohl((time_t)DateTimeIn);
                tmit -= 2208988800U;

                udpSocket->close();

                boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
                boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
                boost::posix_time::time_duration timeDiff = networkTime - localTime;

                if(timeDiff.minutes() < 3)
                    ui->verifyClkResultLbl->setText("Passed");
                else
                    ui->verifyClkResultLbl->setText("Failed (Sync local time with network)");

                this->repaint();
            }
        }
    }
}

void DiagnosticsDialog::TCPFinished() {
    tcpSocket->close();
    ui->verifyTCPPortResultLbl->setText("Passed");
    this->repaint();
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket) {
    ui->verifyTCPPortResultLbl->setText("Failed (Port 32749 may be blocked by your firewall)");
    this->repaint();
}

void DiagnosticsDialog::getGithubVersionFinished(QNetworkReply *reply) {
    if (reply->error())
    {
        // Incase ssl dlls are not present or corrupted; avoid crash
        ui->checkClientVersionResultLbl->setText("Failed (" + reply->errorString() + ")");
        return;
    }
    QByteArray data;
    data = reply->readAll();
    std::string newVersionString;

    QJsonDocument replyJson = QJsonDocument::fromJson(data);
    QJsonObject obj = replyJson.object();
    if(!obj.empty()) {
        QJsonValue newVersionJVal = obj.value("name");
        if(!newVersionJVal.isUndefined()) {
            newVersionString = newVersionJVal.toString().toStdString();
        }
    }

    std::vector<std::string> newVersionStringSplit;
    boost::algorithm::split(newVersionStringSplit, newVersionString, boost::is_any_of("-"));
    newVersionString = newVersionStringSplit.at(0);

    std::string currentVersionString = FormatFullVersion();
    std::vector<std::string> currentVersionStringSplit;
    boost::algorithm::split(currentVersionStringSplit, currentVersionString, boost::is_any_of("-"));
    currentVersionString = currentVersionStringSplit.at(0);
    boost::algorithm::split(currentVersionStringSplit, currentVersionString, boost::is_any_of("v"));
    currentVersionString = currentVersionStringSplit.at(1);

    std::vector<std::string> currentVersionList;
    std::vector<std::string> newVersionList;

    boost::algorithm::split(currentVersionList, currentVersionString, boost::is_any_of("."));
    boost::algorithm::split(newVersionList, newVersionString, boost::is_any_of("."));

    int iNew;
    int iCurrent;
    for(unsigned int i=0; i<newVersionList.size(); i++) {
        iNew = std::stoi(newVersionList.at(i), nullptr, 10);
        iCurrent = std::stoi(currentVersionList.at(i), nullptr, 10);
        if(iNew > iCurrent) {
            ui->checkClientVersionResultLbl->setText("Failed (An update is available, please update)");
            return;
        } else
            ui->checkClientVersionResultLbl->setText("Up to date");
    }
}
