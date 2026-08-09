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
#include <util/proc_hardening.h>
#include <util/syserror.h>
#include "util/threadnames.h"
#include <util/tokenpipe.h>
#include "net.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "init.h"
#include "node/shutdown.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "node/ui_interface.h"
#include "gridcoin/upgrade.h"

#ifdef ENABLE_MULTIPROCESS
#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "ipc/handshake.h"
#include "ipc/peercred.h"
#include "ipc/serve_init.h"
#include "wallet/wallet.h" // pwalletMain / CWallet::GetWalletUuid for the identity token
#include <atomic>
#include <memory>
#endif

#include <boost/thread.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <stdio.h>
#include <stdexcept>

extern bool fQtActive;
// Set true at the end of core init. The GUI sets it via ThreadAppInit2; the
// daemon runs AppInit2 directly, so it sets it here (below) -- otherwise a
// multiprocess GUI's isCoreReady() poll, served from this process, never
// returns true and the GUI hangs on the splash.
extern std::atomic<bool> bGridcoinCoreInitComplete;

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

        SelectParams(gArgs.GetChainName());

        // reread config file after correct chain is selected
        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            return InitError("Config file cannot be parsed. Cannot continue.\n");
        }

        if (!gArgs.InitSettings(error)) {
            return InitError("Error initializing settings.\n");
        }

        // Command-line RPC  - single commands execute and exit. Local to this
        // entry point; formerly a util.h global read nowhere else.
        bool fCommandLine = false;
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

        // Best-effort in-process privilege hardening (Linux): NO_NEW_PRIVS +
        // capability-bounding-set drop, inherited across the daemonizing fork
        // below. Defence-in-depth secondary to the hardened systemd unit; a
        // no-op on non-Linux. Runs for the daemon / -multiprocess node only,
        // never the GUI process.
        if (gArgs.GetBoolArg("-nonewprivs", DEFAULT_NO_NEW_PRIVS)) {
            HardenProcess();
        }

        // Check to see if the user requested to reset blockchain data -- We allow reset blockchain data on testnet.
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
        // Core init succeeded. ThreadAppInit2 (the GUI's init wrapper) sets this
        // for the monolith; the daemon calls AppInit2 directly, so set it here so
        // a multiprocess GUI's isCoreReady() (served from this process) returns
        // true once the core is up.
        bGridcoinCoreInitComplete = true;
#ifdef ENABLE_MULTIPROCESS
        // Multiprocess (RFC #2937): after core init, optionally listen on the
        // AF_UNIX socket and serve a fresh interfaces::Init to each attached GUI.
        // Per-connection auth: a new ServeInit is built per connection; the Ipc
        // lives for the run, torn down after the wait loop.
        std::unique_ptr<interfaces::Ipc> ipc;
        // Note the default: this block runs only when -multiprocess was explicitly
        // set (command line or config file). There is no implicit/optional MP mode
        // in the daemon, so a failure here is a failure to honor an explicit
        // request, not the loss of a nice-to-have.
        if (gArgs.GetBoolArg("-multiprocess", false)) {
            try {
                std::string cookie = ipc::WriteCookie(GetDataDir());
                interfaces::NodeIdentity identity;
                identity.network = Params().NetworkIDString();
                // Fingerprint the wallet this node serves. Empty when there is no
                // wallet, or its UUID could not be minted (e.g. a mockable chain) --
                // the GUI then treats identity as "unavailable".
                if (pwalletMain) {
                    identity.identity_token = ipc::ComputeIdentityToken(pwalletMain->GetWalletUuid());
                }
                // Per-connection auth (RFC #2937 §4.3 hardening): build a FRESH
                // ServeInit for EACH accepted connection, so every client must
                // present the cookie itself -- no shared, sticky authentication.
                // Reject a foreign OS user before serving (CheckPeerCredentials:
                // SO_PEERCRED / getpeereid; Windows leans on the datadir ACL).
                // cookie + identity are captured by value and copied into each
                // ServeInit.
                auto make_serve_init = [cookie = std::move(cookie), identity = std::move(identity)]
                    (int peer_fd) -> std::unique_ptr<interfaces::Init> {
                        if (!ipc::CheckPeerCredentials(peer_fd)) return nullptr;
                        return ipc::MakeServeInit(interfaces::MakeGridcoinInit(), cookie, identity);
                    };
                ipc = interfaces::MakeIpc("gridcoinresearchd", std::move(make_serve_init));
                std::string address = "unix";
                ipc->listenAddress(address);
                LogPrintf("IPC: serving GUI connections on %s\n", address);
            } catch (const std::exception& e) {
                // Fail loudly and fatally. Previously this was logged and the
                // daemon carried on as a non-multiprocess node -- but a fresh
                // ipc.cookie has already been written above, so the GUI finds a
                // cookie, dials a socket nobody is listening on, and fails forever
                // with no explanation, while the one line that says why is buried
                // in debug.log. The failures reaching here are all actionable:
                // another process already listening, a socket path over the
                // 108-byte sockaddr_un limit, or the socket/cookie access control
                // (chmod / owner-only DACL) not being applicable to this data
                // directory.
                InitError(strprintf("Failed to start the multiprocess IPC listener: %s\n\n"
                                    "-multiprocess was requested, so this is fatal. If the node was "
                                    "killed or crashed, a stale socket may be left in the data "
                                    "directory: stop any running gridcoinresearchd, delete "
                                    "'node.sock' there, and start again. Otherwise start without "
                                    "-multiprocess.",
                                    e.what()));
                // Exit non-zero and take the normal shutdown path: the wait loop
                // below sees the request immediately and falls through to
                // Shutdown(), so the node stops cleanly instead of running on as a
                // node the GUI can never reach.
                fRet = false;
                SetShutdownRequested();
            }
        }
#endif
        while (!ShutdownRequested()) {
            UninterruptibleSleep(std::chrono::milliseconds{500});
        }
#ifdef ENABLE_MULTIPROCESS
        if (ipc) ipc->disconnectIncoming();
#endif
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
