// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "chainparams.h"
#include "chainparamsbase.h"
#include "util.h"
#include "util/threadnames.h"
#include "net.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "init.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "node/ui_interface.h"
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
    util::ThreadSetInternalName("gridcoinresearchd-main");

    SetupServerArgs();

    // Note every function above the InitLogging() call must use tfm::format or similar.

    ThreadHandlerPtr threads = std::make_shared<ThreadHandler>();
    bool fRet = false;

    try
    {
        //
        // Parameters
        //
        // If Qt is used, parameters/gridcoinresearch.conf are parsed in qt/bitcoin.cpp's main()
        std::string error;
        if (!gArgs.ParseParameters(argc, argv, error)) {
            return InitError(strprintf("Error parsing command line arguments: %s\n", error));
        }
        if (HelpRequested(gArgs))
        {
            // First part of help message is specific to bitcoind / RPC client
            std::string strUsage = _("Gridcoin version") + " " + FormatFullVersion() + "\n\n" +
                _("Usage:") + "\n" +
                  "  gridcoinresearchd [options]                     " + "\n" +
                  "  gridcoinresearchd [options] <command> [params]  " + _("Send command to -server or gridcoinresearchd") + "\n" +
                  "  gridcoinresearchd [options] help                " + _("List commands") + "\n" +
                  "  gridcoinresearchd [options] help <command>      " + _("Get help for a command") + "\n";
            strUsage += "\n" + gArgs.GetHelpMessage();

            tfm::format(std::cout, "%s", strUsage);
            return true;
        }

        if (gArgs.IsArgSet("-version"))
        {
            tfm::format(std::cout, "%s", VersionMessage().c_str());

            return true;
        }

        if (!CheckDataDirOption()) {
            return InitError(strprintf("Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "")));
        }

        /** Check mainnet config file first in case testnet is set there and not in command line args **/
        SelectParams(CBaseChainParams::MAIN);

        // Currently unused.
        std::string error_msg;

        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            return InitError("Config file cannot be parsed. Cannot continue.\n");
        }

        SelectParams(gArgs.IsArgSet("-testnet") ? CBaseChainParams::TESTNET : CBaseChainParams::MAIN);

        // reread config file after correct chain is selected
        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            return InitError("Config file cannot be parsed. Cannot continue.\n");
        }

        if (!gArgs.InitSettings(error)) {
            return InitError("Error initializing settings.\n");
        }

        // Command-line RPC  - single commands execute and exit.
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "gridcoinresearchd"))
                fCommandLine = true;

        if (fCommandLine)
        {
            return !CommandLineRPC(argc, argv);
        }

        // -server defaults to true for gridcoinresearchd but not for the GUI so do this here
        gArgs.SoftSetBoolArg("-server", true);
        // Initialize logging as early as possible.
        InitLogging();

        // Make sure a user does not request snapshotdownload and resetblockchaindata at same time!
        if (gArgs.IsArgSet("-snapshotdownload") && gArgs.IsArgSet("-resetblockchaindata"))
        {
            return InitError("-snapshotdownload and -resetblockchaindata cannot be used in conjunction");
        }

        // Check to see if the user requested a snapshot and we are not running TestNet!
        if (gArgs.IsArgSet("-snapshotdownload") && !gArgs.IsArgSet("-testnet"))
        {
            GRC::Upgrade snapshot;

            // Let's check make sure Gridcoin is not already running in the data directory.
            // Use new probe feature
            if (!LockDirectory(GetDataDir(), ".lock", false))
            {
                return InitError(strprintf("Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.",
                                           GetDataDir().string()));
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

                    return false;
                }
            }

            // Delete snapshot file
            snapshot.DeleteSnapshot();
        }

        // Check to see if the user requested to reset blockchain data -- We allow reset blockchain data on testnet, but not a snapshot download.
        if (gArgs.IsArgSet("-resetblockchaindata"))
        {
            GRC::Upgrade resetblockchain;

            // Let's check make sure Gridcoin is not already running in the data directory.
            if (!LockDirectory(GetDataDir(), ".lock", false))
            {
                return InitError(strprintf("Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.",
                                           GetDataDir().string()));
            }

            else
            {
                if (resetblockchain.ResetBlockchainData())
                    LogPrintf("ResetBlockchainData: success");

                else
                {
                    LogPrintf("ResetBlockchainData: failed to clean up blockchain data");

                    return InitError(resetblockchain.ResetBlockchainMessages(resetblockchain.CleanUp));
                }
            }
        }

        fRet = AppInit2(threads);
    }
    catch (std::exception& e) {
        LogPrintf("AppInit()Exception1");

        PrintException(&e, "AppInit()");
    } catch (...) {
        LogPrintf("AppInit()Exception2");

        PrintException(nullptr, "AppInit()");
    }

    if (fRet) {
        while (!ShutdownRequested()) {
            UninterruptibleSleep(std::chrono::milliseconds{500});
        }
    }

    Shutdown(nullptr);

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

    // Set global boolean to indicate intended absence of GUI to core...
    fQtActive = false;

    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
