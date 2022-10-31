// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_DIAGNOSE_H
#define GRIDCOIN_WALLET_DIAGNOSE_H

#include "gridcoin/beacon.h"
#include "gridcoin/boinc.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/upgrade.h"
#include "main.h"
#include "net.h"
#include "util.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iomanip>
#include <numeric>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

extern std::atomic<int64_t> g_nTimeBestReceived;
extern std::unique_ptr<GRC::Upgrade> g_UpdateChecker;

class Researcher;
/*
 * This class monitors the tests, upon construction, it will register the test
 * upon destructor, it will mark the test complete
 * if all tests are complete, it will clean the map
 */

namespace DiagnoseLib {

/**
 * This is the base class for all diagnostics than can be run
 * m_results: an enum to indicate warning, failed, or passed test
 * Note: Each derived class must declare its unique name using the member m_test_name,
 * the m_test_name will be used to check the test status
 **/
class Diagnose
{
public:
    static bool m_researcher_mode;
    static bool m_hasEligibleProjects;
    static bool m_hasPoolProjects;
    static bool m_configured_for_investor_mode;

    enum diagnoseResults { PASS,
                           WARNING,
                           FAIL,
                           NONE };
    enum TestNames {
        CheckConnectionCount, // This must remain first, because other tests depend on it.
        CheckOutboundConnectionCount,
        VerifyWalletIsSynced,
        CheckClientVersion,
        VerifyBoincPath,
        VerifyCPIDHasRAC,
        VerifyCPIDIsActive,
        VerifyCPIDValid,
        VerifyClock,
        VerifyTCPPort,
        CheckDifficulty,
        CheckETTS,
        TestSize // add any new test before this entry
    };

    explicit Diagnose(TestNames test)
    {
        m_results = NONE;
        m_results_string = "";
        m_test_name = test;

        registerTest(this);
    }
    virtual ~Diagnose()
    {
        LOCK(cs_diagnostictests);
        removeTestFromMap(m_test_name);
    }

    /**
     *  runCheck(): calls the function will run the test
     */
    virtual void runCheck() = 0;
    /**
     * Get the result of test enum name
     */
    virtual TestNames getTestName() { return m_test_name; }

    /**
     * Get the result of test , Fail, Warning, or Pass
     */
    virtual diagnoseResults getResults() { return m_results; }
    /** Get the string containing a tip for the test being done
     */
    virtual std::string getResultsTip() { return m_results_tip; }
    /**
     * Get the final result string
     */
    virtual std::string getResultsString() { return m_results_string; }
    /**
     * The result can contains arguments using "$1". The functions returns the strings that should replace the argument
     */
    virtual std::vector<std::string> getStringArgs() { return m_results_string_arg; }
    /**
     * The Tip can contains arguments using "$1". The functions returns the strings that should replace the argument
     */
    virtual std::vector<std::string> getTipArgs() { return m_results_tip_arg; }
    /**
     * Register a running test to the map so we can check if it is running or not.
     */

    static void registerTest(Diagnose* test)
    {
        LOCK(cs_diagnostictests);

        m_name_to_test_map[test->m_test_name] = test;

        assert(m_name_to_test_map.size() <= Diagnose::TestSize);
    };
    /**
     * Set the research mode the wallet is running
     */
    static void setResearcherModel()
    {
        GRC::ResearcherPtr researcher = GRC::Researcher::Get();
        bool configured_for_investor_mode = false;
        if (GRC::Researcher::ConfiguredForInvestorMode()) {
            configured_for_investor_mode = true;
        }

        m_hasEligibleProjects = researcher->Id().Which() == GRC::MiningId::Kind::CPID;
        m_hasPoolProjects = researcher->Projects().ContainsPool();
        m_researcher_mode = !(configured_for_investor_mode || (!m_hasEligibleProjects && m_hasPoolProjects));
    }

    /**
     * Get a pointer to the test object if it exists
     * return nullptr if no test available
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
    /**
     * remove the test from the global map so it can not be tracked any more
     * make sure to call this function in the destructor of any test
     */
    static void removeTestFromMap(Diagnose::TestNames test_name)
    {
        LOCK(cs_diagnostictests);
        m_name_to_test_map.erase(test_name);
    }


protected:
    diagnoseResults m_results;                                                    //!< contains the final result of the test
    std::string m_results_tip;                                                    //!< A tip string that can be shown to the user in UI or the console
    std::string m_results_string;                                                 //!< The final string that contains description of the result
    std::vector<std::string> m_results_string_arg;                                //!< Some time the m_results_string can contain args to be substituted, like a number or string.
    std::vector<std::string> m_results_tip_arg;                                   //!< the data to be substituted in the tip string, similar to m_results_string_arg
    static CCriticalSection cs_diagnostictests;                                   //!< used to protect the critical sections, for multithreading
    TestNames m_test_name;                                                        //!< This must be defined for derived classes. Each derived class must declare the name of the test and add to the TestNames enum
    static std::unordered_map<Diagnose::TestNames, Diagnose*> m_name_to_test_map; //!< a map to save the test and a pointer to it. Some tests are related and need to access the results of each other.
    static boost::asio::io_service s_ioService;
};

/**
 * Diagnose class to check if wallet is synced and up to date
 */
class VerifyWalletIsSynced : public Diagnose
{
public:
    VerifyWalletIsSynced() : Diagnose {Diagnose::VerifyWalletIsSynced}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

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

/**
 * Diagnose class to check the number of connections to outer nodes
 */
class CheckOutboundConnectionCount : public Diagnose
{
public:
    CheckOutboundConnectionCount() : Diagnose {Diagnose::CheckOutboundConnectionCount}
    {
    }

    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        int outbound_connections = 0;

        {
            LOCK(cs_vNodes);

            for (const auto& vnodes : vNodes) {
                if (!vnodes->fInbound) ++outbound_connections;
            }
        }

        if (outbound_connections < 1) {
            m_results_tip = "Your outbound connection count is critically low. Please check your the config file and "
                            "ensure your addnode entries are up-to-date. If you recently started the wallet, you may "
                            "want to wait another few minutes for connections to build up and then test again. Please see "
                            "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.";
            m_results_string = "Failed: Count = %1";
            m_results = Diagnose::Diagnose::FAIL;

            std::string ss = ToString(outbound_connections);
            m_results_string_arg.push_back(ss);

        } else if (outbound_connections < 3) {
            m_results_tip = "Your outbound connection count is low. Please check your the config file and "
                            "ensure your addnode entries are up-to-date. If you recently started the wallet, you may "
                            "want to wait another few minutes for connections to build up and then test again. Please see "
                            "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.";
            m_results = Diagnose::WARNING;
        } else {
            m_results_tip = "";
            m_results_string = "Passed: Count = %1";
            std::string ss = ToString(outbound_connections);
            m_results_string_arg.push_back(ss);
            m_results = Diagnose::PASS;
        }
    }
};

/**
 * Diagnose class to check if number of connections is not very low
 */
class CheckConnectionCount : public Diagnose
{
public:
    CheckConnectionCount() : Diagnose {Diagnose::CheckConnectionCount}, m_connections(0)
    {
    }

    size_t getConnectionsNum() { return m_connections; }

    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        {
            LOCK(cs_vNodes);
            m_connections = vNodes.size();
        }

        size_t minimum_connections_to_stake = fTestNet ? 1 : 3;
        std::string s_connections = ToString(m_connections);

        if (m_connections <= 7 && m_connections >= minimum_connections_to_stake) {
            m_results_tip = "Please check your network and also check the config file and ensure your addnode entries "
                            "are up-to-date. If you recently started the wallet, you may want to wait another few "
                            "minutes for connections to build up and test again. Please see "
                            "https://gridcoin.us/wiki/config-file.html and https://addnodes.cycy.me/.";
            m_results = Diagnose::WARNING;
            m_results_string = "Warning: Count = %1 (Pass = 8+)";
            m_results_string_arg.push_back(s_connections);
        } else if (m_connections >= 8) {
            m_results_tip = "";
            m_results_string = "Warning: Count = %1";
            m_results_string_arg.push_back(s_connections);
            m_results = Diagnose::PASS;

        } else {
            m_results_tip = "You will not be able to stake because you have less than %1 connection(s). Please check "
                            "your network and also check the config file and ensure your addnode entries are up-to-date. "
                            "If you recently started the wallet, you may want to wait another few minutes for connections "
                            "to build up and then test again. Please see https://gridcoin.us/wiki/config-file.html and "
                            "https://addnodes.cycy.me/.";
            m_results = Diagnose::FAIL;
            m_results_string = "Warning: Count = %1";
            m_results_string_arg.push_back(s_connections);
            m_results_tip_arg.push_back(ToString(minimum_connections_to_stake));
        }
    }

private:
    size_t m_connections;
};

/**
 * Diagnose class to check number of connection counts
 */
class VerifyClock : public Diagnose
{
private:
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::deadline_timer m_timer;
    boost::array<unsigned char, 48> m_sendBuf = {0x1b, 0, 0, 0, 0, 0, 0, 0, 0};
    boost::array<unsigned char, 1024> m_recvBuf;
    bool m_startedTesting = false;

    void clkReportResults(const int64_t& time_offset, const bool& timeout_during_check = false);
    void sockRecvHandle(const boost::system::error_code& error, std::size_t bytes_transferred);
    void sockSendToHandle(const boost::system::error_code& error, std::size_t bytes_transferred);
    void timerHandle(const boost::system::error_code& error);
    void connectToNTPHost();

public:
    VerifyClock() : Diagnose {Diagnose::VerifyClock}, m_udpSocket(s_ioService), m_timer(s_ioService)
    {
    }
    ~VerifyClock() {}
    void runCheck()
    {
        class CheckConnectionCount* CheckConnectionCount_Test =
                static_cast<class CheckConnectionCount*>(getTest(Diagnose::CheckConnectionCount));

        if (CheckConnectionCount_Test && CheckConnectionCount_Test->getConnectionsNum() >= 5) {
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
        s_ioService.reset();
        s_ioService.run();
    }
};


/**
 * Diagnose class to check the version of the wallet
 */
class CheckClientVersion : public Diagnose
{
public:
    CheckClientVersion() : Diagnose {Diagnose::CheckClientVersion}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

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
    VerifyBoincPath() : Diagnose {Diagnose::VerifyBoincPath}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;

            return;
        }

        // This is now similar to ReadClientStateXml in researcher.
        fs::path boincPath = GRC::GetBoincDataDir();
        std::string contents;

        bool access_error = false;

        try {
            contents = GetFileContents(boincPath / "client_state.xml");
        } catch (boost::filesystem::filesystem_error& e) {
            error("%s: %s", __func__, e.what());
            access_error = true;
        }

        if (!access_error) {
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
};

/**
 * If research mode, then this diagnose class checks the validity of the Boinc CPID
 */
class VerifyCPIDValid : public Diagnose
{
public:
    VerifyCPIDValid() : Diagnose {Diagnose::VerifyCPIDValid}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;
            return;
        }

        if (m_hasEligibleProjects) {
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

/**
 * Check if the CPID has RAC
 */
class VerifyCPIDHasRAC : public Diagnose
{
public:
    VerifyCPIDHasRAC() : Diagnose {Diagnose::VerifyCPIDHasRAC}
    {
    }

    bool hasActiveBeacon()
    {
        /**
         * find if there is Active beacon
         */

        const GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();
        const GRC::CpidOption cpid = GRC::Researcher::Get()->Id().TryCpid();
        if (const GRC::BeaconOption beacon = beacons.Try(*cpid)) {
            if (!beacon->Expired(GetAdjustedTime())) {
                return true;
            }
            for (const auto& beacon_ptr : beacons.FindPending(*cpid)) {
                if (!beacon_ptr->Expired(GetAdjustedTime())) {
                    return true;
                }
            }
        }
        return false;
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;
            return;
        }

        if (hasActiveBeacon()) {
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
    VerifyCPIDIsActive() : Diagnose {Diagnose::VerifyCPIDIsActive}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results_tip = "";
            m_results_string = "";
            m_results = NONE;
            return;
        }

        if (GRC::Researcher::Get()->HasRAC()) {
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

/**
 * Diagnose class to verify correct tcp ports are open
 */
class VerifyTCPPort : public Diagnose
{
private:
    boost::asio::ip::tcp::socket m_tcpSocket;
    void handle_connect(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);

    void TCPFinished();

public:
    VerifyTCPPort() : Diagnose {Diagnose::VerifyTCPPort}, m_tcpSocket(s_ioService)
    {
    }
    ~VerifyTCPPort() {}
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        auto CheckConnectionCount_Test = getTest(Diagnose::CheckConnectionCount);
        if (CheckConnectionCount_Test && CheckConnectionCount_Test->getResults() != Diagnose::NONE && CheckConnectionCount_Test->getResults() != Diagnose::FAIL) {
            m_results = Diagnose::PASS;
            return;
        }

        boost::asio::ip::tcp::resolver resolver(s_ioService);
#if BOOST_VERSION > 106501
        auto endpoint_iterator = resolver.resolve("portquiz.net", "http");
#else
        boost::asio::ip::tcp::resolver::query query(
            "portquiz.net",
            "http");
        auto endpoint_iterator = resolver.resolve(query);
#endif

        if (endpoint_iterator == boost::asio::ip::tcp::resolver::iterator()) {
            m_tcpSocket.close();
            m_results = WARNING;
            m_results_tip = "Outbound communication to TCP port %1 appears to be blocked. ";
            std::string ss = ToString(GetListenPort());
            m_results_string_arg.push_back(ss);
        } else {
            // Attempt a connection to the first endpoint in the list. Each endpoint
            // will be tried until we successfully establish a connection.
            boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
            if (m_tcpSocket.is_open())
                m_tcpSocket.close();

            m_tcpSocket.async_connect(endpoint,
                                      boost::bind(&VerifyTCPPort::handle_connect, this,
                                                  boost::asio::placeholders::error, (++endpoint_iterator)));

            s_ioService.reset();
            s_ioService.run();
        }
    }
};

/**
 * Diagnose class to check if stacking difficulty is not very low or very high
 */
class CheckDifficulty : public Diagnose
{
public:
    CheckDifficulty() : Diagnose {Diagnose::CheckDifficulty}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

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
            std::string ss = ToString(fail_diff);
            m_results_string_arg.push_back(ss);

            m_results_tip = "Your difficulty is low but your wallet is still in initial sync. Please recheck it later "
                            "to see if this passes.";
            m_results = Diagnose::WARNING;
        }
        // If the wallet has been in sync in the past in this run, then apply the normal standards, whether the wallet is
        // in sync or not right now.
        else if (g_nTimeBestReceived > 0 && diff < fail_diff) {
            m_results_string = "Failed: 80 block difficulty is less than %1. This wallet is almost certainly forked.";

            std::string ss = ToString(fail_diff);
            m_results_string_arg.push_back(ss);

            m_results_tip = "Your difficulty is extremely low and your wallet is almost certainly forked. Please ensure "
                            "you are running the latest version and try removing the blockchain database and resyncing "
                            "from genesis using the menu option. (Note this will take 2-4 hours.)";
            m_results = Diagnose::FAIL;
        } else if (g_nTimeBestReceived > 0 && diff < warn_diff) {
            m_results_string = "Warning: 80 block difficulty is less than %1. This wallet is probably forked.";
            std::string ss = ToString(warn_diff);
            m_results_string_arg.push_back(ss);

            m_results_tip = "Your difficulty is very low and your wallet is probably forked. Please ensure you are "
                            "running the latest version and try removing the blockchain database and resyncing from "
                            "genesis using the menu option. (Note this will take 2-4 hours.)";
            m_results = Diagnose::WARNING;
        } else {
            m_results_string = "Passed: 80 block difficulty is %1.";
            std::string ss = ToString(diff);
            m_results_string_arg.push_back(ss);
            m_results = Diagnose::PASS;
        }
    }
};
/**
 * check ETTS
 * This is only checked if wallet is a researcher wallet because the purpose is to
 * alert the owner that his stake time is too long and therefore there is a chance
 * of research rewards loss between stakes due to the 180 day limit.
 */
class CheckETTS : public Diagnose
{
public:
    CheckETTS() : Diagnose {Diagnose::CheckETTS}
    {
    }
    void runCheck()
    {
        m_results_string_arg.clear();
        m_results_tip_arg.clear();

        // This test is only applicable if the wallet is in researcher mode.
        if (!m_researcher_mode) {
            m_results = Diagnose::NONE;
            return;
        }

        double diff;
        {
            LOCK(cs_main);
            diff = GRC::GetAverageDifficulty(80);
        }

        double ETTS = GRC::GetEstimatedTimetoStake(true, diff) / (24.0 * 60.0 * 60.0);
        std::string rounded_ETTS;

        // round appropriately for display.
        if (ETTS >= 100) {
            rounded_ETTS = RoundToString(ETTS, 0);
        } else if (ETTS >= 10) {
            rounded_ETTS = RoundToString(ETTS, 1);
        } else {
            rounded_ETTS = RoundToString(ETTS, 2);
        }

        if (g_nTimeBestReceived == 0 && OutOfSyncByAge()) {
            m_results_tip = "Your wallet is not in sync and has not previously been in sync during this run, please "
                            "wait for the wallet to sync and retest. If there are other failures preventing the "
                            "wallet from syncing, please correct those items and retest to see if this test passes.";
            m_results = Diagnose::WARNING;
        } else {
            // ETTS of zero actually means no coins, i.e. infinite.
            if (ETTS == 0.0) {
                m_results_tip = "You have no balance and will be unable to retrieve your research rewards when solo "
                                "crunching by staking. You can use MRC to retrieve your rewards, or you should "
                                "acquire GRC to stake so you can retrieve your research rewards. "
                                "Please see https://gridcoin.us/guides/boinc-install.htm.";
                m_results_string = "Warning: ETTS is infinite. No coins to stake - increase balance or use MRC";
                m_results = Diagnose::WARNING;
            } else if (ETTS > 90.0) {
                m_results_tip = "Your balance is too low given the current network difficulty to stake in a reasonable "
                                "period of time to retrieve your research rewards when solo crunching. You can use MRC "
                                " to retrieve your rewards, or you should acquire more GRC to stake more often.";
                m_results_string = "Warning: ETTS is > 90 days. It will take a very long time to receive your research "
                                   "rewards by staking - increase balance or use MRC";
                m_results = Diagnose::WARNING;
            } else if (ETTS > 45.0 && ETTS <= 90.0) {
                m_results_tip = "Your balance is low given the current network difficulty to stake in a reasonable "
                                "period of time to retrieve your research rewards when solo crunching. You should consider "
                                "acquiring more GRC to stake more often, or else use MRC to retrieve your rewards.";
                m_results_string = "Warning: 45 days < ETTS = %1 <= 90 days";
                m_results = Diagnose::WARNING;
            } else {
                m_results_string = "Passed: ETTS = %1 <= 45 days";
                m_results_string_arg.push_back(rounded_ETTS);
                m_results = Diagnose::PASS;
            }
        }
    }
};
}; // namespace DiagnoseLib
#endif
