// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "diagnose.h"
#include "net.h"
#include "random.h"

namespace DiagnoseLib {

// Define and initialize the Static members
bool Diagnose::m_researcher_mode = false;
bool Diagnose::m_hasEligibleProjects = false;
bool Diagnose::m_hasPoolProjects = false;
bool Diagnose::m_configured_for_investor_mode = false;
std::unordered_map<Diagnose::TestNames, Diagnose*> Diagnose::m_name_to_test_map;
CCriticalSection Diagnose::cs_diagnostictests;
boost::asio::io_context Diagnose::s_ioService;

/**
 * The function check the time is correct on your PC. It checks the skew in the clock.
 */
void VerifyClock::clkReportResults(const int64_t& time_offset, const bool& timeout_during_check)
{
    m_results_string_arg.clear();
    m_results_tip_arg.clear();

    if (!timeout_during_check) {
        if (abs64(time_offset) < 3 * 60) {
            m_results_tip = "";
            m_results_string = "";
            m_results = PASS;
        } else if (abs64(time_offset) < 5 * 60) {
            m_results_tip = _("You should check your time and time zone settings for your computer.");
            m_results = WARNING;
            m_results_string = _("Warning: Clock skew is between 3 and 5 minutes. Please check your clock settings.");
        } else {
            m_results = FAIL;
            m_results_tip = _("Your clock in your computer is significantly off from UTC or network time and "
                              "this may seriously degrade the operation of the wallet, including maintaining "
                              "connection to the network. You should check your time and time zone settings "
                              "for your computer. A very common problem is the off by one hour caused by a time "
                              "zone issue or problems with daylight savings time.");
            m_results_string = _("Error: Clock skew is 5 minutes or greater. Please check your clock settings.");
        }
    } else {
        m_results = WARNING;
        m_results_tip = _("The wallet has less than five connections to the network and is unable to connect "
                          "to an NTP server to check your computer clock. This is not necessarily a problem. "
                          "You can wait a few minutes and try the test again.");
        m_results_string = _("Warning: Cannot connect to NTP server");
    }
    m_startedTesting = false;
}

void VerifyClock::sockRecvHandle(
        const boost::system::error_code& error, // Result of operation.
        std::size_t bytes_transferred           // Number of bytes received.
        )
{
    if (error) {
        clkReportResults(0, true);
    } else {
        if (bytes_transferred == 48) {
            int nNTPCount = 40;
            uint32_t DateTimeIn = reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount))
                    + (reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount + 1)) << 8)
                    + (reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount + 2)) << 16)
                    + (reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount + 3)) << 24);
            time_t tmit = ntohl(DateTimeIn) - 2208988800U;

            boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
            boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
            boost::posix_time::time_duration timeDiff = networkTime - localTime;

            clkReportResults(timeDiff.total_seconds());

            // The timer cancel has to be here, because you need to cancel the timer if this completes
            // first. In the case that this completes very fast (which is most of the time),
            // waiting 10 seconds for the timer to expire to report is not desirable.
            m_timer.cancel();
        } else { // The other state here is a socket or other indeterminate error.
            clkReportResults(0, true);
        }
    }

    m_udpSocket.close();
}

void VerifyClock::timerHandle(
        const boost::system::error_code& error)
{
    if (m_startedTesting && error != boost::asio::error::operation_aborted) {
        m_udpSocket.close();
        clkReportResults(0, true);
    }
}

/**
 * The function connects to the ntp server to get the correct clock time
 */
void VerifyClock::connectToNTPHost()
{
    m_startedTesting = true;

    try {
        FastRandomContext rng;

        std::string ntp_host = m_ntp_hosts[rng.randrange(m_ntp_hosts.size())];

        boost::asio::ip::udp::resolver resolver(s_ioService);
        auto resolved = resolver.resolve(boost::asio::ip::udp::v4(), ntp_host, "ntp");

        if (m_udpSocket.is_open())
            m_udpSocket.close();
        m_udpSocket.open(boost::asio::ip::udp::v4());
        boost::asio::connect(m_udpSocket, resolved);

        size_t bytes_transferred = m_udpSocket.send(boost::asio::buffer(m_sendBuf));

        if (bytes_transferred != 48) {
            clkReportResults(0, true);
        } else {
            m_udpSocket.async_receive(
                        boost::asio::buffer(m_recvBuf),
                        boost::bind(&VerifyClock::sockRecvHandle, this,
                                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
    } catch (...) {
        clkReportResults(0, true);
    }
}

void VerifyTCPPort::handle_connect(const boost::system::error_code& err)
{
    if (!err) {
        this->TCPFinished();
    } else {
        m_tcpSocket.close();
        m_results = WARNING;
        m_results_tip = _("Outbound communication to TCP port %1 appears to be blocked. ");
        std::string ss = ToString(GetListenPort());
        m_results_string_arg.push_back(ss);

        switch (err.value()) {
        case boost::asio::error::connection_refused:
            m_results_tip += _("The connection to the port test site was refused. This could be a transient problem with "
                               "the port test site, but could also be an issue with your firewall. If you are also failing "
                               "the connection test, your firewall is most likely blocking network communications from the "
                               "Gridcoin client.");

            break;

        case boost::asio::error::connection_aborted:
            m_results_tip += _("The port test site is closed on port. This could be a transient problem with the "
                               "port test site, but could also be an issue with your firewall. If you are also failing the "
                               "connection test, your firewall is most likely blocking network communications from the "
                               "Gridcoin client.");

            break;

        case boost::asio::error::host_unreachable:
            // This does not include the common text above on purpose.
            m_results_tip = _("The IP for the port test site is unable to be resolved. This could mean your DNS is not "
                              "working correctly. The wallet may operate without DNS, but it could be severely degraded, "
                              "especially if the wallet is new and a database of prior successful connections has not been "
                              "built up. Please check your computer and ensure name resolution is operating correctly.");

            break;

        case boost::asio::error::access_denied:
        case boost::asio::error::not_socket:
        case boost::asio::error::timed_out:
        case boost::asio::error::network_down:
        case boost::asio::error::no_permission:
        case boost::asio::error::address_in_use:
        case boost::asio::error::operation_not_supported:
            m_results = FAIL;

            // This does not include the common text above on purpose.
            m_results_tip = _("The network has experienced a low-level error and this probably means your IP address or "
                              "other network connection parameters are not configured correctly. Please check your network "
                              "configuration on your computer.");

            break;

        case boost::asio::error::fault:
            m_results = FAIL;

            // This does not include the common text above on purpose.
            m_results_tip = _("The network is reporting an unspecified socket error. If you also are failing the "
                              "connection test, then please check your computer's network configuration.");
        }
    }

    return;
}

void VerifyTCPPort::TCPFinished()
{
    m_tcpSocket.close();
    m_results = PASS;
    return;
}
} // namespace DiagnoseLib
