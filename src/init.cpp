// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.


#include "chainparams.h"
#include "util.h"
#include "util/threadnames.h"
#include "net.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "banman.h"
#include "rpc/server.h"
#include "init.h"
#include "node/ui_interface.h"
#include "scheduler.h"
#include "gridcoin/gridcoin.h"
#include "miner.h"
#include "node/blockstorage.h"

#include <boost/algorithm/string/predicate.hpp>
#include <openssl/crypto.h>

#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()

static boost::thread_group threadGroup;
static CScheduler scheduler;

extern void ThreadAppInit2(void* parg);

#ifndef WIN32
#include <signal.h>
#include <sys/stat.h>
#endif

using namespace std;
CWallet* pwalletMain;
extern bool fQtActive;
extern bool bGridcoinCoreInitComplete;
extern bool fConfChange;
extern bool fEnforceCanonical;
extern unsigned int nNodeLifespan;
extern unsigned int nDerivationMethodIndex;
extern unsigned int nMinerSleep;
extern bool fUseFastIndex;
// Dump addresses to banlist.dat every 5 minutes (300 s)
static constexpr int DUMP_BANS_INTERVAL = 300;

// RPC client default timeout.
extern constexpr int DEFAULT_WAIT_CLIENT_TIMEOUT = 0;


std::unique_ptr<BanMan> g_banman;

/**
 * The PID file facilities.
 */
static const char* GRIDCOIN_PID_FILENAME = "gridcoinresearchd.pid";

static fs::path GetPidFile(const ArgsManager& args)
{
    return AbsPathForConfigVal(fs::path(args.GetArg("-pid", GRIDCOIN_PID_FILENAME)));
}

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

bool ShutdownRequested()
{
    return fRequestShutdown;
}

void StartShutdown()
{

    LogPrintf("Calling start shutdown...");

    if(fQtActive)
        // ensure we leave the Qt main loop for a clean GUI exit (Shutdown() is called in bitcoin.cpp afterwards)
        uiInterface.QueueShutdown();
    else
        // Without UI, shutdown is initiated and shutdown() is called in AppInit
        fRequestShutdown = true;
}

void Shutdown(void* parg)
{
    static CCriticalSection cs_Shutdown;
    static bool fTaken;

    // Make this thread recognisable as the shutdown thread
    RenameThread("grc-shutoff");

    bool fFirstThread = false;
    {
        TRY_LOCK(cs_Shutdown, lockShutdown);
        if (lockShutdown)
        {
            fFirstThread = !fTaken;
            fTaken = true;
        }
    }
    static bool fExit;
    if (fFirstThread)
    {
         LogPrintf("gridcoinresearch exiting...");
        fShutdown = true;

        // Signal to the scheduler to stop.
        LogPrintf("INFO: %s: Stopping the scheduler.", __func__);
        scheduler.stop();

        // clean up any remaining threads running serviceQueue:
        LogPrintf("INFO: %s: Cleaning up any remaining threads in scheduler.", __func__);
        threadGroup.interrupt_all();
        threadGroup.join_all();

        LogPrintf("INFO: %s: Flushing wallet database.", __func__);
        bitdb.Flush(false);

        // Interrupt all sleeping threads.
        LogPrintf("INFO: %s: Interrupting sleeping threads.", __func__);
        g_thread_interrupt();

        LogPrintf("INFO: %s: Stopping net (node) threads.", __func__);
        StopNode();

        LogPrintf("INFO: %s: Final flush of wallet database and closing wallet database file.", __func__);
        bitdb.Flush(true);

        LogPrintf("INFO: %s: Stopping RPC threads.", __func__);
        StopRPCThreads();

        // This is necessary here to prevent a snapshot download from failing at the cleanup
        // step because of a write lock on accrual/registry.dat.
        GRC::CloseResearcherRegistryFile();

        fs::remove(GetPidFile(gArgs));
        UnregisterWallet(pwalletMain);
        delete pwalletMain;
        // close transaction database to prevent lock issue on restart
        // This causes issues on daemons where it tries to create a second
        // lock file.
        //CTxDB().Close();
        UninterruptibleSleep(std::chrono::milliseconds{50});
        LogPrintf("Gridcoin exited");
        fExit = true;
    }
    else
    {
        while (!fExit)
            UninterruptibleSleep(std::chrono::milliseconds{100});
    }
}

#ifndef WIN32
static void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

static void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    fRequestShutdown = true;
    Sleep(INFINITE);
    return true;
}
#endif


#ifndef WIN32
static void registerSignalHandler(int signal, void(*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}
#endif


bool static Bind(const CService &addr, bool fError = true) {
    if (IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError)) {
        if (fError)
            return InitError(strError);
        return false;
    }
    return true;
}

static void CreateNewConfigFile()
{
    fsbridge::ofstream myConfig;
    myConfig.open(GetConfigFile());

    myConfig
        << "addnode=addnode-us-central.cycy.me\n"
        << "addnode=ec2-3-81-39-58.compute-1.amazonaws.com\n"
        << "addnode=gridcoin.ddns.net\n"
        << "addnode=seeds.gridcoin.ifoggz-network.xyz\n"
        << "addnode=seed.gridcoin.pl\n"
        << "addnode=www.grcpool.com\n";
}

void AddLoggingArgs(ArgsManager& argsman)
{
    argsman.AddArg("-debuglogfile=<file>", strprintf("Specify location of debug log file. Relative paths will be prefixed "
                                                     "by a net-specific datadir location. (-nodebuglogfile to disable; "
                                                     "default: %s)",
                                                     DEFAULT_DEBUGLOGFILE),
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-debug=<category>", "Output debugging information (default: -nodebug, supplying <category> is optional). "
                                        "If <category> is not supplied or if <category> = 1, output all debugging information. <category> can be: "
                   + ListLogCategories() + ". This option can be specified multiple times to output multiple categories.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-debugexclude=<category>", strprintf("Exclude debugging information for a category. Can be used in"
                                                         " conjunction with -debug=1 to output debug logs for all categories"
                                                         " except the specified category. This option can be specified"
                                                         " multiple times to exclude multiple categories."),
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-logtimestamps", strprintf("Prepend debug output with timestamp (default: %u)", DEFAULT_LOGTIMESTAMPS),
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
#ifdef HAVE_THREAD_LOCAL
    argsman.AddArg("-logthreadnames", strprintf("Prepend debug output with name of the originating thread (only available on"
                                                " platforms supporting thread_local) (default: %u)", DEFAULT_LOGTHREADNAMES),
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
#else
    argsman.AddHiddenArgs({"-logthreadnames"});
#endif
    argsman.AddArg("-logtimemicros", strprintf("Add microsecond precision to debug timestamps (default: %u)",
                                               DEFAULT_LOGTIMEMICROS),
                   ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-printtoconsole", "Send trace/debug info to console (default: 0) )",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-printtodebugger", "Send trace/debug info to debugger (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-shrinkdebugfile", "Shrink debug.log file on client startup (default: 1 when no -debug)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-logarchivedaily", "Archive log file to compressed archive daily (default: 1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-deleteoldlogarchives", "Delete oldest log archive files in excess of -logarchiveretainnumfiles "
                                            "setting",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-logarchiveretainnumfiles=<num>", "Specify number of compressed log archive files to retain"
                                                      " (default: 30)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-org=<string>", "Set organization name for identification (default: not set). Required for use "
                                    "on testnet. Do not set this for a wallet on mainnet.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
}

void SetupServerArgs()
{
    ArgsManager& argsman = gArgs;

    SetupHelpOptions(argsman);
    argsman.AddArg("-help-debug", "Print help message with debugging options and exit",
                   ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);

    AddLoggingArgs(argsman);

    const auto defaultBaseParams = CreateBaseChainParams(CBaseChainParams::MAIN);
    const auto testnetBaseParams = CreateBaseChainParams(CBaseChainParams::TESTNET);
    const auto defaultChainParams = CreateChainParams(CBaseChainParams::MAIN);
    const auto testnetChainParams = CreateChainParams(CBaseChainParams::TESTNET);

    // Hidden Options
    std::vector<std::string> hidden_args = {
        "-dbcrashratio", "-forcecompactdb", "-fastindex",
        // GUI args. These will be overwritten by SetupUIArgs for the GUI
        "-choosedatadir", "-lang=<lang>", "-min", "-resetguisettings",
        "-splash", "-style", "-suppressnetworkgraph", "-showorphans"};

    // Listed Options
    // General
    argsman.AddArg("-version", "Print version and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    // TODO: Read-only config file?
    argsman.AddArg("-conf=<file>", strprintf("Specify path to read-only configuration file. Relative paths will be prefixed"
                                             " by datadir location. (default: %s)", GRIDCOIN_CONF_FILENAME),
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-pid=<file>", strprintf("Specify pid file. Relative paths will be prefixed by a net-specific datadir"
                                            " location. (default: %s)", GRIDCOIN_PID_FILENAME),
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-datadir=<dir>", "Specify data directory", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-wallet=<dir>", "Specify wallet file (within data directory)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-dbcache=<n>", "Set database cache size in megabytes (default: 25)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-dblogsize=<n>", "Set database disk log size in megabytes (default: 100)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-synctime", "Sync time with other nodes. Disable if time on your system is precise e.g. syncing with"
                                " NTP (default: 1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-paytxfee=<amt>", "Fee per KB to add to transactions you send",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-mintxfee=<amt>", "Minimum transaction fee for transactions you send or process (default: 0.001 GRC)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-mininput=<amt>", "When creating transactions, ignore inputs with value less than this (default: 0.01)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-daemon", "Run in the background as a daemon and accept commands",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-testnet", "Use the test network", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-blocknotify=<cmd>", "Execute command when the best block changes (%s in cmd is replaced by block hash)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-walletnotify=<cmd>", "Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-confchange", "Require confirmations for change (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-enforcecanonical", "Enforce transaction scripts to use canonical PUSH operators (default: 1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-alertnotify=<cmd>", "Execute command when a relevant alert is received or we see a really long fork"
                                         " (%s in cmd is replaced by message)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-blockminsize=<n>", "Set minimum block size in bytes (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-blockmaxsize=<n>", strprintf("Set maximum block size in bytes (default: %u)", MAX_BLOCK_SIZE_GEN/2),
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-blockprioritysize=<n>", "Set maximum size of high-priority/low-fee transactions in bytes"
                                             " (default: 27000)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-snapshotdownload", "Download and apply latest snapshot",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-snapshoturl=<url>", "Optional: URL for the snapshot.zip file (ex: "
                                         "https://sub.domain.com/location/snapshot.zip)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-snapshotsha256url=<url>", "Optional: URL for the snapshot.sha256 file (ex: "
                                               "https://sub.domain.com/location/snapshot.sha256)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-disableupdatecheck", "Optional: Disable update checks by wallet",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-updatecheckinterval=<n>", "Optional: Check for updates every <n> hours (default: 120, minimum: 1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-updatecheckurl=<url>", "Optional: URL for the update version checks (ex: "
                                            "https://sub.domain.com/location/latest",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-resetblockchaindata", "Reset blockchain data. This argument will remove all previous blockchain data",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-loadblock=<file>", "Imports blocks from external file on startup",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    //TODO: Implement reindex option
    //argsman.AddArg("-reindex", "Rebuild chain state and block index from the blk*.dat files on disk",
    //               ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-settings=<file>", strprintf("Specify path to dynamic settings data file. Can be disabled with"
                                                 " -nosettings. File is written at runtime and not meant to be edited by"
                                                 " users (use %s instead for custom settings). Relative paths will be"
                                                 " prefixed by datadir location. (default: %s)",
                                                 GRIDCOIN_CONF_FILENAME, GRIDCOIN_SETTINGS_FILENAME),
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    //TODO: Implement startupnotify option
    //argsman.AddArg("-startupnotify=<cmd>", "Execute command on startup.", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);


    // Staking
    argsman.AddArg("-enablesidestaking", "Enable side staking functionality (default: 0)",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-staking", "Allow wallet to stake if conditions to stake are met (default: 1)",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-sidestake=<address,percent>", "Sidestake destination and allocation entry. There can be as many "
                                                   "specified as desired. Only six per stake can be sent. If more than "
                                                   "six are specified. Six are randomly chosen for each stake. Only active "
                                                   "if -enablesidestaking is set. These settings are overridden if "
                                                   "-sidestakeaddresses and -stakestakeallocations are set.",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-sidestakeaddresses=<address1,address2,...,addressN>", "Sidestake destination entry. There can be as many "
                                                   "specified as desired. Only six per stake can be sent. If more than "
                                                   "six are specified. Six are randomly chosen for each stake. Only active "
                                                   "if -enablesidestaking is set. If set along with -sidestakeallocations "
                                                   "overrides the -sidestake entries.",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-sidestakeallocations=percent1,percent2,...,percentN>", "Sidestake allocation entry. There can be as many "
                                                   "specified as desired. Only six per stake can be sent. If more than "
                                                   "six are specified. Six are randomly chosen for each stake. Only active "
                                                   "if -enablesidestaking is set. If set along with -sidestakeaddresses "
                                                   "overrides the -sidestake entries.",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-enablestakesplit", "Enable unspent output spitting when staking to optimize staking efficiency "
                                        "(default: 0",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-stakingefficiency=<percent>", "Specify target staking efficiency for stake splitting (default: 90, "
                                                   "clamped to [75, 98])",
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);
    argsman.AddArg("-minstakesplitvalue=<n>", strprintf("Specify minimum output value for post split output when stake "
                                                        "splitting (default: %" PRId64 "GRC)", MIN_STAKE_SPLIT_VALUE_GRC),
                   ArgsManager::ALLOW_ANY | ArgsManager::IMMEDIATE_EFFECT, OptionsCategory::STAKING);

    // Scraper
    argsman.AddArg("-scraper", "Activate scraper for statistics downloads. This will only work if the node has a wallet "
                               "key that is authorized by the network (default: 0).",
                   ArgsManager::ALLOW_ANY, OptionsCategory::SCRAPER);
    argsman.AddArg("-explorer", "Activate extended statistics file retention for the scraper. This will only work if the"
                                "node is authorized for scraping and the scraper is activated (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::SCRAPER);
    argsman.AddArg("-scraperkey=<address>", "Manually specify scraper public key in address form. This is not necessary "
                                            "and will not work if the private key is not present in the scraper wallet file.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::SCRAPER);

    // Researcher
    argsman.AddArg("-email=<email>", "Email address to use for CPID detection. Must match your BOINC account email",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RESEARCHER);
    argsman.AddArg("-boincdatadir=<path>", "Path to the BOINC data directory for CPID detection when the BOINC client uses"
                                           " a non-default directory",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RESEARCHER);
    argsman.AddArg("-forcecpid=<cpid>", "Override automatic CPID detection with the specified CPID",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RESEARCHER);
    argsman.AddArg("-investor", "Disable CPID detection and do not participate in the research reward system",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RESEARCHER);
    argsman.AddArg("-pooloperator", "Skip pool CPID checks for staking nodes run by pool administrators",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RESEARCHER);

    // Wallet
    argsman.AddArg("-upgradewallet", "Upgrade wallet to latest format",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-keypool=<n>", "Set key pool size to <n> (default: 100)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-rescan", "Rescan the block chain for missing wallet transactions",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-salvagewallet", "Attempt to recover private keys from a corrupt wallet.dat",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-zapwallettxes", "Delete all wallet transactions and only recover those parts of the blockchain through"
                                     " -rescan on startup",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-checkblocks=<n>", "How many blocks to check at startup (default: 2500, 0 = all)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-checklevel=<n>", "How thorough the block verification is (0-6, default: 1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-walletbackupinterval=<n>", "DEPRECATED: Optional: Create a wallet backup every <n> blocks. Zero"
                                                " disables backups",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-walletbackupintervalsecs=<n>", "Optional: Create a wallet backup every <n> seconds. Zero disables"
                                                    " backups (default: 86400)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-backupdir=<path>", "Specify backup directory for wallet backups (default: walletbackups).",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-maintainbackupretention", "Activate retention management of backup files (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-walletbackupretainnumfiles=<n>", "Specify maximum number of backup files to retain (default: 365). "
                                                  "Note that the actual files retained is the greater of this setting "
                                                  "and the other setting -walletbackupretainnumdays.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-walletbackupretainnumdays=<n>", "Specify maximum number of backup files to retain (default: 365). "
                                                  "Note that the actual files retained is the greater of this setting "
                                                  "and the other setting -walletbackupretainnumfiles.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-enableaccounts", "DEPRECATED: Enable accounting functionality (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    argsman.AddArg("-maxsigcachesize=<n>", "Set maximum size for signature cache (default: 50000)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);


    // Connections
    argsman.AddArg("-timeout=<n>", "Specify connection timeout in milliseconds (default: 5000)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-peertimeout=<n>", "Specify p2p connection timeout in seconds. This option determines the amount of time"
                                       " a peer may be inactive before the connection to it is dropped. (minimum: 1, default:"
                                       " 45)",
                   ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CONNECTION);
    argsman.AddArg("-proxy=<ip:port>", "Connect through socks proxy", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-socks=<n>", "Select the version of socks proxy to use (4-5, default: 5)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-tor=<ip:port>", "Use proxy to reach Tor onion services (default: same as -proxy)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-dns", "Allow DNS lookups for -addnode, -seednode and -connect",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-port=<port>", "Listen for connections on <port> (default: 32749 or testnet: 32748)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxconnections=<n>", "Maintain at most <n> connections to peers (default: 125)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxoutboundconnections=<n>", "Maximum number of outbound connections (default: 8)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-addnode=<ip>", "Add a node to connect to and attempt to keep the connection open",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-connect=<ip>", "Connect only to the specified node(s)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-seednode=<ip>", "Connect to a node to retrieve peer addresses, and disconnect",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-externalip=<ip>", "Specify your own public address",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-onlynet=<net>", "Make outgoing connections only through network <net> (IPv4, IPv6 or Tor). "
                                     "Incoming connections are not affected by this option. This option can be specified "
                                     "multiple times to allow multiple networks.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-discover", "Discover own IP address (default: 1 when listening and no -externalip)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-listen", "Accept connections from outside (default: 1 if no -proxy or -connect)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-bind=<addr>[:<port>][=onion]", "Bind to given address. Use [host]:port notation for IPv6",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-dnsseed", "Find peers using DNS lookup (default: 1)",
                   ArgsManager::ALLOW_BOOL, OptionsCategory::CONNECTION);
    argsman.AddArg("-banscore=<n>", "Threshold for disconnecting misbehaving peers (default: 100)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-bantime=<n>", strprintf("Default duration (in seconds) of manually configured bans (default: %u)",
                                             DEFAULT_MISBEHAVING_BANTIME),
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxreceivebuffer=<n>", "Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxsendbuffer=<n>", "Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#ifdef USE_UPNP
#if USE_UPNP
    argsman.AddArg("-upnp", "Use UPnP to map the listening port (default: 1 when listening and no -proxy)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#else
    argsman.AddArg("-upnp", "Use UPnP to map the listening port (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#endif
#else
    hidden_args.emplace_back("-upnp");
#endif

    // RPC
    argsman.AddArg("-server", "Accept command line and JSON-RPC commands",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcuser=<user>", "Username for JSON-RPC connections",
                   ArgsManager::ALLOW_ANY | ArgsManager::SENSITIVE, OptionsCategory::RPC);
    argsman.AddArg("-rpcwait", "Wait for RPC server to start.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcwaittimeout=<n>", strprintf("Timeout in seconds to wait for the RPC server to start, or 0 for no "
                                                    "timeout. (default: %d)", DEFAULT_WAIT_CLIENT_TIMEOUT),
                   ArgsManager::ALLOW_INT, OptionsCategory::RPC);
    argsman.AddArg("-rpcpassword=<pw>", "Password for JSON-RPC connections",
                   ArgsManager::ALLOW_ANY | ArgsManager::SENSITIVE, OptionsCategory::RPC);
    argsman.AddArg("-rpcport=<port>", strprintf("Listen for JSON-RPC connections on <port> (default: %u, testnet: %u)",
                                                defaultBaseParams->RPCPort(), testnetBaseParams->RPCPort()),
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcallowip=<ip>", "Allow JSON-RPC connections from specified IP address",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcconnect=<ip>", "Send commands to node running on <ip> (default: 127.0.0.1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcthreads=<n>", "Set the number of threads to service RPC calls (default: 4)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcssl", "Use OpenSSL (https) for JSON-RPC connections", ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcsslcertificatechainfile=<file.cert>", "Server certificate file (default: server.cert)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcsslprivatekeyfile=<file.pem>", "Server private key (default: server.pem)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    argsman.AddArg("-rpcsslciphers=<ciphers>",
                   "Acceptable ciphers (default: TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::RPC);

    // Additional hidden options
    hidden_args.emplace_back("-devbuild");
    hidden_args.emplace_back("-scrapersleep");
    hidden_args.emplace_back("-activebeforesb");
    hidden_args.emplace_back("-clearbeaconhistory");

    // -boinckey should now be removed entirely. It is put here to prevent the executable erroring out on
    // an invalid parameter for old clients that may have left the argument in.
    hidden_args.emplace_back("-boinckey");

    hidden_args.emplace_back("-printstakemodifier");
    hidden_args.emplace_back("-printpriority");
    hidden_args.emplace_back("-printkeypool");

    // -limitfreerelay is probably destined for the trash-heap on the next fee rewrite.
    hidden_args.emplace_back("-limitfreerelay");

    // Rob?
    hidden_args.emplace_back("-autoban");

    // These probably should be removed
    hidden_args.emplace_back("-printcoinage");
    hidden_args.emplace_back("-privdb");

    // This is hidden because it defaults to true and should NEVER be changed unless you know what you are doing.
    hidden_args.emplace_back("-flushwallet");

    SetupChainParamsBaseOptions(argsman);

    // Add the hidden options
    argsman.AddHiddenArgs(hidden_args);
}

std::string VersionMessage()
{
    // Note: this prints the version of the binary. It does not necessarily
    // match the version of the running wallet when using the CLI to invoke
    // RPC functions on a remote server.
    //
    return "Gridcoin Core " + FormatFullVersion() + "\n";
}

/** Sanity checks
 *  Ensure that Bitcoin is running in a usable environment with all
 *  necessary library support.
 */
bool InitSanityCheck(void)
{
    // The below sanity check is still required for OpenSSL via key.cpp until Bitcoin's secp256k1 is ported over. For now we have
    // only ported the accelerated hashing.
    if(!ECC_InitSanityCheck()) {
        InitError("OpenSSL appears to lack support for elliptic curve cryptography. For more "
                  "information, visit https://en.bitcoin.it/wiki/OpenSSL_and_EC_Libraries");
        return false;
    }

    return true;
}

/**
 * Initialize global loggers.
 *
 * Note that this is called very early in the process lifetime, so you should be
 * careful about what global state you rely on here.
 */
void InitLogging()
{
    fPrintToConsole = gArgs.GetBoolArg("-printtoconsole");
    fLogTimestamps = gArgs.GetBoolArg("-logtimestamps", true);

    // This is needed because it is difficult to inject the equivalent of -nodebuglogfile in the testing suite for
    // console only logging, so in the testing suite, -debuglogfile=none is used.
    if (gArgs.IsArgNegated("-debuglogfile") || gArgs.GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE) == "none") {
        LogInstance().m_print_to_file = false;
    } else {
        LogInstance().m_print_to_file = true;
    }

    LogInstance().m_file_path = AbsPathForConfigVal(gArgs.GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
    LogInstance().m_print_to_console = fPrintToConsole;
    LogInstance().m_log_timestamps = fLogTimestamps;
    LogInstance().m_log_time_micros = gArgs.GetBoolArg("-logtimemicros", DEFAULT_LOGTIMEMICROS);
    LogInstance().m_log_threadnames = gArgs.GetBoolArg("-logthreadnames", DEFAULT_LOGTHREADNAMES);

    fLogIPs = gArgs.GetBoolArg("-logips", DEFAULT_LOGIPS);

    if (LogInstance().m_print_to_file)
    {
        // Only shrink debug file at start if log archiving is set to false.
        if (!gArgs.GetBoolArg("-logarchivedaily", true) && gArgs.GetBoolArg("-shrinkdebugfile", LogInstance().DefaultShrinkDebugFile()))
        {
            // Do this first since it both loads a bunch of debug.log into memory,
            // and because this needs to happen before any other debug.log printing
            LogInstance().ShrinkDebugFile();
        }
    }

    if (gArgs.IsArgSet("-debug"))
    {
        // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
        std::vector<std::string> categories;

        if (gArgs.GetArgs("-debug").size())
        {
            for (auto const& sSubParam : gArgs.GetArgs("-debug"))
            {
                categories.push_back(sSubParam);
            }
        }

        if (std::none_of(categories.begin(), categories.end(),
            [](std::string cat){return cat == "0" || cat == "none";}))
        {
            for (const auto& cat : categories)
            {
                if (!LogInstance().EnableCategory(cat))
                {
                    InitWarning(strprintf("Unsupported logging category %s=%s.", "-debug", cat));
                }
            }
        }
    }

    std::vector<std::string> excluded_categories;

    if (gArgs.GetArgs("-debugexclude").size())
    {
        for (auto const& sSubParam : gArgs.GetArgs("-debugexclude"))
        {
            excluded_categories.push_back(sSubParam);
        }
    }

    // Now remove the logging categories which were explicitly excluded
    for (const std::string& cat : excluded_categories)
    {
        if (!LogInstance().DisableCategory(cat))
        {
            InitWarning(strprintf("Unsupported logging category %s=%s.", "-debugexclude", cat));
        }
    }

    if (!LogInstance().StartLogging())
    {
       strprintf("Could not open debug log file %s", LogInstance().m_file_path.string());
    }

    if (!LogInstance().m_log_timestamps)
    {
        LogPrintf("Startup time: %s\n", FormatISO8601DateTime(GetTime()));
    }

    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
    LogPrintf("Using data directory %s\n", GetDataDir().string());

    std::string build_type;
#ifdef DEBUG
    build_type = "debug build";
#else
    build_type = "release build";
#endif
    LogPrintf(PACKAGE_NAME " version %s (%s - %s)", FormatFullVersion(), build_type, CLIENT_DATE);
}



void ThreadAppInit2(ThreadHandlerPtr th)
{
    // Make this thread recognisable
    RenameThread("grc-appinit2");
    util::ThreadSetInternalName("grc-appinit2");

    bGridcoinCoreInitComplete = false;

    LogPrintf("Initializing Core...");

    if (!AppInit2(th)) {
        fRequestShutdown = true;
    }

    LogPrintf("Core Initialized...");

    bGridcoinCoreInitComplete = true;
}





/** Initialize Gridcoin.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2(ThreadHandlerPtr threads)
{
    g_timer.InitTimer("init", false);

    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable heap terminate-on-corruption
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
#endif
#ifndef WIN32
    umask(077);

    // Clean shutdown on SIGTERM
    registerSignalHandler(SIGTERM, HandleSIGTERM);
    registerSignalHandler(SIGINT, HandleSIGTERM);

    // Reopen debug.log on SIGHUP
    registerSignalHandler(SIGHUP, HandleSIGHUP);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#else
    SetConsoleCtrlHandler(consoleCtrlHandler, true);
#endif

    // ********************************************************* Step 2: parameter interactions


    if (IsConfigFileEmpty())
    {
        try
        {
            CreateNewConfigFile();

            std::string error_msg;

            if (!gArgs.ReadConfigFiles(error_msg, true))
            {
                throw error_msg;
            }
        }
        catch (const std::exception& e)
        {
            LogPrintf("WARNING: failed to create configuration file: %s", e.what());
        }
    }

    //6-10-2014: R Halford: Updating Boost version to 1.5.5 to prevent sync issues; print the boost version to verify:
	//5-04-2018: J Owens: Boost now needs to be 1.65 or higher to avoid thread sleep problems with system clock resets.
    std::string boost_version = "";
    std::ostringstream s;
    s << boost_version  << "Using Boost "
          << BOOST_VERSION / 100000     << "."  // major version
          << BOOST_VERSION / 100 % 1000 << "."  // minior version
          << BOOST_VERSION % 100                // patch level
          << "\n";

    LogPrintf("Boost Version: %s", s.str());

    nNodeLifespan = gArgs.GetArg("-addrlifespan", 7);
    fUseFastIndex = gArgs.GetBoolArg("-fastindex", false);

    nMinerSleep = gArgs.GetArg("-minersleep", 8000);

    nDerivationMethodIndex = 0;
    fTestNet = gArgs.GetBoolArg("-testnet");

    if (gArgs.GetArgs("-bind").size()) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        gArgs.SoftSetBoolArg("-listen", true);
    }

    if (gArgs.GetArgs("-connect").size()) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        gArgs.SoftSetBoolArg("-dnsseed", false);
        gArgs.SoftSetBoolArg("-listen", false);
    }

    if (gArgs.GetArgs("-proxy").size()) {
        // to protect privacy, do not listen by default if a proxy server is specified
        gArgs.SoftSetBoolArg("-listen", false);
    }

    if (!gArgs.GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        gArgs.SoftSetBoolArg("-upnp", false);
        gArgs.SoftSetBoolArg("-discover", false);
    }

    if (gArgs.GetArgs("-externalip").size()) {
        // if an explicit public IP is specified, do not try to find others
        gArgs.SoftSetBoolArg("-discover", false);
    }

    if (gArgs.GetBoolArg("-salvagewallet")) {
        // Rewrite just private keys: rescan to find transactions
        gArgs.SoftSetBoolArg("-rescan", true);
    }

     if (gArgs.GetBoolArg("-zapwallettxes", false)) {
        // -zapwallettx implies a rescan
        gArgs.SoftSetBoolArg("-rescan", true);
    }

    // Verify testnet is using the testnet directory for the config file:
    std::string sTestNetSpecificArg = gArgs.GetArg("-testnetarg", "default");
    LogPrintf("Using specific arg %s", sTestNetSpecificArg);


    // ********************************************************* Step 3: parameter-to-internal-flags

    if (gArgs.GetArg("-debug", "false") == "true")
    {
            LogPrintf("Enabling debug category VERBOSE from legacy debug.");
            LogInstance().EnableCategory(BCLog::LogFlags::VERBOSE);
    }

#if defined(WIN32)
    fDaemon = false;
#else
    if(fQtActive)
        fDaemon = false;
    else
        fDaemon = gArgs.GetBoolArg("-daemon");
#endif

    if (fDaemon)
        fServer = true;
    else
        fServer = gArgs.GetBoolArg("-server");

    /* force fServer when running without GUI */
    if(!fQtActive)
        fServer = true;

    if (gArgs.IsArgSet("-timeout"))
    {
        int nNewTimeout = gArgs.GetArg("-timeout", 5000);
        if (nNewTimeout > 0 && nNewTimeout < 600000)
            nConnectTimeout = nNewTimeout;
    }

    if (gArgs.IsArgSet("-peertimeout"))
    {
        int nNewPeerTimeout = gArgs.GetArg("-peertimeout", 45);

        if (nNewPeerTimeout <= 0)
            InitError(strprintf(_("Invalid amount for -peertimeout=<amount>: '%s'"), gArgs.GetArg("-peertimeout", "")));

        PEER_TIMEOUT = nNewPeerTimeout;
    }

    if (gArgs.IsArgSet("-paytxfee"))
    {
        if (!ParseMoney(gArgs.GetArg("-paytxfee", ""), nTransactionFee))
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), gArgs.GetArg("-paytxfee", "")));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
    }

    fConfChange = gArgs.GetBoolArg("-confchange", false);
    fEnforceCanonical = gArgs.GetBoolArg("-enforcecanonical", true);

    if (gArgs.IsArgSet("-mininput"))
    {
        if (!ParseMoney(gArgs.GetArg("-mininput", ""), nMinimumInputValue))
            return InitError(strprintf(_("Invalid amount for -mininput=<amount>: '%s'"), gArgs.GetArg("-mininput", "")));
    }

    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log
    // Sanity check
    if (!InitSanityCheck())
        return InitError(_("Initialization sanity check failed. Gridcoin is shutting down."));

    // Initialize internal hashing code with SSE/AVX2 optimizations. In the future we will also have ARM/NEON optimizations.
    std::string sha256_algo = SHA256AutoDetect();
    LogPrintf("Using the '%s' SHA256 implementation\n", sha256_algo);

    LogPrintf("Block version 11 hard fork configured for block %d", Params().GetConsensus().BlockV11Height);

    fs::path datadir = GetDataDir();
    fs::path walletFileName = gArgs.GetArg("-wallet", "wallet.dat");

    LogPrintf("INFO %s: DataDir = %s.", __func__, datadir.string());

    // WalletFileName must be a plain filename without a directory
    if (walletFileName != walletFileName.filename())
        return InitError(strprintf(_("Wallet %s resides outside data directory %s."), walletFileName.string(), datadir.string()));

    // Make sure only a single Bitcoin process is using the data directory.
    if (!DirIsWritable(datadir)) {
        return InitError(strprintf(_("Cannot write to data directory '%s'; check permissions."), datadir.string()));
    }
    if (!LockDirectory(datadir, ".lock", false)) {
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running."), datadir.string(), PACKAGE_NAME));
    }


#if !defined(WIN32)
    if (fDaemon)
    {
        // Daemonize
        pid_t pid = fork();
        if (pid < 0)
        {
            tfm::format(std::cerr, "Error: fork() returned %d errno %d\n", pid, errno);
            return false;
        }
        if (pid > 0)
        {
            CreatePidFile(GetPidFile(gArgs), pid);

            // Now that we are forked we can request a shutdown so the parent
            // exits while the child lives on.
            StartShutdown();
            return true;
        }

        pid_t sid = setsid();
        if (sid < 0)
            tfm::format(std::cerr, "Error: setsid() returned %d errno %d\n", sid, errno);
    }
#endif

    #if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        LogPrintf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
    #elif defined OPENSSL_VERSION
        LogPrintf("Using OpenSSL version %s\n", OpenSSL_version(OPENSSL_VERSION));
    #elif defined LIBRESSL_VERSION_TEXT
        LogPrintf("Using %s\n", LIBRESSL_VERSION_TEXT);
    #endif

    std::ostringstream strErrors;

    fDevbuildCripple = false;
    if ((CLIENT_VERSION_BUILD != 0) && !fTestNet)
    {
        fDevbuildCripple = true;
        if ((gArgs.GetArg("-devbuild", "") == "override"))
        {
            LogInstance().EnableCategory(BCLog::LogFlags::VERBOSE);
            fDevbuildCripple = false;
            LogPrintf("WARNING: Running development version outside of testnet in override mode!\n"
                      "VERBOSE logging is enabled.");
        }
        else
        {
            LogPrintf("WARNING: Running development version outside of testnet!\n"
                      "Staking and sending transactions will be disabled.");
        }
    }

    if (fDaemon)
        tfm::format(std::cout, "Gridcoin server starting\n");

    // ********************************************************* Step 5: verify database integrity

    uiInterface.InitMessage(_("Verifying database integrity..."));

    g_timer.LogTimer("init", true);
    g_timer.GetTimes("Starting verify of database integrity", "init");

    if (!bitdb.Open(GetDataDir()))
    {
         string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."), datadir.string());
        return InitError(msg);
    }


    if (gArgs.GetBoolArg("-salvagewallet"))
    {
        // Recover readable key pairs:
        if (!CWalletDB::Recover(bitdb, walletFileName.string(), true))
            return false;
    }

    if (fs::exists(GetDataDir() / walletFileName))
    {
        CDBEnv::VerifyResult r = bitdb.Verify(walletFileName.string(), CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK)
        {
            string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                     " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."), datadir.string());
            InitWarning(msg);
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return InitError(_("wallet.dat corrupt, salvage failed"));
    }

    g_timer.GetTimes("Finished verifying database integrity", "init");

    // ********************************************************* Step 6: network initialization

    int nSocksVersion = gArgs.GetArg("-socks", 5);

    if (nSocksVersion != 4 && nSocksVersion != 5)
        return InitError(strprintf(_("Unknown -socks proxy version requested: %i"), nSocksVersion));

    if (gArgs.GetArgs("-onlynet").size()) {
        std::set<enum Network> nets;
        for (auto const& snet : gArgs.GetArgs("-onlynet"))
        {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(_("Unknown network specified in -onlynet: '%s'"), snet));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetReachable(net, false);
        }
    }

    CService addrProxy;
    bool fProxy = false;
    if (gArgs.IsArgSet("-proxy")) {
        addrProxy = CService(gArgs.GetArg("-proxy", ""), 9050);
        if (!addrProxy.IsValid())
            return InitError(strprintf(_("Invalid -proxy address: '%s'"), gArgs.GetArg("-proxy", "")));

        if (!IsLimited(NET_IPV4))
            SetProxy(NET_IPV4, addrProxy, nSocksVersion);
        if (nSocksVersion > 4) {
            if (!IsLimited(NET_IPV6))
                SetProxy(NET_IPV6, addrProxy, nSocksVersion);
            SetNameProxy(addrProxy, nSocksVersion);
        }
        fProxy = true;
    }

    // -tor can override normal proxy, -notor disables Tor entirely
    if (gArgs.IsArgSet("-tor") && (fProxy || gArgs.IsArgSet("-tor"))) {
        CService addrOnion;
        if (!gArgs.IsArgSet("-tor"))
            addrOnion = addrProxy;
        else
            addrOnion = CService(gArgs.GetArg("-tor", ""), 9050);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -tor address: '%s'"), gArgs.GetArg("-tor", "")));
        SetProxy(NET_TOR, addrOnion, 5);
        SetReachable(NET_TOR, true);
    }

    // see Step 2: parameter interactions for more information about these
    fNoListen = !gArgs.GetBoolArg("-listen", true);
    fDiscover = gArgs.GetBoolArg("-discover", true);
    fNameLookup = gArgs.GetBoolArg("-dns", true);
#ifdef USE_UPNP
    fUseUPnP = gArgs.GetBoolArg("-upnp", USE_UPNP);
#endif

    bool fBound = false;
    if (!fNoListen)
    {
        std::string strError;
        if (gArgs.GetArgs("-bind").size()) {
            for (auto const& strBind : gArgs.GetArgs("-bind"))
            {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return InitError(strprintf(_("Cannot resolve -bind address: '%s'"), strBind));
                fBound |= Bind(addrBind);
            }
        } else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
            if (!IsLimited(NET_IPV6))
                fBound |= Bind(CService(in6addr_any, GetListenPort()), false);
            if (!IsLimited(NET_IPV4))
                fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound);
        }
        if (!fBound)
            return InitError(_("Failed to listen on any port. Use -listen=0 if you want this."));
    }

    if (gArgs.GetArgs("-externalip").size())
    {
        for (auto const& strAddr : gArgs.GetArgs("-externalip"))
        {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

    if (gArgs.IsArgSet("-reservebalance")) // ppcoin: reserve balance amount
    {
        if (!ParseMoney(gArgs.GetArg("-reservebalance", ""), nReserveBalance))
        {
            InitError(_("Invalid amount for -reservebalance=<amount>"));
            return false;
        }
    }

    for (auto const& strDest : gArgs.GetArgs("-seednode"))
    {
        AddOneShot(strDest);
    }

    g_timer.GetTimes("Finished initializing network", "init");

    // ********************************************************* Step 7: load blockchain

    if (!bitdb.Open(GetDataDir()))
    {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."), datadir.string());
        return InitError(msg);
    }

    if (gArgs.GetBoolArg("-loadblockindextest"))
    {
        CTxDB txdb("r");
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    uiInterface.InitMessage(_("Loading block index..."));
    LogPrintf("Loading block index...");
    if (!LoadBlockIndex())
        return InitError(_("Error loading blkindex.dat"));

    // as LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill bitcoin-qt during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown)
    {
        LogPrintf("Shutdown requested. Exiting.");
        return false;
    }

    g_timer.GetTimes("Finished loading block chain", "init");

    if (gArgs.GetBoolArg("-printblockindex") || gArgs.GetBoolArg("-printblocktree"))
    {
        PrintBlockTree();
        return false;
    }

    if (gArgs.IsArgSet("-printblock"))
    {
        string strMatch = gArgs.GetArg("-printblock", "");
        int nFound = 0;
        for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = mi->first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = mi->second;
                CBlock block;
                ReadBlockFromDisk(block, pindex, Params().GetConsensus());
                block.print();
                LogPrintf("");
                nFound++;
            }
        }
        if (nFound == 0)
            LogPrintf("No blocks matching %s were found", strMatch);
        return false;
    }

    // ********************************************************* Step 8: load wallet

    uiInterface.InitMessage(_("Loading wallet..."));
    LogPrintf("Loading wallet...");
    bool fFirstRun = true;
    pwalletMain = new CWallet(walletFileName.string());
    DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            string msg(_("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                         " or address book entries might be missing or incorrect."));
            InitWarning(msg);
        }
        else if (nLoadWalletRet == DB_TOO_NEW)
            strErrors << _("Error loading wallet.dat: Wallet requires newer version of Gridcoin") << "\n";
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            strErrors << _("Wallet needed to be rewritten: restart Gridcoin to complete") << "\n";
            LogPrintf("%s", strErrors.str());
            return InitError(strErrors.str());
        }
        else
            strErrors << _("Error loading wallet.dat") << "\n";
    }

    if (gArgs.GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = gArgs.GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        }
        else
            LogPrintf("Allowing wallet upgrade up to %i", nMaxVersion);
        if (nMaxVersion < pwalletMain->GetVersion())
            strErrors << _("Cannot downgrade wallet") << "\n";
        pwalletMain->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun)
    {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();

        // So Clang doesn't complain, even though we are really essentially single-threaded here.
        LOCK(pwalletMain->cs_wallet);

        CPubKey newDefaultKey;
        if (pwalletMain->GetKeyFromPool(newDefaultKey, false)) {
            pwalletMain->SetDefaultKey(newDefaultKey);
            if (!pwalletMain->SetAddressBookName(pwalletMain->vchDefaultKey.GetID(), ""))
                strErrors << _("Cannot write default address") << "\n";
        }
    }

    LogPrintf("%s", strErrors.str());

    g_timer.GetTimes("Finished loading wallet file", "init");

    // Zap wallet transactions if specified as a command line argument.
    if (gArgs.GetBoolArg("-zapwallettxes", false))
    {
        std::vector<CWalletTx> vWtx;

        LogPrintf("Zapping wallet transactions.");

        DBErrors nZapWalletTxRet = pwalletMain->ZapWalletTx(vWtx);

        LogPrintf("%u transactions zapped.", vWtx.size());

        if (nZapWalletTxRet != DBErrors::DB_LOAD_OK)
        {
            strErrors << _("Error loading %s: Wallet corrupted") << walletFileName.string() << "\n";
        }
    }

    RegisterWallet(pwalletMain);

    CBlockIndex *pindexRescan = pindexBest;
    if (gArgs.GetBoolArg("-rescan"))
        pindexRescan = pindexGenesisBlock;
    else
    {
        CWalletDB walletdb(walletFileName.string());
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = locator.GetBlockIndex();
    }
    if (pindexBest != pindexRescan && pindexBest && pindexRescan && pindexBest->nHeight > pindexRescan->nHeight)
    {
        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...", pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);

        g_timer.GetTimes("rescan start", "init");

        pwalletMain->ScanForWalletTransactions(pindexRescan, true);

        g_timer.GetTimes("rescan complete", "init");
    }

    // ********************************************************* Step 9: import blocks

    if (gArgs.GetArgs("-loadblock").size())
    {
        uiInterface.InitMessage(_("Importing blockchain data file."));

        for (auto const& strFile : gArgs.GetArgs("-loadblock"))
        {
            FILE *file = fsbridge::fopen(strFile, "rb");
            if (file) {
                LoadExternalBlockFile(file);
            }
        }
        exit(0);

        g_timer.GetTimes("load blockchain file complete", "init");
    }

    fs::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (fs::exists(pathBootstrap)) {
        uiInterface.InitMessage(_("Importing bootstrap blockchain data file."));

        FILE *file = fsbridge::fopen(pathBootstrap, "rb");
        if (file) {
            fs::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LoadExternalBlockFile(file);
            if (!RenameOver(pathBootstrap, pathBootstrapOld))
            {
                uiInterface.InitMessage(_("Failed to rename bootstrap file to .old for backup purposes."));
            }
        }

        g_timer.GetTimes("load bootstrap file complete", "init");
    }

    // ********************************************************* Step 10: load peers

    // Ban manager instance should not already be instantiated
    assert(!g_banman);
    // Create ban manager instance.
    g_banman = std::make_unique<BanMan>(GetDataDir() / "banlist.dat", &uiInterface, gArgs.GetArg("-bantime", DEFAULT_MISBEHAVING_BANTIME));

    uiInterface.InitMessage(_("Loading addresses..."));
    LogPrint(BCLog::LogFlags::NOISY, "Loading addresses...");

    {
        CAddrDB adb;
        if (!adb.Read(addrman))
            LogPrintf("Invalid or missing peers.dat; recreating");
    }

    LogPrintf("Loaded %i addresses from peers.dat.", addrman.size());
    g_timer.GetTimes("Load peers complete", "init");

    // ********************************************************* Step 11: start node
    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    if (!GRC::Initialize(threads, pindexBest)) {
        return false;
    }

    //// debug print
    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
    {
        LogPrintf("mapBlockIndex.size() = %" PRIszu,   mapBlockIndex.size());
        LogPrintf("nBestHeight = %d",            nBestHeight);

        // So Clang doesn't complian, even though we are essentially single-threaded here.
        LOCK(pwalletMain->cs_wallet);

        LogPrintf("setKeyPool.size() = %" PRIszu,      pwalletMain->setKeyPool.size());
        LogPrintf("mapWallet.size() = %" PRIszu,       pwalletMain->mapWallet.size());
        LogPrintf("mapAddressBook.size() = %" PRIszu,  pwalletMain->mapAddressBook.size());
    }

    if (!threads->createThread(StartNode, nullptr, "Start Thread"))
        InitError(_("Error: could not start node"));

    if (fServer) StartRPCThreads();

    // ********************************************************* Step 12: finished

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    // Add wallet transactions that aren't already in a block to mapTransactions
    pwalletMain->ReacceptWalletTransactions();

    int nMismatchSpent;
    int64_t nBalanceInQuestion;
    pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion);

    // Start the lightweight task scheduler thread
    CScheduler::Function serviceLoop = std::bind(&CScheduler::serviceQueue, &scheduler);
    threadGroup.create_thread(std::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

    // TODO: Do we need this? It would require porting the Bitcoin signal handler.
    // GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

    scheduler.scheduleEvery([]{
        g_banman->DumpBanlist();
    }, std::chrono::seconds{DUMP_BANS_INTERVAL});

    GRC::ScheduleBackgroundJobs(scheduler);

    uiInterface.InitMessage(_("Done loading"));
    g_timer.GetTimes("Done loading", "init");

    g_timer.DeleteTimer("init");

    return true;
}
