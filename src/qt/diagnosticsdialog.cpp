// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "main.h"
#include "util.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"
#include "gridcoin/boinc.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/upgrade.h"
#include "qt/researcher/researchermodel.h"

#include <numeric>

DiagnosticsDialog::DiagnosticsDialog(QWidget *parent, ResearcherModel* researcher_model) :
    QDialog(parent),
    ui(new Ui::DiagnosticsDialog),
    m_researcher_model(researcher_model)
{
    ui->setupUi(this);
}

DiagnosticsDialog::~DiagnosticsDialog()
{
    delete ui;
}

void DiagnosticsDialog::SetResearcherModel(ResearcherModel *researcherModel)
{
    m_researcher_model = researcherModel;
}

void DiagnosticsDialog::SetResultLabel(QLabel *label, DiagnosticTestStatus test_status,
                                       DiagnosticResult test_result, QString override_text)
{
    switch (test_status)
    {
    case DiagnosticTestStatus::unknown:
        label->setText(tr(""));
        label->setStyleSheet("");
        break;
    case DiagnosticTestStatus::pending:
        label->setText(tr("Testing..."));
        label->setStyleSheet("");
        break;
    case DiagnosticTestStatus::completed:
        switch (test_result)
        {
        case DiagnosticResult::NA:
            label->setText(tr("N/A"));
            label->setStyleSheet("color:black;background-color:grey");
            break;
        case DiagnosticResult::passed:
            label->setText(tr("Passed"));
            label->setStyleSheet("color:white;background-color:green");
            break;
        case DiagnosticResult::warning:
            label->setText(tr("Warning"));
            label->setStyleSheet("color:black;background-color:yellow");
            break;
        case DiagnosticResult::failed:
            label->setText(tr("Failed"));
            label->setStyleSheet("color:white;background-color:red");
        }
    }

    if (override_text.size()) label->setText(override_text);
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

unsigned int DiagnosticsDialog::UpdateTestStatus(std::string test_name, QLabel *label,
                                                 DiagnosticTestStatus test_status, DiagnosticResult test_result,
                                                 QString override_text)
{
    LOCK(cs_diagnostictests);

    test_status_map[test_name] = test_status;

    SetResultLabel(label, test_status, test_result, override_text);

    UpdateOverallDiagnosticResult(test_result);

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

    // Set diagnostic_result_status to completed. This is under lock, so no one can snoop.
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

    SetResultLabel(ui->overallResultResultLabel, GetOverallDiagnosticStatus(), GetOverallDiagnosticResult());

    this->repaint();
}

bool DiagnosticsDialog::VerifyBoincPath()
{
    fs::path boincPath = (fs::path) GRC::GetBoincDataDir();

    if (boincPath.empty())
        boincPath = (fs::path) GetArgument("boincdatadir", "");

    boincPath = boincPath / "client_state.xml";

    return fs::exists(boincPath);
}

bool DiagnosticsDialog::VerifyIsCPIDValid()
{
    return m_researcher_model->hasEligibleProjects();
}

bool DiagnosticsDialog::VerifyCPIDIsEligible()
{
    return m_researcher_model->hasActiveBeacon();
}

bool DiagnosticsDialog::VerifyWalletIsSynced()
{
    return !OutOfSyncByAge();
}

bool DiagnosticsDialog::VerifyCPIDHasRAC()
{
    return m_researcher_model->hasRAC();
}

double DiagnosticsDialog::VerifyETTSReasonable()
{
    // We are going to compute the ETTS with ignore_staking_status set to true
    // and also use a 960 block diff as the input, which smooths out short
    // term fluctuations. The standard 1-1/e confidence (mean) is used.

    double diff = GRC::GetAverageDifficulty(960);

    double result = GRC::GetEstimatedTimetoStake(true, diff);

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

    // Tests that are N/A if in investor or pool mode.
    if (m_researcher_model->configuredForInvestorMode() || m_researcher_model->detectedPoolMode())
    {
        // N/A tests for investor/pool mode
        UpdateTestStatus("boincPath", ui->boincPathResultLabel, completed, NA);
        UpdateTestStatus("verifyCPIDValid", ui->verifyCPIDValidResultLabel, completed, NA);
        UpdateTestStatus("verifyCPIDHasRAC", ui->verifyCPIDHasRACResultLabel, completed, NA);
        UpdateTestStatus("verifyCPIDIsActive", ui->verifyCPIDIsActiveResultLabel, completed, NA);
        UpdateTestStatus("checkETTS", ui->checkETTSResultLabel, completed, NA);
    }
    else
    {
        //BOINC path
        UpdateTestStatus("boincPath", ui->boincPathResultLabel, pending, NA);
        this->repaint();

        if (VerifyBoincPath())
        {
            UpdateTestStatus("boincPath", ui->boincPathResultLabel, completed, passed);
        }
        else
        {
            UpdateTestStatus("boincPath", ui->boincPathResultLabel, completed, failed);
        }

        //CPID valid
        UpdateTestStatus("verifyCPIDValid", ui->verifyCPIDValidResultLabel, pending, NA);
        this->repaint();

        if (VerifyIsCPIDValid())
        {
            UpdateTestStatus("verifyCPIDValid", ui->verifyCPIDValidResultLabel, completed, passed);
        }
        else
        {
            UpdateTestStatus("verifyCPIDValid", ui->verifyCPIDValidResultLabel, completed, failed);
        }

        //CPID has rac
        UpdateTestStatus("verifyCPIDHasRAC", ui->verifyCPIDHasRACResultLabel, pending, NA);
        this->repaint();

        if (VerifyCPIDHasRAC())
        {
            UpdateTestStatus("verifyCPIDHasRAC", ui->verifyCPIDHasRACResultLabel, completed, passed);
        }
        else
        {
            UpdateTestStatus("verifyCPIDHasRAC", ui->verifyCPIDHasRACResultLabel, completed, failed);
        }

        //cpid is active
        UpdateTestStatus("verifyCPIDIsActive", ui->verifyCPIDIsActiveResultLabel, pending, NA);
        this->repaint();

        if (VerifyCPIDIsEligible())
        {
            UpdateTestStatus("verifyCPIDIsActive", ui->verifyCPIDIsActiveResultLabel, completed, passed);
        }
        else
        {
            UpdateTestStatus("verifyCPIDIsActive", ui->verifyCPIDIsActiveResultLabel, completed, failed);
        }

        // verify reasonable ETTS
        // This is only checked if wallet is a researcher wallet because the purpose is to
        // alert the owner that his stake time is too long and therefore there is a chance
        // of research rewards loss between stakes due to the 180 day limit.
        UpdateTestStatus("checkETTS", ui->checkETTSResultLabel, pending, NA);

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
            UpdateTestStatus("checkETTS", ui->checkETTSResultLabel, completed, failed,
                             tr("Failed: ETTS is infinite. No coins to stake."));
        }
        else if (ETTS > 90.0)
        {
            UpdateTestStatus("checkETTS", ui->checkETTSResultLabel, completed, failed,
                             tr("Failed: ETTS is infinite. No coins to stake."));
        }
        else if (ETTS > 45.0 && ETTS <= 90.0)
        {
            UpdateTestStatus("checkETTS", ui->checkETTSResultLabel, completed, warning,
                             tr("Warning: 45 days < ETTS = %1 <= 90 days").arg(QString(rounded_ETTS.c_str())));
        }
        else
        {
            UpdateTestStatus("checkETTS", ui->checkETTSResultLabel, completed, passed,
                             tr("Passed: ETTS = %1 <= 45 days").arg(QString(rounded_ETTS.c_str())));
        }
    }

    // Tests that are common to both investor and researcher mode.
    // wallet synced
    UpdateTestStatus("verifyWalletIsSynced", ui->verifyWalletIsSyncedResultLabel, pending, NA);
    this->repaint();

    if (VerifyWalletIsSynced())
    {
        UpdateTestStatus("verifyWalletIsSynced", ui->verifyWalletIsSyncedResultLabel, completed, passed);
    }

    else
    {
        UpdateTestStatus("verifyWalletIsSynced", ui->verifyWalletIsSyncedResultLabel, completed, failed);
    }

    // clock
    UpdateTestStatus("verifyClockResult", ui->verifyClockResultLabel, pending, NA);
    this->repaint();

    VerifyClock();

    // seed nodes
    UpdateTestStatus("verifySeedNodes", ui->verifySeedNodesResultLabel, pending, NA);
    this->repaint();

    unsigned int seed_node_connections = VerifyCountSeedNodes();

    if (seed_node_connections >= 1 && seed_node_connections < 3)
    {
        UpdateTestStatus("verifySeedNodes", ui->verifySeedNodesResultLabel, completed, warning,
                         tr("Warning: Count = %1 (Pass = 3+)").arg(QString::number(seed_node_connections)));
    }
    else if(seed_node_connections >= 3)
    {
        UpdateTestStatus("verifySeedNodes", ui->verifySeedNodesResultLabel, completed, passed,
                         tr("Passed: Count = %1").arg(QString::number(seed_node_connections)));
    }
    else
    {
        UpdateTestStatus("verifySeedNodes", ui->verifySeedNodesResultLabel, completed, failed,
                         tr("Failed: Count = %1").arg(QString::number(seed_node_connections)));
    }

    // connection count
    UpdateTestStatus("verifyConnections", ui->verifyConnectionsResultLabel, pending, NA);
    this->repaint();

    unsigned int connections = VerifyCountConnections();

    if (connections <= 7 && connections >= 1)
    {
        UpdateTestStatus("verifyConnections", ui->verifyConnectionsResultLabel, completed, warning,
                         tr("Warning: Count = %1 (Pass = 8+)").arg(QString::number(connections)));
    }
    else if (connections >= 8)
    {
        UpdateTestStatus("verifyConnections", ui->verifyConnectionsResultLabel, completed, passed,
                         tr("Passed: Count = %1").arg(QString::number(connections)));
    }
    else
    {
        UpdateTestStatus("verifyConnections", ui->verifyConnectionsResultLabel, completed, failed,
                         tr("Failed: Count = %1").arg(QString::number(connections)));
    }

    // tcp port
    UpdateTestStatus("verifyTCPPort", ui->verifyTCPPortResultLabel, pending, NA);
    this->repaint();

    VerifyTCPPort();

    // client version
    UpdateTestStatus("checkClientVersion", ui->checkClientVersionResultLabel, pending, NA);

    std::string client_message;

    if (g_UpdateChecker->CheckForLatestUpdate(false, client_message))
    {
        UpdateTestStatus("checkClientVersion", ui->checkClientVersionResultLabel, completed, warning,
                         tr("Warning: New Client version available:\n %1").arg(QString(client_message.c_str())));
    }
    else
    {
        UpdateTestStatus("checkClientVersion", ui->checkClientVersionResultLabel, completed, passed);
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
                unsigned long DateTimeIn = uchar(BufferSocket.at(nNTPCount))
                        + (uchar(BufferSocket.at(nNTPCount + 1)) << 8)
                        + (uchar(BufferSocket.at(nNTPCount + 2)) << 16)
                        + (uchar(BufferSocket.at(nNTPCount + 3)) << 24);
                long tmit = ntohl((time_t)DateTimeIn);
                tmit -= 2208988800U;

                udpSocket->close();

                boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
                boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
                boost::posix_time::time_duration timeDiff = networkTime - localTime;

                if (timeDiff.minutes() < 3)
                {
                    UpdateTestStatus("verifyClockResult", ui->verifyClockResultLabel, completed, passed);
                }
                else
                {
                    UpdateTestStatus("verifyClockResult", ui->verifyClockResultLabel, completed, failed);
                }

                DisplayOverallDiagnosticResult();

                return;
            }
        }
    }
    else // The other state here is a socket or other indeterminate error such as a timeout (coming from clkSocketError).
    {
        // This is needed to "cancel" the timeout timer. Essentially if the test was marked completed via the normal exits
        // above, then when the timer calls clkFinished again, it will hit this conditional and be a no-op.
        if (GetTestStatus("verifyClockResult") != completed)
        {
            UpdateTestStatus("verifyClockResult", ui->verifyClockResultLabel,completed, warning,
                             tr("Warning: Cannot connect to NTP server"));

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
    UpdateTestStatus("verifyTCPPort", ui->verifyTCPPortResultLabel, completed, passed);

    DisplayOverallDiagnosticResult();

    return;
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket)
{
    UpdateTestStatus("verifyTCPPort", ui->verifyTCPPortResultLabel, completed, warning,
                     tr("Warning: Port 32749 may be blocked by your firewall"));

    DisplayOverallDiagnosticResult();

    return;
}
