#include "main.h"
#include "boinc.h"
#include "beacon.h"
#include "util.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"

#include <numeric>
#include <fstream>

namespace NN { std::string GetPrimaryCpid(); }

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

    return (NN::GetPrimaryCpid() == cpid) ? true : false;
}

bool DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork()
{
    std::string primary_cpid = NN::GetPrimaryCpid();

    if(!IsResearcher(primary_cpid))
        return false;

    for(const auto& entry : GetConsensusBeaconList().mBeaconMap)
    {
        if(boost::iequals(entry.first, primary_cpid))
            return true;
    }

    return false;
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
    ui->boincPathResultLabel->setText("Testing...");
    this->repaint();

    if (VerifyBoincPath())
        ui->boincPathResultLabel->setText("Passed");

    else
        ui->boincPathResultLabel->setText("Failed");

    //find cpid
#ifndef WIN32
    ui->findCPIDResultLabel->setText("N/A");
#else
    ui->findCPIDResultLabel->setText("Testing...");
    this->repaint();

    if (FindCPID())
        ui->findCPIDResultLabel->setText(QString::fromStdString("Passed CPID: " + GetArgument("PrimaryCPID", "")));

    else
        ui->findCPIDResultLabel->setText("Failed (Is PrimaryCPID in gridcoinresearch.conf?)");
#endif
    //cpid valid
    ui->verifyCPIDValidResultLabel->setText("Testing...");
    this->repaint();

    if (VerifyIsCPIDValid())
        ui->verifyCPIDValidResultLabel->setText("Passed");

    else
        ui->verifyCPIDValidResultLabel->setText("Failed (BOINC CPID does not match CPID)");

    //cpid has rac
    ui->verifyCPIDHasRACResultLabel->setText("Testing...");
    this->repaint();

    if (VerifyCPIDHasRAC())
        ui->verifyCPIDHasRACResultLabel->setText(QString::fromStdString("Passed"));

    else
        ui->verifyCPIDHasRACResultLabel->setText("Failed");

    //cpid is in nn
    ui->verifyCPIDIsInNNResultLabel->setText("Testing...");
    this->repaint();

    if (VerifyCPIDIsInNeuralNetwork())
        ui->verifyCPIDIsInNNResultLabel->setText("Passed");

    else
        ui->verifyCPIDIsInNNResultLabel->setText("Failed");

    //wallet synced
    ui->verifyWalletIsSyncedResultLabel->setText("Testing...");
    this->repaint();

    if (VerifyWalletIsSynced())
        ui->verifyWalletIsSyncedResultLabel->setText("Passed");

    else
        ui->verifyWalletIsSyncedResultLabel->setText("Failed");

    //clock
    ui->verifyClkResultLabel->setText("Testing...");
    this->repaint();

    VerifyClock();

    //seed nodes
    ui->verifySeedNodesResultLabel->setText("Testing...");
    this->repaint();

    result = VerifyCountSeedNodes();

    if (result >= 1 && result < 3)
        ui->verifySeedNodesResultLabel->setText("Warning: Count=" + QString::number(result) + " (Pass=3+)");

    else if(result >= 3)
        ui->verifySeedNodesResultLabel->setText("Passed: Count=" + QString::number(result));

    else
        ui->verifySeedNodesResultLabel->setText("Failed: Count=" + QString::number(result));

    //connection count
    ui->verifyConnectionsResultLabel->setText("Testing...");
    this->repaint();

    result = VerifyCountConnections();

    if (result <= 7 && result >= 1)
        ui->verifyConnectionsResultLabel->setText("Warning: Count=" + QString::number(result) + " (Pass=8+)");

    else if (result >=8)
        ui->verifyConnectionsResultLabel->setText("Passed: Count=" + QString::number(result));

    else
        ui->verifyConnectionsResultLabel->setText("Failed: Count=" + QString::number(result));

    //tcp port
    ui->verifyTCPPortResultLabel->setText("Testing...");
    this->repaint();

    VerifyTCPPort();

    //client version
    ui->checkClientVersionResultLabel->setText("Testing...");
    networkManager->get(QNetworkRequest(QUrl("https://api.github.com/repos/gridcoin-community/Gridcoin-Research/releases/latest")));

    return;
}

void DiagnosticsDialog::clkStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ConnectedState)
    {
        connect(udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));

        char NTPMessage[48] = {0x1b, 0, 0, 0 ,0, 0, 0, 0, 0};

        udpSocket->write(NTPMessage, sizeof(NTPMessage));
    }

    return;
}

void DiagnosticsDialog::clkSocketError(QAbstractSocket::SocketError error)
{
    ui->verifyClkResultLabel->setText("Failed to make connection to NTP server");

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
                    ui->verifyClkResultLabel->setText("Passed");

                else
                    ui->verifyClkResultLabel->setText("Failed (Sync local time with network)");

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
    ui->verifyTCPPortResultLabel->setText("Failed (Port 32749 may be blocked by your firewall)");
    this->repaint();

    return;
}

void DiagnosticsDialog::getGithubVersionFinished(QNetworkReply *reply)
{
    if (reply->error())
    {
        // Incase ssl dlls are not present or corrupted; avoid crash
        ui->checkClientVersionResultLabel->setText("Failed (" + reply->errorString() + ")");

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
                ui->checkClientVersionResultLabel->setText("Failed (An update is available, please update to " + QString::fromStdString(newVersionString) + ")");

                return;
            }

            else
                ui->checkClientVersionResultLabel->setText("Up to date");
        }
    }

    catch (std::exception& ex)
    {
        ui->checkClientVersionResultLabel->setText("Failed: std exception occurred -> " + QString::fromUtf8(ex.what()));
    }

    return;
}
