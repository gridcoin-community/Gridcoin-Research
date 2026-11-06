// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/gridcoin.h"
#include "init.h"
#include "logging.h"
#include "node/shutdown.h"
#include "policy/fees.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32
#include "test/umask_guard.h"

#include <csignal>
#include <sys/stat.h>
#include <unistd.h>
#else
#include "test/wine.h"
#include "util/dir_permissions.h"
#include "util/win_acl.h"
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

//! A fresh, empty data directory with -datadir pointing at it, for one case. The guard,
//! constructed before -datadir is set, restores the fixture's settings when the case ends.
struct ScratchDataDir
{
    fs::path path{fs::temp_directory_path() / fs::unique_path("init_tests_%%%%-%%%%-%%%%")};
    grc_test::StateGuard guard;

    ScratchDataDir()
    {
        fs::create_directories(path);
        gArgs.ForceSetArg("-datadir", path.string());
        gArgs.ForceSetArg("-backupdir", (path / "backups").string());
        gArgs.ClearPathCache();
    }

    ~ScratchDataDir()
    {
        gArgs.ClearPathCache();
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string ReadText(const fs::path& path)
{
    fsbridge::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void WriteText(const fs::path& path, const std::string& text)
{
    fsbridge::ofstream stream(path, std::ios::binary);
    stream << text;
}

//! Whether a line of the log captured in \p lines holds \p text.
bool Logged(const std::vector<std::string>& lines, const std::string& text)
{
    return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
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
#ifdef WIN32
    refuse.insert("-autounlockblob");
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

    // The developer and client overrides, and the autounlock options of the service
    // units, are honoured only from the command line.
    std::vector<std::string> command_line_only{"-debugallowtracer", "-rpcskiplistenercheck", "-stdinwalletpassphrase",
                                               "-stdinelevate"};
#ifdef __linux__
    command_line_only.push_back("-autounlocksocket");
#endif
#ifdef WIN32
    command_line_only.push_back("-autounlockblob");
#endif
    for (const std::string& name : command_line_only) {
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

// The core adds generated RPC credentials only to a configuration file this account owns
// (fsmodes::OwnedByUs). A symbolic link is not one, so the file it leads to is left as it
// was, and the log says why nothing was added.
BOOST_AUTO_TEST_CASE(rpc_credentials_are_not_added_to_a_conf_this_account_does_not_own)
{
    // The settings, including what a re-read of the configuration would put there.
    const grc_test::StateGuard state;

    struct RemoveOnExit {
        const fs::path dir = GetDataDir() / "init_tests_conf";
        ~RemoveOnExit()
        {
            boost::system::error_code ec;
            fs::remove_all(dir, ec);
        }
    } scratch;
    fs::create_directories(scratch.dir);

    const std::string contents{"addnode=127.0.0.1\n"};
    const fs::path target = scratch.dir / "provided.conf";
    {
        fsbridge::ofstream stream(target);
        stream << contents;
    }
    const fs::path conf = scratch.dir / "gridcoinresearch.conf";
    fs::create_symlink(target, conf);

    grc_test::ForcedArgGuard conf_arg("conf", conf.string());
    BOOST_REQUIRE(GetConfigFile() == conf);
    BOOST_REQUIRE(gArgs.GetArg("-rpcuser", "").empty());
    BOOST_REQUIRE(gArgs.GetArg("-rpcpassword", "").empty());

    std::vector<std::string> lines;
    const auto callback = LogInstance().PushBackCallback([&lines](const std::string& s) { lines.push_back(s); });

    AddMissingRpcCredentials();

    LogInstance().DeleteCallback(callback);

    std::string after;
    {
        fsbridge::ifstream stream(target);
        after.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
    BOOST_CHECK_EQUAL(after, contents);
    BOOST_CHECK(fs::is_symlink(conf));

    const std::string expected = "not adding RPC credentials to " + conf.string() +
                                 ": it is not owned by this account; the package or the administrator provides them";
    BOOST_CHECK_MESSAGE(std::any_of(lines.begin(), lines.end(),
                                    [&](const std::string& l) { return l.find(expected) != std::string::npos; }),
                        "no log line: " << expected);
}

// The first-run configuration file holds the RPC password, so it is created exclusively and
// owner-only, whatever the umask.
BOOST_AUTO_TEST_CASE(first_run_conf_is_created_owner_only)
{
    const grc_test::UmaskGuard umask(022);
    ScratchDataDir datadir;
    const fs::path conf = GetConfigFile();
    BOOST_REQUIRE(conf.parent_path() == datadir.path);
    BOOST_REQUIRE(IsConfigFileEmpty());

    std::string error;
    BOOST_REQUIRE_MESSAGE(PrepareConfigFile(error), error);

    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(conf.string().c_str(), &st), 0);
    BOOST_CHECK(S_ISREG(st.st_mode));
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0600u);

    const std::string text = ReadText(conf);
    BOOST_CHECK(text.find("rpcuser=gridcoinrpc\n") != std::string::npos);
    BOOST_CHECK(text.find("rpcpassword=") != std::string::npos);
}

// A link at the configuration file's name is not followed: nothing is created where it leads,
// and the start goes on, logging why, to StartRPCThreads' message about the missing password.
BOOST_AUTO_TEST_CASE(first_run_conf_is_not_created_through_a_link)
{
    ScratchDataDir datadir;
    const fs::path conf = GetConfigFile();
    const fs::path target = datadir.path / "elsewhere.conf";
    fs::create_symlink(target, conf);
    BOOST_REQUIRE(IsConfigFileEmpty());

    std::vector<std::string> lines;
    const auto callback = LogInstance().PushBackCallback([&lines](const std::string& s) { lines.push_back(s); });
    std::string error;
    const bool prepared = PrepareConfigFile(error);
    LogInstance().DeleteCallback(callback);

    BOOST_CHECK_MESSAGE(prepared, error);
    BOOST_CHECK(!fs::exists(fs::symlink_status(target)));
    BOOST_CHECK(fs::is_symlink(fs::symlink_status(conf)));
    BOOST_CHECK_MESSAGE(Logged(lines, "not creating the configuration file " + conf.string()), "no log line");
}

// A configuration file that exists but cannot be read is not the missing file the first-run
// step creates, so it is never truncated or rewritten.
BOOST_AUTO_TEST_CASE(unreadable_conf_is_not_rewritten)
{
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("skipped: running as root, which reads a file of any mode");
        return;
    }

    ScratchDataDir datadir;
    const fs::path conf = GetConfigFile();
    const std::string contents{"addnode=127.0.0.1\n"};
    WriteText(conf, contents);
    BOOST_REQUIRE_EQUAL(::chmod(conf.string().c_str(), 0200), 0);
    BOOST_REQUIRE(IsConfigFileEmpty());

    std::string error;
    BOOST_CHECK_MESSAGE(PrepareConfigFile(error), error);

    BOOST_REQUIRE_EQUAL(::chmod(conf.string().c_str(), 0600), 0);
    BOOST_CHECK_EQUAL(ReadText(conf), contents);
}

// The core rewrites its configuration file to drop obsolete keys only when this account owns
// it (fsmodes::OwnedByUs). A link is not one: the link and the file it leads to stay as they
// were.
BOOST_AUTO_TEST_CASE(clean_config_leaves_a_conf_this_account_does_not_own)
{
    ScratchDataDir datadir;
    const std::string contents{"privatekey=x\nrpcuser=a\n"};
    const fs::path target = datadir.path / "provided.conf";
    WriteText(target, contents);
    const fs::path conf = GetConfigFile();
    fs::create_symlink(target, conf);

    std::vector<std::string> lines;
    const auto callback = LogInstance().PushBackCallback([&lines](const std::string& s) { lines.push_back(s); });
    const bool cleaned = GRC::CleanConfig();
    LogInstance().DeleteCallback(callback);

    BOOST_CHECK(cleaned);
    BOOST_CHECK(fs::is_symlink(fs::symlink_status(conf)));
    BOOST_CHECK_EQUAL(ReadText(target), contents);
    BOOST_CHECK_MESSAGE(Logged(lines, "not removing obsolete keys from " + conf.string()), "no log line");
}

// The cleaned file is written under a temporary name that is created exclusively and
// owner-only. A link left at that name is removed, never followed.
BOOST_AUTO_TEST_CASE(clean_config_does_not_follow_a_link_at_its_temporary_name)
{
    ScratchDataDir datadir;
    const fs::path conf = GetConfigFile();
    WriteText(conf, "privatekey=x\nrpcuser=a\n");
    BOOST_REQUIRE_EQUAL(::chmod(conf.string().c_str(), 0600), 0);

    const std::string victim_contents{"victim\n"};
    const fs::path victim = datadir.path / "victim";
    WriteText(victim, victim_contents);
    fs::path temporary = conf;
    temporary.replace_extension("conf~");
    fs::create_symlink(victim, temporary);

    BOOST_CHECK(GRC::CleanConfig());

    BOOST_CHECK_EQUAL(ReadText(victim), victim_contents);
    BOOST_CHECK(!fs::exists(fs::symlink_status(temporary)));

    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(conf.string().c_str(), &st), 0);
    BOOST_CHECK(S_ISREG(st.st_mode));
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0600u);
    BOOST_CHECK_EQUAL(ReadText(conf), "rpcuser=a\n");
}
#endif

#ifdef WIN32
// On Windows too the core adds RPC credentials only to a configuration file this account owns.
// A symbolic link is not one, so the file it leads to is left as it was. Making the link needs
// Developer Mode or an elevated token; without either the case says so and checks nothing.
BOOST_AUTO_TEST_CASE(rpc_credentials_are_not_added_through_a_link_windows)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine has no Windows file ownership; skipping");
        return;
    }

    ScratchDataDir datadir;
    const std::string contents{"addnode=127.0.0.1\n"};
    const fs::path target = datadir.path / "provided.conf";
    WriteText(target, contents);
    const fs::path conf = datadir.path / "gridcoinresearch.conf";
    boost::system::error_code ec;
    fs::create_symlink(target, conf, ec);
    if (ec) {
        BOOST_WARN_MESSAGE(false, "a symbolic link could not be made (" << ec.message() << "); not checked");
        return;
    }

    grc_test::ForcedArgGuard conf_arg("conf", conf.string());
    BOOST_REQUIRE(GetConfigFile() == conf);
    BOOST_REQUIRE(gArgs.GetArg("-rpcpassword", "").empty());

    std::vector<std::string> lines;
    const auto callback = LogInstance().PushBackCallback([&lines](const std::string& s) { lines.push_back(s); });
    AddMissingRpcCredentials();
    LogInstance().DeleteCallback(callback);

    BOOST_CHECK_EQUAL(ReadText(target), contents);
    BOOST_CHECK(fs::is_symlink(fs::symlink_status(conf)));
    BOOST_CHECK_MESSAGE(Logged(lines, "not adding RPC credentials to " + conf.string()), "no log line");
}

// The first-run configuration file is created with the owner-only access list, not the one it
// would inherit from the data directory.
BOOST_AUTO_TEST_CASE(first_run_conf_is_created_owner_only_windows)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; skipping");
        return;
    }

    ScratchDataDir datadir;
    const fs::path conf = GetConfigFile();
    BOOST_REQUIRE(IsConfigFileEmpty());

    std::string error;
    BOOST_REQUIRE_MESSAGE(PrepareConfigFile(error), error);

    const std::optional<std::string> reason =
        util::winacl::CheckMatchesSpec(util::winacl::ReadAcl(conf), util::OwnerOnlyAclSpec(/*inheritable=*/false));
    BOOST_CHECK_MESSAGE(!reason.has_value(), reason.value_or(""));
    BOOST_CHECK(ReadText(conf).find("rpcpassword=") != std::string::npos);

    // Present now, so the next start cleans and completes it instead.
    BOOST_CHECK(!IsConfigFileEmpty());
}
#endif

BOOST_AUTO_TEST_SUITE_END()
