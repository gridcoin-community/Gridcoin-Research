#include "main.h"
#include "boinc.h"
#include "util.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"
#include "neuralnet/researcher.h"
#include "upgrade.h"

#include <numeric>
#include <fstream>

namespace NN { std::string GetPrimaryCpid(); }

DiagnosticsDialog::DiagnosticsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiagnosticsDialog)
{
    ui->setupUi(this);
}

DiagnosticsDialog::~DiagnosticsDialog()
{
    delete ui;
}

unsigned int DiagnosticsDialog::GetNumberOfTestsPending()
{
    LOCK(cs_diagnostictests);

    unsigned int pending_count = 0;

    for (const auto& entry : test_status_map)
    {
        if (entry.second == pending) pending_count++;
    }

    return pending_count;
}

DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetTestStatus(std::string test_name)
{
    LOCK(cs_diagnostictests);

    auto entry = test_status_map.find(test_name);

    if (entry != test_status_map.end())
    {
        return entry->second;
    }
    else
    {
        return unknown;
    }
}

unsigned int DiagnosticsDialog::UpdateTestStatus(std::string test_name, DiagnosticTestStatus test_status)
{
    LOCK(cs_diagnostictests);

    test_status_map[test_name] = test_status;

    return test_status_map.size();
}


void DiagnosticsDialog::ResetOverallDiagnosticResult(unsigned int& number_of_tests_in)
{
    LOCK(cs_diagnostictests);

    number_of_tests = number_of_tests_in;

    test_status_map.clear();

    diagnostic_result_status = pending;

    diagnostic_result = NA;
}

void DiagnosticsDialog::UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in)
{
    LOCK(cs_diagnostictests);

    // Set diagnostic_result_status to completed. This is under lock, so noone can snoop.
    diagnostic_result_status = completed;

    // If the total number of registered tests is less than the initialized number, then
    // the overall status is pending by default.
    if (test_status_map.size() < number_of_tests)
    {
        diagnostic_result_status = pending;
    }

    diagnostic_result = (DiagnosticResult) std::max<int>(diagnostic_result_in,  diagnostic_result);

    // If diagnostic_result_status is still set to completed, then at least all tests
    // are registered. Walk through the map of tests one by one and check the status.
    // The first one encountered that is pending means the overall status is pending.
    if (diagnostic_result_status == completed)
    {
        for (const auto& entry : test_status_map)
        {
            if (entry.second == pending)
            {
                diagnostic_result_status = pending;
                break;
            }
        }
    }
}

// Lock should be taken on cs_diagnostictests before calling this function.
DiagnosticsDialog::DiagnosticResult DiagnosticsDialog::GetOverallDiagnosticResult()
{
    return diagnostic_result;
}

// Lock should be taken on cs_diagnostictests before calling this function.
DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetOverallDiagnosticStatus()
{
    return diagnostic_result_status;
}


void DiagnosticsDialog::DisplayOverallDiagnosticResult()
{
    LOCK(cs_diagnostictests);

    if (GetOverallDiagnosticStatus() == pending)
    {
        ui->overallResultResultLabel->setText(tr("Testing..."));
        ui->overallResultResultLabel->setStyleSheet("");
    }
    else
    {
        // Overall status must be completed, so display overall result
        switch (GetOverallDiagnosticResult())
        {
        case NA:
            ui->overallResultResultLabel->clear();
            ui->overallResultResultLabel->setStyleSheet("");
            break;

        case passed:
            ui->overallResultResultLabel->setText(tr("Passed"));
            ui->overallResultResultLabel->setStyleSheet("color:white;background-color:green");
            break;

        case warning:
            ui->overallResultResultLabel->setText(tr("Warning"));
            ui->overallResultResultLabel->setStyleSheet("color:black;background-color:yellow");
            break;

        case failed:
            ui->overallResultResultLabel->setText(tr("Failed"));
            ui->overallResultResultLabel->setStyleSheet("color:white;background-color:red");
        }
    }

    this->repaint();
}

bool DiagnosticsDialog::VerifyBoincPath()
{
    boost::filesystem::path boincPath = (boost::filesystem::path) GetBoincDataDir();

    if (boincPath.empty())
        boincPath = (boost::filesystem::path) GetArgument("boincdatadir", "");

    boincPath = boincPath / "client_state.xml";

    return boost::filesystem::exists(boincPath) ? true : false;
}

bool DiagnosticsDialog::VerifyIsCPIDValid()
{
    boost::filesystem::path clientStatePath = GetBoincDataDir();

    if (!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";

    else
        clientStatePath = (boost::filesystem::path) GetArgument("boincdatadir", "") / "client_state.xml";

    if (clientStatePath.empty())
        return false;

    fsbridge::ifstream clientStateStream;
    std::string clientState;

    clientStateStream.open(clientStatePath);
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
    return NN::Researcher::Get()->Eligible();
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

    fsbridge::ifstream clientStateStream;
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

double DiagnosticsDialog::VerifyETTSReasonable()
{
    // We are going to compute the ETTS with ignore_staking_status set to true
    // and also use a 960 block diff as the input, which smooths out short
    // term fluctuations. The standard 1-1/e confidence (mean) is used.

    double diff = GetAverageDifficulty(960);

    double result = GetEstimatedTimetoStake(true, diff);

    return result;
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

void DiagnosticsDialog::on_testButton_clicked()
{
    // Check to see if there is already a test run in progress, and if so return.
    // We do not want overlapping test runs.

    if (GetNumberOfTestsPending())
    {
        LogPrintf("INFO: DiagnosticsDialog::on_testButton_clicked: Tests still in progress from a prior run: %u", GetNumberOfTestsPending());
        this->repaint();
        return;
    }

    // This needs to be updated if there are more tests added.
    unsigned int number_of_tests = 11;

    ResetOverallDiagnosticResult(number_of_tests);
    DisplayOverallDiagnosticResult();

    // Tests that are N/A if in investor mode.
    if (NN::Researcher::ConfiguredForInvestorMode())
    {
        // N/A tests for investor mode
        ui->boincPathResultLabel->setText(tr("N/A"));
        ui->boincPathResultLabel->setStyleSheet("color:black;background-color:grey");
        UpdateTestStatus("boincPath", completed);

        ui->verifyCPIDValidResultLabel->setText(tr("N/A"));
        ui->verifyCPIDValidResultLabel->setStyleSheet("color:black;background-color:grey");
        UpdateTestStatus("verifyCPIDValid", completed);

        ui->verifyCPIDHasRACResultLabel->setText(tr("N/A"));
        ui->verifyCPIDHasRACResultLabel->setStyleSheet("color:black;background-color:grey");
        UpdateTestStatus("verifyCPIDHasRAC", completed);

        ui->verifyCPIDIsInNNResultLabel->setText(tr("N/A"));
        ui->verifyCPIDIsInNNResultLabel->setStyleSheet("color:black;background-color:grey");
        UpdateTestStatus("verifyCPIDIsInNN", completed);

        ui->checkETTSResultLabel->setText(tr("N/A"));
        ui->checkETTSResultLabel->setStyleSheet("color:black;background-color:grey");
        UpdateTestStatus("checkETTS", completed);

    }
    else
    {
        //BOINC path
        ui->boincPathResultLabel->setStyleSheet("");
        ui->boincPathResultLabel->setText(tr("Testing..."));
        UpdateTestStatus("boincPath", pending);
        this->repaint();

        if (VerifyBoincPath())
        {
            ui->boincPathResultLabel->setText(tr("Passed"));
            ui->boincPathResultLabel->setStyleSheet("color:white;background-color:green");
            UpdateTestStatus("boincPath", completed);
            UpdateOverallDiagnosticResult(passed);
        }
        else
        {
            ui->boincPathResultLabel->setText(tr("Failed"));
            ui->boincPathResultLabel->setStyleSheet("color:white;background-color:red");
            UpdateTestStatus("boincPath", completed);
            UpdateOverallDiagnosticResult(failed);
        }

        //CPID valid
        ui->verifyCPIDValidResultLabel->setStyleSheet("");
        ui->verifyCPIDValidResultLabel->setText(tr("Testing..."));
        UpdateTestStatus("verifyCPIDValid", pending);
        this->repaint();

        if (VerifyIsCPIDValid())
        {
            ui->verifyCPIDValidResultLabel->setText(tr("Passed"));
            ui->verifyCPIDValidResultLabel->setStyleSheet("color:white;background-color:green");
            UpdateTestStatus("verifyCPIDValid", completed);
            UpdateOverallDiagnosticResult(passed);
        }
        else
        {
            ui->verifyCPIDValidResultLabel->setText(tr("Failed: BOINC CPID does not match CPID"));
            ui->verifyCPIDValidResultLabel->setStyleSheet("color:white;background-color:red");
            UpdateTestStatus("verifyCPIDValid", completed);
            UpdateOverallDiagnosticResult(failed);
        }

        //CPID has rac
        ui->verifyCPIDHasRACResultLabel->setStyleSheet("");
        ui->verifyCPIDHasRACResultLabel->setText(tr("Testing..."));
        UpdateTestStatus("verifyCPIDHasRAC", pending);
        this->repaint();

        if (VerifyCPIDHasRAC())
        {
            ui->verifyCPIDHasRACResultLabel->setText(tr("Passed"));
            ui->verifyCPIDHasRACResultLabel->setStyleSheet("color:white;background-color:green");
            UpdateTestStatus("verifyCPIDHasRAC", completed);
            UpdateOverallDiagnosticResult(passed);

        }
        else
        {
            ui->verifyCPIDHasRACResultLabel->setText(tr("Failed"));
            ui->verifyCPIDHasRACResultLabel->setStyleSheet("color:white;background-color:red");
            UpdateTestStatus("verifyCPIDHasRAC", completed);
            UpdateOverallDiagnosticResult(failed);
        }

        //cpid is in nn
        ui->verifyCPIDIsInNNResultLabel->setStyleSheet("");
        ui->verifyCPIDIsInNNResultLabel->setText(tr("Testing..."));
        UpdateTestStatus("verifyCPIDIsInNN", pending);
        this->repaint();

        if (VerifyCPIDIsInNeuralNetwork())
        {
            ui->verifyCPIDIsInNNResultLabel->setText(tr("Passed"));
            ui->verifyCPIDIsInNNResultLabel->setStyleSheet("color:white;background-color:green");
            UpdateTestStatus("verifyCPIDIsInNN", completed);
            UpdateOverallDiagnosticResult(passed);
        }
        else
        {
            ui->verifyCPIDIsInNNResultLabel->setText(tr("Failed"));
            ui->verifyCPIDIsInNNResultLabel->setStyleSheet("color:white;background-color:red");
            UpdateTestStatus("verifyCPIDIsInNN", completed);
            UpdateOverallDiagnosticResult(failed);
        }

        // verify reasonable ETTS
        // This is only checked if wallet is a researcher wallet because the purpose is to
        // alert the owner that his stake time is too long and therefore there is a chance
        // of research rewards loss between stakes due to the 180 day limit.
        ui->checkETTSResultLabel->setStyleSheet("");
        ui->checkETTSResultLabel->setText(tr("Testing..."));
        UpdateTestStatus("checkETTS", pending);

        double ETTS = VerifyETTSReasonable() / (24.0 * 60.0 * 60.0);

        std::string rounded_ETTS;

        //round appropriately for display.
        if (ETTS >= 100)
        {
            rounded_ETTS = RoundToString(ETTS, 0);
        }
        else if (ETTS >= 10)
        {
            rounded_ETTS = RoundToString(ETTS, 1);
        }
        else
        {
            rounded_ETTS = RoundToString(ETTS, 2);
        }

        // ETTS of zero actually means no coins, i.e. infinite.
        if (ETTS == 0.0)
        {
            ui->checkETTSResultLabel->setText(tr("Failed: ETTS is infinite. No coins to stake."));
            ui->checkETTSResultLabel->setStyleSheet("color:white;background-color:red");
            UpdateTestStatus("checkETTS", completed);
            UpdateOverallDiagnosticResult(failed);
        }
        else if (ETTS > 90.0)
        {
            ui->checkETTSResultLabel->setText(tr("Failed: ETTS = %1 > 90 days")
                                              .arg(QString(rounded_ETTS.c_str())));
            ui->checkETTSResultLabel->setStyleSheet("color:white;background-color:red");
            UpdateTestStatus("checkETTS", completed);
            UpdateOverallDiagnosticResult(failed);
        }
        else if (ETTS > 45.0 && ETTS <= 90.0)
        {
            ui->checkETTSResultLabel->setText(tr("Warning: 45 days < ETTS = %1 <= 90 days")
                                              .arg(QString(rounded_ETTS.c_str())));
            ui->checkETTSResultLabel->setStyleSheet("color:black;background-color:yellow");
            UpdateTestStatus("checkETTS", completed);
            UpdateOverallDiagnosticResult(warning);
        }
        else
        {
            ui->checkETTSResultLabel->setText(tr("Passed: ETTS = %1 <= 45 days")
                                              .arg(QString(rounded_ETTS.c_str())));
            ui->checkETTSResultLabel->setStyleSheet("color:white;background-color:green");
            UpdateTestStatus("checkETTS", completed);
            UpdateOverallDiagnosticResult(passed);
        }
    }

    // Tests that are common to both investor and researcher mode.
    // wallet synced
    ui->verifyWalletIsSyncedResultLabel->setStyleSheet("");
    ui->verifyWalletIsSyncedResultLabel->setText(tr("Testing..."));
    UpdateTestStatus("verifyWalletIsSynced", pending);
    this->repaint();

    if (VerifyWalletIsSynced())
    {
        ui->verifyWalletIsSyncedResultLabel->setText(tr("Passed"));
        ui->verifyWalletIsSyncedResultLabel->setStyleSheet("color:white;background-color:green");
        UpdateTestStatus("verifyWalletIsSynced", completed);
        UpdateOverallDiagnosticResult(passed);
    }

    else
    {
        ui->verifyWalletIsSyncedResultLabel->setText(tr("Failed"));
        ui->verifyWalletIsSyncedResultLabel->setStyleSheet("color:white;background-color:red");
        UpdateTestStatus("verifyWalletIsSynced", completed);
        UpdateOverallDiagnosticResult(failed);
    }

    // clock
    ui->verifyClockResultLabel->setStyleSheet("");
    ui->verifyClockResultLabel->setText(tr("Testing..."));
    UpdateTestStatus("verifyClockResult", pending);
    this->repaint();

    VerifyClock();

    // seed nodes
    ui->verifySeedNodesResultLabel->setStyleSheet("");
    ui->verifySeedNodesResultLabel->setText(tr("Testing..."));
    UpdateTestStatus("verifySeedNodes", pending);
    this->repaint();

    unsigned int seed_node_connections = VerifyCountSeedNodes();

    if (seed_node_connections >= 1 && seed_node_connections < 3)
    {
        ui->verifySeedNodesResultLabel->setText(tr("Warning: Count = %1 (Pass = 3+)").arg(QString::number(seed_node_connections)));
        ui->verifySeedNodesResultLabel->setStyleSheet("color:black;background-color:yellow");
        UpdateTestStatus("verifySeedNodes", completed);
        UpdateOverallDiagnosticResult(warning);
    }
    else if(seed_node_connections >= 3)
    {
        ui->verifySeedNodesResultLabel->setText(tr("Passed: Count = %1").arg(QString::number(seed_node_connections)));
        ui->verifySeedNodesResultLabel->setStyleSheet("color:white;background-color:green");
        UpdateTestStatus("verifySeedNodes", completed);
        UpdateOverallDiagnosticResult(passed);
    }
    else
    {
        ui->verifySeedNodesResultLabel->setText(tr("Failed: Count = %1").arg(QString::number(seed_node_connections)));
        ui->verifySeedNodesResultLabel->setStyleSheet("color:white;background-color:red");
        UpdateTestStatus("verifySeedNodes", completed);
        UpdateOverallDiagnosticResult(failed);
    }

    // connection count
    ui->verifyConnectionsResultLabel->setStyleSheet("");
    ui->verifyConnectionsResultLabel->setText(tr("Testing..."));
    UpdateTestStatus("verifyConnections", pending);
    this->repaint();

    unsigned int connections = VerifyCountConnections();

    if (connections <= 7 && connections >= 1)
    {
        ui->verifyConnectionsResultLabel->setText(tr("Warning: Count = %1 (Pass = 8+)").arg(QString::number(connections)));
        ui->verifyConnectionsResultLabel->setStyleSheet("color:black;background-color:yellow");
        UpdateTestStatus("verifyConnections", completed);
        UpdateOverallDiagnosticResult(warning);
    }
    else if (connections >= 8)
    {
        ui->verifyConnectionsResultLabel->setText(tr("Passed: Count = %1").arg(QString::number(connections)));
        ui->verifyConnectionsResultLabel->setStyleSheet("color:white;background-color:green");
        UpdateTestStatus("verifyConnections", completed);
        UpdateOverallDiagnosticResult(passed);
    }
    else
    {
        ui->verifyConnectionsResultLabel->setText(tr("Failed: Count = %1").arg(QString::number(connections)));
        ui->verifyConnectionsResultLabel->setStyleSheet("color:white;background-color:red");
        UpdateTestStatus("verifyConnections", completed);
        UpdateOverallDiagnosticResult(failed);
    }

    // tcp port
    ui->verifyTCPPortResultLabel->setStyleSheet("");
    ui->verifyTCPPortResultLabel->setText(tr("Testing..."));
    UpdateTestStatus("verifyTCPPort", pending);
    this->repaint();

    VerifyTCPPort();

    // client version
    ui->checkClientVersionResultLabel->setStyleSheet("");
    ui->checkClientVersionResultLabel->setText(tr("Testing..."));
    UpdateTestStatus("checkClientVersion", pending);

    std::string client_message;

    if (g_UpdateChecker->CheckForLatestUpdate(false, client_message))
    {
        ui->checkClientVersionResultLabel->setText(tr("Warning: New Client version available:\n %1").arg(QString(client_message.c_str())));
        ui->checkClientVersionResultLabel->setStyleSheet("color:black;background-color:yellow;height:2em");
        UpdateTestStatus("checkClientVersion", completed);
        UpdateOverallDiagnosticResult(warning);
    }
    else
    {
        ui->checkClientVersionResultLabel->setText(tr("Passed"));
        ui->checkClientVersionResultLabel->setStyleSheet("color:white;background-color:green");
        UpdateTestStatus("checkClientVersion", completed);
        UpdateOverallDiagnosticResult(passed);
    }

    DisplayOverallDiagnosticResult();
}

void DiagnosticsDialog::VerifyClock()
{
    QTimer *timerVerifyClock = new QTimer();

    // Set up a timeout clock of 10 seconds as a fail-safe.
    connect(timerVerifyClock, SIGNAL(timeout()), this, SLOT(clkFinished()));
    timerVerifyClock->start(10 * 1000);

    QHostInfo NTPHost = QHostInfo::fromName("pool.ntp.org");
    udpSocket = new QUdpSocket(this);

    connect(udpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(clkStateChanged(QAbstractSocket::SocketState)));
    connect(udpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(clkSocketError()));

    udpSocket->connectToHost(QHostAddress(NTPHost.addresses().first()), 123, QIODevice::ReadWrite);
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

void DiagnosticsDialog::clkSocketError()
{
    udpSocket->close();

    // Call clkFinished to handle.
    clkFinished();

    return;
}

void DiagnosticsDialog::clkFinished()
{
    if (udpSocket->waitForReadyRead(10 * 1000))
    {
        int64_t start_time = GetAdjustedTime();

        // Only allow this loop to run for 5 seconds maximum.
        while (udpSocket->hasPendingDatagrams() && GetAdjustedTime() - start_time <= 5)
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
                {
                    ui->verifyClockResultLabel->setText(tr("Passed"));
                    ui->verifyClockResultLabel->setStyleSheet("color:white;background-color:green");
                    UpdateTestStatus("verifyClockResult", completed);

                    // We need this here because there is no return call (callback) to the on_testButton_clicked function
                    UpdateOverallDiagnosticResult(passed);
                }
                else
                {
                    ui->verifyClockResultLabel->setText(tr("Failed: Sync local time with network"));
                    ui->verifyClockResultLabel->setStyleSheet("color:white;background-color:red");
                    UpdateTestStatus("verifyClockResult", completed);

                    // We need this here because there is no return call (callback) to the on_testButton_clicked function
                    UpdateOverallDiagnosticResult(failed);
                }

                // We need this here because there is no return call (callback) to the on_testButton_clicked function
                DisplayOverallDiagnosticResult();

                return;
            }
        }
    }
    else // The other state here is a socket or other indeterminate error such as a timeout (coming from clkSocketError).
    {
        // This is needed to "cancel" the timout timer. Essentially if the test was marked completed via the normal exits
        // above, then when the timer calls clkFinished again, it will hit this conditional and be a no-op.
        if (GetTestStatus("verifyClockResult") != completed)
        {
            ui->verifyClockResultLabel->setText(tr("Warning: Cannot connect to NTP server"));
            ui->verifyClockResultLabel->setStyleSheet("color:black;background-color:yellow");
            UpdateTestStatus("verifyClockResult", completed);

            // We need this here because there is no return call (callback) to the on_testButton_clicked function
            UpdateOverallDiagnosticResult(warning);

            DisplayOverallDiagnosticResult();
        }

        return;
    }
}

void DiagnosticsDialog::VerifyTCPPort()
{
    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(TCPFinished()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(TCPFailed(QAbstractSocket::SocketError)));

    tcpSocket->connectToHost("portquiz.net", GetListenPort());
}

void DiagnosticsDialog::TCPFinished()
{
    tcpSocket->close();
    ui->verifyTCPPortResultLabel->setText(tr("Passed"));
    ui->verifyTCPPortResultLabel->setStyleSheet("color:white;background-color:green");

    // We need this here because there is no return call (callback) to the on_testButton_clicked function
    UpdateTestStatus("verifyTCPPort", completed);
    UpdateOverallDiagnosticResult(passed);

    DisplayOverallDiagnosticResult();

    return;
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket)
{
    ui->verifyTCPPortResultLabel->setText(tr("Warning: Port 32749 may be blocked by your firewall"));
    ui->verifyTCPPortResultLabel->setStyleSheet("color:black;background-color:yellow");

    // We need this here because there is no return call (callback) to the on_testButton_clicked function
    UpdateTestStatus("verifyTCPPort", completed);
    UpdateOverallDiagnosticResult(failed);

    DisplayOverallDiagnosticResult();

    return;
}
