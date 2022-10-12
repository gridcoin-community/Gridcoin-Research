
#include "diagnose.h"
#include "net.h"

namespace DiagnoseLib {

// Define and initialize the Static members
bool Diagnose::m_researcher_mode = false;
bool Diagnose::m_hasEligibleProjects = false;
bool Diagnose::m_hasPoolProjects = false;
bool Diagnose::m_configured_for_investor_mode = false;
// const ResearcherModel* Diagnose::m_researcher_model = nullptr;
std::unordered_map<Diagnose::TestNames, Diagnose*> Diagnose::m_name_to_test_map;
CCriticalSection Diagnose::cs_diagnostictests;
boost::asio::io_service Diagnose::s_ioService;

/**
 * The function check the time is correct on your PC. It checks the skew in the clock.
 *
 */
void VerifyClock::clkReportResults(const int64_t& time_offset, const bool& timeout_during_check)
{
    if (!timeout_during_check) {
        if (abs64(time_offset) < 3 * 60) {
            m_results = PASS;
        } else if (abs64(time_offset) < 5 * 60) {
            m_results_tip = "You should check your time and time zone settings for your computer.";
            m_results = WARNING;
            m_results_string = "Warning: Clock skew is between 3 and 5 minutes. Please check your clock settings.";
        } else {
            m_results = FAIL;
            m_results_tip = "Your clock in your computer is significantly off from UTC or network time and "
                            "this may seriously degrade the operation of the wallet, including maintaining "
                            "connection to the network. You should check your time and time zone settings "
                            "for your computer. A very common problem is the off by one hour caused by a time "
                            "zone issue or problems with daylight savings time.";
            m_results_string = "Error: Clock skew is 5 minutes or greater. Please check your clock settings.";
        }
    } else {
        m_results = WARNING;
        m_results_tip = "The wallet has less than five connections to the network and is unable to connect "
                        "to an NTP server to check your computer clock. This is not necessarily a problem. "
                        "You can wait a few minutes and try the test again.";
        m_results_string = "Warning: Cannot connect to NTP server";
    }
    m_startedTesting = false;
}

/*
 * Function that will be called when the Socket Send To is done
 */
void VerifyClock::sockSendToHandle(
    const boost::system::error_code& error, // Result of operation.
    std::size_t bytes_transferred           // Number of bytes sent.
)
{
    if (error) {
        clkReportResults(0, true);
    } else {
        boost::asio::ip::udp::endpoint sender_endpoint;

        m_udpSocket.async_receive_from(
            boost::asio::buffer(m_recvBuf),
            sender_endpoint,
            boost::bind(&VerifyClock::sockRecvHandle, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
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
            uint32_t DateTimeIn = reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount)) + (reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount + 1)) << 8) + (reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount + 2)) << 16) + (reinterpret_cast<unsigned char>(m_recvBuf.at(nNTPCount + 3)) << 24);
            time_t tmit = ntohl(DateTimeIn) - 2208988800U;

            m_udpSocket.close();

            boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
            boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
            boost::posix_time::time_duration timeDiff = networkTime - localTime;

            clkReportResults(timeDiff.total_seconds());

            return;
        } else // The other state here is a socket or other indeterminate error such as a timeout (coming from clkSocketError).
        {
            // This is needed to "cancel" the timeout timer. Essentially if the test was marked completed via the normal exits
            // above, then when the timer calls clkFinished again, it will hit this conditional and be a no-op.

            auto VerifyClock_Test = getTest(Diagnose::VerifyClock);
            if (VerifyClock_Test->getResults() != Diagnose::NONE) {
                clkReportResults(0, true);
            }

            return;
        }
    }
    m_udpSocket.close();
}

void VerifyClock::timerHandle(
    const boost::system::error_code& error)
{
    if (m_startedTesting) {
        m_udpSocket.close();
        m_timer.cancel();
        clkReportResults(0, true);
    }
}

/**
 * The function connects to the ntp server to get the correct clock time
 */
void VerifyClock::connectToNTPHost()
{
    m_startedTesting = true;
    boost::asio::ip::udp::resolver resolver(s_ioService);
#if BOOST_VERSION > 106501
        auto receiver_endpoint = *resolver.resolve(boost::asio::ip::udp::v4(),
                                               "pool.ntp.org", "ntp");

#else
        boost::asio::ip::udp::resolver::query query(
													 boost::asio::ip::udp::v4(),
													 this->_host_name,
													 "ntp");
        auto receiver_endpoint  = *resolver.resolve(query);
#endif

   
    if (m_udpSocket.is_open())
        m_udpSocket.close();
    m_udpSocket.open(boost::asio::ip::udp::v4());


    m_udpSocket.async_send_to(boost::asio::buffer(m_sendBuf), receiver_endpoint,
                              boost::bind(&VerifyClock::sockSendToHandle, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void VerifyTCPPort::handle_connect(const boost::system::error_code& err,
                                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    if (!err) {
        this->TCPFinished();
    } else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator()) {
        // The connection failed. Try the next endpoint in the list.
        m_tcpSocket.close();
        boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
        m_tcpSocket.async_connect(endpoint,
                                  boost::bind(&VerifyTCPPort::handle_connect, this,
                                              boost::asio::placeholders::error, ++endpoint_iterator));
    } else {
        m_tcpSocket.close();
        m_results = WARNING;
        m_results_tip = "Outbound communication to TCP port %1 appears to be blocked. ";
        std::string ss = argToString(GetListenPort());
        m_results_string_arg.push_back(ss);

        switch (err.value()) {
        case boost::asio::error::connection_refused:
            m_results_tip += "The connection to the port test site was refused. This could be a transient problem with the "
                             "port test site, but could also be an issue with your firewall. If you are also failing the "
                             "connection test, your firewall is most likely blocking network communications from the "
                             "Gridcoin client.";

            break;

        case boost::asio::error::connection_aborted:
            m_results_tip += "The port test site is closed on port. This could be a transient problem with the "
                             "port test site, but could also be an issue with your firewall. If you are also failing the "
                             "connection test, your firewall is most likely blocking network communications from the "
                             "Gridcoin client.";

            break;

        case boost::asio::error::host_unreachable:
            // This does not include the common text above on purpose.
            m_results_tip = "The IP for the port test site is unable to be resolved. This could mean your DNS is not working "
                            "correctly. The wallet may operate without DNS, but it could be severely degraded, especially if "
                            "the wallet is new and a database of prior successful connections has not been built up. Please "
                            "check your computer and ensure name resolution is operating correctly.";

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
            m_results_tip = "The network has experienced a low-level error and this probably means your IP address or other "
                            "network connection parameters are not configured correctly. Please check your network configuration "
                            "on your computer.";

            break;

        case boost::asio::error::fault:
            m_results = FAIL;

            // This does not include the common text above on purpose.
            m_results_tip = "The network is reporting an unspecified socket error. If you also are failing the connection test, "
                            "then please check your computer's network configuration.";
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
