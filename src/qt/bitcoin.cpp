/*
 * W.J. van der Laan 2011-2012
 */


#include <QApplication>
#include <QTimer>

#include "bitcoingui.h"
#include "chainparams.h"
#include "chainparamsbase.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "researcher/researchermodel.h"
#include "voting/votingmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "qt/intro.h"
#include "guiconstants.h"
#include "init.h"
#include "ui_interface.h"
#include "qtipcserver.h"
#include "txdb.h"
#include "util.h"
#include "winshutdownmonitor.h"
#include "gridcoin/upgrade.h"
#include "gridcoin/gridcoin.h"
#include "policy/fees.h"
#include "upgradeqt.h"
#include "validation.h"

#include <QMessageBox>
#include <QDebug>
#include <QTextCodec>
#include <QLocale>
#include <QTranslator>
#include <QSplashScreen>
#include <QLibraryInfo>
#include <QProcess>

#if defined(BITCOIN_NEED_QT_PLUGINS) && !defined(_BITCOIN_QT_PLUGINS_INCLUDED)
#define _BITCOIN_QT_PLUGINS_INCLUDED
#define __INSURE__
#include <QtPlugin>
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
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
extern bool bGridcoinCoreInitComplete;

// Need a global reference for the notifications to find the GUI
static BitcoinGUI *guiref;
static QSplashScreen *splashref;

static void RegisterMetaTypes()
{
    // Register meta types used for QMetaObject::invokeMethod and Qt::QueuedConnection
    // (...Gridcoin has none yet...)

    // Register typedefs (see https://doc.qt.io/qt-5/qmetatype.html#qRegisterMetaType)
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<uint32_t>("uint32_t");
}

int StartGridcoinQt(int argc, char *argv[], QApplication& app, OptionsModel& optionsModel);

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
}

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
        LogPrintf("%s: %s", caption, message);
        fprintf(stderr, "%s: %s\n", caption.c_str(), message.c_str());
    }
}

static bool ThreadSafeAskFee(int64_t nFeeRequired, const std::string& strCaption)
{
    if(!guiref)
        return false;

    int64_t nMinFee;

    {
        LOCK(cs_main);

        CTransaction txDummy;

        // Min Fee
        nMinFee = GetBaseFee(txDummy, GMF_SEND);
    }

    if(nFeeRequired < nMinFee || nFeeRequired <= nTransactionFee || fDaemon)
        return true;
    bool payFee = false;

    QMetaObject::invokeMethod(guiref, "askFee", GUIUtil::blockingGUIThreadConnection(),
                               Q_ARG(qint64, nFeeRequired),
                               Q_ARG(bool*, &payFee));

    return payFee;
}


static bool ThreadSafeAskQuestion(std::string strCaption, std::string Body)
{
    if(!guiref) return false;
    bool result = false;
    QMetaObject::invokeMethod(guiref, "askQuestion", GUIUtil::blockingGUIThreadConnection(),
                               Q_ARG(std::string, strCaption), Q_ARG(std::string, Body),
                               Q_ARG(bool*, &result));
    return result;
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

static void UpdateMessageBox(const std::string& version, const std::string& message)
{
    std::string caption = _("Gridcoin Update Available");

    if (guiref)
    {
        std::string guiaddition = version + _("Click \"Show Details\" to view changes in latest update.");
        QMetaObject::invokeMethod(guiref, "update", Qt::QueuedConnection,
                                   Q_ARG(QString, QString::fromStdString(caption)),
                                   Q_ARG(QString, QString::fromStdString(guiaddition)),
                                   Q_ARG(QString, QString::fromStdString(message)));
    }

    else
    {
        LogPrintf("\r\n%s:\r\n%s", caption, message);
        fprintf(stderr, "\r\n%s:\r\n%s\r\n", caption.c_str(), message.c_str());
    }
}

static void QueueShutdown()
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("bitcoin-core", psz).toStdString();
}

/* qDebug() message handler --> debug.log */
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        LogPrint(BCLog::LogFlags::QT, "GUI: %s\n", msg.toStdString());
    } else {
        LogPrintf("GUI: %s\n", msg.toStdString());
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
    SetupServerArgs();
    SetupUIArgs(gArgs);

    // Note every function above the InitLogging() call must use fprintf or similar.

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

    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Initiate the app here to support choosing the data directory.
    Q_INIT_RESOURCE(bitcoin);
    Q_INIT_RESOURCE(bitcoin_locale);

    RegisterMetaTypes();
    QApplication app(argc, argv);

#if defined(WIN32) && defined(QT_GUI)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

    // We notify the user here and exit the application from the command line parse failure above. This is the earliest
    // a dialog can be raised, and is the latest that is safe if the command line is not parseable.
    if (command_line_parse_failure) {
        tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error);
        ThreadSafeMessageBox(strprintf("Error reading configuration file.\n"),
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME, QObject::tr("Error: Cannot parse command line arguments. Please check "
                                                                 "the arguments and ensure they are valid and formatted "
                                                                 "correctly."));
        return EXIT_FAILURE;
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

#if defined(WIN32)
    // Install global event filter for processing Windows session related Windows messages (WM_QUERYENDSESSION and WM_ENDSESSION)
    app.installNativeEventFilter(new WinShutdownMonitor());
#endif

    // Load the optionsModel. This has to be loaded before the translations, because the language selection is
    // a setting that can be stored in options.
    OptionsModel optionsModel;

    // Get desired locale (e.g. "de_DE") from command line or use system locale
    QString lang_territory = QString::fromStdString(gArgs.GetArg("-lang", QLocale::system().name().toStdString()));
    QString lang = lang_territory;
    // Convert to "de" only by truncating "_DE"
    lang.truncate(lang_territory.lastIndexOf('_'));

    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
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
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME, QObject::tr("Error: Cannot read configuration file. Please check the "
                                                                 "path and format of the file."));
        return EXIT_FAILURE;
    }

    // Do this to pickup -testnet from the command line.
    SelectParams(gArgs.IsArgSet("-testnet") ? CBaseChainParams::TESTNET : CBaseChainParams::MAIN);

    // Determine availability of data directory and parse gridcoinresearch.conf
    // Do not call GetDataDir(true) before this step finishes
    if (!CheckDataDirOption()) {
        ThreadSafeMessageBox(strprintf("Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "")),
                             "", CClientUIInterface::ICON_ERROR | CClientUIInterface::OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
            QObject::tr("Error: Specified data directory \"%1\" does not exist.")
                              .arg(QString::fromStdString(gArgs.GetArg("-datadir", ""))));
        return EXIT_FAILURE;
    }

    // This check must be done before logging is initialized or the config file is read. We do not want another
    // instance writing into an already running Gridcoin instance's logs. This is checked in init too,
    // but that is too late.
    fs::path dataDir = GetDataDir();

    if (!LockDirectory(dataDir, ".lock", false)) {
        std::string str = strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running "
                                      "and using that directory."),
                                    dataDir, PACKAGE_NAME);
        ThreadSafeMessageBox(str, _("Gridcoin"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
            QObject::tr("Error: Cannot obtain a lock on the specified data directory. "
                        "An instance is probably already using that directory."));

        return EXIT_FAILURE;
    }

    // Reread config file after correct chain is selected
    if (!gArgs.ReadConfigFiles(error, true)) {
        ThreadSafeMessageBox(strprintf("Error reading configuration file: %s\n", error),
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
            QObject::tr("Error: Cannot parse configuration file: %1.").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    if (!gArgs.InitSettings(error)) {
        ThreadSafeMessageBox(strprintf("Error initializing settings.\n"),
                "", CClientUIInterface::ICON_ERROR | CClientUIInterface::OK | CClientUIInterface::MODAL);
        QMessageBox::critical(nullptr, PACKAGE_NAME,
                              QObject::tr("Error initializing settings: %1").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    // Initialize logging as early as possible.
    InitLogging();

    // Do this early as we don't want to bother initializing if we are just calling IPC
    ipcScanRelay(argc, argv);

    // Make sure a user does not request snapshotdownload and resetblockchaindata at same time!
    if (gArgs.IsArgSet("-snapshotdownload") && gArgs.IsArgSet("-resetblockchaindata"))
    {
        LogPrintf("-snapshotdownload and -resetblockchaindata cannot be used in conjunction");

        return EXIT_FAILURE;
    }

    // Run snapshot main if Gridcoin was started with the snapshot argument and we are not TestNet
    if (gArgs.IsArgSet("-snapshotdownload") && !gArgs.IsArgSet("-testnet"))
    {
        GRC::Upgrade snapshot;

        try
        {
            snapshot.SnapshotMain();
        }

        catch (std::runtime_error& e)
        {
            LogPrintf("Snapshot Downloader: Runtime exception occurred in SnapshotMain() (%s)", e.what());

            snapshot.DeleteSnapshot();

            return EXIT_FAILURE;
        }

        // Delete snapshot regardless of result.
        snapshot.DeleteSnapshot();
    }

    // Check to see if the user requested to reset blockchain data -- We allow on testnet.
    if (gArgs.IsArgSet("-resetblockchaindata"))
    {
        GRC::Upgrade resetblockchain;

        if (resetblockchain.ResetBlockchainData())
            LogPrintf("ResetBlockchainData: success");

        else
        {
            LogPrintf("ResetBlockchainData: failed to clean up blockchain data");

            std::string inftext = resetblockchain.ResetBlockchainMessages(resetblockchain.CleanUp);

            ThreadSafeMessageBox(inftext, _("Gridcoin"), CClientUIInterface::OK | CClientUIInterface::MODAL);
            QMessageBox::critical(nullptr, PACKAGE_NAME, QString::fromStdString(inftext));

            return EXIT_FAILURE;
        }
    }

    /** Start Qt as normal before it was moved into this function **/
    StartGridcoinQt(argc, argv, app, optionsModel);

    // We got a request to apply snapshot from GUI Menu selection
    // We got this request and everything should be shutdown now.
    // Except what we cannot shutdown.
    // In future once code base for the databases are updated we won't need
    // the wallet user to start the QT wallet themselves after the snapshot
    // has been applied. We will be able to restart it ourselves
    // See Bitcoin's database files and pointers involved and Reset()
    // What prevents us from starting the wallet again internally
    // is the lingering of database.
    // The linger does not affect the snapshot process but prevents restart of wallet within main area

    /** This is only if the GUI menu option was used! **/
    if (fSnapshotRequest)
    {
        UpgradeQt Snapshot;

        // Release LevelDB file handles on Windows so we can remove the old
        // blockchain files:
        //
        // We should really close it in Shutdown() when the main application
        // exits. Before we can do that, we need to solve an old outstanding
        // conflict with the behavior of "-daemon" on Linux that prematurely
        // closes the DB when the process forks.
        //
        CTxDB().Close();

        if (Snapshot.SnapshotMain(app))
            LogPrintf("Snapshot: Success!");

        else
        {
            if (GRC::fCancelOperation)
                LogPrintf("Snapshot: Failed!; Canceled by user.");

            else
                LogPrintf("Snapshot: Failed!");
        }

        Snapshot.DeleteSnapshot();
    }

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
            LogPrintf("ResetBlockchainData: success");

        else
            LogPrintf("ResetBlockchainData: failed");
    }

    return EXIT_SUCCESS;
}

int StartGridcoinQt(int argc, char *argv[], QApplication& app, OptionsModel& optionsModel)
{
    // Set global boolean to indicate intended presence of GUI to core.
    fQtActive = true;

    std::shared_ptr<ThreadHandler> threads = std::make_shared<ThreadHandler>();

    // Install qDebug() message handler to route to debug.log
    qInstallMessageHandler(DebugMessageHandler);

    // Subscribe to global signals from core
    uiInterface.ThreadSafeMessageBox.connect(ThreadSafeMessageBox);
    uiInterface.ThreadSafeAskFee.connect(ThreadSafeAskFee);
    uiInterface.ThreadSafeAskQuestion.connect(ThreadSafeAskQuestion);

    uiInterface.ThreadSafeHandleURI.connect(ThreadSafeHandleURI);
    uiInterface.InitMessage.connect(InitMessage);
    uiInterface.QueueShutdown.connect(QueueShutdown);
    uiInterface.Translate.connect(Translate);

    uiInterface.UpdateMessageBox.connect(UpdateMessageBox);

    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (HelpRequested(gArgs))
    {
        GUIUtil::HelpMessageBox help;
        help.showOrPrint();
        return EXIT_FAILURE;
    }

    QSplashScreen splash(QPixmap(":/images/splash"));
    if (gArgs.GetBoolArg("-splash", true) && !gArgs.GetBoolArg("-min"))
    {
        splash.setEnabled(false);
        splash.show();
        splashref = &splash;
    }

    app.processEvents();

    app.setQuitOnLastWindowClosed(false);

    try
    {
        BitcoinGUI window;
        guiref = &window;

        LogPrintf("Starting Gridcoin");

        if (!threads->createThread(ThreadAppInit2,threads,"AppInit2 Thread"))
        {
                LogPrintf("Error; NewThread(ThreadAppInit2) failed");
                return EXIT_FAILURE;
        }
        else
        {
             //10-31-2015
            while (!bGridcoinCoreInitComplete)
            {
                app.processEvents();

                // The sleep here has to be pretty short to avoid a buffer overflow crash with
                // fast CPUs due to too many events. It originally was set to 300 ms and has
                // been reduced to 100 ms.
                MilliSleep(100);
            }

            if (splashref)
                splash.finish(&window);

            if (!fRequestShutdown) {
                // Put this in a block, so that the Model objects are cleaned up before
                // calling Shutdown().

                ClientModel clientModel(&optionsModel);
                WalletModel walletModel(pwalletMain, &optionsModel);
                ResearcherModel researcherModel;
                VotingModel votingModel(clientModel, optionsModel, walletModel);

                window.setResearcherModel(&researcherModel);
                window.setClientModel(&clientModel);
                window.setWalletModel(&walletModel);
                window.setVotingModel(&votingModel);

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
                ipcInit(argc, argv);

#if defined(WIN32) && defined(QT_GUI)
                WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("%1 didn't yet exit safely...").arg(QObject::tr(PACKAGE_NAME)), (HWND)window.winId());
#endif

                LogPrintf("GUI loaded.");

                // Regenerate startup link, to fix links to old versions
                GUIUtil::SetStartOnSystemStartup(optionsModel.getStartAtStartup(), optionsModel.getStartMin());

                app.exec();

                window.hide();
                window.setClientModel(nullptr);
                window.setWalletModel(nullptr);
                window.setResearcherModel(nullptr);
                guiref = nullptr;
            }
            // Shutdown the core and its threads, but don't exit Bitcoin-Qt here
            LogPrintf("Main calling Shutdown...");
            Shutdown(nullptr);
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
