// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "util/dir_permissions.h"
#include "chainparams.h"
#include "chainparamsbase.h"
#include "util.h"
#include <util/proc_hardening.h>
#include <util/syserror.h>
#include "util/threadnames.h"
#include <util/childreaper.h>
#include <util/sdnotify.h>
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
#include "wallet/autounlock.h"
#include "wallet/wallet.h"

#ifdef ENABLE_MULTIPROCESS
#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "interfaces/notification_dispatcher.h"
#include "ipc/handshake.h"
#include "ipc/peercred.h"
#include "ipc/process.h"
#include "ipc/serve_init.h"
#ifdef WIN32
#include "ipc/win_endpoint.h" // AttachGroupSid
#endif
#include "util/strencodings.h"
#include "wallet/wallet.h" // pwalletMain / CWallet::GetWalletUuid for the identity token
#include <atomic>
#include <memory>
#include <optional>
#endif

#include <boost/thread.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <csignal>
#include <stdio.h>
#include <stdexcept>

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
    // AppInit2 failed because a stop was requested during it, not for an error.
    bool startup_stopped = false;

    // Extends the service manager start timeout on each startup progress message.
    // Disconnected once readiness has been reported.
    boost::signals2::connection init_message_connection;

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

        // The tracer override is read here, from the command line only, before the config file and the settings
        // file are read, so a line in either cannot enable it.
        const bool allow_tracer = gArgs.GetBoolArg("-debugallowtracer", false);
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

        std::string error_msg;
        const auto config_error = [&error_msg] {
            return InitError(strprintf("Config file cannot be parsed%s. Cannot continue.\n",
                                       error_msg.empty() ? "" : ": " + error_msg));
        };

        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            return config_error();
        }

        SelectParams(gArgs.GetChainName());

        // reread config file after correct chain is selected
        if (!gArgs.ReadConfigFiles(error_msg, true))
        {
            return config_error();
        }

        // Command-line RPC  - single commands execute and exit. Local to this
        // entry point; formerly a util.h global read nowhere else.
        bool fCommandLine = false;
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "gridcoinresearchd"))
                fCommandLine = true;

        if (fCommandLine)
        {
#ifndef WIN32
            // The command line client is not the node: a termination signal ends it at
            // once, as before the node's handlers were installed at the top of main().
            signal(SIGTERM, SIG_DFL);
            signal(SIGINT, SIG_DFL);
#endif
            // The client reads the node's read-write settings file but ignores its refused keys, the connection
            // options (rpcconnect, rpcport, rpcuser, rpcpassword, ...) included, which it takes only from the
            // command line and the config file. It never writes the file: InitSettings rewrites it through a
            // fixed temporary name, so clients calling the same node at once raced on it, and a client could
            // write back a setting the node had just changed.
            std::vector<std::string> errors;
            if (!gArgs.ReadSettingsFile(&errors)) {
                return InitError(strprintf("Failed loading settings file:\n- %s\n", Join(errors, "\n- ")));
            }
            return !CommandLineRPC(argc, argv);
        }

        if (!gArgs.InitSettings(error)) {
            return InitError("Error initializing settings.\n" + error);
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
        LogNonDumpableStatus();

        // Best-effort in-process privilege hardening (Linux): NO_NEW_PRIVS +
        // capability-bounding-set drop, inherited across the daemonizing fork
        // below. Defence-in-depth secondary to the hardened systemd unit; a
        // no-op on non-Linux. Runs for the daemon / -multiprocess node only,
        // never the GUI process.
        if (gArgs.GetBoolArg("-nonewprivs", DEFAULT_NO_NEW_PRIVS)) {
            HardenProcess();
        }

#ifdef ENABLE_MULTIPROCESS
        // The attach group's checks need only the arguments and the data directory, so a
        // -multiprocess start they refuse stops here: before the daemonizing fork, so a -daemon
        // parent reports the failure, and before AppInit2 loads the wallet or binds RPC. The IPC
        // directory itself is checked again where it is opened (ipc::ApplyAttachGroupAccess; on
        // Windows ipc::win::ApplyAttachGroupDirectoryAccess).
        // -noipcattachgroup counts as unset.
        if (gArgs.GetBoolArg("-multiprocess", false)) {
            if (const std::optional<std::string> refusal = ipc::AttachGroupIpcDirRefusal()) {
                return InitError(*refusal);
            }

            const std::string attach_group = ipc::ConfiguredOption("-ipcattachgroup");
            if (!ipc::ConfigureAttachGroup(attach_group)) {
                const std::optional<std::string> problem = ipc::AttachGroupProblem(attach_group);
                return InitError(problem ? *problem
                                         : strprintf("The IPC attach group '%s' could not be configured.",
                                                     SanitizeString(attach_group)));
            }
        }
#endif

        const bool daemonize =
            gArgs.GetBoolArg("-daemon", DEFAULT_DAEMON) || gArgs.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT);
        // A service manager waiting for READY=1 would never get it from a forked child. Checked
        // before anything is changed on disk, -resetblockchaindata included.
        if (const std::optional<std::string> refusal = sdnotify::DaemonizeRefusal(daemonize)) {
            return InitError(*refusal);
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

        // A stop requested so far (during -resetblockchaindata, for one) ends the start
        // here, before the daemonizing fork and AppInit2.
        if (ShutdownRequested()) {
            LogPrintf("Shutdown requested during startup. Exiting.");
            // This return skips the status block at the end; say why the start ended.
            sdnotify::NotifyStartupStopped();
            return false;
        }

        if (daemonize) {
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

        // Refuse to run under a debugger or tracer unless the developer override is set. Done here, after the
        // command line client form has returned and the daemonizing fork, so it sees the process that will run.
        {
            const std::optional<int> tracer_pid = ReadOwnTracerPid();
            if (const std::optional<std::string> warning = TracerStateWarning(tracer_pid, TracerCheckSupported())) {
                LogPrintf("%s", *warning);
            }
            switch (DecideTracer(tracer_pid, allow_tracer)) {
            case TracerVerdict::Refuse:
                return InitError(strprintf("A debugger or tracer (pid %d) is attached to this process; refusing to "
                                           "start. See doc/developer-notes.md if you are debugging.\n",
                                           *tracer_pid));
            case TracerVerdict::StartDumpableForDebugging:
                SetProcessDumpableForDebugging();
                LogPrintf("WARNING: -debugallowtracer is set: this process is dumpable and may be traced");
                break;
            case TracerVerdict::Start:
                break;
            }
        }

        init_message_connection = uiInterface.InitMessage_connect(
            [](const std::string& message) { sdnotify::ExtendStartTimeout(message); });

#ifndef WIN32
        // The daemonizing fork is behind us. Under a PID namespace this process is
        // process 1 and must reap what the notify commands leave behind. The reaper
        // is POSIX only.
        util::StartOrphanReaper();
#endif

        fRet = AppInit2(threads);
        startup_stopped = !fRet && ShutdownRequested();
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

    // Startup is over, successful or not, so later progress messages must not
    // extend the start timeout.
    init_message_connection.disconnect();

    if (fRet) {
        // Core init succeeded. ThreadAppInit2 (the GUI's init wrapper) sets this
        // for the monolith; the daemon calls AppInit2 directly, so set it here so
        // a multiprocess GUI's isCoreReady() (served from this process) returns
        // true once the core is up.
        bGridcoinCoreInitComplete = true;

        // Unlock for staking only from the platform passphrase backend, if one is configured. No core lock is held
        // here, and a GUI that attaches afterwards sees the result.
        if (pwalletMain) {
            wallet::TryAutoUnlockStakingOnly(*pwalletMain);
        }

#ifdef ENABLE_MULTIPROCESS
        // Multiprocess (RFC #2937): after core init, optionally listen on the
        // AF_UNIX socket and serve a fresh interfaces::Init to each attached GUI.
        // Per-connection auth: a new ServeInit is built per connection; the Ipc
        // lives for the run, torn down after the wait loop.
        std::unique_ptr<interfaces::Ipc> ipc;
        // Whether this run minted an IPC cookie, so shutdown knows to remove it.
        // Tracked separately from `ipc`: WriteCookie runs before the Ipc is built,
        // so a failure in between would otherwise leave the credential on disk.
        bool cookie_written = false;
        // Note the default: this block runs only when -multiprocess was explicitly
        // set (command line or config file). There is no implicit/optional MP mode
        // in the daemon, so a failure here is a failure to honor an explicit
        // request, not the loss of a nice-to-have.
        const bool multiprocess = gArgs.GetBoolArg("-multiprocess", false);
        // The attach group was resolved, and its arguments checked, before AppInit2.
        if (multiprocess) {
            // Whether a failure came from the listener (bind, listen), the only
            // step after which a stale socket file can be the cause.
            bool listening = false;
            try {
                // An explicit -ipcdir may not exist yet. It is created restricted
                // to this account; a failure is fatal like the rest of this block.
                // With an attach group, the directory, the cookie and the socket
                // are given that group before anything is served, and a failure
                // is fatal like the rest of this block. The directory is then
                // given its access first, and a change to it is logged. On
                // Windows the group is the SID ConfigureAttachGroup resolved, and
                // an Administrators-owned directory (the service layout) is
                // checked rather than changed.
#ifndef WIN32
                const std::optional<gid_t> attach_group = ipc::AttachGroupGid();
#else
                const std::optional<std::string> attach_group = ipc::win::AttachGroupSid();
#endif
                if (attach_group) {
                    ipc::PrepareAttachGroupDirectory(ipc::GetIpcDir());
                } else {
                    ipc::PrepareIpcDirectory(ipc::GetIpcDir());
                }
                std::string cookie = ipc::WriteCookie(ipc::GetIpcDir(), attach_group);
                cookie_written = true;
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
                // Refuse a peer before serving it when its identity cannot be
                // read or ServerPeerPolicy refuses it: another OS user outside
                // the -ipcattachgroup group, or root. Windows has no peer
                // credentials on the socket, and kj gives this factory no socket
                // there, so nothing is read from it: the DACLs on node.sock and
                // ipc.cookie decide who may connect (ipc/win_endpoint.h), and
                // every connection, in every mode, must pass the identity check
                // (ipc/win_attest.h) before authenticate() will compare its
                // cookie. ServerPeerPolicy decides on what that check read.
                // cookie + identity are captured by value and copied into each
                // ServeInit.
                //
                // One GUI at a time: every connection shares this listener's one
                // attach slot, and only the connection holding it is served the
                // wallet and node interfaces. When that GUI goes away, its teardown
                // ends the wallet's IPC capability before another GUI can claim the
                // slot, so a capability never passes from one GUI to the next. The
                // hook runs on the IPC event-loop thread; EndIpcElevation takes
                // neither cs_main nor cs_wallet. It passes no connection check: it
                // runs as the connection is torn down, when the check would
                // already answer closed, and its job is to end the slot.
                auto make_serve_init = [cookie = std::move(cookie), identity = std::move(identity),
                                        slot = ipc::MakeAttachSlot()]
                    (int peer_fd) -> std::unique_ptr<interfaces::Init> {
                        ipc::ServeInitOptions options;
#ifndef WIN32
                        const std::optional<ipc::PeerIdentity> peer = ipc::ReadPeerIdentity(peer_fd);
                        if (!peer || ipc::ServerPeerPolicy(*peer) == ipc::PeerVerdict::Refuse) return nullptr;
#else
                        (void)peer_fd;
                        options.require_attestation = true;
#endif
                        return ipc::MakeServeInit(interfaces::MakeGridcoinInit(), cookie, identity, slot,
                                                  [] { if (pwalletMain) pwalletMain->EndIpcElevation({}); },
                                                  options);
                    };
                // Client notifications are posted to one dispatcher thread, so the
                // core thread that emits one never waits for the GUI.
                interfaces::SetNotificationDelivery(interfaces::NotificationDelivery::Dispatched);
                interfaces::StartNotificationDispatcher();
                ipc = interfaces::MakeIpc("gridcoinresearchd", std::move(make_serve_init));
                std::string address = "unix";
                listening = true;
                ipc->listenAddress(address);
                LogPrintf("IPC: serving GUI connections on %s\n", address);
                // Say which access controls are actually in force, rather than
                // leaving an operator to infer it from the platform. The peer is
                // identified from the socket on POSIX and by the identity check on
                // Windows; debug.log should not require the reader to know that.
                LogPrintf("IPC: peer-credential enforcement: %s\n", ipc::PeerCredentialEnforcement());
            } catch (const std::exception& e) {
                // Fail loudly and fatally. Previously this was logged and the
                // daemon carried on as a non-multiprocess node -- but a fresh
                // ipc.cookie has already been written above, so the GUI finds a
                // cookie, dials a socket nobody is listening on, and fails forever
                // with no explanation, while the one line that says why is buried
                // in debug.log. The failures reaching here are all actionable:
                // the IPC directory cannot be created or given its access (also
                // the attach group, or it opens as the data directory), the
                // cookie cannot be written, another process is already listening, a socket path is over the
                // 108-byte sockaddr_un limit, or the socket access control
                // (chmod / owner-only DACL) is not applicable. Only a failure of
                // the listener itself gets the advice about a stale socket.
                const std::string stale_advice = listening ?
                    strprintf("If the node was killed or crashed, a stale socket may be left in the IPC "
                              "directory (%s): stop any running gridcoinresearchd, delete 'node.sock' "
                              "there, and start again. Otherwise start without -multiprocess.",
                              ipc::GetIpcDir().string()) :
                    std::string{"Fix the cause above, or start without -multiprocess."};
                InitError(strprintf("Failed to start the multiprocess IPC listener: %s\n\n"
                                    "-multiprocess was requested, so this is fatal. %s",
                                    e.what(), stale_advice));
                // Exit non-zero and take the normal shutdown path: the wait loop
                // below sees the request immediately and falls through to
                // Shutdown(), so the node stops cleanly instead of running on as a
                // node the GUI can never reach.
                fRet = false;
                SetShutdownRequested();
            }
        }
#endif
        // Readiness is reported only when startup, including the multiprocess
        // listener when requested, succeeded.
        if (fRet) sdnotify::NotifyReady();

        while (!ShutdownRequested()) {
            UninterruptibleSleep(std::chrono::milliseconds{500});
        }
        // Only a start that reported readiness reports that it is stopping.
        if (fRet) sdnotify::NotifyStopping();
#ifdef ENABLE_MULTIPROCESS
        if (ipc) ipc->disconnectIncoming();
        // Also reached on the fatal-error path above, so the thread is never left
        // running at shutdown. A no-op when it was never started.
        interfaces::StopNotificationDispatcher();
        // The cookie is a live credential for as long as the file exists, so it goes
        // when the socket does -- mirroring upstream's DeleteAuthCookie(). It is also
        // what ConnectToNode checks before dialling, so leaving it behind sends the
        // next GUI to a socket nobody is listening on.
        if (cookie_written) ipc::DeleteCookie(ipc::GetIpcDir());
#endif
    }

    // A start that did not finish never sent READY=1. Its status says why, rather than
    // the last startup step: stopped on request, or failed.
    if (!fRet) {
        if (startup_stopped) {
            sdnotify::NotifyStartupStopped();
        } else {
            sdnotify::NotifyStartupFailed();
        }
    }

    Shutdown(nullptr);
#ifndef WIN32
    util::StopOrphanReaper();
#endif

    // delete thread handler
    threads->interruptAll();
    threads->removeAll();
    threads.reset();

    return fRet;

}

extern void noui_connect();
int main(int argc, char* argv[])
{
    SetProcessNonDumpable();

    // Before anything can create a file: debug.log, the read-write settings file
    // and the block subdirectories are all created without an explicit mode, so
    // without this they inherit a default desktop umask of 022 and land
    // world-readable. Narrowing here rather than after the fact leaves no window.
    util::SetOwnerOnlyUmask();

    // Capture and remove NOTIFY_SOCKET while the process is still single-threaded.
    // The process-hardening statements come before this one.
    sdnotify::CaptureFromEnvironment();
    // Once a stop has been requested the start timeout is no longer extended, so the
    // stop runs into its own timeout.
    sdnotify::SetShutdownCheck(ShutdownRequested);

    // A stop requested at any point of startup must be honoured. Under a unit with
    // PrivatePIDs= this process is process 1 of its PID namespace, and a signal to
    // process 1 with no handler is discarded, so without this a stop sent before
    // AppInit2 installed the handlers was lost and the start ran on until the stop
    // timeout killed it. The handlers only request a shutdown.
    RegisterShutdownSignalHandlers();

    // Reinit default timer to ensure it is zeroed out at the start of main.
    g_timer.InitTimer("default", false);

    // Set global boolean to indicate intended absence of GUI to core...
    fQtActive = false;

    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
