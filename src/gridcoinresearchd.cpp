// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "chainparams.h"
#include "chainparamsbase.h"
#include "util.h"
#include "net.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "init.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "ui_interface.h"
#include "gridcoin/upgrade.h"

#include <boost/thread.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <stdio.h>

extern bool fQtActive;

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif

    SetupEnvironment();

    // Note every function above the InitLogging() call must use fprintf or similar.

    ThreadHandlerPtr threads = std::make_shared<ThreadHandler>();
    bool fRet = false;

    try
    {
        //
        // Parameters
        //
        // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's main()
        ParseParameters(argc, argv);

        if (mapArgs.count("-?") || mapArgs.count("-help"))
        {
            // First part of help message is specific to bitcoind / RPC client
            std::string strUsage = _("Gridcoin version") + " " + FormatFullVersion() + "\n\n" +
                _("Usage:") + "\n" +
                  "  gridcoinresearchd [options]                     " + "\n" +
                  "  gridcoinresearchd [options] <command> [params]  " + _("Send command to -server or gridcoinresearchd") + "\n" +
                  "  gridcoinresearchd [options] help                " + _("List commands") + "\n" +
                  "  gridcoinresearchd [options] help <command>      " + _("Get help for a command") + "\n";

            strUsage += "\n" + HelpMessage();

            fprintf(stdout, "%s", strUsage.c_str());
            return false;
        }

        if (mapArgs.count("-version"))
        {
            fprintf(stdout, "%s", VersionMessage().c_str());

            return false;
        }

        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified directory does not exist\n");
            Shutdown(NULL);
        }

        /** Check here config file in case TestNet is set there and not in mapArgs **/
        SelectParams(CBaseChainParams::MAIN);
        ReadConfigFile(mapArgs, mapMultiArgs);
        SelectParams(mapArgs.count("-testnet") ? CBaseChainParams::TESTNET : CBaseChainParams::MAIN);

        // Command-line RPC  - Test this - ensure single commands execute and exit please.
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "gridcoinresearchd"))
                fCommandLine = true;

        if (fCommandLine)
        {
            int ret = CommandLineRPC(argc, argv);
            exit(ret);
        }

        // Initialize logging as early as possible.
        InitLogging();

        // Check to see if the user requested a snapshot and we are not running TestNet!
        if (mapArgs.count("-snapshotdownload") && !mapArgs.count("-testnet"))
        {
            GRC::Upgrade snapshot;

            // Let's check make sure gridcoin is not already running in the data directory.
            // Use new probe feature
            if (!LockDirectory(GetDataDir(), ".lock", false))
            {
                fprintf(stderr, "Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.", GetDataDir().string().c_str());

                exit(1);
            }

            else
            {
                try
                {
                    snapshot.SnapshotMain();
                }

                catch (std::runtime_error& e)
                {
                    LogPrintf("Snapshot Downloader: Runtime exception occurred in SnapshotMain() (%s)", e.what());

                    snapshot.DeleteSnapshot();

                    exit(1);
                }
            }

            // Delete snapshot file
            snapshot.DeleteSnapshot();
        }

        LogPrintf("AppInit");

        fRet = AppInit2(threads);
    }
    catch (std::exception& e) {
        LogPrintf("AppInit()Exception1");

        PrintException(&e, "AppInit()");
    } catch (...) {
        LogPrintf("AppInit()Exception2");

        PrintException(NULL, "AppInit()");
    }
    if(fRet)
    {
        while (!ShutdownRequested())
            MilliSleep(500);
    }

    Shutdown(NULL);

    // delete thread handler
    threads->interruptAll();
    threads->removeAll();
    threads.reset();

    return fRet;

}

extern void noui_connect();
int main(int argc, char* argv[])
{
    // Reinit default timer to ensure it is zeroed out at the start of main.
    g_timer.InitTimer("default", false);

    bool fRet = false;

    // Set global boolean to indicate intended absence of GUI to core...
    fQtActive = false;

    // Connect bitcoind signal handlers
    noui_connect();

    fRet = AppInit(argc, argv);

    if (fRet && fDaemon)
        return 0;

    return 1;
}
