// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "chainparams.h"
#include "util.h"
#include "net.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "banman.h"
#include "rpc/server.h"
#include "init.h"
#include "ui_interface.h"
#include "scheduler.h"
#include "gridcoin/gridcoin.h"

#include <boost/algorithm/string/predicate.hpp>
#include <openssl/crypto.h>

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()

static boost::thread_group threadGroup;
static CScheduler scheduler;

extern void ThreadAppInit2(void* parg);
bool IsConfigFileEmpty();

#ifndef WIN32
#include <signal.h>
#include <sys/stat.h>
#endif

using namespace std;
CWallet* pwalletMain;
CClientUIInterface uiInterface;
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

std::unique_ptr<BanMan> g_banman;

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

        // clean up the threads running serviceQueue:
        threadGroup.interrupt_all();
        threadGroup.join_all();

        bitdb.Flush(false);
        StopNode();
        bitdb.Flush(true);
        StopRPCThreads();

        // This is necessary here to prevent a snapshot download from failing at the cleanup
        // step because of a write lock on accrual/registry.dat.
        GRC::CloseResearcherRegistryFile();

        fs::remove(GetPidFile());
        UnregisterWallet(pwalletMain);
        delete pwalletMain;
        // close transaction database to prevent lock issue on restart
        // This causes issues on daemons where it tries to create a second
        // lock file.
        //CTxDB().Close();
        MilliSleep(50);
        LogPrintf("Gridcoin exited");
        fExit = true;
    }
    else
    {
        while (!fExit)
            MilliSleep(100);
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

bool static InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, _("Gridcoin"), CClientUIInterface::OK | CClientUIInterface::MODAL);
    return false;
}

bool static InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, _("Gridcoin"), CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
    return true;
}


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
        << "addnode=gridcoin.crypto.fans\n"
        << "addnode=gridcoin.ddns.net\n"
        << "addnode=london.grcnode.co.uk\n"
        << "addnode=seeds.gridcoin.ifoggz-network.xyz\n"
        << "addnode=seed.gridcoin.pl\n"
        << "addnode=www.grcpool.com\n";
}

// Core-specific options shared between UI and daemon
std::string HelpMessage()
{
    //gridcoinresearch ports: testnet ? 32748 : 32749;
    string strUsage = _("Options:") + "\n" +
        "  -?                     " + _("This help message") + "\n" +
        "  -version               " + _("Print version and exit") + "\n" +
        "  -conf=<file>           " + _("Specify configuration file (default: gridcoinresearch.conf)") + "\n" +
        "  -pid=<file>            " + _("Specify pid file (default: gridcoind.pid)") + "\n" +
        "  -datadir=<dir>         " + _("Specify data directory") + "\n" +
        "  -wallet=<dir>          " + _("Specify wallet file (within data directory)") + "\n" +
        "  -dbcache=<n>           " + _("Set database cache size in megabytes (default: 25)") + "\n" +
        "  -dblogsize=<n>         " + _("Set database disk log size in megabytes (default: 100)") + "\n" +
        "  -timeout=<n>           " + _("Specify connection timeout in milliseconds (default: 5000)") + "\n" +
        "  -peertimeout=<n>       " + _("Specify p2p connection timeout in seconds. This option determines the amount of time a peer may be inactive before the connection to it is dropped. (minimum: 1, default: 45)") + "\n"
        "  -proxy=<ip:port>       " + _("Connect through socks proxy") + "\n" +
        "  -socks=<n>             " + _("Select the version of socks proxy to use (4-5, default: 5)") + "\n" +
        "  -tor=<ip:port>         " + _("Use proxy to reach tor hidden services (default: same as -proxy)") + "\n"
        "  -dns                   " + _("Allow DNS lookups for -addnode, -seednode and -connect") + "\n" +
        "  -port=<port>           " + _("Listen for connections on <port> (default: 32749 or testnet: 32748)") + "\n" +
        "  -maxconnections=<n>    "    + _("Maintain at most <n> connections to peers (default: 125)") + "\n" +
        "  -maxoutboundconnections=<n>"+ _("Maximum number of outbound connections (default: 8)") + "\n" +
        "  -addnode=<ip>          " + _("Add a node to connect to and attempt to keep the connection open") + "\n" +
        "  -connect=<ip>          " + _("Connect only to the specified node(s)") + "\n" +
        "  -seednode=<ip>         " + _("Connect to a node to retrieve peer addresses, and disconnect") + "\n" +
        "  -externalip=<ip>       " + _("Specify your own public address") + "\n" +
        "  -onlynet=<net>         " + _("Only connect to nodes in network <net> (IPv4, IPv6 or Tor)") + "\n" +
        "  -discover              " + _("Discover own IP address (default: 1 when listening and no -externalip)") + "\n" +
        "  -listen                " + _("Accept connections from outside (default: 1 if no -proxy or -connect)") + "\n" +
        "  -bind=<addr>           " + _("Bind to given address. Use [host]:port notation for IPv6") + "\n" +
        "  -dnsseed               " + _("Find peers using DNS lookup (default: 1)") + "\n" +
        "  -synctime              " + _("Sync time with other nodes. Disable if time on your system is precise e.g. syncing with NTP (default: 1)") + "\n" +
        "  -banscore=<n>          " + _("Threshold for disconnecting misbehaving peers (default: 100)") + "\n" +
        "  -bantime=<n>           " + _("Number of seconds to keep misbehaving peers from reconnecting (default: 86400)") + "\n" +
        "  -maxreceivebuffer=<n>  " + _("Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)") + "\n" +
        "  -maxsendbuffer=<n>     " + _("Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)") + "\n" +
#ifdef USE_UPNP
#if USE_UPNP
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 1 when listening)") + "\n" +
#else
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 0)") + "\n" +
#endif
#endif
        "  -paytxfee=<amt>        " + _("Fee per KB to add to transactions you send") + "\n" +
        "  -mininput=<amt>        " + _("When creating transactions, ignore inputs with value less than this (default: 0.01)") + "\n";
	if(fQtActive)
		strUsage +=
        "  -server                " + _("Accept command line and JSON-RPC commands") + "\n";
#if !defined(WIN32)
    if(!fQtActive)
		strUsage +=
        "  -daemon                " + _("Run in the background as a daemon and accept commands") + "\n";
#endif
    strUsage +=
        "  -testnet               " + _("Use the test network") + "\n" +
        "  -debug                 " + _("Output extra debugging information.") + "\n" +
        "  -logtimestamps         " + _("Prepend debug output with timestamp") + "\n" +
        "  -shrinkdebugfile       " + _("Shrink debug.log file on client startup (default: 1 when no -debug)") + "\n" +
        "  -printtoconsole        " + _("Send trace/debug info to console instead of debug.log file") + "\n" +
#ifdef WIN32
        "  -printtodebugger       " + _("Send trace/debug info to debugger") + "\n" +
#endif
        "  -rpcuser=<user>        " + _("Username for JSON-RPC connections") + "\n" +
        "  -rpcpassword=<pw>      " + _("Password for JSON-RPC connections") + "\n" +
        "  -rpcport=<port>        " + _("Listen for JSON-RPC connections on <port> (default: 15715 or testnet: 25715)") + "\n" +
        "  -rpcallowip=<ip>       " + _("Allow JSON-RPC connections from specified IP address") + "\n" +
        "  -rpcconnect=<ip>       " + _("Send commands to node running on <ip> (default: 127.0.0.1)") + "\n" +
        "  -rpcthreads=<n>        " + _("Set the number of threads to service RPC calls (default: 4)") + "\n" +
        "  -blocknotify=<cmd>     " + _("Execute command when the best block changes (%s in cmd is replaced by block hash)") + "\n" +
        "  -walletnotify=<cmd>    " + _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)") + "\n" +
        "  -confchange            " + _("Require a confirmations for change (default: 0)") + "\n" +
        "  -enforcecanonical      " + _("Enforce transaction scripts to use canonical PUSH operators (default: 1)") + "\n" +
        "  -alertnotify=<cmd>     " + _("Execute command when a relevant alert is received (%s in cmd is replaced by message)") + "\n" +
        "  -upgradewallet         " + _("Upgrade wallet to latest format") + "\n" +
        "  -keypool=<n>           " + _("Set key pool size to <n> (default: 100)") + "\n" +
        "  -rescan                " + _("Rescan the block chain for missing wallet transactions") + "\n" +
        "  -salvagewallet         " + _("Attempt to recover private keys from a corrupt wallet.dat") + "\n" +
        "  -zapwallettxes         " + _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") + "\n" +
        "  -checkblocks=<n>       " + _("How many blocks to check at startup (default: 2500, 0 = all)") + "\n" +
        "  -checklevel=<n>        " + _("How thorough the block verification is (0-6, default: 1)") + "\n" +
        "  -loadblock=<file>      " + _("Imports blocks from external blk000?.dat file") + "\n" +

        "  -walletbackupinterval=<n>     " + _("DEPRECATED: Optional: Create a wallet backup every <n> blocks. Zero disables backups") + "\n"
        "  -walletbackupintervalsecs=<n> " + _("Optional: Create a wallet backup every <n> seconds. Zero disables backups (default: 86400)") + "\n"

        "\n" + _("Block creation options:") + "\n" +
        "  -blockminsize=<n>      "   + _("Set minimum block size in bytes (default: 0)") + "\n" +
        "  -blockmaxsize=<n>      "   + _("Set maximum block size in bytes (default: 250000)") + "\n" +
        "  -blockprioritysize=<n> "   + _("Set maximum size of high-priority/low-fee transactions in bytes (default: 27000)") + "\n" +

        "\n" + _("Research reward system options:") + "\n" +
        "  -email=<email>         "   + _("Email address to use for CPID detection. Must match your BOINC account email") + "\n" +
        "  -boincdatadir=<path>   "   + _("Path to the BOINC data directory for CPID detection when the BOINC client uses a non-default directory") + "\n" +
        "  -forcecpid=<cpid>      "   + _("Override automatic CPID detection with the specified CPID") + "\n" +
        "  -investor              "   + _("Disable CPID detection and do not participate in the research reward system") + "\n" +
        "  -pooloperator          "   + _("Skip pool CPID checks for staking nodes run by pool administrators") + "\n" +

        "\n" + _("SSL options: (see the Bitcoin Wiki for SSL setup instructions)") + "\n" +
        "  -rpcssl                                  " + _("Use OpenSSL (https) for JSON-RPC connections") + "\n" +
        "  -rpcsslcertificatechainfile=<file.cert>  " + _("Server certificate file (default: server.cert)") + "\n" +
        "  -rpcsslprivatekeyfile=<file.pem>         " + _("Server private key (default: server.pem)") + "\n" +
        "  -rpcsslciphers=<ciphers>                 " + _("Acceptable ciphers (default: TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH)") + "\n"

        "\n" + _("Update/Snapshot options:") + "\n"
        "  -snapshotdownload            " + _("Download and apply latest snapshot") + "\n"
        "  -snapshoturl=<url>           " + _("Optional: Specify url of snapshot.zip file (ex: https://sub.domain.com/location/snapshot.zip)") + "\n"
        "  -snapshotsha256url=<url>     " + _("Optional: Specify url of snapshot.sha256 file (ex: https://sub.domain.com/location/snapshot.sha256)") + "\n"
        "  -disableupdatecheck          " + _("Optional: Disable update checks by wallet") + "\n"
        "  -updatecheckinterval=<hours> " + _("Optional: Specify custom update interval checks in hours (Default: 120 hours (minimum 1 hour))") + "\n"
        "  -updatecheckurl=<url>        " + _("Optional: Specify url of update version checks (ex: https://sub.domain.com/location/latest") + "\n";

    return strUsage;
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
    fPrintToConsole = GetBoolArg("-printtoconsole");
    fPrintToDebugger = GetBoolArg("-printtodebugger");
    fLogTimestamps = GetBoolArg("-logtimestamps", true);

    LogInstance().m_print_to_file = !IsArgNegated("-debuglogfile");
    LogInstance().m_file_path = AbsPathForConfigVal(GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
    LogInstance().m_print_to_console = fPrintToConsole;
    LogInstance().m_log_timestamps = fLogTimestamps;
    LogInstance().m_log_time_micros = GetBoolArg("-logtimemicros", DEFAULT_LOGTIMEMICROS);
    LogInstance().m_log_threadnames = GetBoolArg("-logthreadnames", DEFAULT_LOGTHREADNAMES);

    fLogIPs = GetBoolArg("-logips", DEFAULT_LOGIPS);

    if (LogInstance().m_print_to_file)
    {
        // Only shrink debug file at start if log archiving is set to false.
        if (!GetBoolArg("-logarchivedaily", true) && GetBoolArg("-shrinkdebugfile", LogInstance().DefaultShrinkDebugFile()))
        {
            // Do this first since it both loads a bunch of debug.log into memory,
            // and because this needs to happen before any other debug.log printing
            LogInstance().ShrinkDebugFile();
        }
    }

    if (IsArgSet("-debug"))
    {
        // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
        std::vector<std::string> categories;

        if (mapArgs.count("-debug") && mapMultiArgs["-debug"].size() > 0)
        {
            for (auto const& sSubParam : mapMultiArgs["-debug"])
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

    if (mapArgs.count("-debugexclude") && mapMultiArgs["-debugexclude"].size() > 0)
    {
        for (auto const& sSubParam : mapMultiArgs["-debugexclude"])
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

    bGridcoinCoreInitComplete = false;

    LogPrintf("Initializing Core...");

    AppInit2(th);

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
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
// We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
// which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL (WINAPI *PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != NULL) setProcDEPPol(PROCESS_DEP_ENABLE);
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
            ReadConfigFile(mapArgs, mapMultiArgs);
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

    nNodeLifespan = GetArg("-addrlifespan", 7);
    fUseFastIndex = GetBoolArg("-fastindex", false);

    nMinerSleep = GetArg("-minersleep", 8000);

    nDerivationMethodIndex = 0;
    fTestNet = GetBoolArg("-testnet");

    if (mapArgs.count("-bind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        SoftSetBoolArg("-listen", true);
    }

    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        SoftSetBoolArg("-dnsseed", false);
        SoftSetBoolArg("-listen", false);
    }

    if (mapArgs.count("-proxy")) {
        // to protect privacy, do not listen by default if a proxy server is specified
        SoftSetBoolArg("-listen", false);
    }

    if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        SoftSetBoolArg("-upnp", false);
        SoftSetBoolArg("-discover", false);
    }

    if (mapArgs.count("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        SoftSetBoolArg("-discover", false);
    }

    if (GetBoolArg("-salvagewallet")) {
        // Rewrite just private keys: rescan to find transactions
        SoftSetBoolArg("-rescan", true);
    }

     if (GetBoolArg("-zapwallettxes", false)) {
        // -zapwallettx implies a rescan
        SoftSetBoolArg("-rescan", true);
    }

    // Verify testnet is using the testnet directory for the config file:
    std::string sTestNetSpecificArg = GetArgument("testnetarg","default");
    LogPrintf("Using specific arg %s",sTestNetSpecificArg);


    // ********************************************************* Step 3: parameter-to-internal-flags

    if (GetArg("-debug", "false") == "true")
    {
            LogPrintf("Enabling debug category VERBOSE from legacy debug.");
            LogInstance().EnableCategory(BCLog::LogFlags::VERBOSE);
    }

    if (GetArg("-debug10", "false") == "true")
    {
            LogPrintf("Entering debug category NOISY from legacy debug mode 10.");
            LogInstance().EnableCategory(BCLog::LogFlags::NOISY);
    }


#if defined(WIN32)
    fDaemon = false;
#else
    if(fQtActive)
        fDaemon = false;
    else
        fDaemon = GetBoolArg("-daemon");
#endif

    if (fDaemon)
        fServer = true;
    else
        fServer = GetBoolArg("-server");

    /* force fServer when running without GUI */
    if(!fQtActive)
        fServer = true;

    if (mapArgs.count("-timeout"))
    {
        int nNewTimeout = GetArg("-timeout", 5000);
        if (nNewTimeout > 0 && nNewTimeout < 600000)
            nConnectTimeout = nNewTimeout;
    }

    if (mapArgs.count("-peertimeout"))
    {
        int nNewPeerTimeout = GetArg("-peertimeout", 45);

        if (nNewPeerTimeout <= 0)
            InitError(strprintf(_("Invalid amount for -peertimeout=<amount>: '%s'"), mapArgs["-peertimeout"]));

        PEER_TIMEOUT = nNewPeerTimeout;
    }

    if (mapArgs.count("-paytxfee"))
    {
        if (!ParseMoney(mapArgs["-paytxfee"], nTransactionFee))
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"]));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
    }

    fConfChange = GetBoolArg("-confchange", false);
    fEnforceCanonical = GetBoolArg("-enforcecanonical", true);

    if (mapArgs.count("-mininput"))
    {
        if (!ParseMoney(mapArgs["-mininput"], nMinimumInputValue))
            return InitError(strprintf(_("Invalid amount for -mininput=<amount>: '%s'"), mapArgs["-mininput"]));
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
    fs::path walletFileName = GetArg("-wallet", "wallet.dat");

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
            fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
            return false;
        }
        if (pid > 0)
        {
            CreatePidFile(GetPidFile(), pid);

            // Now that we are forked we can request a shutdown so the parent
            // exits while the child lives on.
            StartShutdown();
            return true;
        }

        pid_t sid = setsid();
        if (sid < 0)
            fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
    }
#endif

    LogPrintf("Using OpenSSL version %s", SSLeay_version(SSLEAY_VERSION));

    std::ostringstream strErrors;

    fDevbuildCripple = false;
    if((CLIENT_VERSION_BUILD != 0) && !fTestNet)
    {
        fDevbuildCripple = true;
        LogPrintf("WARNING: Running development version outside of testnet!\n"
                  "Staking and sending transactions will be disabled.");
        if( (GetArg("-devbuild", "") == "override") && LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
            fDevbuildCripple = false;
    }

    if (fDaemon)
        fprintf(stdout, "Gridcoin server starting\n");

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


    if (GetBoolArg("-salvagewallet"))
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
            uiInterface.ThreadSafeMessageBox(msg, _("Gridcoin"),
                CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return InitError(_("wallet.dat corrupt, salvage failed"));
    }

    g_timer.GetTimes("Finished verifying database integrity", "init");

    // ********************************************************* Step 6: network initialization

    int nSocksVersion = GetArg("-socks", 5);

    if (nSocksVersion != 4 && nSocksVersion != 5)
        return InitError(strprintf(_("Unknown -socks proxy version requested: %i"), nSocksVersion));

    if (mapArgs.count("-onlynet")) {
        std::set<enum Network> nets;
        for (auto const& snet : mapMultiArgs["-onlynet"])
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
    if (mapArgs.count("-proxy")) {
        addrProxy = CService(mapArgs["-proxy"], 9050);
        if (!addrProxy.IsValid())
            return InitError(strprintf(_("Invalid -proxy address: '%s'"), mapArgs["-proxy"]));

        if (!IsLimited(NET_IPV4))
            SetProxy(NET_IPV4, addrProxy, nSocksVersion);
        if (nSocksVersion > 4) {
            if (!IsLimited(NET_IPV6))
                SetProxy(NET_IPV6, addrProxy, nSocksVersion);
            SetNameProxy(addrProxy, nSocksVersion);
        }
        fProxy = true;
    }

    // -tor can override normal proxy, -notor disables tor entirely
    if (!(mapArgs.count("-tor") && mapArgs["-tor"] == "0") && (fProxy || mapArgs.count("-tor"))) {
        CService addrOnion;
        if (!mapArgs.count("-tor"))
            addrOnion = addrProxy;
        else
            addrOnion = CService(mapArgs["-tor"], 9050);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -tor address: '%s'"), mapArgs["-tor"]));
        SetProxy(NET_TOR, addrOnion, 5);
        SetReachable(NET_TOR, true);
    }

    // see Step 2: parameter interactions for more information about these
    fNoListen = !GetBoolArg("-listen", true);
    fDiscover = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);
#ifdef USE_UPNP
    fUseUPnP = GetBoolArg("-upnp", USE_UPNP);
#endif

    bool fBound = false;
    if (!fNoListen)
    {
        std::string strError;
        if (mapArgs.count("-bind")) {
            for (auto const& strBind : mapMultiArgs["-bind"])
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

    if (mapArgs.count("-externalip"))
    {
        for (auto const& strAddr : mapMultiArgs["-externalip"])
        {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

    if (mapArgs.count("-reservebalance")) // ppcoin: reserve balance amount
    {
        if (!ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        {
            InitError(_("Invalid amount for -reservebalance=<amount>"));
            return false;
        }
    }

    for (auto const& strDest : mapMultiArgs["-seednode"])
        AddOneShot(strDest);

    g_timer.GetTimes("Finished initializing network", "init");

    // ********************************************************* Step 7: load blockchain

    if (!bitdb.Open(GetDataDir()))
    {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."), datadir.string());
        return InitError(msg);
    }

    if (GetBoolArg("-loadblockindextest"))
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

    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree"))
    {
        PrintBlockTree();
        return false;
    }

    if (mapArgs.count("-printblock"))
    {
        string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                block.ReadFromDisk(pindex);
                block.BuildMerkleTree();
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
            uiInterface.ThreadSafeMessageBox(msg, _("Gridcoin"), CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
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

    if (GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = GetArg("-upgradewallet", 0);
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
    if (GetBoolArg("-zapwallettxes", false))
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
    if (GetBoolArg("-rescan"))
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

    if (mapArgs.count("-loadblock"))
    {
        uiInterface.InitMessage(_("Importing blockchain data file."));

        for (auto const& strFile : mapMultiArgs["-loadblock"])
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
            RenameOver(pathBootstrap, pathBootstrapOld);
        }

        g_timer.GetTimes("load bootstrap file complete", "init");
    }

    // ********************************************************* Step 10: load peers

    // Ban manager instance should not already be instantiated
    assert(!g_banman);
    // Create ban manager instance.
    g_banman = MakeUnique<BanMan>(GetDataDir() / "banlist.dat", &uiInterface, GetArg("-bantime", DEFAULT_MISBEHAVING_BANTIME));

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

    GRC::Initialize(threads, pindexBest);

    //// debug print
    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
    {
        LogPrintf("mapBlockIndex.size() = %" PRIszu,   mapBlockIndex.size());
        LogPrintf("nBestHeight = %d",            nBestHeight);
        LogPrintf("setKeyPool.size() = %" PRIszu,      pwalletMain->setKeyPool.size());
        LogPrintf("mapWallet.size() = %" PRIszu,       pwalletMain->mapWallet.size());
        LogPrintf("mapAddressBook.size() = %" PRIszu,  pwalletMain->mapAddressBook.size());
    }

    if (!threads->createThread(StartNode, NULL, "Start Thread"))
        InitError(_("Error: could not start node"));

    if (fServer)
        StartRPCThreads();

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
    }, DUMP_BANS_INTERVAL * 1000);

    GRC::ScheduleBackgroundJobs(scheduler);

    uiInterface.InitMessage(_("Done loading"));
    g_timer.GetTimes("Done loading", "init");

    g_timer.DeleteTimer("init");

    return true;
}
