// Copyright (c) 2014-2021 The Gridcoin developers
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

extern std::atomic<int64_t> g_nTimeBestReceived;

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
                                       DiagnosticResult test_result, QString override_text, QString tooltip_text)
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
    label->setToolTip(tooltip_text);

    this->repaint();
}

unsigned int DiagnosticsDialog::GetNumberOfTestsPending()
{
    LOCK(cs_diagnostictests);

    unsigned int pending_count = 0;

    for (const auto& entry : m_test_status_map)
    {
        if (entry.second == pending) pending_count++;
    }

    return pending_count;
}

DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetTestStatus(std::string test_name)
{
    LOCK(cs_diagnostictests);

    auto entry = m_test_status_map.find(test_name);

    if (entry != m_test_status_map.end())
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
                                                 QString override_text, QString tooltip_text)
{
    LOCK(cs_diagnostictests);

    m_test_status_map[test_name] = test_status;

    SetResultLabel(label, test_status, test_result, override_text, tooltip_text);

    UpdateOverallDiagnosticResult(test_result);

    return m_test_status_map.size();
}


void DiagnosticsDialog::ResetOverallDiagnosticResult()
{
    LOCK(cs_diagnostictests);

    m_test_status_map.clear();

    m_overall_diagnostic_result_status = pending;

    m_overall_diagnostic_result = NA;
}

void DiagnosticsDialog::UpdateOverallDiagnosticResult(DiagnosticResult diagnostic_result_in)
{
    LOCK(cs_diagnostictests);

    // Set diagnostic_result_status to completed. This is under lock, so no one can snoop.
    m_overall_diagnostic_result_status = completed;

    // If the total number of registered tests is less than the initialized number, then
    // the overall status is pending by default.
    if (m_test_status_map.size() < m_number_of_tests)
    {
        m_overall_diagnostic_result_status = pending;
    }

    m_overall_diagnostic_result = (DiagnosticResult) std::max<int>(diagnostic_result_in,  m_overall_diagnostic_result);

    // If diagnostic_result_status is still set to completed, then at least all tests
    // are registered. Walk through the map of tests one by one and check the status.
    // The first one encountered that is pending means the overall status is pending.
    if (m_overall_diagnostic_result_status == completed)
    {
        for (const auto& entry : m_test_status_map)
        {
            if (entry.second == pending)
            {
                m_overall_diagnostic_result_status = pending;
                break;
            }
        }
    }
}

// Lock should be taken on cs_diagnostictests before calling this function.
DiagnosticsDialog::DiagnosticResult DiagnosticsDialog::GetOverallDiagnosticResult()
{
    return m_overall_diagnostic_result;
}

// Lock should be taken on cs_diagnostictests before calling this function.
DiagnosticsDialog::DiagnosticTestStatus DiagnosticsDialog::GetOverallDiagnosticStatus()
{
    return m_overall_diagnostic_result_status;
}


void DiagnosticsDialog::DisplayOverallDiagnosticResult()
{
    LOCK(cs_diagnostictests);

    DiagnosticsDialog::DiagnosticTestStatus overall_diagnostic_status = GetOverallDiagnosticStatus();
    DiagnosticsDialog::DiagnosticResult overall_diagnostic_result = GetOverallDiagnosticResult();

    QString tooltip;

    if (overall_diagnostic_result == warning)
    {
        tooltip = tr("One or more tests have generated a warning status. Wallet operation may be degraded. Please see "
                     "the individual test tooltips for details and recommended action(s).");
    }
    else if (overall_diagnostic_result == failed)
    {
        tooltip = tr("One or more tests have failed. Proper wallet operation may be significantly degraded or impossible. "
                     "Please see the individual test tooltips for details and recommended action(s).");
    }
    else if (overall_diagnostic_result == passed)
    {
        tooltip = tr("All tests passed. Your wallet operation is normal.");
    }
    else
    {
        tooltip = QString();
    }

    SetResultLabel(ui->overallResultResultLabel, overall_diagnostic_status, overall_diagnostic_result,
                   QString(), tooltip);
}

void DiagnosticsDialog::on_testButton_clicked()
{
    // Check to see if there is already a test run in progress, and if so return.
    // We do not want overlapping test runs.

    if (GetNumberOfTestsPending())
    {
        LogPrintf("INFO: DiagnosticsDialog::on_testButton_clicked: Tests still in progress from a prior run: %u",
                  GetNumberOfTestsPending());

        return;
    }

    ResetOverallDiagnosticResult();
    DisplayOverallDiagnosticResult();

    m_researcher_mode = !(m_researcher_model->configuredForInvestorMode() || m_researcher_model->detectedPoolMode());

    VerifyWalletIsSynced();

    unsigned int connections = CheckConnectionCount();

    CheckOutboundConnectionCount();

    VerifyClock(connections);

    VerifyTCPPort();

    double diff = CheckDifficulty();

    CheckClientVersion();

    VerifyBoincPath();

    VerifyCPIDValid();

    VerifyCPIDHasRAC();

    VerifyCPIDIsActive();

    CheckETTS(diff);

    DisplayOverallDiagnosticResult();
}

void DiagnosticsDialog::VerifyWalletIsSynced()
{
    UpdateTestStatus(__func__, ui->verifyWalletIsSyncedResultLabel, pending, NA);

    if (g_nTimeBestReceived == 0 && OutOfSyncByAge())
    {
        QString tooltip = tr("Your wallet is still in initial sync. If this is a sync from the beginning (genesis), the "
                             "sync process can take from 2 to 4 hours, or longer on a slow computer. If you have synced "
                             "your wallet before but you just started the wallet up, then wait a few more minutes and "
                             "retry the diagnostics again.");

        UpdateTestStatus(__func__, ui->verifyWalletIsSyncedResultLabel, completed, warning, QString(), tooltip);
    }
    else if (g_nTimeBestReceived > 0 && OutOfSyncByAge())
    {
        QString tooltip = tr("Your wallet is out of sync with the network but was in sync before. If this fails there is "
                             "likely a severe problem that is preventing the wallet from syncing. If the lack of sync "
                             "is due to network connection issues, you will see failures on the network connection "
                             "test(s). If the network connections pass, but your wallet fails this test, and continues to "
                             "fail this test on repeated attempts with a few minutes in between, this could indicate a "
                             "more serious issue. In that case you should check the debug log to see if it sheds light "
                             "on the cause for no sync.");

        UpdateTestStatus(__func__, ui->verifyWalletIsSyncedResultLabel, completed, failed, QString(), tooltip);

    }
    else
    {
        UpdateTestStatus(__func__, ui->verifyWalletIsSyncedResultLabel, completed, passed);
    }
}

int DiagnosticsDialog::CheckConnectionCount()
{
    UpdateTestStatus(__func__, ui->checkConnectionCountResultLabel, pending, NA);

    int connections = 0;
    int minimum_connections_to_stake = fTestNet ? 1 : 3;
    {
        LOCK(cs_vNodes);

        connections = vNodes.size();
    }

    if (connections <= 7 && connections >= minimum_connections_to_stake)
    {
        QString tooltip = tr("Please check your network and also check the config file and ensure your addnode entries "
                             "are up-to-date. If you recently started the wallet, you may want to wait another few "
                             "minutes for connections to build up and test again. Please see "
                             "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.");

        UpdateTestStatus(__func__, ui->checkConnectionCountResultLabel, completed, warning,
                         tr("Warning: Count = %1 (Pass = 8+)").arg(QString::number(connections)), tooltip);
    }
    else if (connections >= 8)
    {
        UpdateTestStatus(__func__, ui->checkConnectionCountResultLabel, completed, passed,
                         tr("Passed: Count = %1").arg(QString::number(connections)));
    }
    else
    {
        QString tooltip = tr("You will not be able to stake because you have less than %1 connection(s). Please check "
                             "your network and also check the config file and ensure your addnode entries are up-to-date. "
                             "If you recently started the wallet, you may want to wait another few minutes for connections "
                             "to build up and then test again. Please see https://gridcoin.us/wiki/config-file.html and "
                             "https://addnodes.cycy.me/.").arg(QString::number(minimum_connections_to_stake));

        UpdateTestStatus(__func__, ui->checkConnectionCountResultLabel, completed, failed,
                         tr("Failed: Count = %1").arg(QString::number(connections)), tooltip);
    }

    return connections;
}

void DiagnosticsDialog::CheckOutboundConnectionCount()
{
    UpdateTestStatus(__func__, ui->checkOutboundConnectionCountResultLabel, pending, NA);

    LOCK(cs_vNodes);

    int outbound_connections = 0;

    for (const auto& vnodes : vNodes)
    {
        if (!vnodes->fInbound) ++outbound_connections;
    }

    if (outbound_connections < 1)
    {
        QString tooltip = tr("Your outbound connection count is critically low. Please check your the config file and "
                             "ensure your addnode entries are up-to-date. If you recently started the wallet, you may "
                             "want to wait another few minutes for connections to build up and then test again. Please see "
                             "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.");

        UpdateTestStatus(__func__, ui->checkOutboundConnectionCountResultLabel, completed, failed,
                         tr("Failed: Count = %1").arg(QString::number(outbound_connections)), tooltip);
    }
    else if (outbound_connections < 3)
    {
        QString tooltip = tr("Your outbound connection count is low. Please check your the config file and "
                             "ensure your addnode entries are up-to-date. If you recently started the wallet, you may "
                             "want to wait another few minutes for connections to build up and then test again. Please see "
                             "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.");

        UpdateTestStatus(__func__, ui->checkOutboundConnectionCountResultLabel, completed, warning,
                         tr("Warning: Count = %1 (Pass = 3+)").arg(QString::number(outbound_connections)), tooltip);

    }
    else
    {
        UpdateTestStatus(__func__, ui->checkOutboundConnectionCountResultLabel, completed, passed,
                         tr("Passed: Count = %1").arg(QString::number(outbound_connections)));
    }
}

void DiagnosticsDialog::VerifyClock(unsigned int connections)
{
    UpdateTestStatus(__func__, ui->verifyClockResultLabel, pending, NA);

    // This is aligned to the minimum sample size in AddTimeData to compute a nTimeOffset from the sampling of
    // connected nodes. If a sufficient sample exists, use the existing time offset from the node rather than
    // going through the ntp connection code. The ntp connection code is retained to help people resolve no connection
    // situations where their node is so far off they are banned and disconnected from the network.
    if (connections >= 5)
    {
        int64_t time_offset = 0;

        {
            LOCK(cs_main);

            time_offset = GetTimeOffset();
        }

        clkReportResults(time_offset);

        return;
    }

    QTimer *timerVerifyClock = new QTimer();

    // Set up a timeout clock of 10 seconds as a fail-safe.
    connect(timerVerifyClock, SIGNAL(timeout()), this, SLOT(clkFinished()));
    timerVerifyClock->start(10 * 1000);

    QHostInfo NTPHost = QHostInfo::fromName("pool.ntp.org");
    m_udpSocket = new QUdpSocket(this);

    connect(m_udpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this,
            SLOT(clkStateChanged(QAbstractSocket::SocketState)));
    connect(m_udpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(clkSocketError()));

    if (!NTPHost.addresses().empty())
    {
        m_udpSocket->connectToHost(QHostAddress(NTPHost.addresses().first()), 123, QIODevice::ReadWrite);
    }
    else
    {
        clkSocketError();
    }
}

void DiagnosticsDialog::clkStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ConnectedState)
    {
        connect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));

        char NTPMessage[48] = {0x1b, 0, 0, 0 ,0, 0, 0, 0, 0};

        m_udpSocket->write(NTPMessage, sizeof(NTPMessage));
    }
}

void DiagnosticsDialog::clkSocketError()
{
    m_udpSocket->close();

    clkReportResults(0, true);
}

void DiagnosticsDialog::clkFinished()
{
    if (m_udpSocket->waitForReadyRead(10 * 1000))
    {
        int64_t start_time = GetAdjustedTime();

        // Only allow this loop to run for 5 seconds maximum.
        while (m_udpSocket->hasPendingDatagrams() && GetAdjustedTime() - start_time <= 5)
        {
            QByteArray BufferSocket = m_udpSocket->readAll();

            if (BufferSocket.size() == 48)
            {
                int nNTPCount = 40;
                uint32_t DateTimeIn = uchar(BufferSocket.at(nNTPCount))
                        + (uchar(BufferSocket.at(nNTPCount + 1)) << 8)
                        + (uchar(BufferSocket.at(nNTPCount + 2)) << 16)
                        + (uchar(BufferSocket.at(nNTPCount + 3)) << 24);
                time_t tmit = ntohl(DateTimeIn) - 2208988800U;

                m_udpSocket->close();

                boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
                boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
                boost::posix_time::time_duration timeDiff = networkTime - localTime;

                clkReportResults(timeDiff.total_seconds());

                return;
            }
        }
    }
    else // The other state here is a socket or other indeterminate error such as a timeout (coming from clkSocketError).
    {
        // This is needed to "cancel" the timeout timer. Essentially if the test was marked completed via the normal exits
        // above, then when the timer calls clkFinished again, it will hit this conditional and be a no-op.
        if (GetTestStatus("VerifyClock") != completed)
        {
            clkReportResults(0, true);
        }

        return;
    }
}

void DiagnosticsDialog::clkReportResults(const int64_t& time_offset, const bool& timeout_during_check)
{
    if (!timeout_during_check)
    {
        if (abs64(time_offset) < 3 * 60)
        {
            UpdateTestStatus("VerifyClock", ui->verifyClockResultLabel, completed, passed);
        }
        else if (abs64(time_offset) < 5 * 60)
        {
            QString tooltip = tr("You should check your time and time zone settings for your computer.");

            UpdateTestStatus("VerifyClock", ui->verifyClockResultLabel, completed, warning,
                             tr("Warning: Clock skew is between 3 and 5 minutes. Please check your clock settings."),
                             tooltip);
        }
        else
        {
            QString tooltip = tr("Your clock in your computer is significantly off from UTC or network time and "
                                 "this may seriously degrade the operation of the wallet, including maintaining "
                                 "connection to the network. You should check your time and time zone settings "
                                 "for your computer. A very common problem is the off by one hour caused by a time "
                                 "zone issue or problems with daylight savings time.");

            UpdateTestStatus("VerifyClock", ui->verifyClockResultLabel, completed, failed,
                             tr("Error: Clock skew is 5 minutes or greater. Please check your clock settings."),
                             tooltip);
        }
    }
    else
    {
        QString tooltip = tr("The wallet has less than five connections to the network and is unable to connect "
                             "to an NTP server to check your computer clock. This is not necessarily a problem. "
                             "You can wait a few minutes and try the test again.");

        UpdateTestStatus("VerifyClock", ui->verifyClockResultLabel, completed, warning,
                         tr("Warning: Cannot connect to NTP server"), tooltip);
    }

    DisplayOverallDiagnosticResult();
}

void DiagnosticsDialog::VerifyTCPPort()
{
    UpdateTestStatus(__func__, ui->verifyTCPPortResultLabel, pending, NA);

    m_tcpSocket = new QTcpSocket(this);

    connect(m_tcpSocket, SIGNAL(connected()), this, SLOT(TCPFinished()));
    connect(m_tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(TCPFailed(QAbstractSocket::SocketError)));

    m_tcpSocket->connectToHost("portquiz.net", GetListenPort());
}

void DiagnosticsDialog::TCPFinished()
{
    m_tcpSocket->close();
    UpdateTestStatus("VerifyTCPPort", ui->verifyTCPPortResultLabel, completed, passed);

    DisplayOverallDiagnosticResult();

    return;
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket_error)
{
    DiagnosticResult test_result = warning;
    QString tooltip = tr("Outbound communication to TCP port %1 appears to be blocked. ").arg(GetListenPort());

    switch (socket_error)
    {
    case QAbstractSocket::SocketError::ConnectionRefusedError:
        tooltip += tr("The connection to the port test site was refused. This could be a transient problem with the "
                      "port test site, but could also be an issue with your firewall. If you are also failing the "
                      "connection test, your firewall is most likely blocking network communications from the "
                      "Gridcoin client.");

        break;

    case QAbstractSocket::SocketError::RemoteHostClosedError:
        tooltip += tr("The port test site is closed on port. This could be a transient problem with the "
                      "port test site, but could also be an issue with your firewall. If you are also failing the "
                      "connection test, your firewall is most likely blocking network communications from the "
                      "Gridcoin client.");

        break;

    case QAbstractSocket::SocketError::HostNotFoundError:
        // This does not include the common text above on purpose.
        tooltip = tr("The IP for the port test site is unable to be resolved. This could mean your DNS is not working "
                     "correctly. The wallet may operate without DNS, but it could be severely degraded, especially if "
                     "the wallet is new and a database of prior successful connections has not been built up. Please "
                     "check your computer and ensure name resolution is operating correctly.");

        break;

    case QAbstractSocket::SocketError::SocketAccessError:
    case QAbstractSocket::SocketError::SocketResourceError:
    case QAbstractSocket::SocketError::SocketTimeoutError:
    case QAbstractSocket::SocketError::DatagramTooLargeError:
    case QAbstractSocket::SocketError::NetworkError:
    case QAbstractSocket::SocketError::AddressInUseError:
    case QAbstractSocket::SocketError::SocketAddressNotAvailableError:
    case QAbstractSocket::SocketError::UnsupportedSocketOperationError:
    case QAbstractSocket::SocketError::UnfinishedSocketOperationError:
    case QAbstractSocket::SocketError::OperationError:
        test_result = failed;

        // This does not include the common text above on purpose.
        tooltip = tr("The network has experienced a low-level error and this probably means your IP address or other "
                     "network connection parameters are not configured correctly. Please check your network configuration "
                     "on your computer.");

        break;

    case QAbstractSocket::SocketError::ProxyAuthenticationRequiredError:
    case QAbstractSocket::SocketError::ProxyConnectionRefusedError:
    case QAbstractSocket::SocketError::ProxyConnectionClosedError:
    case QAbstractSocket::SocketError::ProxyConnectionTimeoutError:
    case QAbstractSocket::SocketError::ProxyNotFoundError:
    case QAbstractSocket::SocketError::ProxyProtocolError:
        test_result = failed;

        tooltip += tr("Your network may be using a proxy server to communicate to public IP addresses on the Internet, and "
                      "the wallet is not configured properly to use it. Please check the proxy settings under Options -> "
                      "Network -> Connect through SOCKS proxy.");

        break;

    // SSL errors will NOT be triggered unless we implement a test and site to actually do an SSL connection test. This
    // is put here for completeness.
    case QAbstractSocket::SocketError::SslHandshakeFailedError:
    case QAbstractSocket::SocketError::SslInternalError:
    case QAbstractSocket::SocketError::SslInvalidUserDataError:
        test_result = failed;

        // This does not include the common text above on purpose.
        tooltip = tr("The network is reporting an SSL error. If you also failed or got a warning on your clock test, you "
                     "should check your clock settings, including your time and time zone. If your clock is ok, please "
                     "check your computer's network configuration.");

        break;

    case QAbstractSocket::SocketError::TemporaryError:
    case QAbstractSocket::SocketError::UnknownSocketError:
        test_result = failed;

        // This does not include the common text above on purpose.
        tooltip = tr("The network is reporting an unspecified socket error. If you also are failing the connection test, "
                     "then please check your computer's network configuration.");
    }

    UpdateTestStatus("VerifyTCPPort", ui->verifyTCPPortResultLabel, completed, test_result,
                     QString(), tooltip);

    DisplayOverallDiagnosticResult();

    return;
}

double DiagnosticsDialog::CheckDifficulty()
{
    UpdateTestStatus(__func__, ui->checkDifficultyResultLabel, pending, NA);

    double diff = 0;
    double scale_factor = 1.0;

    {
        LOCK(cs_main);

        scale_factor = fTestNet ? 0.1 : 1.0;

        diff = GRC::GetAverageDifficulty(80);
    }

    double fail_diff = scale_factor;
    double warn_diff = scale_factor * 5.0;

    // If g_nTimeBestReceived == 0, the wallet is still in the initial sync process. In that case use the failure
    // standard and just warn, with a different explanation.
    if (g_nTimeBestReceived == 0 && OutOfSyncByAge() && diff < fail_diff)
    {
        QString override_text = tr("Warning: 80 block difficulty is less than %1.")
                .arg(QString::number(fail_diff, 'f', 1));

        QString tooltip = tr("Your difficulty is low but your wallet is still in initial sync. Please recheck it later "
                             "to see if this passes.");

        UpdateTestStatus(__func__, ui->checkDifficultyResultLabel, completed, warning,
                         override_text, tooltip);
    }
    // If the wallet has been in sync in the past in this run, then apply the normal standards, whether the wallet is
    // in sync or not right now.
    else if (g_nTimeBestReceived > 0 && diff < fail_diff)
    {
        QString override_text = tr("Failed: 80 block difficulty is less than %1. This wallet is almost certainly forked.")
                .arg(QString::number(fail_diff, 'f', 1));

        QString tooltip = tr("Your difficulty is extremely low and your wallet is almost certainly forked. Please ensure "
                             "you are running the latest version and try removing the blockchain database and resyncing "
                             "from genesis using the menu option. (Note this will take 2-4 hours.)");

        UpdateTestStatus(__func__, ui->checkDifficultyResultLabel, completed, failed,
                         override_text, tooltip);
    }
    else if (g_nTimeBestReceived > 0 && diff < warn_diff)
    {
        QString override_text = tr("Warning: 80 block difficulty is less than %1. This wallet is probably forked.")
                .arg(QString::number(warn_diff, 'f', 1));

        QString tooltip = tr("Your difficulty is very low and your wallet is probably forked. Please ensure you are "
                             "running the latest version and try removing the blockchain database and resyncing from "
                             "genesis using the menu option. (Note this will take 2-4 hours.)");

        UpdateTestStatus(__func__, ui->checkDifficultyResultLabel, completed, warning,
                         override_text, tooltip);
    }
    else
    {
        QString override_text = tr("Passed: 80 block difficulty is %1.")
                .arg(QString::number(diff, 'f', 1));

        UpdateTestStatus(__func__, ui->checkDifficultyResultLabel, completed, passed, override_text);
    }

    return diff;
}

void DiagnosticsDialog::CheckClientVersion()
{
    UpdateTestStatus(__func__, ui->checkClientVersionResultLabel, pending, NA);

    std::string client_message;

    if (g_UpdateChecker->CheckForLatestUpdate(client_message, false)
            && client_message.find("mandatory") != std::string::npos)
    {
        QString tooltip = tr("There is a new mandatory version available and you should upgrade as soon as possible to "
                             "ensure your wallet remains in consensus with the network.");

        UpdateTestStatus(__func__, ui->checkClientVersionResultLabel, completed, failed,
                         tr("Warning: New Client version available:\n %1").arg(QString(client_message.c_str())));
    }
    else if (g_UpdateChecker->CheckForLatestUpdate(client_message, false)
             && client_message.find("mandatory") == std::string::npos)
    {
        QString tooltip = tr("There is a new leisure version available and you should upgrade as soon as practical.");

        UpdateTestStatus(__func__, ui->checkClientVersionResultLabel, completed, warning,
                         tr("Warning: New Client version available:\n %1").arg(QString(client_message.c_str())));
    }
    else
    {
        UpdateTestStatus(__func__, ui->checkClientVersionResultLabel, completed, passed);
    }
}

void DiagnosticsDialog::VerifyBoincPath()
{
    // This test is only applicable if the wallet is in researcher mode.
    if (!m_researcher_mode)
    {
        UpdateTestStatus(__func__, ui->verifyBoincPathResultLabel, completed, NA);

        return;
    }

    UpdateTestStatus(__func__, ui->verifyBoincPathResultLabel, pending, NA);

    fs::path boincPath = (fs::path) GRC::GetBoincDataDir();

    if (boincPath.empty())
        boincPath = (fs::path) GetArgument("boincdatadir", "");

    boincPath = boincPath / "client_state.xml";

    if (fs::exists(boincPath))
    {
        UpdateTestStatus(__func__, ui->verifyBoincPathResultLabel, completed, passed);
    }
    else
    {
        QString tooltip = tr("Check that BOINC is installed and that you have the correct path in the config file "
                             "if you installed it to a nonstandard location.");

        UpdateTestStatus(__func__, ui->verifyBoincPathResultLabel, completed, failed, QString(), tooltip);
    }
}

void DiagnosticsDialog::VerifyCPIDValid()
{
    // This test is only applicable if the wallet is in researcher mode.
    if (!m_researcher_mode)
    {
        UpdateTestStatus(__func__, ui->verifyCPIDValidResultLabel, completed, NA);

        return;
    }

    UpdateTestStatus(__func__, ui->verifyCPIDValidResultLabel, pending, NA);

    if (m_researcher_model->hasEligibleProjects())
    {
        UpdateTestStatus(__func__, ui->verifyCPIDValidResultLabel, completed, passed);
    }
    else
    {
        if (g_nTimeBestReceived == 0 && OutOfSyncByAge())
        {
            QString tooltip = tr("Your wallet is not in sync and has not previously been in sync during this run, please "
                                 "wait for the wallet to sync and retest. If there are other failures preventing the "
                                 "wallet from syncing, please correct those items and retest to see if this test passes.");

            UpdateTestStatus(__func__, ui->verifyCPIDValidResultLabel, completed, warning, QString(), tooltip);
        }
        else
        {
            QString tooltip = tr("Verify (1) that you have BOINC installed correctly, (2) that you have attached at least "
                                 "one whitelisted project, (3) that you advertised your beacon with the same email as you "
                                 "use for your BOINC project(s), and (4) that the CPID on the overview screen matches the "
                                 "CPID when you login to your BOINC project(s) online.");

            UpdateTestStatus(__func__, ui->verifyCPIDValidResultLabel, completed, failed, QString(), tooltip);
        }
    }
}

void DiagnosticsDialog::VerifyCPIDHasRAC()
{
    // This test is only applicable if the wallet is in researcher mode.
    if (!m_researcher_mode)
    {
        UpdateTestStatus(__func__, ui->verifyCPIDHasRACResultLabel, completed, NA);

        return;
    }

    UpdateTestStatus(__func__, ui->verifyCPIDHasRACResultLabel, pending, NA);

    if (m_researcher_model->hasRAC())
    {
        UpdateTestStatus(__func__, ui->verifyCPIDHasRACResultLabel, completed, passed);
    }
    else
    {
        if (g_nTimeBestReceived == 0 && OutOfSyncByAge())
        {
            QString tooltip = tr("Your wallet is not in sync and has not previously been in sync during this run, please "
                                 "wait for the wallet to sync and retest. If there are other failures preventing the "
                                 "wallet from syncing, please correct those items and retest to see if this test passes.");

            UpdateTestStatus(__func__, ui->verifyCPIDHasRACResultLabel, completed, warning, QString(), tooltip);
        }
        else
        {
            QString tooltip = tr("Verify that you have actually completed workunits for the projects you have attached and "
                                 "that you have authorized the export of statistics. Please see "
                                 "https://gridcoin.us/guides/whitelist.htm.");

            UpdateTestStatus(__func__, ui->verifyCPIDHasRACResultLabel, completed, failed, QString(), tooltip);
        }
    }
}

void DiagnosticsDialog::VerifyCPIDIsActive()
{
    // This test is only applicable if the wallet is in researcher mode.
    if (!m_researcher_mode)
    {
        UpdateTestStatus(__func__, ui->verifyCPIDIsActiveResultLabel, completed, NA);

        return;
    }

    UpdateTestStatus(__func__, ui->verifyCPIDIsActiveResultLabel, pending, NA);

    if (m_researcher_model->hasActiveBeacon())
    {
        UpdateTestStatus(__func__, ui->verifyCPIDIsActiveResultLabel, completed, passed);
    }
    else
    {
        if (g_nTimeBestReceived == 0 && OutOfSyncByAge())
        {
            QString tooltip = tr("Your wallet is not in sync and has not previously been in sync during this run, please "
                                 "wait for the wallet to sync and retest. If there are other failures preventing the "
                                 "wallet from syncing, please correct those items and retest to see if this test passes.");

            UpdateTestStatus(__func__, ui->verifyCPIDIsActiveResultLabel, completed, warning, QString(), tooltip);
        }
        else
        {
            QString tooltip = tr("Please ensure that you have followed the process to advertise and verify your beacon. "
                                 "You can use the research wizard (the beacon button on the overview screen).");

            UpdateTestStatus(__func__, ui->verifyCPIDIsActiveResultLabel, completed, failed, QString(), tooltip);
        }
    }
}

// check ETTS
// This is only checked if wallet is a researcher wallet because the purpose is to
// alert the owner that his stake time is too long and therefore there is a chance
// of research rewards loss between stakes due to the 180 day limit.
void DiagnosticsDialog::CheckETTS(const double& diff)
{
    // This test is only applicable if the wallet is in researcher mode.
    if (!m_researcher_mode)
    {
        UpdateTestStatus(__func__, ui->checkETTSResultLabel, completed, NA);

        return;
    }

    UpdateTestStatus(__func__, ui->checkETTSResultLabel, pending, NA);

    double ETTS = GRC::GetEstimatedTimetoStake(true, diff) / (24.0 * 60.0 * 60.0);

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

    if (g_nTimeBestReceived == 0 && OutOfSyncByAge())
    {
        QString tooltip = tr("Your wallet is not in sync and has not previously been in sync during this run, please "
                             "wait for the wallet to sync and retest. If there are other failures preventing the "
                             "wallet from syncing, please correct those items and retest to see if this test passes.");

        UpdateTestStatus(__func__, ui->checkETTSResultLabel, completed, warning, QString(), tooltip);
    }
    else
    {
        // ETTS of zero actually means no coins, i.e. infinite.
        if (ETTS == 0.0)
        {
            QString tooltip = tr("You have no balance and will be unable to retrieve your research rewards when solo "
                                 "mining. You should acquire GRC to stake so you can retrieve your research rewards. "
                                 "Please see https://gridcoin.us/guides/boinc-install.htm.");

            UpdateTestStatus(__func__, ui->checkETTSResultLabel, completed, failed,
                             tr("Failed: ETTS is infinite. No coins to stake."), tooltip);
        }
        else if (ETTS > 90.0)
        {
            QString tooltip = tr("Your balance is too low given the current network difficulty to stake in a reasonable "
                                 "period of time to retrieve your research rewards when solo mining. You should acquire "
                                 "more GRC to stake more often.");

            UpdateTestStatus(__func__, ui->checkETTSResultLabel, completed, failed,
                             tr("Failed: ETTS is > 90 days. It will take a very long time to receive your research "
                                "rewards."),
                             tooltip);
        }
        else if (ETTS > 45.0 && ETTS <= 90.0)
        {
            QString tooltip = tr("Your balance is low given the current network difficulty to stake in a reasonable "
                                 "period of time to retrieve your research rewards when solo mining. You should consider "
                                 "acquiring more GRC to stake more often.");

            UpdateTestStatus(__func__, ui->checkETTSResultLabel, completed, warning,
                             tr("Warning: 45 days < ETTS = %1 <= 90 days").arg(QString(rounded_ETTS.c_str())),
                             tooltip);
        }
        else
        {
            UpdateTestStatus(__func__, ui->checkETTSResultLabel, completed, passed,
                             tr("Passed: ETTS = %1 <= 45 days").arg(QString(rounded_ETTS.c_str())));
        }
    }
}

