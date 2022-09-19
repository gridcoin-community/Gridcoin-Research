
#include "diagnose.h"
#include "net.h"
#include <QtNetwork>

namespace DiagnoseLib {

// Define the Static members
bool Diagnose::m_researcher_mode = false;
const ResearcherModel* Diagnose::m_researcher_model = nullptr;
std::unordered_map<Diagnose::TestNames, Diagnose*> Diagnose::m_name_to_test_map;
CCriticalSection Diagnose::cs_diagnostictests;

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
    time_t tmit;
    if (getResults() != NONE) return;
    if (error) {
        clkReportResults(0, true);
    } else {
        tmit = ntohl((time_t)m_recvBuf[4]) - 2208988800U; // Unix time starts from 01/01/1970 == 2208988800U

        boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
        boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
        boost::posix_time::time_duration timeDiff = networkTime - localTime;

        clkReportResults(timeDiff.total_seconds());
    }
    m_udpSocket.cancel();
}

void VerifyClock::timerHandle(
    const boost::system::error_code& error)
{
    if (getResults() != NONE) return;
    m_udpSocket.cancel();
    m_timer.cancel();
    clkReportResults(0, true);
}

void VerifyClock::connectToNTPHost()
{
    boost::asio::ip::udp::resolver resolver(m_ioService);

    boost::asio::ip::udp::endpoint receiver_endpoint = *resolver.resolve(boost::asio::ip::udp::v4(),
                                                                         "pool.ntp.org", "ntp");

    m_udpSocket.open(boost::asio::ip::udp::v4());


    m_udpSocket.async_send_to(boost::asio::buffer(m_sendBuf), receiver_endpoint,
                              boost::bind(&VerifyClock::sockSendToHandle, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

/*
void VerifyClock::clkStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ConnectedState) {
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &VerifyClock::clkFinished);

        char NTPMessage[48] = {0x1b, 0, 0, 0, 0, 0, 0, 0, 0};

        m_udpSocket->write(NTPMessage, sizeof(NTPMessage));
    }
}*/

/*void VerifyClock::clkFinished()
{
    if (m_udpSocket->waitForReadyRead(10 * 1000)) {
        int64_t start_time = GetAdjustedTime();

        // Only allow this loop to run for 5 seconds maximum.
        while (m_udpSocket->hasPendingDatagrams() && GetAdjustedTime() - start_time <= 5) {
            QByteArray BufferSocket = m_udpSocket->readAll();

            if (BufferSocket.size() == 48) {
                int nNTPCount = 40;
                uint32_t DateTimeIn = uchar(BufferSocket.at(nNTPCount)) + (uchar(BufferSocket.at(nNTPCount + 1)) << 8) + (uchar(BufferSocket.at(nNTPCount + 2)) << 16) + (uchar(BufferSocket.at(nNTPCount + 3)) << 24);
                time_t tmit = ntohl(DateTimeIn) - 2208988800U;

                m_udpSocket->close();

                boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
                boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(tmit);
                boost::posix_time::time_duration timeDiff = networkTime - localTime;

                clkReportResults(timeDiff.total_seconds());

                return;
            }
        }
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
}*/

/*void VerifyClock::clkSocketError()
{
    m_udpSocket->close();

    clkReportResults(0, true);
}*/

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
        m_results = WARNING;
        m_results_tip = "Outbound communication to TCP port %1 appears to be blocked. ";
        m_results_string_arg.push_back(std::to_string(GetListenPort()));

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
        //case QAbstractSocket::SocketError::DatagramTooLargeError:
        case boost::asio::error::network_down:
        case boost::asio::error::no_permission:
        case boost::asio::error::address_in_use:
       // case QAbstractSocket::SocketError::SocketAddressNotAvailableError:
        case boost::asio::error::operation_not_supported:
       // case QAbstractSocket::SocketError::UnfinishedSocketOperationError:
            m_results = FAIL;

            // This does not include the common text above on purpose.
            m_results_tip = "The network has experienced a low-level error and this probably means your IP address or other "
                            "network connection parameters are not configured correctly. Please check your network configuration "
                            "on your computer.";

            break;

        //case QAbstractSocket::SocketError::ProxyAuthenticationRequiredError:
        //case QAbstractSocket::SocketError::ProxyConnectionRefusedError:
        //case QAbstractSocket::SocketError::ProxyConnectionClosedError:
        //case QAbstractSocket::SocketError::ProxyConnectionTimeoutError:
        //case QAbstractSocket::SocketError::ProxyNotFoundError:
        //case QAbstractSocket::SocketError::ProxyProtocolError:
        //    m_results = FAIL;

       //     m_results_tip += "Your network may be using a proxy server to communicate to public IP addresses on the Internet, and "
                             "the wallet is not configured properly to use it. Please check the proxy settings under Options -> "
       //                      "Network -> Connect through SOCKS5 proxy.";

        //    break;

        // SSL errors will NOT be triggered unless we implement a test and site to actually do an SSL connection test. This
        // is put here for completeness.
        //case QAbstractSocket::SocketError::SslHandshakeFailedError:
        //case QAbstractSocket::SocketError::SslInternalError:
        //case QAbstractSocket::SocketError::SslInvalidUserDataError:
            m_results = FAIL;

            // This does not include the common text above on purpose.
            m_results_tip = "The network is reporting an SSL error. If you also failed or got a warning on your clock test, you "
                            "should check your clock settings, including your time and time zone. If your clock is ok, please "
                            "check your computer's network configuration.";

            break;

        //case QAbstractSocket::SocketError::TemporaryError:
        //case QAbstractSocket::SocketError::UnknownSocketError:
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
