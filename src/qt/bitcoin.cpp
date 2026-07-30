/*
 * W.J. van der Laan 2011-2012
 */


#include <QApplication>
#include "qt/guilog.h"
#include <QTimer>

#include "bitcoingui.h"
#include "guiipcinfo.h"
#include "chainparams.h"
#include "chainparamsbase.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "researcher/researchermodel.h"
#include "mrcmodel.h"
#include "voting/votingmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "qt/intro.h"
#include "guiconstants.h"
#include "interfaces/init.h"
#include "interfaces/mrc.h"
#include "interfaces/node.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"
#include "init.h"
#include "node/shutdown.h"
#ifdef ENABLE_MULTIPROCESS
#include "ipc/connect.h"
#include "ipc/handshake.h"
#endif
#include "node/ui_interface.h"
#include "qtipcserver.h"
#include "txdb.h"
#include "util.h"
#include "util/threadnames.h"
#include "winshutdownmonitor.h"
#include "gridcoin/upgrade.h"
#include "gridcoin/gridcoin.h"
#include "policy/fees.h"
#include "upgradeqt.h"
#include "validation.h"
#include "decoration.h"

#include <atomic>
#ifndef WIN32
#include <QSocketNotifier>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <thread>

#include <QMessageBox>
#include <QCryptographicHash>
#include <QPushButton>
#include <QSettings>
#include <QGridLayout>
#include <QDebug>
#include <QTextCodec>
#include <QLocale>
#include <QTranslator>
#include "qt/splashscreen.h"
#include <QLibraryInfo>
#include <QProcess>

// This eliminates the linter false positive on double include of QtPlugin
#if (defined(BITCOIN_NEED_QT_PLUGINS) && !defined(_BITCOIN_QT_PLUGINS_INCLUDED)) || defined(QT_STATICPLUGIN)
#include <QtPlugin>
#endif

#if defined(BITCOIN_NEED_QT_PLUGINS) && !defined(_BITCOIN_QT_PLUGINS_INCLUDED)
#define _BITCOIN_QT_PLUGINS_INCLUDED
#define __INSURE__
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif

#if defined(QT_STATICPLUGIN)
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
Q_IMPORT_PLUGIN(QSvgPlugin);
Q_IMPORT_PLUGIN(QSvgIconPlugin);
#endif

extern bool fQtActive;

// Need a global reference for the notifications to find the GUI
static BitcoinGUI *guiref;
static SplashScreen *splashref;

static void RegisterMetaTypes()
{
    // Register meta types used for QMetaObject::invokeMethod and Qt::QueuedConnection
    // (...Gridcoin has none yet...)

    // Register typedefs (see https://doc.qt.io/qt-5/qmetatype.html#qRegisterMetaType)
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<uint32_t>("uint32_t");
}

int StartGridcoinQt(int argc, char *argv[], QApplication& app, OptionsModel& optionsModel,
                    interfaces::Node& gui_node, interfaces::Init* interface_init,
                    const GuiIpcInfo& ipc_info);

//! Early, deliberately LIMITED local settings read — run in main() BEFORE the Qt
//! translator is installed and BEFORE the Intro data-directory dialog.
//!
//! Exactly two GUI-client-scoped preferences must be in gArgs this early:
//!   -lang    : the Qt translator (installed right after) reads it; it is a
//!              GUI-only setting the core never reads.
//!   -datadir : the Intro dialog's default and GetDataDir() resolution need it.
//!
//! Both are read straight from the GUI's own QSettings (Gridcoin-Qt.conf) — no
//! node, no IPC, no core. This is intentionally NOT the full OptionsModel: in the
//! multiprocess build OptionsModel is constructed LATER, after the node connection,
//! because its core-state references (m_node / m_sidestake_manager) are bound to
//! the *remote* Init — which cannot exist yet here, since the datadir it connects
//! on is only chosen by the Intro dialog that this read precedes. So we read only
//! these two GUI-local args now and defer everything else (all core-state settings
//! included) to that later OptionsModel. Its QSettings-backed GUI-local prefs are
//! unaffected by where it is constructed.
static void EarlyReadGuiLangAndDatadir()
{
    QSettings settings;

    // -lang: GUI-local; SoftSet so an explicit command-line / conf -lang still wins.
    const QString language = settings.value("language", "").toString();
    if (!language.isEmpty()) {
        gArgs.SoftSetArg("-lang", language.toStdString());
    }

    // -datadir: a non-default datadir configured in the GUI becomes the default for
    // the Intro dialog / GetDataDir(). ShortPathString yields an 8.3 path on Windows
    // for characters outside the system code page (gArgs is a narrow-string store).
    const QString configured_datadir = settings.value("dataDir").toString();
    if (!configured_datadir.isEmpty()
            && configured_datadir != GUIUtil::getDefaultDataDirectory()) {
        const std::string datadir_narrow =
            fsbridge::ShortPathString(GUIUtil::qstringToBoostPath(configured_datadir));
        if (!datadir_narrow.empty()) {
            gArgs.SoftSetArg("-datadir", datadir_narrow);
        }
    }
}

static void SetupUIArgs(ArgsManager& argsman)
{
    argsman.AddArg("-choosedatadir", strprintf("Choose data directory on startup (default: %u)", DEFAULT_CHOOSE_DATADIR),
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-lang=<lang>", "Set language, for example \"de_DE\" (default: system locale)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-min", "Start minimized", ArgsManager::ALLOW_ANY, OptionsCategory::GUI);

    //TODO: Implement -resetguisettings. For right now this just does the same as -choosedatadir.
    argsman.AddArg("-resetguisettings", "Reset all settings changed in the GUI",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-splash", "Show splash screen on startup (default: 1)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-style", "Specify GUI style for Qt to use on Windows and MacOS (default: fusion)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-suppressnetworkgraph", "Suppress network graph (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-showorphans", "Include stale (orphaned) coinstake transactions in the transaction list",
                   ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-autotrustidentity",
                   "In -multiprocess mode, silently re-bind when the daemon's wallet identity changes "
                   "for this data directory instead of prompting -- for instances where the wallet is "
                   "swapped deliberately, and for headless/scripted starts (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-nobuildwarn",
                   "In -multiprocess mode, suppress the warning logged when the GUI and daemon were "
                   "built from different commits (default: 0)",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    argsman.AddArg("-guilogfile=<file>",
                   "In -multiprocess mode the GUI runs as a separate process from the node and MUST NOT "
                   "share the node's debug log file (the node rotates it by rename, which would orphan the "
                   "GUI's writes). The GUI logs to its own file: this option names it (relative paths are "
                   "under the data directory). Default: the -debuglogfile name with \"_gui\" inserted "
                   "(e.g. debug_gui.log). Must resolve to a different path than -debuglogfile. Ignored "
                   "outside -multiprocess.",
                   ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
}

#ifdef ENABLE_MULTIPROCESS
//! Derive the default GUI log path from the node's log path by inserting "_gui"
//! before the extension (e.g. /data/debug.log -> /data/debug_gui.log), so the two
//! processes default to distinct files in the shared data directory.
static fs::path DeriveGuiLogPath(const fs::path& node_log)
{
    const std::string stem = node_log.stem().string();
    const std::string ext = node_log.extension().string();
    return node_log.parent_path() / fs::path(stem + "_gui" + ext);
}
#endif

static void ThreadSafeMessageBox(const std::string& message, const std::string& caption, int style)
{
    // Message from network thread
    if(guiref)
    {
        bool modal = (style & CClientUIInterface::MODAL);
        // in case of modal message, use blocking connection to wait for user to click OK
        QMetaObject::invokeMethod(guiref, "error",
                                   modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                                   Q_ARG(QString, QString::fromStdString(caption)),
                                   Q_ARG(QString, QString::fromStdString(message)),
                                   Q_ARG(bool, modal));
    }
    else
    {
        GUILogPrintf("%s: %s", caption, message);
        tfm::format(std::cerr, "%s: %s\n", caption.c_str(), message.c_str());
    }
}

static void ThreadSafeHandleURI(const std::string& strURI)
{
    if(!guiref)
        return;

    QMetaObject::invokeMethod(guiref, "handleURI", GUIUtil::blockingGUIThreadConnection(),
                               Q_ARG(QString, QString::fromStdString(strURI)));
}

static void InitMessage(const std::string &message)
{
    if(splashref)
    {
        QMetaObject::invokeMethod(splashref, "showMessage",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString(message)),
                                  Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter));
    }
}

static void UpdateMessageBox(const std::string& version, const int& update_version, const std::string& message)
{
    std::string caption = _("Gridcoin Update Available");

    if (guiref)
    {
        QMetaObject::invokeMethod(guiref, "update", Qt::QueuedConnection,
                                   Q_ARG(QString, QString::fromStdString(caption)),
                                   Q_ARG(QString, QString::fromStdString(version)),
                                   Q_ARG(int, update_version),
                                   Q_ARG(QString, QString::fromStdString(message)));
    }

    else
    {
        GUILogPrintf("\r\n%s:\r\n%s", caption, message);
        tfm::format(std::cerr, "\r\n%s:\r\n%s\r\n", caption.c_str(), message.c_str());
    }
}

static void QueueShutdown()
{
    // Route a core-initiated shutdown through BitcoinGUI::requestQuit() so the
    // explicit-shutdown flag is set and minimize-on-close doesn't veto the quit
    // on Qt6 (see BitcoinGUI::closeEvent / issue #2995). The functor overload is
    // compile-time checked, unlike a string-named invokeMethod.
    QMetaObject::invokeMethod(QCoreApplication::instance(),
                              [] { BitcoinGUI::requestQuit(); },
                              Qt::QueuedConnection);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("bitcoin-core", psz).toStdString();
}

/* qDebug() message handler --> debug.log */
static void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        GUILogPrint(GUILogCategory::QT, "GUI: %s", msg.toStdString());
    } else {
        GUILogPrintf("GUI: %s", msg.toStdString());
    }
}

/* Handle runaway exceptions. Shows a message box with the problem and quits the program.
 */
static void handleRunawayException(std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    QMessageBox::critical(nullptr, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. Gridcoin can no longer continue safely and will quit.") + QString("\n") + QString::fromStdString(strMiscWarning));
    exit(1);
}

//! Latch so the graceful-quit path (and its log line) fires only once even if
//! many proxy calls fail in quick succession while the connection tears down.
static std::atomic<bool> g_daemon_connection_lost{false};

//! Post a graceful, popup-free GUI quit to the Qt main thread when the node
//! connection is lost -- the same path a core-initiated shutdown takes
//! (QueueShutdown / requestQuit). Deliberately NO modal dialog: a modal would
//! hang an unattended or remote GUI. Idempotent and safe if the QCoreApplication
//! is already gone.
static void QuitOnDaemonConnectionLost(const char* reason)
{
    if (g_daemon_connection_lost.exchange(true)) return;
    GUILogPrintf("IPC: %s; closing the GUI", reason);
    if (QCoreApplication* qapp = QCoreApplication::instance()) {
        QMetaObject::invokeMethod(qapp, [] { BitcoinGUI::requestQuit(); }, Qt::QueuedConnection);
    }
}

#ifdef ENABLE_MULTIPROCESS
//! QSettings key under which the GUI remembers the node identity token it bound to
//! for this data directory. Keyed by a hash of the canonical datadir so no path is
//! stored; the datadir is the rendezvous point the GUI and node already share.
static QString NodeIdentitySettingsKey()
{
    fs::path dir;
    try {
        dir = fs::canonical(GetDataDir());
    } catch (const std::exception&) {
        dir = GetDataDir(); // canonical() can throw (permissions); a stable raw path still works
    }
    const std::string s = dir.string();
    const QByteArray h = QCryptographicHash::hash(QByteArray(s.data(), static_cast<int>(s.size())),
                                                  QCryptographicHash::Sha256)
                             .toHex();
    return QStringLiteral("ipc/identity/") + QString::fromLatin1(h);
}

//! GUI-side identity binding + soft-warnings after a successful handshake
//! (Phase-2 design section 4.2/4.3). The wire compatibility (schema/protocol/
//! network) was already verified in ClientHandshake; here we confirm the node is
//! serving the same wallet the GUI bound to for this datadir, and surface any
//! mixed-build warning. Returns false only when the GUI must exit (a wallet change
//! the user declined). -autotrustidentity turns the prompt into a silent rebind
//! for instances that swap wallets deliberately (and for headless/scripted starts).
static bool ResolveNodeIdentity(const ipc::HandshakeResult& hs)
{
    QSettings settings;
    const QString key = NodeIdentitySettingsKey();
    const QString reported = QString::fromStdString(hs.remote_ident.identity_token);
    const QString stored = settings.value(key).toString();

    const auto persist = [&](const QString& token) {
        // Never persist an empty token: that would erase the binding and silently
        // disable the wallet-swap guard. (Only Mismatch/FirstSeen persist, and both
        // have a non-empty reported token; this is a defensive backstop.)
        if (token.isEmpty()) return;
        settings.setValue(key, token);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            GUILogPrintf("WARN: IPC: could not persist the node-identity binding "
                         "(QSettings status %d); the wallet-swap guard is disabled this session",
                         static_cast<int>(settings.status()));
        }
    };

    const bool autotrust = gArgs.GetBoolArg("-autotrustidentity", false);

    switch (ipc::CheckIdentityBinding(reported.toStdString(), stored.toStdString())) {
    case ipc::BindOutcome::Match:
        break;
    case ipc::BindOutcome::UnavailableFresh:
        GUILogPrintf("WARN: IPC: the daemon reported no wallet identity (binding unavailable); "
                     "proceeding without the wallet-swap guard");
        break;
    case ipc::BindOutcome::FirstSeen:
        GUILogPrint(GUILogCategory::IPC,
                    "IPC: bound to the daemon's wallet identity for this data directory");
        persist(reported);
        break;
    case ipc::BindOutcome::Mismatch: {
        // A different (non-empty) wallet identity than the one bound here.
        if (autotrust) {
            GUILogPrintf("WARN: IPC: the daemon's wallet identity changed; -autotrustidentity set -> "
                         "re-binding to the new wallet without prompting");
            persist(reported);
            break;
        }
        QMessageBox box(QMessageBox::Warning, PACKAGE_NAME,
                        QObject::tr("The wallet in this data directory appears to have changed since "
                                    "you last connected to the Gridcoin daemon from here (it may have "
                                    "been replaced or restored).\n\nTrust this wallet and remember it, "
                                    "or quit?"));
        QPushButton* trust = box.addButton(QObject::tr("Trust this wallet"), QMessageBox::AcceptRole);
        QPushButton* quit = box.addButton(QObject::tr("Quit"), QMessageBox::RejectRole);
        box.setDefaultButton(quit);
        box.exec();
        if (box.clickedButton() == trust) {
            GUILogPrintf("IPC: user trusted the changed wallet identity -> re-binding");
            persist(reported);
            break;
        }
        GUILogPrintf("IPC: user declined the changed wallet identity -> quitting "
                     "(relaunch with -autotrustidentity to accept it non-interactively)");
        return false;
    }
    case ipc::BindOutcome::UnavailableStored: {
        // The daemon reports NO wallet identity though a binding exists here (a
        // downgrade signal -- e.g. the node's UUID could not be minted). Never
        // erase the existing binding by persisting empty; keep it armed so a later
        // real token is still checked. Do not silently auto-trust this away, even
        // under -autotrustidentity (that flag accepts a *changed* wallet, not the
        // loss of the identity we bound to).
        if (autotrust) {
            GUILogPrintf("WARN: IPC: the daemon reports no wallet identity though a binding exists; "
                         "-autotrustidentity set -> proceeding, keeping the existing binding");
            break;
        }
        QMessageBox box(QMessageBox::Warning, PACKAGE_NAME,
                        QObject::tr("The Gridcoin daemon is no longer reporting a wallet identity, "
                                    "though this front end was bound to one for this data directory. "
                                    "Proceed this time (the existing binding is kept), or quit?"));
        QPushButton* proceed = box.addButton(QObject::tr("Proceed"), QMessageBox::AcceptRole);
        QPushButton* quit = box.addButton(QObject::tr("Quit"), QMessageBox::RejectRole);
        box.setDefaultButton(quit);
        box.exec();
        if (box.clickedButton() == proceed) {
            GUILogPrintf("IPC: user proceeded past the missing wallet identity -> keeping the binding");
            break;
        }
        GUILogPrintf("IPC: user declined the missing wallet identity -> quitting");
        return false;
    }
    }

    // Soft (non-fatal) findings.
    for (const ipc::SoftWarn w : hs.soft) {
        if (w == ipc::SoftWarn::GuiOlderMinor) {
            GUILogPrint(GUILogCategory::IPC,
                        "IPC: the daemon speaks a newer IPC schema minor than this GUI "
                        "(forward-compatible; some newer features may be unavailable)");
        } else if (w == ipc::SoftWarn::GitCommitMismatch && !gArgs.GetBoolArg("-nobuildwarn", false)) {
            GUILogPrintf("WARN: IPC: the GUI (%s) and daemon (%s) were built from different "
                         "commits; mixed builds can behave unexpectedly",
                         ipc::GetLocalBuildInfo().git_commit, hs.remote_build.git_commit);
        }
    }
    return true;
}
#endif // ENABLE_MULTIPROCESS

//! QApplication subclass that contains exceptions escaping Qt event handlers.
//! In -multiprocess mode the GUI polls the node with synchronous proxy calls from
//! timers/slots; if the daemon vanishes mid-call, libmultiprocess raises "IPC
//! client method call interrupted/called after disconnect", which must not
//! propagate through Qt (Qt forbids exceptions crossing the event loop -- it is
//! undefined behavior). Treat that as the node going away and route to the same
//! silent quit as the on_disconnect callback; any genuine exception still goes to
//! handleRunawayException, exactly as the outer try/catch around exec() does.
class GridcoinApplication : public QApplication
{
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (std::exception& e) {
            // Route IPC-disconnect exceptions to the silent quit rather than the
            // modal fatal-error path. Detect them two ways so we do not depend
            // solely on libmultiprocess's exact raise text:
            //   (a) that raise text ("... interrupted/called after disconnect"), and
            //   (b) the connection already being known lost -- once on_disconnect
            //       (or a prior notify()) has latched g_daemon_connection_lost, the
            //       GUI is tearing down and further proxy calls will keep throwing,
            //       so any event-handler exception in that state is disconnect fallout.
            const std::string msg{e.what()};
            if (g_daemon_connection_lost.load()
                || msg.find("interrupted by disconnect") != std::string::npos
                || msg.find("called after disconnect") != std::string::npos) {
                QuitOnDaemonConnectionLost("a GUI call was interrupted by the daemon disconnecting");
                return true;
            }
            handleRunawayException(&e);
        } catch (...) {
            handleRunawayException(nullptr);
        }
        return false;
    }
};

#ifndef WIN32
//! Self-pipe so a Unix termination signal can reach the Qt event loop. The signal
//! handler is async-signal-safe -- it only write()s one byte -- and a
//! QSocketNotifier on the read end fires on the Qt main thread and requests a
//! graceful quit (the same path QueueShutdown / on_disconnect use). This is only
//! needed in the -multiprocess GUI: the monolithic GUI runs the core in-process
//! and inherits init.cpp's HandleSIGTERM. Without it a SIGTERM/SIGINT -- e.g.
//! `kill(1)`, or the wallet control script stopping a split instance's GUI while
//! its core keeps running under systemd -- hits the default disposition and
//! hard-terminates the GUI with no teardown or final log flush.
static int g_signal_pipe[2] = {-1, -1};
extern "C" void GuiTerminationSignalHandler(int)
{
    const char byte = 0;
    const ssize_t rc = ::write(g_signal_pipe[1], &byte, 1);
    (void)rc; // best-effort; nothing safe to do on failure inside a signal handler
}
//! Set O_NONBLOCK + FD_CLOEXEC on one end of the self-pipe. Returns false on any
//! fcntl failure.
static bool SetSelfPipeFdFlags(int fd)
{
    const int fl = ::fcntl(fd, F_GETFL, 0);
    if (fl == -1 || ::fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1) return false;
    const int fdfl = ::fcntl(fd, F_GETFD, 0);
    if (fdfl == -1 || ::fcntl(fd, F_SETFD, fdfl | FD_CLOEXEC) == -1) return false;
    return true;
}
static void InstallGuiTerminationHandler(QCoreApplication& app)
{
    // pipe() + fcntl() rather than pipe2(O_CLOEXEC | O_NONBLOCK): pipe2() is
    // Linux/BSD-only and absent on macOS. Set non-blocking and close-on-exec on
    // both ends explicitly for portability.
    if (::pipe(g_signal_pipe) != 0) {
        GUILogPrintf("WARN: IPC: could not install the GUI termination-signal handler "
                     "(pipe failed); SIGTERM/SIGINT will hard-terminate the GUI");
        return;
    }
    if (!SetSelfPipeFdFlags(g_signal_pipe[0]) || !SetSelfPipeFdFlags(g_signal_pipe[1])) {
        GUILogPrintf("WARN: IPC: could not install the GUI termination-signal handler "
                     "(fcntl failed); SIGTERM/SIGINT will hard-terminate the GUI");
        ::close(g_signal_pipe[0]);
        ::close(g_signal_pipe[1]);
        g_signal_pipe[0] = g_signal_pipe[1] = -1;
        return;
    }
    auto* notifier = new QSocketNotifier(g_signal_pipe[0], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, &app, [] {
        char buf;
        while (::read(g_signal_pipe[0], &buf, 1) > 0) { /* drain */ }
        GUILogPrintf("IPC: received a termination signal; closing the GUI");
        BitcoinGUI::requestQuit();
    });
    struct sigaction sa = {};
    sa.sa_handler = GuiTerminationSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGINT, &sa, nullptr);
}
#endif // !WIN32

#ifndef BITCOIN_QT_TEST
int main(int argc, char *argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif

    // Reinit default timer to ensure it is zeroed out at the start of main.
    g_timer.InitTimer("default", false);

    SetupEnvironment();
    util::ThreadSetInternalName("gridcoinresearch-main");

    SetupServerArgs();
    SetupUIArgs(gArgs);

    // Note every function above the InitLogging() call must use tfm::format or similar.

    // Command-line options take precedence:
    // Before this would of been done in main then config file loaded.
    // We will load config file here as well.
    std::string error;
    // This is required to delay the exit until after the init of the Qt app, so a dialog can be raised, otherwise
    // this is effectively a silent failure, because most people running the GUI app are running it from an icon,
    // and won't see the output to std error.
    bool command_line_parse_failure = !gArgs.ParseParameters(argc, argv, error);

    /** Check mainnet config file first in case testnet is set there and not in command line args **/
    SelectParams(CBaseChainParams::MAIN);

#ifdef Q_OS_WIN
    // Use Qt's built-in FreeType rendering engine to display text on Windows.
    // We use the Inter font's OpenType format which doesn't render clearly on
    // Windows in Qt applications with the default engine. The TrueType format
    // works fine in either case, but the OpenType appearance is more legible.
    // Apply this before instantiating QApplication. This environment variable
    // configures the option for Qt's Windows integration plugin which doesn't
    // have a C++ API.
    //
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
    }
#endif

    // Generate high-dpi pixmaps. This is now wrapped by a macro conditional because these are always
    // on for Qt 6.0+ and are deprecated.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#else
    // QT6: Disable scale factor rounding. This ensures that if a user manually
    // sets QT_FONT_DPI or uses fractional scaling, icons/windows scale smoothly.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    // Initiate the app here to support choosing the data directory.
    Q_INIT_RESOURCE(bitcoin);
    Q_INIT_RESOURCE(bitcoin_locale);

    RegisterMetaTypes();
    GridcoinApplication app(argc, argv);

#if defined(WIN32) && defined(QT_GUI)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

    // We notify the user here and exit the application from the command line parse failure above. This is the earliest
    // a dialog can be raised, and is the latest that is safe if the command line is not parseable.
    if (command_line_parse_failure) {
        tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error);
        QMessageBox::critical(nullptr, PACKAGE_NAME, QObject::tr("Error: Cannot parse command line arguments. Please "
                                                                     "check the arguments and ensure they are valid and "
                                                                     "formatted correctly: \n\n")
                                                     + QString::fromStdString(error));
        return EXIT_FAILURE;
    }


    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (HelpRequested(gArgs))
    {
        GUIUtil::HelpMessageBox help;

        QSize size = GRC::ScaleSize((QPaintDevice *) &help, 550);

        QSpacerItem* horizontalSpacer = new QSpacerItem(size.width(), 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = (QGridLayout*) help.layout();
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());

        help.showAndPrint();
        return EXIT_SUCCESS;
    }

    if (gArgs.IsArgSet("-version"))
    {
        tfm::format(std::cout, "%s", "Version: " + VersionMessage());
        QMessageBox::information(nullptr, PACKAGE_NAME, QString::fromStdString("Version: " + VersionMessage()));

        return EXIT_SUCCESS;
    }

    // Application identification (must be set before OptionsModel is initialized,
    // as it is used to locate QSettings)
    app.setOrganizationName("Gridcoin");
    //XXX app.setOrganizationDomain("");
    if(gArgs.GetBoolArg("-testnet")) // Separate UI settings for testnet
        app.setApplicationName("Gridcoin-Qt-testnet");
    else
        app.setApplicationName("Gridcoin-Qt");

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    // Apply Qt's built-in "Fusion" theme as the application's base styles to
    // normalize layout discrepancies between platforms and fix some high-DPI
    // scaling issues on Windows. Gridcoin uses highly-customized stylesheets
    // which obscure most of the platform's styles anyway. That said, respect
    // the presence of Qt's "-style" option to bypass this if necessary. Skip
    // the override on Linux for now so that a user's window manager Qt theme
    // comes through for widgets without an explicit application style.
    //
    if (!gArgs.IsArgSet("-style")) {
        app.setStyle("Fusion");
    }
#endif

    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));

    // Install global event filter that suppresses help context question mark
    app.installEventFilter(new GUIUtil::WindowContextHelpButtonHintFilter(&app));

    // Early LIMITED local read of the two pre-Intro GUI-local args (-lang, -datadir)
    // straight from QSettings -- see EarlyReadGuiLangAndDatadir(). The translator
    // (installed just below) needs -lang, and the Intro datadir dialog needs
    // -datadir. The full OptionsModel is deliberately NOT built here: in the split
    // build its core-state references (m_node / m_sidestake_manager) are bound to
    // the remote node's Init, which cannot exist until the datadir is chosen (by
    // Intro, below) and the connection is made -- so OptionsModel is constructed
    // later, after that connect (search "OptionsModel optionsModel").
    EarlyReadGuiLangAndDatadir();

    // Get desired locale (e.g. "de_DE") from command line or use system locale
    QString lang_territory = QString::fromStdString(gArgs.GetArg("-lang", QLocale::system().name().toStdString()));
    QString lang = lang_territory;
    // Convert to "de" only by truncating "_DE"
    lang.truncate(lang_territory.lastIndexOf('_'));

    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

// Near line 394
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString transPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    QString transPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, transPath))
        app.installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, transPath))
        app.installTranslator(&qtTranslator);

    // Load e.g. bitcoin_de.qm (shortcut "de" needs to be defined in bitcoin.qrc)
    if (translatorBase.load(lang, ":/translations/"))
        app.installTranslator(&translatorBase);

    // Load e.g. bitcoin_de_DE.qm (shortcut "de_DE" needs to be defined in bitcoin.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        app.installTranslator(&translator);

    // Now that settings and translations are available, ask user for data directory
    bool did_show_intro = false;
    // In -multiprocess the core (a separate gridcoinresearchd) owns the datadir --
    // the node.sock the GUI connects to lives in it -- so datadir selection is a
    // core-setup concern, not a GUI first-run concern. Skip the Intro chooser here;
    // the GUI takes the datadir from -datadir/default and connects. A missing
    // node.sock then yields the existing "could not connect to the daemon" error
    // rather than a chooser that could pick a datadir the core is not serving.
    if (!gArgs.GetBoolArg("-multiprocess", false) && !Intro::showIfNeeded(did_show_intro)) {
        return EXIT_SUCCESS;
    }

    // Not currently useful.
    std::string error_msg;

    if (!gArgs.ReadConfigFiles(error_msg, true)) {
        ThreadSafeMessageBox(strprintf("Error reading configuration file.\n"),
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME, QObject::tr("Error: Cannot read configuration file. Please check the "
                                                                 "path and format of the file."));
        return EXIT_FAILURE;
    }

    // Do this to pickup -testnet / -regtest from the command line.
    SelectParams(gArgs.GetChainName());

    // Determine availability of data directory and parse gridcoinresearch.conf
    // Do not call GetDataDir(true) before this step finishes
    if (!CheckDataDirOption()) {
        std::string datadir_display = fsbridge::LongPathString(fs::path(gArgs.GetArg("-datadir", "")));
        ThreadSafeMessageBox(strprintf("Specified data directory \"%s\" does not exist.\n", datadir_display),
                             "", CClientUIInterface::ICON_ERROR | CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
            QObject::tr("Error: Specified data directory \"%1\" does not exist.")
                              .arg(QString::fromStdString(datadir_display)));
        return EXIT_FAILURE;
    }

    // This check must be done before logging is initialized or the config file is read. We do not want another
    // instance writing into an already running Gridcoin instance's logs. This is checked in init too,
    // but that is too late.
    fs::path dataDir = GetDataDir();

    // In multiprocess mode the core runs in a separate gridcoinresearchd, which
    // owns the datadir and already holds this lock; the GUI runs no core, so it
    // must not contend for the lock (it would always fail against the running
    // daemon). The short-circuit skips LockDirectory entirely in that mode.
    if (!gArgs.GetBoolArg("-multiprocess", false) && !LockDirectory(dataDir, ".lock", false)) {
        std::string str = strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running "
                                      "and using that directory."),
                                    dataDir, PACKAGE_NAME);
        ThreadSafeMessageBox(str, _("Gridcoin"), CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
            QObject::tr("Error: Cannot obtain a lock on the specified data directory. "
                        "An instance is probably already using that directory."));

        return EXIT_FAILURE;
    }

    // Reread config file after correct chain is selected
    if (!gArgs.ReadConfigFiles(error, true)) {
        ThreadSafeMessageBox(strprintf("Error reading configuration file: %s\n", error),
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
            QObject::tr("Error: Cannot parse configuration file: %1.").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    if (!gArgs.InitSettings(error)) {
        ThreadSafeMessageBox(strprintf("Error initializing settings.\n"),
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
                              QObject::tr("Error initializing settings: %1").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    // NOTE: OptionsModel is not constructed yet (see below, after the node
    // connection), so optionsModel.readNodeSettings() / migrateCoreSettings() are
    // deferred to right after that construction rather than run here.

#ifdef ENABLE_MULTIPROCESS
    // In -multiprocess mode the GUI and the node are separate processes sharing a
    // data directory, and they must NOT write the same log file. The node rotates
    // its debug log by renaming it (BCLog::Logger::archive), which would orphan a
    // second writer's open fd -- the GUI's lines would then trail into the renamed,
    // soon-unlinked file rather than the live one -- and on Windows the node's
    // rename would be blocked outright while the GUI holds the file open. So the GUI
    // logs to its OWN file. InitLogging() (below) reads -debuglogfile and opens the
    // log (it calls StartLogging), so the redirect has to happen HERE, before it:
    // compute the GUI path, refuse a same-file override, and force -debuglogfile to
    // the GUI file for this (GUI) process only. InitLogging then opens/shrinks the
    // GUI file exactly as it would the node's. The node is a separate process and
    // keeps its own -debuglogfile.
    if (gArgs.GetBoolArg("-multiprocess", false)) {
        const fs::path node_log = AbsPathForConfigVal(gArgs.GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
        const fs::path gui_log = gArgs.IsArgSet("-guilogfile")
            ? AbsPathForConfigVal(fs::path(gArgs.GetArg("-guilogfile", "")))
            : DeriveGuiLogPath(node_log);

        if (gui_log == node_log) {
            const std::string msg = strprintf(
                "The GUI and the Gridcoin daemon cannot share the log file %s in -multiprocess mode. "
                "Set -guilogfile to a different path, or unset it to use the default (%s).",
                node_log.string(), DeriveGuiLogPath(node_log).string());
            // Logging is not up yet, so report to stderr as well as a dialog.
            tfm::format(std::cerr, "%s\n", msg.c_str());
            QMessageBox::critical(nullptr, PACKAGE_NAME, QString::fromStdString(msg));
            return EXIT_FAILURE;
        }

        gArgs.ForceSetArg("-debuglogfile", gui_log.string());
    }
#endif

    // Initialize logging as early as possible.
    InitLogging();

    // Do this early as we don't want to bother initializing if we are just calling IPC
    ipcScanRelay(argc, argv);

    // Check to see if the user requested to reset blockchain data -- We allow on testnet.
    if (gArgs.IsArgSet("-resetblockchaindata"))
    {
        GRC::Upgrade resetblockchain;

        if (resetblockchain.ResetBlockchainData())
            GUILogPrintf("ResetBlockchainData: success");

        else
        {
            GUILogPrintf("ResetBlockchainData: failed to clean up blockchain data");

            std::string inftext = resetblockchain.ResetBlockchainMessages(resetblockchain.CleanUp);

            ThreadSafeMessageBox(inftext, _("Gridcoin"), CClientUIInterface::BTN_OK | CClientUIInterface::MODAL);
            QMessageBox::critical(nullptr, PACKAGE_NAME, QString::fromStdString(inftext));

            return EXIT_FAILURE;
        }
    }

    // --- Process Init: local monolith, or the remote node over IPC -------------
    // gui_node is a LOCAL node used ONLY for the GUI's own shutdown wiring
    // (WinShutdownMonitor + StartGridcoinQt's handleInitShutdown). It stays local
    // deliberately: routing handleInitShutdown over IPC would turn a daemon stop
    // into a core->GUI "shutdown-imminent" push, which is out of scope -- the split
    // GUI quits on the socket disconnect (#3227), not on a remote notification.
    // local_init IS the interface_init in the monolithic build (and provides the
    // LOCAL gui_node above).
    std::unique_ptr<interfaces::Init> local_init = interfaces::MakeGridcoinInit();
    std::unique_ptr<interfaces::Node> gui_node = local_init->makeNode();

#if defined(WIN32)
    // Windows session-end (WM_QUERYENDSESSION/WM_ENDSESSION) -> request shutdown
    // through the LOCAL node (see gui_node note above). Installed well before
    // app.exec(), soon enough to catch session-end.
    app.installNativeEventFilter(new WinShutdownMonitor(*gui_node));
#endif

    // interface_init is the Init the data models consume core state through: the
    // remote node's in the split build, the local one in the monolith. OptionsModel
    // is built from it too (its m_node / m_sidestake_manager), so its core-state
    // getters/setters reach the daemon over IPC in the split build; OptionsModel's
    // QSettings-backed GUI-local prefs are unaffected by that.
    interfaces::Init* interface_init = local_init.get();
    GuiIpcInfo ipc_info;
    const bool multiprocess = gArgs.GetBoolArg("-multiprocess", false);
#ifdef ENABLE_MULTIPROCESS
    std::optional<ipc::GuiConnection> node_connection;
    if (multiprocess)
    {
        // Connect to the separately-started daemon (moved here from StartGridcoinQt
        // so OptionsModel can be built from the remote Init). Logging is up
        // (InitLogging above), so failures land in the GUI log file.
        std::string ipc_error;
        node_connection = ipc::ConnectToNode(GetDataDir(), ipc_error,
            /*on_disconnect=*/[] {
                QuitOnDaemonConnectionLost("lost connection to the Gridcoin daemon");
            });
        if (!node_connection)
        {
            GUIError("IPC: could not connect to the Gridcoin daemon: %s", ipc_error);
            QMessageBox::critical(nullptr, PACKAGE_NAME,
                    QObject::tr("Could not connect to the Gridcoin daemon:\n%1").arg(QString::fromStdString(ipc_error)));
            return EXIT_FAILURE;
        }
        interface_init = node_connection->init.get();

        // GUI-side identity binding (prompt / -autotrustidentity / exit). Needs
        // only QSettings -- no main window -- so it runs here. The git_commit
        // mixed-build banner needs the window, so its data is handed to
        // StartGridcoinQt (which shows it) rather than acted on here.
        if (!ResolveNodeIdentity(node_connection->handshake))
        {
            return EXIT_FAILURE;
        }
        // Gather the IPC connection facts for the About dialog (and the mixed-build
        // banner). This is the -multiprocess split build, so the section is active.
        const interfaces::BuildInfo local_build = ipc::GetLocalBuildInfo();
        const interfaces::BuildInfo& node_build = node_connection->handshake.remote_build;
        const interfaces::NodeIdentity& node_ident = node_connection->handshake.remote_ident;
        ipc_info.active = true;
        ipc_info.gui_version = QString::fromStdString(local_build.git_commit);
        ipc_info.node_version = QString::fromStdString(node_build.git_commit);
        ipc_info.node_built_at = QString::fromStdString(node_build.built_at);
        ipc_info.ipc_schema = QStringLiteral("%1.%2").arg(node_build.schema_major).arg(node_build.schema_minor);
        ipc_info.ipc_protocol = QString::number(node_build.protocol_version);
        ipc_info.socket_path = GUIUtil::boostPathToQString(GetDataDir() / "node.sock");
        // Raw token (empty = unavailable); the dialog renders the empty case.
        ipc_info.node_identity = QString::fromStdString(node_ident.identity_token);
        ipc_info.network = QString::fromStdString(node_ident.network);
        if (!gArgs.GetBoolArg("-nobuildwarn", false))
        {
            for (const ipc::SoftWarn w : node_connection->handshake.soft)
            {
                if (w == ipc::SoftWarn::GitCommitMismatch)
                {
                    ipc_info.git_commit_mismatch = true;
                }
            }
        }
    }
#else
    if (multiprocess)
    {
        GUIError("IPC: -multiprocess requested but this build has no multiprocess (IPC) support");
        QMessageBox::critical(nullptr, PACKAGE_NAME,
                QObject::tr("This build was compiled without multiprocess (IPC) support."));
        return EXIT_FAILURE;
    }
#endif

    // Build OptionsModel from interface_init (the REMOTE node's Init in the split
    // build): its core-state node + sidestake manager go over IPC to the daemon.
    // Its QSettings-backed GUI-local preferences are read exactly as before. The
    // makeX / migrateCoreSettings calls are IPC round-trips in the split build; if
    // the daemon drops here (after a successful connect) libmultiprocess throws, and
    // main() has no enclosing try (unlike StartGridcoinQt, which the models used to
    // live inside), so wrap them and exit gracefully rather than std::terminate.
    // optionsModel is heap-allocated only so it can be declared before the try and
    // outlive it for the StartGridcoinQt call below.
    std::unique_ptr<interfaces::SideStakeManager> sidestake_manager;
    std::unique_ptr<interfaces::Node> options_node;
    std::unique_ptr<OptionsModel> optionsModel;
    try
    {
        sidestake_manager = interface_init->makeSideStakeManager();
        options_node = interface_init->makeNode();
        optionsModel = std::make_unique<OptionsModel>(*options_node, *sidestake_manager);
        // Per-node QSettings prefs (the datadir is finalized now) + the one-time
        // migration of proxy / UPnP / reservebalance / etc. from Gridcoin-Qt.conf
        // into the core read-write settings (the daemon's, over IPC, in the split
        // build).
        optionsModel->readNodeSettings();
        optionsModel->migrateCoreSettings();
    }
    catch (const std::exception& e)
    {
        // In the split build the wrapped calls are IPC round-trips, so a throw here
        // is almost always the daemon dropping after the connect; in the monolith
        // they are in-process, so report a generic startup error instead of a
        // misleading "lost the daemon connection".
        if (multiprocess)
        {
            GUIError("IPC: lost the daemon connection during GUI startup: %s", e.what());
            QMessageBox::critical(nullptr, PACKAGE_NAME,
                    QObject::tr("Lost connection to the Gridcoin daemon during startup:\n%1")
                        .arg(QString::fromStdString(e.what())));
        }
        else
        {
            GUILogPrintf("Error during GUI startup: %s", e.what());
            QMessageBox::critical(nullptr, PACKAGE_NAME,
                    QObject::tr("An error occurred during startup:\n%1")
                        .arg(QString::fromStdString(e.what())));
        }
        return EXIT_FAILURE;
    }

    /** Start Qt as normal before it was moved into this function **/
    StartGridcoinQt(argc, argv, app, *optionsModel, *gui_node, interface_init, ipc_info);

    // We received a request to remove blockchain data so client user can start to sync from 0
    if (fResetBlockchainRequest)
    {
        UpgradeQt resetblockchain;

        // Release LevelDB file handles on Windows so we can remove the old
        // blockchain files:
        //
        // We should really close it in Shutdown() when the main application
        // exits. Before we can do that, we need to solve an old outstanding
        // conflict with the behavior of "-daemon" on Linux that prematurely
        // closes the DB when the process forks.
        //
        CTxDB().Close();

        if (resetblockchain.ResetBlockchain(app))
            GUILogPrintf("ResetBlockchainData: success");

        else
            GUILogPrintf("ResetBlockchainData: failed");
    }

    return EXIT_SUCCESS;
}

int StartGridcoinQt(int argc, char *argv[], QApplication& app, OptionsModel& optionsModel,
                    interfaces::Node& gui_node, interfaces::Init* interface_init,
                    const GuiIpcInfo& ipc_info)
{
    // Set global boolean to indicate intended presence of GUI to core.
    fQtActive = true;

    std::shared_ptr<ThreadHandler> threads = std::make_shared<ThreadHandler>();

    // Install qDebug() message handler to route to debug.log
    qInstallMessageHandler(DebugMessageHandler);

    // Subscribe to global signals from core
    uiInterface.ThreadSafeMessageBox_connect(ThreadSafeMessageBox);
    uiInterface.InitMessage_connect(InitMessage);
    uiInterface.Translate_connect(Translate);

    // Core-initiated shutdown (RPC stop / SIGTERM / low-disk abort) -> quit the
    // GUI, via the node interface's QueueShutdown bridge. gui_node is deliberately
    // the LOCAL node (main()'s local_init), NOT the remote one: in the split build
    // the GUI quits on the socket disconnect (#3227), and delivering the daemon's
    // shutdown over IPC would be the descoped core->GUI "shutdown-imminent" push.
    // So in the monolith this fires on real core shutdown; in the split build it is
    // dormant (the local node's QueueShutdown never fires) and the disconnect hook
    // drives the quit. The returned Handler is kept only to keep the subscription
    // alive (RAII) for this function's scope; gui_node outlives this call.
    [[maybe_unused]] std::unique_ptr<interfaces::Handler> shutdown_handler =
        gui_node.handleInitShutdown(QueueShutdown);

    uiInterface.UpdateMessageBox_connect(UpdateMessageBox);

    // Custom splash (a normal top-level QWidget, not a QSplashScreen) so the
    // block-loading progress stays on top under Xwayland, where the
    // Qt::SplashScreen window type is dropped behind the other windows -- see
    // qt/splashscreen.h.
    SplashScreen splash;
    if (gArgs.GetBoolArg("-splash", true) && !gArgs.GetBoolArg("-min"))
    {
        splash.show();
        splashref = &splash;
    }

    app.processEvents();

    app.setQuitOnLastWindowClosed(false);

    try
    {
        BitcoinGUI window;
        guiref = &window;

        GUILogPrintf("Starting Gridcoin");

        // Process topology (doc/multiprocess_design.md). The monolithic build
        // runs core init in this process (ThreadAppInit2 -> AppInit2) and wraps
        // the in-process core with a local Init. The multiprocess build
        // (-multiprocess) runs no core here: it connects to a separately-started
        // gridcoinresearchd, which runs the core and serves the interfaces over
        // IPC, and drives the models through that remote Init.
        const bool multiprocess = gArgs.GetBoolArg("-multiprocess", false);

        if (multiprocess)
        {
            // The connect + GUI-side identity binding already ran in main() (so
            // OptionsModel could be built from the remote node's Init, which is
            // `interface_init` here). What remains needs the main window / event
            // loop: surface the mixed-build banner (empty commit strings = nothing
            // to warn about, or -nobuildwarn), and start the GUI's own log-archive
            // timer.
            if (ipc_info.git_commit_mismatch)
            {
                window.showBuildMismatchWarning(ipc_info.gui_version, ipc_info.node_version);
            }
            // Populate the About dialog's multiprocess connection section.
            window.setIpcConnectionInfo(ipc_info);

            // The multiprocess GUI logs to its own file (set up in main() before
            // this function) but, unlike the node, runs no core scheduler to rotate
            // it. Drive the same daily-archive check the node schedules in
            // GRC::ScheduleBackgroundJobs: every 5 minutes archive(false) is a cheap
            // no-op until the day boundary is crossed (and a full no-op when
            // -logarchivedaily=false). Run it OFF the Qt main thread: when it does
            // archive, archive()'s gzip compression is synchronous and would freeze
            // the UI (the node runs archive() on its scheduler thread for the same
            // reason). archive() serializes on the logger mutex, so it is safe
            // off-thread; a guard skips a tick while a previous (slow) archive is
            // still compressing. The GUI owns this file exclusively, so the
            // close-rename-reopen is safe on every platform. Timer parented to
            // `app`, so it lives for the run.
            static std::atomic<bool> gui_archive_running{false};
            QTimer* logArchiveTimer = new QTimer(&app);
            QObject::connect(logArchiveTimer, &QTimer::timeout, [] {
                bool expected = false;
                if (!gui_archive_running.compare_exchange_strong(expected, true)) {
                    return;  // a previous archive pass is still running
                }
                try {
                    std::thread([] {
                        RenameThread("gui-log-archive");
                        try {
                            fs::path plogfile_out;
                            LogInstance().archive(false, plogfile_out);
                        } catch (const std::exception& e) {
                            // A throw here would terminate the detached thread (and the
                            // process); contain it. The logger is left valid (the file is
                            // reopened before the cleanup step); the next tick retries.
                            GUILogPrintf("WARNING: GUI log archive pass failed: %s", e.what());
                        }
                        gui_archive_running = false;
                    }).detach();
                } catch (const std::exception& e) {
                    // std::thread construction can throw (e.g. resource exhaustion).
                    // Reset the guard so a later tick can retry, and don't let it
                    // escape the Qt slot (which would abort the GUI).
                    GUILogPrintf("WARNING: could not start GUI log archive thread: %s", e.what());
                    gui_archive_running = false;
                }
            });
            logArchiveTimer->start(300000);  // 5 minutes, matching the node's schedule
        }
        else
        {
            // Monolithic build: run core init in this process. interface_init (the
            // local Init created in main()) is already set; just start the init
            // thread. The readiness wait below polls interface_init->isCoreReady().
            if (!threads->createThread(ThreadAppInit2,threads,"AppInit2 Thread"))
            {
                GUILogPrintf("Error; NewThread(ThreadAppInit2) failed");
                return EXIT_FAILURE;
            }
        }

        {
             //10-31-2015
            while (!interface_init->isCoreReady())
            {
                app.processEvents();

                // The sleep here has to be pretty short to avoid a buffer overflow crash with
                // fast CPUs due to too many events. It originally was set to 300 ms and has
                // been reduced to 100 ms.
                UninterruptibleSleep(std::chrono::milliseconds{100});
            }

            if (splashref)
                splash.finish();

            if (!ShutdownRequested()) {
                // Put this in a block, so that the Model objects are cleaned up
                // before calling Shutdown(). interface_init (created above, ahead
                // of the readiness wait) outlives every model constructed here.
                std::unique_ptr<interfaces::Node> node = interface_init->makeNode();
                std::unique_ptr<interfaces::StakingStatus> staking_status = interface_init->makeStakingStatus();
                // Wallet startup completed above, so pwalletMain is set and
                // the monolithic Init must hand out a wallet interface. If
                // that invariant is ever broken, fail loudly instead of
                // dereferencing null below (assert alone compiles out under
                // NDEBUG).
                std::unique_ptr<interfaces::Wallet> wallet = interface_init->makeWallet();
                if (!wallet) {
                    // Throw rather than return: the enclosing catch funnels
                    // through handleRunawayException, and the normal
                    // Shutdown()/thread-teardown path still runs.
                    throw std::runtime_error("wallet interface unavailable after init");
                }
                // The windowed tx-table source (its store, worker thread, and
                // producer subscriptions). Owned here so it outlives the
                // WalletModel that drives it and is torn down — worker joined,
                // producers severed — before Shutdown() destroys the wallet.
                std::shared_ptr<interfaces::WalletTxSource> wallet_tx_source =
                    interface_init->makeWalletTxSource();
                if (!wallet_tx_source) {
                    throw std::runtime_error("wallet tx source unavailable after init");
                }
                // The Manual Research Claim interface over the node's wallet
                // (Phase 1d-i). Owned here so it outlives the MRCModel that
                // drives it.
                std::unique_ptr<interfaces::MRC> mrc = interface_init->makeMRC();
                if (!mrc) {
                    throw std::runtime_error("MRC interface unavailable after init");
                }
                // The voting interface (Phase 1d-iii): the poll table over the
                // core result cache and poll/vote submission. Owned here so it
                // outlives the VotingModel that drives it.
                std::unique_ptr<interfaces::VotingManager> voting_manager = interface_init->makeVotingManager();
                if (!voting_manager) {
                    throw std::runtime_error("voting interface unavailable after init");
                }
                // The researcher/beacon interface (Phase 1d-iv) over the node's
                // wallet. Owned here so it outlives the ResearcherModel that drives
                // it (and the MRCModel/VotingModel that read researcher state).
                std::unique_ptr<interfaces::ResearcherContext> researcher_context =
                    interface_init->makeResearcherContext();
                if (!researcher_context) {
                    throw std::runtime_error("researcher interface unavailable after init");
                }
                // The PSGT pool + multisig-workbench interface (Phase 1d-v) over
                // the node's wallet. Owned here so it outlives the PSGTPoolPage /
                // PSGTPoolTableModel and MultisignPSGTDialog that use it; cleared
                // from them on teardown below before this is destroyed.
                std::unique_ptr<interfaces::PSGTPoolContext> psgt_pool_context =
                    interface_init->makePSGTPoolContext();
                if (!psgt_pool_context) {
                    throw std::runtime_error("PSGT pool interface unavailable after init");
                }

                ClientModel clientModel(*node, *staking_status, &optionsModel);
                WalletModel walletModel(*wallet, *wallet_tx_source, &optionsModel);
                ResearcherModel researcherModel(*researcher_context);
                MRCModel mrcModel(*mrc, &walletModel, &clientModel, &researcherModel);
                VotingModel votingModel(*voting_manager, *researcher_context, clientModel, optionsModel, walletModel);

                // Thread the PSGT pool interface to the page + dialog before
                // setWalletModel() below builds the page's table model.
                window.setPSGTPoolContext(psgt_pool_context.get());
                window.setResearcherModel(&researcherModel);
                window.setClientModel(&clientModel);
                window.setWalletModel(&walletModel);
                window.setMRCModel(&mrcModel);
                window.setVotingModel(&votingModel);

                // Exception-safe teardown of the tx-table view models (Phase
                // 1c-ii-c). `window` is declared in an OUTER scope than
                // walletModel / wallet_tx_source, so on an exception thrown below
                // (window.show / ipcInit / app.exec) the stack unwind frees the
                // model and source FIRST, then destroys `window` — and
                // ~OverviewTxModel / ~DetailedTxModel would call
                // txSource().unregisterView() against an already-freed source
                // (UAF). This guard is declared AFTER the model and source, so
                // its destructor runs BEFORE theirs on every exit path (normal or
                // exception): it detaches the wallet from the window — destroying
                // the view models while the source is still alive — so their
                // unregisterView() always reaches a live source. The explicit
                // setWalletModel(nullptr) on the normal path below makes this a
                // no-op there; on the throw path it is the only teardown.
                struct WalletModelDetachGuard {
                    BitcoinGUI& window;
                    ~WalletModelDetachGuard() { window.setWalletModel(nullptr); }
                } wallet_model_detach_guard{window};

                // If -min option passed, start window minimized.
                if(gArgs.GetBoolArg("-min"))
                {
                    window.showMinimized();
                }
                else
                {
                    window.show();
                }

                // Place this here as guiref has to be defined if we don't want to lose URIs
                ipcInit(argc, argv, ThreadSafeHandleURI);

#if defined(WIN32) && defined(QT_GUI)
                WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("%1 didn't yet exit safely...").arg(QObject::tr(PACKAGE_NAME)), (HWND)window.winId());
#endif

                GUILogPrintf("GUI loaded.");

                // Regenerate startup link, to fix links to old versions
                GUIUtil::SetStartOnSystemStartup(optionsModel.getStartAtStartup(), optionsModel.getStartMin());

#ifndef WIN32
                // In the split (-multiprocess) build the GUI runs no in-process
                // core, so it lacks init.cpp's SIGTERM/SIGINT handler. Install one
                // here so `kill`/the wallet control script can stop the GUI
                // gracefully while the systemd core keeps running. (Windows uses
                // WM_CLOSE via WinShutdownMonitor; the monolith uses the core's.)
                if (multiprocess) InstallGuiTerminationHandler(app);
#endif
                app.exec();

                // Stop the GUI-process-local URI-listener thread now that the
                // event loop has returned (it no longer reads the core shutdown
                // state; see qtipcserver ipcShutdown()).
                ipcShutdown();

                window.hide();
                window.setClientModel(nullptr);
                // Normal-path detach of the tx-table view models, in order with
                // the other models. Destroys OverviewTxModel / DetailedTxModel
                // (via BitcoinGUI -> {transactionView->setModel,
                // overviewPage->setWalletModel}(nullptr)) while walletModel and
                // wallet_tx_source below are still alive, so their
                // unregisterView() reaches a live source. wallet_model_detach_guard
                // above enforces the same on the exception path; this explicit
                // call then becomes an idempotent no-op for the guard.
                window.setWalletModel(nullptr);
                window.setResearcherModel(nullptr);
                // Clear the voting model BEFORE the enclosing block exits and
                // destroys the stack-allocated VotingModel: this propagates to
                // PollTableModel::setModel(nullptr) which drains any in-flight
                // QtConcurrent refresh worker still dereferencing the model.
                window.setVotingModel(nullptr);
                // Clear the PSGT pool interface from the page + dialog BEFORE the
                // enclosing block destroys psgt_pool_context: the page's table
                // model holds a reference to it, so it must be torn down first.
                window.setPSGTPoolContext(nullptr);
                guiref = nullptr;
            }
            // Shut down the core and its threads (but don't exit Bitcoin-Qt
            // here). Only in the monolithic build: there the core runs in this
            // process. In the multiprocess build the core lives in the daemon
            // and manages its own lifetime; the GUI merely drops its connection
            // when main() unwinds after this returns (node_connection now lives in
            // main(), so it and the interfaces built from it outlive this call).
            if (!multiprocess)
            {
                GUILogPrintf("Main calling Shutdown...");
                Shutdown(nullptr);
            }
        }

    }
    catch (std::exception& e)
    {
        handleRunawayException(&e);
    }
    catch (...)
    {
        handleRunawayException(nullptr);
    }

    // delete thread handler
    threads->interruptAll();
    threads->removeAll();
    threads.reset();


    return EXIT_SUCCESS;
}

#endif // BITCOIN_QT_TEST
