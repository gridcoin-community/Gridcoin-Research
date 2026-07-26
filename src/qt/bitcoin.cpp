/*
 * W.J. van der Laan 2011-2012
 */


#include <QApplication>
#include "qt/guilog.h"
#include <QTimer>

#include "bitcoingui.h"
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
#include <stdexcept>
#include <thread>

#include <QMessageBox>
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

int StartGridcoinQt(int argc, char *argv[], QApplication& app, OptionsModel& optionsModel, interfaces::Node& gui_node);

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
            const std::string msg{e.what()};
            if (msg.find("interrupted by disconnect") != std::string::npos ||
                msg.find("called after disconnect") != std::string::npos) {
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

    // The sidestake registry interface backs the OptionsModel's sidestake table
    // model (Phase 1d-ii). Created here, before optionsModel, so it outlives it
    // (reverse destruction order). It wraps the global registry, so it needs no
    // node/wallet; Phase 2 will hand this out from the single process Init
    // instead of a locally-minted one.
    std::unique_ptr<interfaces::Init> gui_init = interfaces::MakeGridcoinInit();
    std::unique_ptr<interfaces::SideStakeManager> sidestake_manager = gui_init->makeSideStakeManager();
    // Settings command/query surface for OptionsModel. Minted here (like the
    // sidestake manager) so it outlives optionsModel; Phase 2 hands this out from
    // the single process Init. The node wraps globals and reads them at call time,
    // so it is safe to construct before core init -- OptionsModel only reads/writes
    // settings through it later (dialog open, migrateCoreSettings()).
    std::unique_ptr<interfaces::Node> gui_node = gui_init->makeNode();

#if defined(WIN32)
    // Install global event filter for processing Windows session related Windows
    // messages (WM_QUERYENDSESSION and WM_ENDSESSION). Placed after gui_node so
    // the monitor can request shutdown through the node interface; installing it
    // here (still well before app.exec()) is soon enough to catch session-end.
    app.installNativeEventFilter(new WinShutdownMonitor(*gui_node));
#endif

    // Load the optionsModel. This has to be loaded before the translations, because the language selection is
    // a setting that can be stored in options.
    OptionsModel optionsModel(*gui_node, *sidestake_manager);

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
    // Gracefully exit if the user cancels
    if (!Intro::showIfNeeded(did_show_intro)) return EXIT_SUCCESS;

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

    // The datadir is now finalized (network selected, -datadir resolved), so the
    // per-node QSettings group is known: read the GUI preferences that are keyed
    // per node. OptionsModel's constructor (Init) ran earlier -- before the
    // datadir was chosen -- so it deliberately deferred these until now.
    optionsModel.readNodeSettings();

    // Now that the config file, network selection and read-write settings file
    // are loaded, migrate any proxy / UPnP / reservebalance / update-check values
    // the user had in Gridcoin-Qt.conf into the core read-write settings (one-time).
    optionsModel.migrateCoreSettings();

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

    /** Start Qt as normal before it was moved into this function **/
    StartGridcoinQt(argc, argv, app, optionsModel, *gui_node);

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

int StartGridcoinQt(int argc, char *argv[], QApplication& app, OptionsModel& optionsModel, interfaces::Node& gui_node)
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
    // GUI. Routed through the node interface rather than a raw
    // uiInterface.QueueShutdown_connect so Phase 2 can deliver it over IPC. The
    // returned Handler is kept only to keep the subscription alive (RAII): it
    // must outlive app.exec(), so it lives for this function's scope. gui_node
    // (main()'s early node) outlives this call. Wired here, before AppInit2, so a
    // shutdown requested during core init still reaches the GUI.
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

        // Owns the interface factory the models below consume core state through
        // -- the local in-process Init in the monolith, the remote node's Init
        // in the multiprocess build. `interface_init` points at whichever is in
        // use; its owner (declared here) outlives every model constructed below.
        std::unique_ptr<interfaces::Init> local_init;
#ifdef ENABLE_MULTIPROCESS
        std::optional<ipc::GuiConnection> node_connection;
#endif
        interfaces::Init* interface_init = nullptr;

        if (multiprocess)
        {
#ifdef ENABLE_MULTIPROCESS
            // MakeIpc needs an Init even for a connect-only client (it is the
            // Init this process would serve if it listened, which the GUI never
            // does); the local one wraps globals and is cheap. It must outlive
            // node_connection, so it is declared first.
            local_init = interfaces::MakeGridcoinInit();
            std::string ipc_error;
            node_connection = ipc::ConnectToNode(GetDataDir(), *local_init, ipc_error,
                /*on_disconnect=*/[] {
                    // Fires on the IPC event-loop thread when the daemon vanishes
                    // without a clean shutdown message (crash / SIGKILL, or a stop
                    // that raced the handshake). Route to the shared graceful-quit
                    // path (same as a core-initiated shutdown) so the GUI tears down
                    // cleanly instead of faulting on the next call to a now-dead
                    // proxy. Shares the once-latch with GridcoinApplication::notify(),
                    // so whichever detects the loss first wins and the other is a
                    // no-op; safe if the QCoreApplication is already gone.
                    QuitOnDaemonConnectionLost("lost connection to the Gridcoin daemon");
                });
            if (!node_connection)
            {
                // One dialog: this runs after ThreadSafeMessageBox_connect, so a
                // uiInterface message box here would double up with the direct one.
                GUILogPrintf("IPC: could not connect to the Gridcoin daemon: %s", ipc_error);
                QMessageBox::critical(nullptr, PACKAGE_NAME,
                        QObject::tr("Could not connect to the Gridcoin daemon:\n%1").arg(QString::fromStdString(ipc_error)));
                return EXIT_FAILURE;
            }
            interface_init = node_connection->init.get();

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
#else
            GUILogPrintf("IPC: -multiprocess requested but this build has no multiprocess (IPC) support");
            QMessageBox::critical(nullptr, PACKAGE_NAME,
                    QObject::tr("This build was compiled without multiprocess (IPC) support."));
            return EXIT_FAILURE;
#endif
        }
        else
        {
            if (!threads->createThread(ThreadAppInit2,threads,"AppInit2 Thread"))
            {
                GUILogPrintf("Error; NewThread(ThreadAppInit2) failed");
                return EXIT_FAILURE;
            }
            // The monolithic-build interface implementations that the models
            // consume core state through (doc/multiprocess_design.md). Created
            // here, before the readiness wait, so the GUI polls
            // interface_init->isCoreReady() rather than the raw
            // bGridcoinCoreInitComplete global; it outlives every model
            // constructed below.
            local_init = interfaces::MakeGridcoinInit();
            interface_init = local_init.get();
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
            // (node_connection's destructor) as the stack unwinds.
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
