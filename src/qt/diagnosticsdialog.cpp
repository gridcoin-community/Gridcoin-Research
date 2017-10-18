#include <fstream>
#include "main.h"
#include "util.h"
#include "boinc.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"

#include <numeric>

std::string GetListOf(std::string datatype);
double PreviousBlockAge();

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

int DiagnosticsDialog::VarifyBoincPath() {
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

int DiagnosticsDialog::VarifyIsCPIDValid() {
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

int DiagnosticsDialog::VarifyWalletIsSynced() {
    std::string walletAgeString = ToString(PreviousBlockAge());
    long int walletAge = std::stoi(walletAgeString,nullptr,10);

    if(walletAgeString.length() == 0)
        return -1;
    if(walletAge < (60 * 60))
        return 1;
    else
        return 0;

}

int DiagnosticsDialog::VarifyCPIDHasRAC() {
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

int DiagnosticsDialog::VarifyAddNode() {
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

void DiagnosticsDialog::VarifyClock() {
    QHostInfo info = QHostInfo::fromName("pool.ntp.org");
    udpSocket = new QUdpSocket(this);

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));

    udpSocket->bind(QHostAddress::LocalHost, 123);

    QByteArray sendBuffer = QByteArray(47, 0);
    sendBuffer.insert(0, 35);

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));
    udpSocket->writeDatagram(sendBuffer, info.addresses().first(), 123);
}



void DiagnosticsDialog::VarifyTCPPort() {
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
    result = DiagnosticsDialog::VarifyBoincPath();
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
    ui->varifyCPIDValidResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyIsCPIDValid();
    if(result == 1)
        ui->varifyCPIDValidResultLbl->setText("Passed");
    else
        ui->varifyCPIDValidResultLbl->setText("Failed (BOINC CPID and gridcoinresearch.conf CPID do not match)");
    //cpid has rac
    ui->varifyCPIDHasRACResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyCPIDHasRAC();
    if(result > 0)
        ui->varifyCPIDHasRACResultLbl->setText(QString::fromStdString("Passed RAC: " + std::to_string(result)));
    else
        ui->varifyCPIDHasRACResultLbl->setText("Failed");
    //cpid is in nn
    ui->varifyCPIDIsInNNResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork();
    if(result == 1)
        ui->varifyCPIDIsInNNResultLbl->setText("Passed");
    else if (result == -1)
        ui->varifyCPIDIsInNNResultLbl->setText("Beacon data empty, sync wallet");
    else
        ui->varifyCPIDIsInNNResultLbl->setText("Failed");
    //wallet synced
    ui->varifyWalletIsSyncedResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyWalletIsSynced();
    if(result == 1)
        ui->varifyWalletIsSyncedResultLbl->setText("Passed");
    else if (result == -1)
        ui->varifyWalletIsSyncedResultLbl->setText("Sync data empty");
    else
        ui->varifyWalletIsSyncedResultLbl->setText("Failed");
    //clock
    ui->varifyClkResultLbl->setText("Testing...");
    this->repaint();
    DiagnosticsDialog::VarifyClock();
    //addnode
    ui->varifyAddnodeResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyAddNode();
    if(result == 1)
        ui->varifyAddnodeResultLbl->setText("Passed");
    else if(result == 2)
        ui->varifyAddnodeResultLbl->setText("Passed (Addnode does not exist but peers are synced)");
    else if(result == 3)
        ui->varifyAddnodeResultLbl->setText("Passed (Addnode exists but peers not synced)");
    else if(result == -1)
        ui->varifyAddnodeResultLbl->setText("Failed (Please add addnode=node.gridcoin.us in conf file)");
    else
        ui->varifyAddnodeResultLbl->setText("Failed");
    //tcp port
    ui->varifyTCPPortResultLbl->setText("Testing...");
    this->repaint();
    DiagnosticsDialog::VarifyTCPPort();
    //client version
    ui->checkClientVersionResultLbl->setText("Testing...");
    networkManager->get(QNetworkRequest(QUrl("https://api.github.com/repos/gridcoin/Gridcoin-Research/releases/latest")));

}

void DiagnosticsDialog::clkFinished() {
    time_t ntpTime(0);

    while(udpSocket->pendingDatagramSize() != -1) {
        char recvBuffer[udpSocket->pendingDatagramSize()];
        udpSocket->readDatagram(&recvBuffer[0], 48, nullptr, nullptr);
        if(sizeof(recvBuffer) == 48){
            int count= 40;
            unsigned long DateTimeIn= uchar(recvBuffer[count])+ (uchar(recvBuffer[count+ 1]) << 8)+ (uchar(recvBuffer[count+ 2]) << 16)+ (uchar(recvBuffer[count+ 3]) << 24);
            ntpTime = ntohl((time_t)DateTimeIn);
            ntpTime -= 2208988800U;
        }
    }

    udpSocket->close();

    boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(ntpTime);

    boost::posix_time::time_duration timeDiff = networkTime - localTime;

    if(timeDiff.minutes() < 3)
        ui->varifyClkResultLbl->setText("Passed");
    else
        ui->varifyClkResultLbl->setText("Failed (Sync local time with network)");

    this->repaint();
}

void DiagnosticsDialog::TCPFinished() {
    tcpSocket->close();
    ui->varifyTCPPortResultLbl->setText("Passed");
    this->repaint();
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket) {
    ui->varifyTCPPortResultLbl->setText("Failed (Port 32749 may be blocked by your firewall)");
    this->repaint();
}

void DiagnosticsDialog::getGithubVersionFinished(QNetworkReply *reply) {
    // Qt didn't get JSON support until 5.x. Ignore version checks when using
    // Qt4 an drop this condition once we drop Qt4 support.
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    ui->checkClientVersionResultLbl->setText("N/A");
#else
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
#endif
}
