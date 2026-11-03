// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/contract/registry.h"
#include "init.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <set>
#include <string>

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
        "-rpcsslciphers", "-socks"};
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

    for (const char* name : {"-blocknotify", "-walletnotify", "-projectnotify", "-pollnotify", "-psgtnotify",
                             "-alertnotify", "-startupnotify", "-shutdownnotify", "-rpcuser", "-rpcpassword"}) {
        BOOST_CHECK_MESSAGE(all_flags.at(name) & ArgsManager::SENSITIVE, name);
    }
}

BOOST_AUTO_TEST_SUITE_END()
