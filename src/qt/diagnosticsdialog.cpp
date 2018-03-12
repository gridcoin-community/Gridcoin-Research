#include "main.h"
#include "util.h"
#include "boinc.h"
#include "appcache.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"

#include <numeric>
#include <fstream>


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

bool DiagnosticsDialog::VerifyBoincPath()
{
    boost::filesystem::path boincPath = (boost::filesystem::path) GetBoincDataDir();

    if (boincPath.empty())
        boincPath = (boost::filesystem::path) GetArgument("boincdatadir", "");

    boincPath = boincPath / "client_state.xml";

    return boost::filesystem::exists(boincPath) ? true : false;
}

bool DiagnosticsDialog::FindCPID()
{
    std::string cpid = GetArgument("PrimaryCPID", "");

    return IsResearcher(cpid) ? true : false;
}

bool DiagnosticsDialog::VerifyIsCPIDValid()
{
    boost::filesystem::path clientStatePath = (boost::filesystem::path) GetBoincDataDir();

    if (!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";

    else
        clientStatePath = (boost::filesystem::path) GetArgument("boincdatadir", "") / "client_state.xml";

    if (clientStatePath.empty())
        return false;

    std::ifstream clientStateStream;
    std::string clientState;

    clientStateStream.open(clientStatePath.string());
    clientState.assign((std::istreambuf_iterator<char>(clientStateStream)), std::istreambuf_iterator<char>());
    clientStateStream.close();

    size_t pos = 0;
    std::string cpid;

    if ((pos = clientState.find("<external_cpid>")) != std::string::npos)
    {
        cpid = clientState.substr(pos + 15, clientState.length());
        pos = cpid.find("</external_cpid>");
        cpid.erase(pos, cpid.length());
    }

    return (msPrimaryCPID == cpid) ? true : false;
}

bool DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork()
{
    std::string beacons = GetListOf("beacon");
    boost::algorithm::to_lower(beacons);

    return IsResearcher(msPrimaryCPID) && beacons.find(msPrimaryCPID) != std::string::npos;
}

bool DiagnosticsDialog::VerifyWalletIsSynced()
{
    int64_t nwalletAge = PreviousBlockAge();

    return (nwalletAge < (60 * 60)) ? true : false;
}

bool DiagnosticsDialog::VerifyCPIDHasRAC()
{
    double racValue = 0;

    boost::filesystem::path clientStatePath = (boost::filesystem::path) GetBoincDataDir();

    if (!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";

    else
        clientStatePath = (boost::filesystem::path) GetArgument("boincdatadir", "") / "client_state.xml";

    if (clientStatePath.empty())
        return false;

    std::ifstream clientStateStream;
    std::string clientState;
    std::vector<std::string> racStrings;

    clientStateStream.open(clientStatePath.string());
    clientState.assign((std::istreambuf_iterator<char>(clientStateStream)), std::istreambuf_iterator<char>());
    clientStateStream.close();

    if (clientState.length() > 0)
    {
        size_t pos = 0;
        std::string delim = "</user_expavg_credit>";
        std::string line;

        while ((pos = clientState.find(delim)) != std::string::npos)
        {
            line = clientState.substr(0, pos);
            clientState.erase(0, pos + delim.length());
            pos = line.find("<user_expavg_credit>");
            racStrings.push_back(line.substr(pos + 20, line.length()));
        }

        for (auto const& racString : racStrings)
        {
            try
            {
                racValue += std::stod(racString);
            }

            catch (std::exception& ex)
            {
                continue;
            }
        }
    }

    return (racValue >= 1) ? true : false;
}

int DiagnosticsDialog::VerifyCountSeedNodes()
{
    LOCK(cs_vNodes);

    int count = 0;

    for (auto const& vnodes : vNodes)
        if (!vnodes->fInbound)
            count++;

    return count;
}

int DiagnosticsDialog::VerifyCountConnections()
{
    LOCK(cs_vNodes);

    return (int)vNodes.size();
}

void DiagnosticsDialog::VerifyClock()
{
    QHostInfo NTPHost = QHostInfo::fromName("pool.ntp.org");
    udpSocket = new QUdpSocket(this);

    connect(udpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(clkStateChanged(QAbstractSocket::SocketState)));
    connect(udpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(clkSocketError(QAbstractSocket::SocketError)));

    udpSocket->connectToHost(QHostAddress(NTPHost.addresses().first()), 123, QIODevice::ReadWrite);
}



void DiagnosticsDialog::VerifyTCPPort()
{
    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(TCPFinished()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(TCPFailed(QAbstractSocket::SocketError)));

    tcpSocket->connectToHost("portquiz.net", GetListenPort());
}

void DiagnosticsDialog::on_testBtn_clicked()
{
    int result = 0;

    //boin path
    ui->boincPathResultLbl->setText("Testing...");
    this->repaint();

    if (VerifyBoincPath())
        ui->boincPathResultLbl->setText("Passed");

    else
        ui->boincPathResultLbl->setText("Failed");

    //find cpid
#ifndef WIN32
    ui->findCPIDResultLbl->setText("N/A");
#else
    ui->findCPIDResultLbl->setText("Testing...");
    this->repaint();

    if (FindCPID())
        ui->findCPIDResultLbl->setText(QString::fromStdString("Passed CPID: " + GetArgument("PrimaryCPID", "")));

    else
        ui->findCPIDResultLbl->setText("Failed (Is PrimaryCPID in gridcoinresearch.conf?)");
#endif
    //cpid valid
    ui->verifyCPIDValidResultLbl->setText("Testing...");
    this->repaint();

    if (VerifyIsCPIDValid())
        ui->verifyCPIDValidResultLbl->setText("Passed");

    else
        ui->verifyCPIDValidResultLbl->setText("Failed (BOINC CPID does not match CPID)");

    //cpid has rac
    ui->verifyCPIDHasRACResultLbl->setText("Testing...");
    this->repaint();

    if (VerifyCPIDHasRAC())
        ui->verifyCPIDHasRACResultLbl->setText(QString::fromStdString("Passed"));

    else
        ui->verifyCPIDHasRACResultLbl->setText("Failed");

    //cpid is in nn
    ui->verifyCPIDIsInNNResultLbl->setText("Testing...");
    this->repaint();

    if (VerifyCPIDIsInNeuralNetwork())
        ui->verifyCPIDIsInNNResultLbl->setText("Passed");

    else
        ui->verifyCPIDIsInNNResultLbl->setText("Failed");

    //wallet synced
    ui->verifyWalletIsSyncedResultLbl->setText("Testing...");
    this->repaint();

    if (VerifyWalletIsSynced())
        ui->verifyWalletIsSyncedResultLbl->setText("Passed");

    else
        ui->verifyWalletIsSyncedResultLbl->setText("Failed");

    //clock
    ui->verifyClkResultLbl->setText("Testing...");
    this->repaint();

    VerifyClock();

    //seed nodes
    ui->verifySeedNodesResultLbl->setText("Testing...");
    this->repaint();

    result = VerifyCountSeedNodes();

    if (result >= 1 && result < 3)
        ui->verifySeedNodesResultLbl->setText("Warning: Count=" + QString::number(result) + " (Pass=3+)");

    else if(result >= 3)
        ui->verifySeedNodesResultLbl->setText("Passed: Count=" + QString::number(result));

    else
        ui->verifySeedNodesResultLbl->setText("Failed: Count=" + QString::number(result));

    //connection count
    ui->verifyConnectionsResultLbl->setText("Testing...");
    this->repaint();

    result = VerifyCountConnections();

    if (result <= 7 && result >= 1)
        ui->verifyConnectionsResultLbl->setText("Warning: Count=" + QString::number(result) + " (Pass=8+)");

    else if (result >=8)
        ui->verifyConnectionsResultLbl->setText("Passed: Count=" + QString::number(result));

    else
        ui->verifyConnectionsResultLbl->setText("Failed: Count=" + QString::number(result));

    //tcp port
    ui->verifyTCPPortResultLbl->setText("Testing...");
    this->repaint();

    VerifyTCPPort();

    //client version
    ui->checkClientVersionResultLbl->setText("Testing...");
    networkManager->get(QNetworkRequest(QUrl("https://api.github.com/repos/gridcoin/Gridcoin-Research/releases/latest")));

    return;
}

void DiagnosticsDialog::clkStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ConnectedState)
    {
        connect(udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));

        char NTPMessage[48] = {0x1b, 0, 0, 0 ,0, 0, 0, 0, 0};

        udpSocket->writeDatagram(NTPMessage, sizeof(NTPMessage), udpSocket->peerAddress(), udpSocket->peerPort());
    }

    return;
}

void DiagnosticsDialog::clkSocketError(QAbstractSocket::SocketError error)
{
    ui->verifyClkResultLbl->setText("Failed to make connection to NTP server");

    udpSocket->close();

    return;
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

                if (timeDiff.minutes() < 3)
                    ui->verifyClkResultLbl->setText("Passed");

                else
                    ui->verifyClkResultLbl->setText("Failed (Sync local time with network)");

                this->repaint();
            }
        }
    }

    return;
}

void DiagnosticsDialog::TCPFinished()
{
    tcpSocket->close();
    ui->verifyTCPPortResultLbl->setText("Passed");
    this->repaint();

    return;
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket)
{
    ui->verifyTCPPortResultLbl->setText("Failed (Port 32749 may be blocked by your firewall)");
    this->repaint();

    return;
}

void DiagnosticsDialog::getGithubVersionFinished(QNetworkReply *reply)
{
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

    if (!obj.empty())
    {
        QJsonValue newVersionJVal = obj.value("name");

        if (!newVersionJVal.isUndefined())
            newVersionString = newVersionJVal.toString().toStdString();
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

    try
    {
        for (unsigned int i=0; i<newVersionList.size(); i++) {
            iNew = std::stoi(newVersionList.at(i), nullptr, 10);
            iCurrent = std::stoi(currentVersionList.at(i), nullptr, 10);

            if (iNew > iCurrent) {
                ui->checkClientVersionResultLbl->setText("Failed (An update is available, please update to " + QString::fromStdString(newVersionString) + ")");

                return;
            }

            else
                ui->checkClientVersionResultLbl->setText("Up to date");
        }
    }

    catch (std::exception& ex)
    {
        ui->checkClientVersionResultLbl->setText("Failed: std exception occured -> " + QString::fromUtf8(ex.what()));
    }

    return;
}
