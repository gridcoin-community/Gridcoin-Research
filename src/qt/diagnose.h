
#include "fs.h"
#include "gridcoin/boinc.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/upgrade.h"
#include "main.h"
#include "net.h"
#include "qt/researcher/researchermodel.h"
#include "util.h"
#include <QAbstractSocket>
#include <QHostInfo>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iomanip>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>


extern std::atomic<int64_t> g_nTimeBestReceived;
extern std::unique_ptr<GRC::Upgrade> g_UpdateChecker;

#ifndef BITCOIN_DIAGNOSTICSLIB_H
#define BITCOIN_DIAGNOSTICSLIB_H

/*
 * This class monitors the tests, upon construction, it will register the test
 * upon destructor, it will mark the test complete
 * if all tests are complete, it will clean the map
 */

namespace DiagnoseLib {

/*
 * This is the base class for all diagnostics than can be run
 * m_results: an enum to indicate warning, failed, or passed test
 * Note: Each derived class must declare its unique name using he member m_test_name,
 * the m_test_name will be used to check the test status
 */
class Diagnose
{
public:
    static bool m_researcher_mode;
    static const ResearcherModel* m_researcher_model;

    enum diagnoseResults { PASS,
                           WARNING,
                           FAIL,
                           NONE };
    enum TestNames {
        VerifyWalletIsSynced,
        CheckClientVersion,
        CheckConnectionCount,
        CheckOutboundConnectionCount,
        VerifyBoincPath,
        VerifyCPIDHasRAC,
        VerifyCPIDIsActive,
        VerifyCPIDValid,
        VerifyClock,
        VerifyTCPPort,
        CheckDifficulty,
        TestSize // add any new test before this entry
    };

    Diagnose()
    {
        m_results = NONE;
        m_results_string = "";
        m_researcher_mode = false;
        registerTest(this);
    }
    virtual ~Diagnose()
    {
        LOCK(cs_diagnostictests);
        removeTestFromMap(m_test_name);
    }

    /* runCheck(): calling the function will run the test*/
    virtual void runCheck() = 0;
    /*Get teh result of test , Fail, Warning, or PAss*/
    virtual diagnoseResults getResults() { return m_results; }
    /*Return a string containing a tip for the test being done*/
    virtual std::string getResultsTip() { return m_results_tip; }
    /*return the final result string*/
    virtual std::string getResultsString() { return m_results_string; }
    /*The result can contains arguments using "$1". The functions returns the strings that should replace the argument*/
    virtual std::vector<std::string> getStringArgs() { return m_results_string_arg; }
    /*The Tip can contains arguments using "$1". The functions returns the strings that should replace the argument*/
    virtual std::vector<std::string> getTipArgs() { return m_results_tip_arg; }
    /*Register a running test to the map so we can check if it is runnin or not.*/
    static void registerTest(Diagnose* test)
    {
        LOCK(cs_diagnostictests);
        m_name_to_test_map[test->m_test_name] = test;
        assert(m_name_to_test_map.size() < Diagnose::TestSize);
    };
    /*Set the research mode the wallet is running */
    static void setResearcherModel(ResearcherModel* model)
    {
        m_researcher_model = model;
        m_researcher_mode = !(m_researcher_model->configuredForInvestorMode() || m_researcher_model->detectedPoolMode());
    }
    /*Get the research mode used during testing */
    static bool getResearcherModel() { return m_researcher_model; }

    /*Get a pointer to the test object if it exists
     * retun nullptr if no test available
     */
    static Diagnose* getTest(Diagnose::TestNames test_name)
    {
        LOCK(cs_diagnostictests);
        auto entry = m_name_to_test_map.find(test_name);
        if (entry != m_name_to_test_map.end()) {
            return entry->second;
        } else {
            return nullptr;
        }
    }
    // remove the test from the globa map so it can not be tracked any more
    // make sure to call this function in the destructor of any test
    static void removeTestFromMap(Diagnose::TestNames test_name)
    {
        LOCK(cs_diagnostictests);
        m_name_to_test_map.erase(test_name);
    }


protected:
    diagnoseResults m_results;
    std::string m_results_tip;
    std::string m_results_string;
    std::vector<std::string> m_results_string_arg;
    std::vector<std::string> m_results_tip_arg;
    static CCriticalSection cs_diagnostictests;
    TestNames m_test_name;
    static std::unordered_map<Diagnose::TestNames, Diagnose*> m_name_to_test_map;
};

/*
 * Diagnose class to check if wallet is synced and up to date
 */
class VerifyWalletIsSynced : public Diagnose
{
public:
    VerifyWalletIsSynced()
    {
        m_test_name = Diagnose::VerifyWalletIsSynced;
    }
    void runCheck()
    {
        if (g_nTimeBestReceived == 0 && OutOfSyncByAge()) {
            m_results = Diagnose::WARNING;
            m_results_tip = "Your wallet is still in initial sync. If this is a sync from the beginning (genesis), the "
                            "sync process can take from 2 to 4 hours, or longer on a slow computer. If you have synced "
                            "your wallet before but you just started the wallet up, then wait a few more minutes and "
                            "retry the diagnostics again.";
        } else if (g_nTimeBestReceived > 0 && OutOfSyncByAge()) {
            m_results = Diagnose::FAIL;
            m_results_tip = "Your wallet is out of sync with the network but was in sync before. If this fails there is "
                            "likely a severe problem that is preventing the wallet from syncing. If the lack of sync "
                            "is due to network connection issues, you will see failures on the network connection "
                            "test(s). If the network connections pass, but your wallet fails this test, and continues to "
                            "fail this test on repeated attempts with a few minutes in between, this could indicate a "
                            "more serious issue. In that case you should check the debug log to see if it sheds light "
                            "on the cause for no sync.";
        } else {
            m_results = Diagnose::PASS;
            m_results_tip = "Passed";
        }
        m_results_string = "";
    }
    ~VerifyWalletIsSynced()
    {
    }
};

/*
 * Diagnose class to check the number of connections to outer nodes
 */
class CheckOutboundConnectionCount : public Diagnose
{
public:
    CheckOutboundConnectionCount()
    {
        m_test_name = Diagnose::CheckOutboundConnectionCount;
    }
    void runCheck()
    {
        LOCK(cs_vNodes);

        int outbound_connections = 0;

        for (const auto& vnodes : vNodes) {
            if (!vnodes->fInbound) ++outbound_connections;
        }

        if (outbound_connections < 1) {
            m_results_tip = "Your outbound connection count is critically low. Please check your the config file and "
                            "ensure your addnode entries are up-to-date. If you recently started the wallet, you may "
                            "want to wait another few minutes for connections to build up and then test again. Please see "
                            "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.";
            m_results_string = "Failed: Count = %1";
            m_results = Diagnose::Diagnose::FAIL;
            m_results_string_arg.push_back(std::to_string(outbound_connections));

        } else if (outbound_connections < 3) {
            m_results_tip = "Your outbound connection count is low. Please check your the config file and "
                            "ensure your addnode entries are up-to-date. If you recently started the wallet, you may "
                            "want to wait another few minutes for connections to build up and then test again. Please see "
                            "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.";
            m_results = Diagnose::WARNING;
        } else {
            m_results_tip = "";
            m_results_string = "Passed: Count = %1";
            m_results_string_arg.push_back(std::to_string(outbound_connections));
            m_results = Diagnose::PASS;
        }
    }
};

/*
 * Diagnose class to check if number of connections is not very low
 */
class CheckConnectionCount : public Diagnose
{
protected:
    size_t m_connections;

public:
    CheckConnectionCount()
    {
        m_connections = 0;
        {
            LOCK(cs_vNodes);
            m_connections = vNodes.size();
        }

        m_test_name = Diagnose::CheckConnectionCount;
    }
    size_t getConnectionsNum() { return m_connections; }
    void runCheck()
    {
        size_t minimum_connections_to_stake = fTestNet ? 1 : 3;

        if (m_connections <= 7 && m_connections >= minimum_connections_to_stake) {
            m_results_tip = "Please check your network and also check the config file and ensure your addnode entries "
                            "are up-to-date. If you recently started the wallet, you may want to wait another few "
                            "minutes for connections to build up and test again. Please see "
                            "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.";
            m_results = Diagnose::WARNING;
            m_results_string = "Warning: Count = %1 (Pass = 8+)";
            m_results_string_arg.push_back(std::to_string(m_connections));
        } else if (m_connections >= 8) {
            m_results_tip = "";
            m_results_string = "Warning: Count = %1";
            m_results_string_arg.push_back(std::to_string(m_connections));
            m_results = Diagnose::PASS;

        } else {
            m_results_tip = "You will not be able to stake because you have less than %1 connection(s). Please check "
                            "your network and also check the config file and ensure your addnode entries are up-to-date. "
                            "If you recently started the wallet, you may want to wait another few minutes for connections "
                            "to build up and then test again. Please see https://gridcoin.us/wiki/config-file.html and "
                            "https://addnodes.cycy.me/.";
            m_results = Diagnose::FAIL;
            m_results_string = "Warning: Count = %1";
            m_results_string_arg.push_back(std::to_string(minimum_connections_to_stake));
        }
    }
};

/*
 * Diagnose class to check number of connection counts
 */
class VerifyClock : public CheckConnectionCount
{
    
private:
    // QUdpSocket *m_udpSocket;

    boost::asio::io_service m_ioService;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::deadline_timer m_timer;
    boost::array<unsigned char, 48> m_sendBuf = {010, 0, 0, 0, 0, 0, 0, 0, 0};
    boost::array<unsigned char, 1024> m_recvBuf;

    void clkReportResults(const int64_t& time_offset, const bool& timeout_during_check = false);
    void sockRecvHandle(const boost::system::error_code& error, std::size_t bytes_transferred);
    void sockSendToHandle(const boost::system::error_code& error, std::size_t bytes_transferred);
    void timerHandle(const boost::system::error_code& error);
    void connectToNTPHost();

public:
    VerifyClock() : m_ioService(), m_udpSocket(m_ioService), m_timer(m_ioService)
    {
        m_test_name = Diagnose::VerifyClock;
    }
    ~VerifyClock() {}
    void runCheck()
    {
        if (m_connections >= 5) {
            int64_t time_offset = 0;

            {
                LOCK(cs_main);
                time_offset = GetTimeOffset();
            }
            clkReportResults(time_offset);
        } else {
            m_timer.expires_from_now(boost::posix_time::seconds(10));
            m_timer.async_wait(boost::bind(&VerifyClock::timerHandle, this, boost::asio::placeholders::error));
            connectToNTPHost();
        }
    }
};


/*
 * Diagnose class to check the version of the wallet
 */
class CheckClientVersion : public Diagnose
{
public:
    CheckClientVersion()
    {
        m_test_name = Diagnose::CheckClientVersion;
    }
    void runCheck()
    {
        std::string client_message;

        if (g_UpdateChecker->CheckForLatestUpdate(client_message, false) && client_message.find("mandatory") != std::string::npos) {
            m_results_tip = "There is a new mandatory version available and you should upgrade as soon as possible to "
                            "ensure your wallet remains in consensus with the network.";
            m_results = Diagnose::FAIL;

        } else if (g_UpdateChecker->CheckForLatestUpdate(client_message, false) && client_message.find("mandatory") == std::string::npos) {
            m_results_tip = "There is a new leisure version available and you should upgrade as soon as practical.";
            m_results = Diagnose::WARNING;
        } else {
            m_results_tip = "";
            m_results = Diagnose::PASS;
        }
    }
};

class VerifyBoincPath : public Diagnose
{
public:
    VerifyBoincPath()
    {
        m_test_name = Diagnose::VerifyBoincPath;
    }
    void runCheck()
    {
        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;

            return;
        }


        fs::path boincPath = (fs::path)GRC::GetBoincDataDir();

        if (boincPath.empty()) {
            boincPath = (fs::path)gArgs.GetArg("-boincdatadir", "");

            boincPath = boincPath / "client_state.xml";

            if (fs::exists(boincPath)) {
                m_results_tip = "";
                m_results_string = "";
                m_results = Diagnose::PASS;
            } else {
                m_results_tip = "Check that BOINC is installed and that you have the correct path in the config file "
                                "if you installed it to a nonstandard location.";

                m_results_string = "";
                m_results = Diagnose::FAIL;
            }
        }
    }
};

/*
 * If research mode, then this diagnose class checks the validity of the Boinc CPIC
 */
class VerifyCPIDValid : public Diagnose
{
public:
    VerifyCPIDValid()
    {
        m_test_name = Diagnose::VerifyCPIDValid;
    }
    void runCheck()
    {
        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;
            return;
        }

        if (m_researcher_model && m_researcher_model->hasEligibleProjects()) {
            m_results = Diagnose::PASS;
        } else {
            if (g_nTimeBestReceived == 0 && OutOfSyncByAge()) {
                m_results_tip = "Your wallet is not in sync and has not previously been in sync during this run, please "
                                "wait for the wallet to sync and retest. If there are other failures preventing the "
                                "wallet from syncing, please correct those items and retest to see if this test passes.";
                m_results_string = "";
                m_results = Diagnose::WARNING;

            } else {
                m_results_tip = "Verify (1) that you have BOINC installed correctly, (2) that you have attached at least "
                                "one whitelisted project, (3) that you advertised your beacon with the same email as you "
                                "use for your BOINC project(s), and (4) that the CPID on the overview screen matches the "
                                "CPID when you login to your BOINC project(s) online.";
                m_results_string = "";
                m_results = Diagnose::FAIL;
            }
        }
    }
};

/*
 * Check if the CPID has RAC
 */
class VerifyCPIDHasRAC : public Diagnose
{
public:
    VerifyCPIDHasRAC()
    {
        m_test_name = Diagnose::VerifyCPIDHasRAC;
    }
    void runCheck()
    {
        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;
            return;
        }


        if (m_researcher_model->hasActiveBeacon()) {
            m_results = Diagnose::PASS;
            m_results_tip = "";
            m_results_string = "";

        } else {
            if (g_nTimeBestReceived == 0 && OutOfSyncByAge()) {
                m_results_tip = "Your wallet is not in sync and has not previously been in sync during this run, please "
                                "wait for the wallet to sync and retest. If there are other failures preventing the "
                                "wallet from syncing, please correct those items and retest to see if this test passes.";
                m_results_string = "";
                m_results = Diagnose::WARNING;

            } else {
                m_results_tip = "Please ensure that you have followed the process to advertise and verify your beacon. "
                                "You can use the research wizard (the beacon button on the overview screen).";
                m_results_string = "";
                m_results = Diagnose::FAIL;
            }
        }
    }
};

/*
 * Diagnose class to Check that CPID is active
 */
class VerifyCPIDIsActive : public Diagnose
{
public:
    VerifyCPIDIsActive()
    {
        m_test_name = Diagnose::VerifyCPIDIsActive;
    }
    void runCheck()
    {
        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;
            return;
        }


        if (m_researcher_model->hasRAC()) {
            m_results = Diagnose::PASS;
            m_results_tip = "";
            m_results_string = "";

        } else {
            if (g_nTimeBestReceived == 0 && OutOfSyncByAge()) {
                m_results_tip = "Your wallet is not in sync and has not previously been in sync during this run, please "
                                "wait for the wallet to sync and retest. If there are other failures preventing the "
                                "wallet from syncing, please correct those items and retest to see if this test passes.";
                m_results_string = "";
                m_results = Diagnose::WARNING;
            } else {
                m_results_tip = "Verify that you have actually completed workunits for the projects you have attached and "
                                "that you have authorized the export of statistics. Please see "
                                "https://gridcoin.us/guides/whitelist.htm.";
                m_results_string = "";
                m_results = Diagnose::FAIL;
            }
        }
    }
};

/*
 * Diagnose class to verify correct tcp ports are open
 */
class VerifyTCPPort : public Diagnose, public QObject
{
private:
    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::socket m_tcpSocket;
    void handle_connect(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);

    void TCPFinished();
    
public:
    VerifyTCPPort() : m_tcpSocket(m_ioService)
    {
        m_test_name = Diagnose::VerifyTCPPort;
    }
    ~VerifyTCPPort() {}
    void runCheck()
    {
        auto CheckConnectionCount_Test = getTest(Diagnose::CheckConnectionCount);
        if (CheckConnectionCount_Test && CheckConnectionCount_Test->getResults() != Diagnose::NONE && CheckConnectionCount_Test->getResults() != Diagnose::FAIL) {
            m_results = Diagnose::PASS;
            return;
        }

        boost::asio::ip::tcp::resolver resolver(m_ioService);
        boost::asio::ip::tcp::resolver::results_type endpoint_iterator = resolver.resolve("portquiz.net", "http");

        // Attempt a connection to the first endpoint in the list. Each endpoint
        // will be tried until we successfully establish a connection.
        boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
        m_tcpSocket.async_connect(endpoint,
                                  boost::bind(&VerifyTCPPort::handle_connect, this,
                                              boost::asio::placeholders::error, (++endpoint_iterator)));
    }
};

/*
 * Diagnose class to check if stacking difficulty is not very low or very high
 */
class CheckDifficulty : public Diagnose
{
public:
    CheckDifficulty()
    {
        m_test_name = Diagnose::CheckDifficulty;
    }
    void runCheck()
    {
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
        if (g_nTimeBestReceived == 0 && OutOfSyncByAge() && diff < fail_diff) {
            m_results_string = "Warning: 80 block difficulty is less than %1.";
            m_results_string_arg.push_back(std::to_string(fail_diff));

            m_results_tip = "Your difficulty is low but your wallet is still in initial sync. Please recheck it later "
                            "to see if this passes.";
            m_results = Diagnose::WARNING;
        }
        // If the wallet has been in sync in the past in this run, then apply the normal standards, whether the wallet is
        // in sync or not right now.
        else if (g_nTimeBestReceived > 0 && diff < fail_diff) {
            m_results_string = "Failed: 80 block difficulty is less than %1. This wallet is almost certainly forked.";
            m_results_string_arg.push_back(std::to_string(fail_diff));

            m_results_tip = "Your difficulty is extremely low and your wallet is almost certainly forked. Please ensure "
                            "you are running the latest version and try removing the blockchain database and resyncing "
                            "from genesis using the menu option. (Note this will take 2-4 hours.)";
            m_results = Diagnose::FAIL;
        } else if (g_nTimeBestReceived > 0 && diff < warn_diff) {
            m_results_string = "Warning: 80 block difficulty is less than %1. This wallet is probably forked.";
            m_results_string_arg.push_back(std::to_string(warn_diff));

            m_results_tip = "Your difficulty is very low and your wallet is probably forked. Please ensure you are "
                            "running the latest version and try removing the blockchain database and resyncing from "
                            "genesis using the menu option. (Note this will take 2-4 hours.)";
            m_results = Diagnose::WARNING;
        } else {
            m_results_string = "Passed: 80 block difficulty is %1.";
            m_results_string_arg.push_back(std::to_string(diff));
            m_results = Diagnose::PASS;
        }
    }
};

}; // namespace DiagnoseLib
#endif
