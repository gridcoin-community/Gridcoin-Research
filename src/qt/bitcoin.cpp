/*
 * W.J. van der Laan 2011-2012
 */


#include <QApplication>
#include <QTimer>

#include "bitcoingui.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "global_objects_noui.hpp"
#include "guiutil.h"
#include "guiconstants.h"
#include "init.h"
#include "ui_interface.h"
#include "qtipcserver.h"
#include "util.h"
#include "winshutdownmonitor.h"
#include "upgrade.h"
#include "upgradeqt.h"

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
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
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

// Need a global reference for the notifications to find the GUI
static BitcoinGUI *guiref;
static QSplashScreen *splashref;

int StartGridcoinQt(int argc, char *argv[]);

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
    if(nFeeRequired < MIN_TX_FEE || nFeeRequired <= nTransactionFee || fDaemon)
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

static void UpdateMessageBox(const std::string& message)
{
    std::string caption = _("Gridcoin Update Available");

    if (guiref)
    {
        QMetaObject::invokeMethod(guiref, "update", Qt::QueuedConnection,
                                   Q_ARG(QString, QString::fromStdString(caption)),
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
#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char *msg)
{
    if (type == QtDebugMsg) {
        LogPrint("Qt", "GUI: %s\n", msg);
    } else {
        LogPrintf("GUI: %s\n", msg);
    }
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        LogPrint("Qt", "GUI: %s\n", msg.toStdString());
    } else {
        LogPrintf("GUI: %s\n", msg.toStdString());
    }
}
#endif


void timerfire()
{

}

/* Handle runaway exceptions. Shows a message box with the problem and quits the program.
 */
static void handleRunawayException(std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    QMessageBox::critical(0, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. Gridcoin can no longer continue safely and will quit.") + QString("\n") + QString::fromStdString(strMiscWarning));
    exit(1);
}

#ifndef BITCOIN_QT_TEST
int main(int argc, char *argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif

    SetupEnvironment();

    // Set default value to exit properly. Exit code 42 will trigger restart of the wallet.
    int currentExitCode = 0;

    // Do this early as we don't want to bother initializing if we are just calling IPC
    ipcScanRelay(argc, argv);

    // Command-line options take precedence:
    // Before this would of been done in main then config file loaded.
    // But we need to check for snapshot argument anyway so doing so here is safe.
    ParseParameters(argc, argv);

    // Here we do it if it was started with the snapshot argument
    if (mapArgs.count("-snapshotdownload"))
    {
        Upgrade Snapshot;

        // Let's check make sure gridcoin is not already running in the data directory.
        if (Snapshot.IsDataDirInUse())
        {
            fprintf(stderr, "Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.", GetDataDir().string().c_str());

            exit(1);
        }

        else
            Snapshot.SnapshotMain();
    }

    /** Start Qt as normal before it was moved into this function **/
    currentExitCode = StartGridcoinQt(argc, argv);

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
        UpgradeQt test;

        if (test.SnapshotMain())
            LogPrintf("Snapshot: Success!");

        else
        {
            if (fCancelOperation)
                LogPrintf("Snapshot: Failed!; Canceled by user.");

            else
                LogPrintf("Sanpshot: Failed!");
        }
    }

    return 0;
}

#endif // BITCOIN_QT_TEST
