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
    SetupServerArgs();

    // Note every function above the InitLogging() call must use fprintf or similar.

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
            tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error);
            return EXIT_FAILURE;
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

            return EXIT_SUCCESS;
        }

        if (gArgs.IsArgSet("-version"))
        {
            fprintf(stdout, "%s", VersionMessage().c_str());

            return false;
        }

        if (!CheckDataDirOption()) {
            tfm::format(std::cerr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", ""));
            return EXIT_FAILURE;
        }

        /** Check mainnet config file first in case testnet is set there and not in command line args **/
        SelectParams(CBaseChainParams::MAIN);

        // Currently unused.
        std::string error_msg;

        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            tfm::format(std::cerr, "Config file cannot be parsed. Cannot continue.\n");
            exit(1);
        }

        SelectParams(gArgs.IsArgSet("-testnet") ? CBaseChainParams::TESTNET : CBaseChainParams::MAIN);

        // reread config file after correct chain is selected
        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            tfm::format(std::cerr, "Config file cannot be parsed. Cannot continue.\n");
            exit(1);
        }

        if (!gArgs.InitSettings(error)) {
            tfm::format(std::cerr, "Error initializing settings.\n");
            exit(1);
        }

        // Command-line RPC  - single commands execute and exit.
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

        // Make sure a user does not request snapshotdownload and resetblockchaindata at same time!
        if (gArgs.IsArgSet("-snapshotdownload") && gArgs.IsArgSet("-resetblockchaindata"))
        {
            tfm::format(std::cerr, "-snapshotdownload and -resetblockchaindata cannot be used in conjunction");

            exit(1);
        }

        // Check to see if the user requested a snapshot and we are not running TestNet!
        if (gArgs.IsArgSet("-snapshotdownload") && !gArgs.IsArgSet("-testnet"))
        {
            GRC::Upgrade snapshot;

            // Let's check make sure gridcoin is not already running in the data directory.
            // Use new probe feature
            if (!LockDirectory(GetDataDir(), ".lock", false))
            {
                tfm::format(std::cerr, "Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.",
                            GetDataDir().string().c_str());

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

        // Check to see if the user requested to reset blockchain data -- We allow reset blockchain data on testnet, but not a snapshot download.
        if (gArgs.IsArgSet("-resetblockchaindata"))
        {
            GRC::Upgrade resetblockchain;

            // Let's check make sure gridcoin is not already running in the data directory.
            if (!LockDirectory(GetDataDir(), ".lock", false))
            {
                tfm::format(std::cerr, "Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.", GetDataDir().string().c_str());

                exit(1);
            }

            else
            {
                if (resetblockchain.ResetBlockchainData())
                    LogPrintf("ResetBlockchainData: success");

                else
                {
                    LogPrintf("ResetBlockchainData: failed to clean up blockchain data");

                    std::string inftext = resetblockchain.ResetBlockchainMessages(resetblockchain.CleanUp);

                    tfm::format(std::cerr, "%s", inftext.c_str());

                    exit(1);
                }
            }
        }

        LogPrintf("AppInit");

        fRet = AppInit2(threads);
    }
    catch (std::exception& e) {
        LogPrintf("AppInit()Exception1");

        PrintException(&e, "AppInit()");
    } catch (...) {
        LogPrintf("AppInit()Exception2");

        PrintException(nullptr, "AppInit()");
    }
    if(fRet)
    {
        while (!ShutdownRequested())
            MilliSleep(500);
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
