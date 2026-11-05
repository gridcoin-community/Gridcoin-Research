// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/contract/registry.h"
#include "init.h"
#include "node/shutdown.h"
#include "policy/fees.h"
#include "test/test_gridcoin.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32
#include <csignal>
#endif

namespace {
//! ReadConfigStream is protected, so expose it the way the other argument tests do.
class TestArgsManager : public ArgsManager
{
public:
    using ArgsManager::ReadConfigStream;
};

void RegisterArgs(ArgsManager& args)
{
    for (const char* a : {"-guilogfile=<f>", "-autotrustidentity", "-ipclogtrace", "-staking", "-resetblockchaindata", "-datadir=<dir>"}) {
        args.AddArg(a, "", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    }
}

bool Contains(const std::vector<std::string>& v, const std::string& key)
{
    return std::find(v.begin(), v.end(), key) != v.end();
}
} // namespace

BOOST_AUTO_TEST_SUITE(init_tests)

BOOST_AUTO_TEST_CASE(every_registered_argument_has_its_settings_class)
{
    gArgs.ClearArgs();
    struct ClearOnExit { ~ClearOnExit() { gArgs.ClearArgs(); } } clear_on_exit;
    SetupServerArgs();

    const std::set<std::string> free{
        "-addnode", "-addrlifespan", "-banscore", "-bantime", "-blockmaxsize", "-blockminsize", "-checkblocks",
        "-checklevel", "-coherencewalkmax", "-confchange", "-connect", "-contractchangetoinputaddress",
        "-dbcache", "-dblogsize", "-debug", "-debugexclude", "-deleteoldlogarchives", "-disableupdatecheck",
        "-email", "-enableaccounts", "-enablestakesplit", "-enforcecanonical", "-explorer", "-forcecpid",
        "-investor", "-keypool", "-logarchivedaily", "-logarchiveretainnumfiles", "-logips", "-logthreadnames",
        "-logtimemicros", "-logtimestamps", "-maxconnections", "-maxmempool", "-maxoutboundconnections",
        "-maxreceivebuffer", "-maxsendbuffer", "-maxsigcachesize", "-minersleep", "-mininput",
        "-minstakesplitvalue", "-mintxfee", "-noncruncher", "-org", "-peertimeout", "-persistmempool",
        "-pollexpirewarningtime", "-pooloperator", "-port", "-printtoconsole", "-printtodebugger",
        "-reservebalance", "-scraper", "-scraperkey", "-seednode", "-showorphans", "-shrinkdebugfile",
        "-staking", "-stakingefficiency", "-synctime", "-timeout", "-txindexdbcache", "-updatecheckinterval"};
    const std::set<std::string> elevate{
        "-?", "-bind", "-discover", "-dns", "-dnsseed", "-enablesidestaking", "-externalip", "-listen",
        "-onlynet", "-paytxfee", "-proxy", "-sidestake", "-sidestakeaddresses", "-sidestakeallocations",
        "-sidestakedescriptions", "-tor", "-upnp"};
    std::set<std::string> refuse{
        "-h", "-help", "-help-debug", "-version", "-conf", "-pid", "-datadir", "-blocksdir", "-nonewprivs",
        "-wallet", "-debuglogfile", "-chain", "-testnet", "-regtest", "-blocknotify", "-walletnotify",
        "-projectnotify", "-pollnotify", "-psgtnotify", "-alertnotify", "-startupnotify", "-shutdownnotify",
        "-updatecheckurl", "-resetblockchaindata", "-loadblock", "-reindex", "-settings", "-boincdatadir",
        "-rescan", "-rescanmrcrequests", "-salvagewallet", "-zapwallettxes", "-walletbackupinterval",
        "-walletbackupintervalsecs", "-backupdir", "-maintainbackupretention", "-walletbackupretainnumfiles",
        "-walletbackupretainnumdays", "-server", "-multiprocess", "-ipclogtrace", "-rpcuser", "-rpcwait",
        "-rpcwaittimeout", "-rpcpassword", "-rpcport", "-rpcallowip", "-rpcbind", "-rpcconnect", "-rpcthreads",
        "-rpcservertimeout", "-daemon", "-daemonwait", "-dbcrashratio", "-forcecompactdb", "-fastindex",
        "-autotrustidentity", "-choosedatadir", "-devsyntheticcoins", "-guilogfile", "-lang", "-min",
        "-nobuildwarn", "-resetguisettings", "-splash", "-style", "-suppressnetworkgraph", "-devbuild",
        "-scrapersleep", "-activebeforesb", "-pollmultiaddressheight", "-autogreylistdeepcopyheight",
        "-autogreylisttotalcreditfixheight", "-autogreylistredesignheight", "-clearallregistryhistory",
        "-boinckey", "-printstakemodifier", "-printpriority", "-printkeypool", "-limitfreerelay", "-autoban",
        "-printcoinage", "-printblock", "-printblockindex", "-printblocktree", "-loadblockindextest",
        "-privdb", "-flushwallet", "-upgradewallet", "-blockv15height", "-messagecontractdisableheight",
        "-pendingpoolretention", "-rpcssl", "-rpcsslcertificatechainfile", "-rpcsslprivatekeyfile",
        "-rpcsslciphers", "-socks", "-debugallowtracer", "-rpcserviceuser", "-rpcskiplistenercheck",
        "-stdinwalletpassphrase", "-stdinelevate", "-ipcdir", "-ipcattachgroup",
        "-ipcserviceuser"};
#ifdef __linux__
    refuse.insert("-autounlocksocket");
#endif
    for (const auto& type : GRC::RegistryBookmarks::CONTRACT_TYPES_WITH_REG_DB) {
        refuse.insert("-clear" + GRC::Contract::Type::ToString(type) + "history");
    }

    std::set<std::string> registered;
    for (const auto& [name, flags] : gArgs.GetRegisteredArgFlags()) registered.insert(name);
    std::set<std::string> listed{free};
    listed.insert(elevate.begin(), elevate.end());
    listed.insert(refuse.begin(), refuse.end());
    // A new argument fails here until it is put in one list.
    BOOST_CHECK_EQUAL_COLLECTIONS(registered.begin(), registered.end(), listed.begin(), listed.end());

    const std::map<std::string, unsigned int> all_flags = gArgs.GetRegisteredArgFlags();
    for (const std::string& name : registered) {
        const SettingClass want = free.count(name) ? SettingClass::FREE
                                : elevate.count(name) ? SettingClass::ELEVATE : SettingClass::REFUSE;
        BOOST_CHECK_MESSAGE(gArgs.GetSettingClass(name.substr(1)) == want, name);
        const unsigned int flags = all_flags.at(name);
        // The core and gridcoin-service refuse the same keys: the belt never decides alone.
        if (flags & ArgsManager::COMMAND_LINE_ONLY) BOOST_CHECK_MESSAGE(IsRefusedSettingKey(name.substr(1)), name);
        // A refused argument carries no Free flag.
        if (refuse.count(name)) BOOST_CHECK_MESSAGE(!(flags & ArgsManager::SETTING_FREE), name);
    }

    // The developer and client overrides are honoured only from the command line.
    for (const char* name : {"-debugallowtracer", "-rpcskiplistenercheck", "-stdinwalletpassphrase", "-stdinelevate"}) {
        BOOST_CHECK_MESSAGE(all_flags.at(name) & ArgsManager::COMMAND_LINE_ONLY, name);
    }

    for (const char* name : {"-blocknotify", "-walletnotify", "-projectnotify", "-pollnotify", "-psgtnotify",
                             "-alertnotify", "-startupnotify", "-shutdownnotify", "-rpcuser", "-rpcpassword"}) {
        BOOST_CHECK_MESSAGE(all_flags.at(name) & ArgsManager::SENSITIVE, name);
    }
}

BOOST_AUTO_TEST_CASE(paytxfee_above_the_settxfee_maximum_warns_at_startup)
{
    // The warning starts where settxfee and changesettings stop accepting a rate.
    BOOST_CHECK(!PayTxFeeWarnsAtStartup(0));
    BOOST_CHECK(!PayTxFeeWarnsAtStartup(GetBaseFee(CTransaction(), GMF_SEND)));
    BOOST_CHECK(!PayTxFeeWarnsAtStartup(MaxTxFeeRate()));
    BOOST_CHECK(PayTxFeeWarnsAtStartup(MaxTxFeeRate() + 1));
    BOOST_CHECK(PayTxFeeWarnsAtStartup(COIN / 4));
}

BOOST_AUTO_TEST_CASE(gui_only_args_from_config_are_ignored)
{
    TestArgsManager args;
    RegisterArgs(args);

    // The options appear at top level and in a network section.
    std::istringstream conf("guilogfile=/home/x/.bashrc\nautotrustidentity=1\nipclogtrace=1\nstaking=0\n"
                            "[test]\nguilogfile=/home/x/.profile\n");
    std::string err;
    BOOST_REQUIRE(args.ReadConfigStream(conf, "test.conf", err, true));

    const std::vector<std::string> removed = IgnoreGuiOnlyConfigArgs(args);

    BOOST_CHECK(!args.IsArgSet("-guilogfile"));
    BOOST_CHECK(!args.IsArgSet("-autotrustidentity"));
    BOOST_CHECK(!args.IsArgSet("-ipclogtrace"));
    BOOST_CHECK(args.IsArgSet("-staking"));

    // Keys are returned with the leading '-', once each, even when set in two sections.
    BOOST_CHECK_EQUAL(removed.size(), 3u);
    BOOST_CHECK(Contains(removed, "-guilogfile"));
    BOOST_CHECK(Contains(removed, "-autotrustidentity"));
    BOOST_CHECK(Contains(removed, "-ipclogtrace"));

    // The network section's copy is gone too.
    args.SelectConfigNetwork("test");
    BOOST_CHECK(!args.IsArgSet("-guilogfile"));
}

BOOST_AUTO_TEST_CASE(gui_only_args_on_command_line_are_kept)
{
    TestArgsManager args;
    RegisterArgs(args);

    const char* argv[] = {"x", "-guilogfile=/tmp/g.log", "-ipclogtrace=1"};
    std::string err;
    BOOST_REQUIRE(args.ParseParameters(3, argv, err));

    std::istringstream conf("guilogfile=/home/x/.bashrc\n");
    BOOST_REQUIRE(args.ReadConfigStream(conf, "test.conf", err, true));

    const std::vector<std::string> removed = IgnoreGuiOnlyConfigArgs(args);

    BOOST_CHECK_EQUAL(args.GetArg("-guilogfile", ""), "/tmp/g.log");
    BOOST_CHECK(args.GetBoolArg("-ipclogtrace", false));
    BOOST_CHECK_EQUAL(removed.size(), 1u);
}

BOOST_AUTO_TEST_CASE(resetblockchaindata_refused_in_multiprocess_gui)
{
    const std::string expected =
        "-resetblockchaindata is not available in the multiprocess GUI. "
        "Stop the core and run gridcoinresearchd -resetblockchaindata.";

    {
        TestArgsManager args;
        RegisterArgs(args);
        BOOST_CHECK(!MultiprocessGuiArgError(args).has_value());
    }
    {
        TestArgsManager args;
        RegisterArgs(args);
        const char* argv[] = {"x", "-resetblockchaindata"};
        std::string err;
        BOOST_REQUIRE(args.ParseParameters(2, argv, err));
        const auto msg = MultiprocessGuiArgError(args);
        BOOST_REQUIRE(msg.has_value());
        BOOST_CHECK_EQUAL(*msg, expected);
    }
    {
        TestArgsManager args;
        RegisterArgs(args);
        std::istringstream conf("resetblockchaindata=1\n");
        std::string err;
        BOOST_REQUIRE(args.ReadConfigStream(conf, "test.conf", err, true));
        const auto msg = MultiprocessGuiArgError(args);
        BOOST_REQUIRE(msg.has_value());
        BOOST_CHECK_EQUAL(*msg, expected);
    }
}

BOOST_AUTO_TEST_CASE(datadir_from_config_is_not_refused_in_multiprocess_gui)
{
    std::string err;

    {
        // From the configuration file alone.
        TestArgsManager args;
        RegisterArgs(args);
        std::istringstream conf("datadir=/somewhere/else\n");
        BOOST_REQUIRE(args.ReadConfigStream(conf, "test.conf", err, true));
        BOOST_CHECK(!MultiprocessGuiArgError(args).has_value());
    }
    {
        // From a network section of the configuration file.
        TestArgsManager args;
        RegisterArgs(args);
        std::istringstream conf("[test]\ndatadir=/somewhere/else\n");
        BOOST_REQUIRE(args.ReadConfigStream(conf, "test.conf", err, true));
        BOOST_CHECK(!MultiprocessGuiArgError(args).has_value());
    }
    {
        // From the command line.
        TestArgsManager args;
        RegisterArgs(args);
        const char* argv[] = {"x", "-datadir=/tmp/dd"};
        BOOST_REQUIRE(args.ParseParameters(2, argv, err));
        BOOST_CHECK(!MultiprocessGuiArgError(args).has_value());
    }
}

BOOST_AUTO_TEST_CASE(gui_only_args_list_matches_the_literal)
{
    const std::vector<std::string>& list = GuiOnlyArgs();
    BOOST_CHECK(Contains(list, "-guilogfile"));
    BOOST_CHECK(Contains(list, "-autotrustidentity"));
    BOOST_CHECK(!Contains(list, "-ipclogtrace"));
}

#ifndef WIN32
namespace {
//! Puts back the dispositions of SIGTERM and SIGINT and clears the shutdown request when
//! it goes, however the case ends.
class ShutdownSignalsGuard
{
public:
    ShutdownSignalsGuard()
    {
        ::sigaction(SIGTERM, nullptr, &m_term);
        ::sigaction(SIGINT, nullptr, &m_int);
    }

    ~ShutdownSignalsGuard()
    {
        ::sigaction(SIGTERM, &m_term, nullptr);
        ::sigaction(SIGINT, &m_int, nullptr);
        AbortShutdown();
    }

    ShutdownSignalsGuard(const ShutdownSignalsGuard&) = delete;
    ShutdownSignalsGuard& operator=(const ShutdownSignalsGuard&) = delete;

private:
    struct sigaction m_term{};
    struct sigaction m_int{};
};
} // namespace

// The handlers that RegisterShutdownSignalHandlers installs for SIGTERM and SIGINT
// request a shutdown and do nothing else. The process's dispositions and the request
// flag are put back afterwards, however the case ends.
BOOST_AUTO_TEST_CASE(shutdown_signal_handlers_request_a_shutdown)
{
    const ShutdownSignalsGuard guard;
    BOOST_REQUIRE(!ShutdownRequested());

    RegisterShutdownSignalHandlers();

    for (const int signal : {SIGTERM, SIGINT}) {
        // Checked before raising, so that a missing handler fails the case rather
        // than ending the test process.
        struct sigaction installed;
        BOOST_REQUIRE_EQUAL(::sigaction(signal, nullptr, &installed), 0);
        BOOST_REQUIRE_MESSAGE(installed.sa_handler != SIG_DFL && installed.sa_handler != SIG_IGN,
                              "no handler for signal " << signal);

        BOOST_REQUIRE(!ShutdownRequested());
        BOOST_REQUIRE_EQUAL(::raise(signal), 0);
        BOOST_CHECK_MESSAGE(ShutdownRequested(), "signal " << signal << " did not request a shutdown");
        AbortShutdown();
    }
}
#endif

BOOST_AUTO_TEST_SUITE_END()
