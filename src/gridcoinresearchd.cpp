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
#include <util/syserror.h>
#include "util/threadnames.h"
#include <util/tokenpipe.h>
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

#if HAVE_DECL_FORK

/** Custom implementation of daemon(). This implements the same order of operations as glibc.
 * Opens a pipe to the child process to be able to wait for an event to occur.
 *
 * @returns 0 if successful, and in child process.
 *          >0 if successful, and in parent process.
 *          -1 in case of error (in parent process).
 *
 *          In case of success, endpoint will be one end of a pipe from the child to parent process,
 *          which can be used with TokenWrite (in the child) or TokenRead (in the parent).
 */
int fork_daemon(bool nochdir, bool noclose, TokenPipeEnd& endpoint)
{
    // communication pipe with child process
    std::optional<TokenPipe> umbilical = TokenPipe::Make();
    if (!umbilical) {
        return -1; // pipe or pipe2 failed.
    }

    int pid = fork();
    if (pid < 0) {
        return -1; // fork failed.
    }
    if (pid != 0) {
        // Parent process gets read end, closes write end.
        endpoint = umbilical->TakeReadEnd();
        umbilical->TakeWriteEnd().Close();

        int status = endpoint.TokenRead();
        if (status != 0) { // Something went wrong while setting up child process.
            endpoint.Close();
            return -1;
        }

        return pid;
    }
    // Child process gets write end, closes read end.
    endpoint = umbilical->TakeWriteEnd();
    umbilical->TakeReadEnd().Close();

#if HAVE_DECL_SETSID
    if (setsid() < 0) {
        exit(1); // setsid failed.
    }
#endif

    if (!nochdir) {
        if (chdir("/") != 0) {
            exit(1); // chdir failed.
        }
    }
    if (!noclose) {
        // Open /dev/null, and clone it into STDIN, STDOUT and STDERR to detach
        // from terminal.
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            bool err = dup2(fd, STDIN_FILENO) < 0 || dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0;
            // Don't close if fd<=2 to try to handle the case where the program was invoked without any file descriptors open.
            if (fd > 2) close(fd);
            if (err) {
                exit(1); // dup2 failed.
            }
        } else {
            exit(1); // open /dev/null failed.
        }
    }
    endpoint.TokenWrite(0); // Success
    return 0;
}

#endif

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

#if HAVE_DECL_FORK
        // Communication with parent after daemonizing. This is used for signalling in the following ways:
        // - a boolean token is sent when the initialization process (all the Init* functions) have finished to indicate
        // that the parent process can quit, and whether it was successful/unsuccessful.
        // - an unexpected shutdown of the child process creates an unexpected end of stream at the parent
        // end, which is interpreted as failure to start.
        TokenPipeEnd daemon_ep;
#endif

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

        if (gArgs.GetBoolArg("-printtoconsole", false) && gArgs.GetBoolArg("-daemon", DEFAULT_DAEMON)) {
            return InitError("-printtoconsole && -daemon cannot be specified at the same time,\n"
                             "because Gridcoin follows proper daemonization and disconnects the\n"
                             "console when the process is forked. This is consistent with Bitcoin\n"
                             "Core. Please see https://github.com/bitcoin/bitcoin/issues/10132.\n"
                             "If you are not specifying -daemon as a startup parameter, but only\n"
                             "-printtoconsole, and you are getting this error, please check the\n"
                             "gridcoinresearch.conf and comment out daemon=1, or use -nodaemon.");
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

        if (gArgs.GetBoolArg("-daemon", DEFAULT_DAEMON) || gArgs.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT)) {
#if HAVE_DECL_FORK
            tfm::format(std::cout, PACKAGE_NAME " starting\n");

            // Daemonize
            switch (fork_daemon(1, 0, daemon_ep)) { // don't chdir (1), do close FDs (0)
            case 0: // Child: continue.
                // If -daemonwait is not enabled, immediately send a success token the parent.
                if (!gArgs.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT)) {
                    daemon_ep.TokenWrite(1);
                    daemon_ep.Close();
                }
                break;
            case -1: // Error happened.
                return InitError(strprintf("fork_daemon() failed: %s\n", SysErrorString(errno)));
            default: { // Parent: wait and exit.
                int token = daemon_ep.TokenRead();
                if (token) { // Success
                    exit(EXIT_SUCCESS);
                } else { // fRet = false or token read error (premature exit).
                    tfm::format(std::cerr, "Error during initialization - check debug.log for details\n");
                    exit(EXIT_FAILURE);
                }
            }
            }
#else
            return InitError("-daemon is not supported on this operating system\n");
#endif // HAVE_DECL_FORK
        }

        fRet = AppInit2(threads);
#if HAVE_DECL_FORK
        if (daemon_ep.IsOpen()) {
            // Signal initialization status to parent, then close pipe.
            daemon_ep.TokenWrite(fRet);
            daemon_ep.Close();
        }
#endif
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
