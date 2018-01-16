/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */


#include <QProcess>

#if defined(WIN32) && defined(QT_GUI)
#include <QAxObject>
#include <ActiveQt/qaxbase.h>
#include <ActiveQt/qaxobject.h>
#endif

#include <QInputDialog>
// include <QtSql> // Future Use

#include <fstream>
#include "util.h"

#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"

#include "diagnosticsdialog.h"
#include "upgradedialog.h"
#include "upgrader.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "votingdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "init.h"
#include "block.h"
#include "miner.h"
#include "main.h"
#include "backup.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QStandardPaths>
#endif

#include <QTimer>
#include <QDragEnterEvent>
#include <QDesktopServices> // for opening URLs
#include <QUrl>
#include <QStyle>
#include <QNetworkInterface>
#include <QDesktopWidget>

#include <boost/lexical_cast.hpp>

#include "bitcoinrpc.h"

#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include "boinc.h"

extern CWallet* pwalletMain;
int ReindexWallet();
extern QString ToQstring(std::string s);
extern void qtSetSessionInfo(std::string defaultgrcaddress, std::string cpid, double magnitude);
extern void qtSyncWithDPORNodes(std::string data);
extern double qtExecuteGenericFunction(std::string function,std::string data);
extern std::string getMacAddress();
extern bool PushGridcoinDiagnostics();
extern double qtPushGridcoinDiagnosticData(std::string data);

extern std::string FromQString(QString qs);
extern std::string qtExecuteDotNetStringFunction(std::string function, std::string data);

std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2);
std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5);
std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5, std::string arg6);

std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);

extern std::string qtGetNeuralHash(std::string data);
extern std::string qtGetNeuralContract(std::string data);
json_spirit::Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool bIncludeExpired);

extern int64_t IsNeural();

std::string getfilecontents(std::string filename);
int nRegVersion;

extern int CreateRestorePoint();
extern int DownloadBlocks();
void GetGlobalStatus();

bool IsConfigFileEmpty();
void HarvestCPIDs(bool cleardata);
extern int RestartClient();
extern int ReindexWallet();
void ReinstantiateGlobalcom();

#ifdef WIN32
QAxObject *globalcom = NULL;
QAxObject *globalwire = NULL;
#endif

QString ToQstring(std::string s)
{
    QString str1 = QString::fromUtf8(s.c_str());
    return str1;
}



BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    unlockWalletAction(0),
    lockWalletAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0),
    upgrader(0),
    nWeight(0)
{

    setGeometry(QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,QSize(980,550),qApp->desktop()->availableGeometry()));

    setWindowTitle(tr("Gridcoin") + " " + tr("Wallet"));

#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    votingPage = new VotingDialog(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    centralWidget->addWidget(votingPage);
    setCentralWidget(centralWidget);

    // Create status bar
    // statusBar();

    syncIconMovie = new QMovie(":/movies/update_spinner", "GIF", this);

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    upgrader = new UpgradeDialog(this);
    connect(upgradeAction, SIGNAL(triggered()), upgrader, SLOT(show()));
    connect(upgradeAction, SIGNAL(triggered()), upgrader, SLOT(upgrade()));
    upgradeAction->setVisible(false);
    connect(downloadAction, SIGNAL(triggered()), upgrader, SLOT(show()));
    connect(downloadAction, SIGNAL(triggered()), upgrader, SLOT(blocks()));

    diagnosticsDialog = new DiagnosticsDialog(this);


    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    gotoOverviewPage();
}

BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}





int ReindexWallet()
{
    if (!bGlobalcomInitialized)
        return 0;

#ifdef WIN32
    globalcom->dynamicCall(fTestNet
                           ? "ReindexWalletTestNet()"
                           : "ReindexWallet()");
#endif

    StartShutdown();
    return 1;
}

int CreateRestorePoint()
{
    if (!bGlobalcomInitialized)
        return 0;

#ifdef WIN32
    globalcom->dynamicCall(fTestNet
                           ? "CreateRestorePointTestNet()"
                           : "CreateRestorePoint()");
#endif

    return 1;
}

int DownloadBlocks()
{
    printf("Download blocks.");

    // Instantiate globalcom if not created.
    if (!bGlobalcomInitialized)
        ReinstantiateGlobalcom();

    // If it still isn't created then there's nothing we can do.
    if (!bGlobalcomInitialized)
        return 0;

#ifdef WIN32
    std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
    qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
    globalcom->dynamicCall("DownloadBlocks()");
    StartShutdown();
#endif

    return 1;
}



int RestartClient()
{
    if (!bGlobalcomInitialized)
        return 0;

#ifdef WIN32
    globalcom->dynamicCall("RebootClient()");
#endif

    StartShutdown();
    return 1;
}

double qtPushGridcoinDiagnosticData(std::string data)
{
    if (!bGlobalcomInitialized) return 0;
    int result = 0;
    #if defined(WIN32) && defined(QT_GUI)
            QString qsData = ToQstring(data);
            result = globalcom->dynamicCall("PushGridcoinDiagnosticData(Qstring)",qsData).toInt();
    #endif
    return result;
}

//R Halford - 6/19/2015 - Let's clean up the windows side by removing all these functions and making a generic interface for comm between Windows and Linux; Start with one new generic function here:

double qtExecuteGenericFunction(std::string function, std::string data)
{
    if (!bGlobalcomInitialized) return 0;

    int result = 0;
    #if defined(WIN32) && defined(QT_GUI)
        QString qsData = ToQstring(data);
        QString qsFunction = ToQstring(function +"(Qstring)");
        std::string sFunction = function+"(Qstring)";
        if (data=="")
        {
            sFunction = function + "()";
            globalcom->dynamicCall(sFunction.c_str());
        }
        else
        {
            result = globalcom->dynamicCall(sFunction.c_str(),qsData).toInt();
        }
    #endif
    return result;
}



std::string qtExecuteDotNetStringFunction(std::string function, std::string data)
{
    std::string sReturnData = "";
    if (!bGlobalcomInitialized) return "";

    #if defined(WIN32) && defined(QT_GUI)
        if (!bGlobalcomInitialized) return "?";
        QString qsData = ToQstring(data);
        QString qsFunction = ToQstring(function +"(Qstring)");
        std::string sFunction = function+"(Qstring)";
        QString qsReturnData = globalcom->dynamicCall(sFunction.c_str(),qsData).toString();
        sReturnData = FromQString(qsReturnData);
        return sReturnData;
    #endif
    return sReturnData;
}



void qtSyncWithDPORNodes(std::string data)
{

    #if defined(WIN32) && defined(QT_GUI)
        if (!bGlobalcomInitialized) return;
        int result = 0;
        QString qsData = ToQstring(data);
        if (fDebug3) printf("FullSyncWDporNodes");
        std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
        double function_call = qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
        result = globalcom->dynamicCall("SyncCPIDsWithDPORNodes(Qstring)",qsData).toInt();
        printf("Done syncing. %f %f\r\n",function_call,(double)result);
    #endif
}


std::string FromQString(QString qs)
{
    std::string sOut = qs.toUtf8().constData();
    return sOut;
}



std::string qtGetNeuralContract(std::string data)
{

    #if defined(WIN32) && defined(QT_GUI)
    try
    {
        if (!bGlobalcomInitialized) return "NA";
        QString qsData = ToQstring(data);
        //if (fDebug3) printf("GNC# ");
        QString sResult = globalcom->dynamicCall("GetNeuralContract()").toString();
        std::string result = FromQString(sResult);
        return result;
    }
    catch(...)
    {
        return "?";
    }
    #else
        return "?";
    #endif
}



std::string qtGetNeuralHash(std::string data)
{

    #if defined(WIN32) && defined(QT_GUI)
    try
    {
        if (!bGlobalcomInitialized) return "NA";

        QString qsData = ToQstring(data);
        QString sResult = globalcom->dynamicCall("GetNeuralHash()").toString();
        std::string result = FromQString(sResult);
        return result;
    }
    catch(...)
    {
        return "?";
    }
    #else
        return "?";
    #endif
}

void qtSetSessionInfo(std::string defaultgrcaddress, std::string cpid, double magnitude)
{

    if (!bGlobalcomInitialized) return;

    #if defined(WIN32) && defined(QT_GUI)
        int result = 0;
        std::string session = defaultgrcaddress + "<COL>" + cpid + "<COL>" + RoundToString(magnitude,1);
        QString qsSession = ToQstring(session);
        result = globalcom->dynamicCall("SetSessionInfo(Qstring)",qsSession).toInt();
        printf("rs%f",(double)result);
    #endif
}

bool IsUpgradeAvailable()
{
   if (!bGlobalcomInitialized)
      return false;

   bool upgradeAvailable = false;
   if (!fTestNet)
   {
#ifdef WIN32
      upgradeAvailable = globalcom->dynamicCall("ClientNeedsUpgrade()").toInt();
#endif
   }

   return upgradeAvailable;
}


int64_t IsNeural()
{
    if (!bGlobalcomInitialized) return 0;
    try
    {
        //NeuralNetwork
        int nNeural = 0;
#ifdef WIN32
        nNeural = globalcom->dynamicCall("NeuralNetwork()").toInt();
#endif
        return nNeural;
    }
    catch(...)
    {
        printf("Exception\r\n");
        return 0;
    }
}



int UpgradeClient()
{
    if (!bGlobalcomInitialized)
        return 0;

    printf("Executing upgrade");

#ifdef WIN32
    globalcom->dynamicCall(fTestNet
                           ? "UpgradeWallet()"
                           : "UpgradeWalletTestnet()");
#endif

    StartShutdown();
    return 1;
}


void BitcoinGUI::setOptionsStyleSheet(QString qssFileName)
{
    // setting the style sheets for the app
    QFile qss(":stylesheets/"+qssFileName);
    if (qss.open(QIODevice::ReadOnly)){
        QTextStream qssStream(&qss);
        QString sMainWindowHTML = qssStream.readAll();
        qss.close();

        qApp->setStyleSheet(sMainWindowHTML);
    }

}


void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), tabGroup);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send"), tabGroup);
    sendCoinsAction->setToolTip(tr("Send coins to a Gridcoin address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive"), tabGroup);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), tabGroup);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), tabGroup);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));

    votingAction = new QAction(QIcon(":/icons/voting"), tr("&Voting"), tabGroup);
    votingAction->setToolTip(tr("Voting"));
    votingAction->setCheckable(true);
    votingAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));

    bxAction = new QAction(QIcon(":/icons/block"), tr("&Block Explorer"), this);
    bxAction->setStatusTip(tr("Block Explorer"));
    bxAction->setMenuRole(QAction::TextHeuristicRole);

    exchangeAction = new QAction(QIcon(":/icons/ex"), tr("&Exchange"), this);
    exchangeAction->setStatusTip(tr("Web Site"));
    exchangeAction->setMenuRole(QAction::TextHeuristicRole);

    websiteAction = new QAction(QIcon(":/icons/www"), tr("&Web Site"), this);
    websiteAction->setStatusTip(tr("Web Site"));
    websiteAction->setMenuRole(QAction::TextHeuristicRole);

    chatAction = new QAction(QIcon(":/icons/chat"), tr("&GRC Chat Room"), this);
    chatAction->setStatusTip(tr("GRC Chatroom"));
    chatAction->setMenuRole(QAction::TextHeuristicRole);

    boincAction = new QAction(QIcon(":/images/boinc"), tr("&BOINC"), this);
    boincAction->setStatusTip(tr("Gridcoin rewards distributed computing with BOINC"));
    boincAction->setMenuRole(QAction::TextHeuristicRole);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    connect(votingAction, SIGNAL(triggered()), this, SLOT(votingClicked()));

    connect(websiteAction, SIGNAL(triggered()), this, SLOT(websiteClicked()));
    connect(bxAction, SIGNAL(triggered()), this, SLOT(bxClicked()));
    connect(exchangeAction, SIGNAL(triggered()), this, SLOT(exchangeClicked()));
    connect(boincAction, SIGNAL(triggered()), this, SLOT(boincClicked()));
    connect(chatAction, SIGNAL(triggered()), this, SLOT(chatClicked()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);



    rebuildAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Rebuild Block Chain"), this);
    rebuildAction->setStatusTip(tr("Rebuild Block Chain"));
    rebuildAction->setMenuRole(QAction::TextHeuristicRole);

    downloadAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Download Blocks"), this);
    downloadAction->setStatusTip(tr("Download Blocks"));
    downloadAction->setMenuRole(QAction::TextHeuristicRole);

    upgradeAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Upgrade Client"), this);
    upgradeAction->setStatusTip(tr("Upgrade Client"));
    upgradeAction->setMenuRole(QAction::TextHeuristicRole);

    rebootAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Reboot Client"), this);
    rebootAction->setStatusTip(tr("Reboote Client"));
    rebootAction->setMenuRole(QAction::TextHeuristicRole);

    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About Gridcoin"), this);
    aboutAction->setToolTip(tr("Show information about Gridcoin"));
    aboutAction->setMenuRole(QAction::AboutRole);

    miningAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Neural Network"), this);
    miningAction->setStatusTip(tr("Neural Network"));
    miningAction->setMenuRole(QAction::TextHeuristicRole);

    configAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Advanced Configuration"), this);
    configAction->setStatusTip(tr("Advanced Configuration"));
    configAction->setMenuRole(QAction::TextHeuristicRole);

    newUserWizardAction = new QAction(QIcon(":/icons/bitcoin"), tr("&New User Wizard"), this);
    newUserWizardAction->setStatusTip(tr("New User Wizard"));
    newUserWizardAction->setMenuRole(QAction::TextHeuristicRole);

    foundationAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Foundation"), this);
    foundationAction->setStatusTip(tr("Foundation"));
    foundationAction->setMenuRole(QAction::TextHeuristicRole);

    diagnosticsAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Diagnostics"), this);
    diagnosticsAction->setStatusTip(tr("Diagnostics"));
    diagnosticsAction->setMenuRole(QAction::TextHeuristicRole);


    faqAction = new QAction(QIcon(":/icons/bitcoin"), tr("FA&Q"), this);
    faqAction->setStatusTip(tr("Interactive FAQ"));
    faqAction->setMenuRole(QAction::TextHeuristicRole);

    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for Gridcoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet/Config..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet/config to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(QIcon(":/icons/lock_open"), tr("&Unlock Wallet..."), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Lock Wallet"), this);
    lockWalletAction->setToolTip(tr("Lock wallet"));
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    connect(rebuildAction, SIGNAL(triggered()), this, SLOT(rebuildClicked()));
    connect(upgradeAction, SIGNAL(triggered()), this, SLOT(upgradeClicked()));
    connect(downloadAction, SIGNAL(triggered()), this, SLOT(downloadClicked()));
    connect(configAction, SIGNAL(triggered()), this, SLOT(configClicked()));

    connect(miningAction, SIGNAL(triggered()), this, SLOT(miningClicked()));

    connect(diagnosticsAction, SIGNAL(triggered()), this, SLOT(diagnosticsClicked()));

    connect(foundationAction, SIGNAL(triggered()), this, SLOT(foundationClicked()));
    connect(faqAction, SIGNAL(triggered()), this, SLOT(faqClicked()));

    connect(newUserWizardAction, SIGNAL(triggered()), this, SLOT(newUserWizardClicked()));
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);

    settings->addAction(unlockWalletAction);
    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *community = appMenuBar->addMenu(tr("&Community"));
    community->addAction(bxAction);
    community->addAction(exchangeAction);
    community->addAction(boincAction);
    community->addAction(chatAction);
    community->addSeparator();
    community->addAction(websiteAction);

    QMenu *qmAdvanced = appMenuBar->addMenu(tr("&Advanced"));
#ifdef WIN32  // Some actions in this menu are implemented in Visual Basic and thus only work on Windows
    qmAdvanced->addAction(configAction);
    qmAdvanced->addAction(miningAction);
//	qmAdvanced->addAction(newUserWizardAction);
    qmAdvanced->addSeparator();
    qmAdvanced->addAction(faqAction);
    qmAdvanced->addAction(foundationAction);
//	qmAdvanced->addAction(diagnosticsAction);

#endif /* defined(WIN32) */
    qmAdvanced->addSeparator();
    qmAdvanced->addAction(rebuildAction);
#ifdef WIN32
    qmAdvanced->addAction(downloadAction);
#endif

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(diagnosticsAction);
    help->addSeparator();
    help->addAction(aboutAction);
#ifdef WIN32
    help->addSeparator();
    help->addAction(upgradeAction);
#endif

}

void BitcoinGUI::createToolBars()
{
    QToolBar *toolbar = addToolBar("Tabs toolbar");
    toolbar->setObjectName("toolbar");
    addToolBar(Qt::LeftToolBarArea,toolbar);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setMovable( false );
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolbar->setIconSize(QSize(50,25));
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->addAction(votingAction);

    // Prevent Lock from falling off the page

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar->addWidget(spacer);
    spacer->setObjectName("spacer");
    // Unlock Wallet
    toolbar->addAction(unlockWalletAction);
    toolbar->addAction(lockWalletAction);
    QWidget* webSpacer = new QWidget();

    webSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    webSpacer->setMaximumHeight(10);
    toolbar->addWidget(webSpacer);
    webSpacer->setObjectName("WebSpacer");


    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();

    frameBlocks->setContentsMargins(0,0,0,0);

    QVBoxLayout *frameBlocksLayout = new QVBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(1,0,1,0);
    frameBlocksLayout->setSpacing(-1);
    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    frameBlocksLayout->addWidget(labelEncryptionIcon);

    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addWidget(labelBlocksIcon);
    //12-21-2015 Prevent Lock from falling off the page

    frameBlocksLayout->addStretch();

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

    frameBlocks->setObjectName("frame");
    addToolBarBreak(Qt::LeftToolBarArea);
    QToolBar *toolbar2 = addToolBar("Tabs toolbar");
    addToolBar(Qt::LeftToolBarArea,toolbar2);
    toolbar2->setOrientation(Qt::Vertical);
    toolbar2->setMovable( false );
    toolbar2->setObjectName("toolbar2");
    toolbar2->addWidget(frameBlocks);

    addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *toolbar3 = addToolBar("Logo bar");
    addToolBar(Qt::TopToolBarArea,toolbar3);
    toolbar3->setOrientation(Qt::Horizontal);
    toolbar3->setMovable( false );
    toolbar3->setObjectName("toolbar3");
    QLabel *grcLogoLabel = new QLabel();
    grcLogoLabel->setPixmap(QPixmap(":/images/logo_hz"));
    toolbar3->addWidget(grcLogoLabel);
    QWidget* logoSpacer = new QWidget();
    logoSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar3->addWidget(logoSpacer);
    logoSpacer->setObjectName("logoSpacer");
    QLabel *boincLogoLabel = new QLabel();
    boincLogoLabel->setText("<html><head/><body><p align=\"center\"><a href=\"https://boinc.berkeley.edu\"><span style=\" text-decoration: underline; color:#0000ff;\"><img src=\":/images/boinc\"/></span></a></p></body></html>");
    boincLogoLabel->setTextFormat(Qt::RichText);
    boincLogoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    boincLogoLabel->setOpenExternalLinks(true);
    toolbar3->addWidget(boincLogoLabel);


}

void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("Gridcoin client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // set stylesheet
        setOptionsStyleSheet(this->clientModel->getOptionsModel()->getCurrentStyle());
        connect(this->clientModel->getOptionsModel(),SIGNAL(walletStylesheetChanged(QString)),this,SLOT(setOptionsStyleSheet(QString)));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void BitcoinGUI::createTrayIcon()
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setToolTip(tr("Gridcoin client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    trayIcon->show();
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

void BitcoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}


void BitcoinGUI::votingClicked()
{
    votingAction->setChecked(true);
    votingPage->resetData();
    centralWidget->setCurrentWidget(votingPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}


void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%1 active connection(s) to Gridcoin network").arg(count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // return if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip(tr("Processed %n block(s) of transaction history.", "", count));

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
         text = tr("%1 second(s) ago").arg(secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%1 minute(s) ago").arg(secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%1 hour(s) ago").arg(secs/(60*60));
    }
    else
    {
        text = tr("%1 day(s) ago").arg(secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        overviewPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}


void BitcoinGUI::askQuestion(std::string caption, std::string body, bool *result)
{

        QString qsCaption = tr(caption.c_str());
        QString qsBody = tr(body.c_str());
        QMessageBox::StandardButton retval = QMessageBox::question(this, qsCaption, qsBody, QMessageBox::Yes|QMessageBox::Cancel,   QMessageBox::Cancel);
        *result = (retval == QMessageBox::Yes);

}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
          "which goes to the nodes that process your transaction and helps to support the network.  "
          "Do you want to pay the fee?").arg(
                BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Confirm transaction fee"), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}


std::string tostdstring(QString q)
{
    std::string ss1 = q.toLocal8Bit().constData();
    return ss1;
}




bool CreateNewConfigFile(std::string boinc_email)
{
    std::string filename = "gridcoinresearch.conf";
    boost::filesystem::path path = GetDataDir() / filename;
    std::ofstream myConfig;
    myConfig.open (path.string().c_str());
    std::string row = "email=" + boinc_email + "\r\n";
    myConfig << row;
    row = "addnode=node.gridcoin.us \r\n";
    myConfig << row;
    row = "addnode=www.grcpool.com \r\n";
    myConfig << row;
    row = "addnode=seeds.gridcoin.ifoggz-network.xyz \r\n";
    myConfig << row;
    myConfig.close();
    return true;
}


bool ForceInAddNode(std::string sMyAddNode)
{
        LOCK(cs_vAddedNodes);
        std::vector<std::string>::iterator it = vAddedNodes.begin();
        for(; it != vAddedNodes.end(); it++)
            if (sMyAddNode == *it)
            break;
        if (it != vAddedNodes.end()) return false;
        vAddedNodes.push_back(sMyAddNode);
        return true;
}

void BitcoinGUI::NewUserWizard()
{
    if (!IsConfigFileEmpty()) return;
        QString boincemail = "";
        //Typhoon- Check to see if boinc exists in default path - 11-19-2014

        std::string sourcefile = GetBoincDataDir() + "client_state.xml";
        std::string sout = "";
        sout = getfilecontents(sourcefile);
        //bool BoincInstalled = true;
        std::string sBoincNarr = "";
        if (sout == "-1")
        {
            printf("Boinc not installed in default location! \r\n");
            //BoincInstalled=false;
            std::string nicePath = GetBoincDataDir();
            sBoincNarr = "Boinc is not installed in default location " + nicePath + "!  Please set boincdatadir=c:\\programdata\\boinc\\    to the correct path where Boincs programdata directory resides.";
        }

        bool ok;
        boincemail = QInputDialog::getText(this, tr("New User Wizard"),
                                          tr("Please enter your boinc E-mail address, or click <Cancel> to skip for now:"),
                                          QLineEdit::Normal,
                                          "", &ok);
        if (ok && !boincemail.isEmpty())
        {
            std::string new_email = tostdstring(boincemail);
            boost::to_lower(new_email);
            printf("User entered %s \r\n",new_email.c_str());
            //Create Config File
            CreateNewConfigFile(new_email);
            QString strMessage = tr("Created new Configuration File Successfully. ");
            QMessageBox::warning(this, tr("New Account Created - Welcome Aboard!"), strMessage);
            //Load CPIDs:
            HarvestCPIDs(true);
        }
        else
        {
            //Create Config File
            CreateNewConfigFile("investor");
            QString strMessage = tr("To get started with Boinc, run the boinc client, choose projects, then populate the gridcoinresearch.conf file in %appdata%\\GridcoinResearch with your boinc e-mail address.  To run this wizard again, please delete the gridcoinresearch.conf file. ");
            QMessageBox::warning(this, tr("New User Wizard - Skipped"), strMessage);
        }
        // Read in the mapargs, and set the seed nodes 10-13-2015
        ReadConfigFile(mapArgs, mapMultiArgs);
        //Force some addnodes in to get user started
        ForceInAddNode("node.gridcoin.us");
        ForceInAddNode("london.grcnode.co.uk");
        ForceInAddNode("gridcoin.crypto.fans");
        ForceInAddNode("seeds.gridcoin.ifoggz-network.xyz");
        ForceInAddNode("nuad.de");
        ForceInAddNode("www.grcpool.com");

        if (sBoincNarr != "")
        {
                QString qsMessage = tr(sBoincNarr.c_str());
                QMessageBox::warning(this, tr("Attention! - Boinc Path Error!"), qsMessage);
        }


}



void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
                            TransactionTableModel::ToAddress, parent)
                        .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount)<0 ? tr("Sent transaction") :
                                         tr("Incoming transaction"),
                              tr("Date: %1\n"
                                 "Amount: %2\n"
                                 "Type: %3\n"
                                 "Address: %4\n")
                              .arg(date)
                              .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
    }
}

void BitcoinGUI::rebuildClicked()
{
    printf("Rebuilding...");
    ReindexWallet();
}

void BitcoinGUI::upgradeClicked()
{
    printf("Upgrading Gridcoin...");
    UpgradeClient();
}

void BitcoinGUI::downloadClicked()
{
    DownloadBlocks();
}

void BitcoinGUI::rebootClicked()
{
    qApp->exit(EXIT_CODE_REBOOT);
}

void BitcoinGUI::configClicked()
{
#ifdef WIN32
    if (!bGlobalcomInitialized) return;
    std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
    qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
    globalcom->dynamicCall("ShowConfig()");
#endif
}

void BitcoinGUI::diagnosticsClicked()
{
    diagnosticsDialog->show();
    diagnosticsDialog->raise();
    diagnosticsDialog->activateWindow();
}

void BitcoinGUI::foundationClicked()
{
#ifdef WIN32
    if (!bGlobalcomInitialized) return;
    std::string sVotingPayload = "";
    GetJSONPollsReport(true,"",sVotingPayload,true);
    qtExecuteGenericFunction("SetGenericVotingData",sVotingPayload);
    std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
    qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
    qtSetSessionInfo(DefaultWalletAddress(), GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.Magnitude);
    globalcom->dynamicCall("ShowFoundation()");
#endif
}


void BitcoinGUI::faqClicked()
{
#ifdef WIN32
    if (!bGlobalcomInitialized) return;
    std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
    double function_call = qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
    globalcom->dynamicCall("ShowFAQ()");
#endif
}


void BitcoinGUI::newUserWizardClicked()
{
#ifdef WIN32
    if (!bGlobalcomInitialized) return;
    globalcom->dynamicCall("ShowNewUserWizard()");
#endif
}

void BitcoinGUI::miningClicked()
{

#ifdef WIN32
    if (!bGlobalcomInitialized) return;
    std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
    double function_call = qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
    globalcom->dynamicCall("ShowMiningConsole()");
#endif
}


// links to websites and services outside the gridcoin client
void BitcoinGUI::bxClicked()
{
    QDesktopServices::openUrl(QUrl("https://www.gridcoinstats.eu/block#pk_campaign=GridcoinWallet&pk_kwd=" + QString::fromStdString(FormatFullVersion())));
}


void BitcoinGUI::chatClicked()
{
    QDesktopServices::openUrl(QUrl("https://gridcoin.us/contact.htm#GridcoinWallet"));
}

void BitcoinGUI::boincClicked()
{
    QDesktopServices::openUrl(QUrl("https://boincstats.com/en/stats/-1/team/detail/118094994/overview"));
}

void BitcoinGUI::websiteClicked()
{
    QDesktopServices::openUrl(QUrl("https://www.gridcoin.us#GridcoinWallet"));
}

void BitcoinGUI::exchangeClicked()
{
    QDesktopServices::openUrl(QUrl("https://gridcoin.us/exchange.htm#GridcoinWallet"));
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid Gridcoin address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    }
    else
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid Gridcoin address or malformed URI parameters."));
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
                                     AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::backupWallet()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#endif
    QString walletfilename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!walletfilename.isEmpty()) {
        if(!BackupWallet(*pwalletMain, FromQString(walletfilename))) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
    QString configfilename = QFileDialog::getSaveFileName(this, tr("Backup Config"), saveDir, tr("Wallet Config (*.conf)"));
    if(!configfilename.isEmpty()) {
        if(!BackupConfigFile(FromQString(configfilename))) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog::Mode mode = sender() == unlockWalletAction ?
              AskPassphraseDialog::UnlockStaking : AskPassphraseDialog::Unlock;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BitcoinGUI::lockWallet()
{
    if(!walletModel)
        return;

    walletModel->setWalletLocked(true);
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}



bool Timer(std::string timer_name, int max_ms)
{
    mvTimers[timer_name] = mvTimers[timer_name] + 1;
    if (mvTimers[timer_name] > max_ms)
    {
        mvTimers[timer_name]=0;
        return true;
    }
    return false;
}


void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::updateWeight()
{
    if (!pwalletMain)
        return;

    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;

    TRY_LOCK(pwalletMain->cs_wallet, lockWallet);
    if (!lockWallet)
        return;

    pwalletMain->GetStakeWeight(nWeight);
}


std::string getMacAddress()
{
    std::string myMac = "?:?:?:?";
    foreach(QNetworkInterface netInterface, QNetworkInterface::allInterfaces())
    {
        // Return only the first non-loopback MAC Address
        if (!(netInterface.flags() & QNetworkInterface::IsLoopBack))
        {
           myMac =  netInterface.hardwareAddress().toUtf8().constData();
        }
    }
    return myMac;
}


void ReinstantiateGlobalcom()
{
#ifdef WIN32
    if (bGlobalcomInitialized)
        return;

    // Note, on Windows, if the performance counters are corrupted, rebuild them
    // by going to an elevated command prompt and issue the command: lodctr /r
    // (to rebuild the performance counters in the registry)
    printf("Instantiating globalcom for Windows %f",(double)0);
    try
    {
        globalcom = new QAxObject("BoincStake.Utilization");
        printf("Instantiated globalcom for Windows");
    }
    catch(...)
    {
        printf("Failed to instantiate globalcom.");
    }

    bGlobalcomInitialized = true;
#endif
}

void BitcoinGUI::timerfire()
{
    try
    {
        if ( (nRegVersion==0 || Timer("start",10))  &&  !bGlobalcomInitialized)
        {            
            ReinstantiateGlobalcom();
            nRegVersion=9999;

            static bool bNewUserWizardNotified = false;
            if (!bNewUserWizardNotified)
            {
                bNewUserWizardNotified=true;
                NewUserWizard();
            }
#ifdef WIN32
            if (!bGlobalcomInitialized) return;

            nRegVersion = globalcom->dynamicCall("Version()").toInt();
            sRegVer = boost::lexical_cast<std::string>(nRegVersion);
#endif
        }


        if (bGlobalcomInitialized)
        {
                //R Halford - Allow .NET to talk to Core: 6-21-2015
                #ifdef WIN32
                    std::string sData = qtExecuteDotNetStringFunction("GetDotNetMessages","");
                    if (!sData.empty())
                    {
                        std::string RPCCommand = ExtractXML(sData,"<COMMAND>","</COMMAND>");
                        std::string Argument1 = ExtractXML(sData,"<ARG1>","</ARG1>");
                        std::string Argument2 = ExtractXML(sData,"<ARG2>","</ARG2>");

                        if (RPCCommand=="vote")
                        {
                            std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
                            double function_call = qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
                            std::string response = ExecuteRPCCommand("vote",Argument1,Argument2);
                            double resultcode = qtExecuteGenericFunction("SetRPCResponse"," "+response);
                        }
                        else if (RPCCommand=="rain")
                        {
                            std::string response = ExecuteRPCCommand("rain",Argument1,Argument2);
                            double resultcode = qtExecuteGenericFunction("SetRPCResponse"," "+response);
                        }
                        else if (RPCCommand=="addpoll")
                        {
                            std::string testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
                            double function_call = qtExecuteGenericFunction("SetTestNetFlag",testnet_flag);
                            std::string Argument3 = ExtractXML(sData,"<ARG3>","</ARG3>");
                            std::string Argument4 = ExtractXML(sData,"<ARG4>","</ARG4>");
                            std::string Argument5 = ExtractXML(sData,"<ARG5>","</ARG5>");
                            std::string Argument6 = ExtractXML(sData,"<ARG6>","</ARG6>");
                            std::string response = ExecuteRPCCommand("addpoll",Argument1,Argument2,Argument3,Argument4,Argument5,Argument6);
                            double resultcode = qtExecuteGenericFunction("SetRPCResponse"," "+response);
                        }
                        else if (RPCCommand == "addattachment")
                        {
                            msAttachmentGuid = Argument1;
                            printf("\r\n attachment added %s \r\n",msAttachmentGuid.c_str());
                        }

                    }
                #endif
        }


        if (Timer("status_update",5))
        {
            GetGlobalStatus();
            bForceUpdate=true;
        }

        if (bForceUpdate)
        {
                bForceUpdate=false;
                overviewPage->updateglobalstatus();
                setNumConnections(clientModel->getNumConnections());
        }

    }
    catch(std::runtime_error &e)
    {
            printf("GENERAL RUNTIME ERROR!");
    }


}

double GetPOREstimatedTime(double RSAWeight)
{
    if (RSAWeight == 0) return 0;
    //RSA Weight ranges from 0 - 5600
    double orf = 5600-RSAWeight;
    if (orf < 1) orf = 1;
    double eta = orf/5600;
    if (eta > 1) orf = 1;
    eta = eta * (60*60*24);
    return eta;
}

QString BitcoinGUI::GetEstimatedTime(unsigned int nEstimateTime)
{
    QString text;
    if (nEstimateTime < 60)
    {
            text = tr("%n second(s)", "", nEstimateTime);
    }
        else if (nEstimateTime < 60*60)
    {
            text = tr("%n minute(s)", "", nEstimateTime/60);
    }
        else if (nEstimateTime < 24*60*60)
    {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
    }
        else
    {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
    }
    return text;
}



void BitcoinGUI::updateStakingIcon()
{
    uint64_t nWeight, nLastInterval;
    std::string ReasonNotStaking;
    { LOCK(MinerStatus.lock);
        // not using real weigh to not break calculation
        nWeight = MinerStatus.ValueSum;
        nLastInterval = MinerStatus.nLastCoinStakeSearchInterval;
        ReasonNotStaking = MinerStatus.ReasonNotStaking;
    }

    uint64_t nNetworkWeight = GetPoSKernelPS();
    bool staking = nLastInterval && nWeight;
    uint64_t nEstimateTime = staking ? (GetTargetSpacing(nBestHeight) * nNetworkWeight / nWeight) : 0;

    if (staking)
    {
        if (fDebug10) printf("StakeIcon Vitals BH %f, NetWeight %f, Weight %f \r\n", (double)GetTargetSpacing(nBestHeight),(double)nNetworkWeight,(double)nWeight);
        QString text = GetEstimatedTime(nEstimateTime);
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br><b>Estimated</b> time to earn reward is %3.")
                                     .arg(nWeight).arg(nNetworkWeight).arg(text));

    }
    else
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        //Part of this string wont be translated :(
        labelStakingIcon->setToolTip(tr("Not staking; %1").arg(QString(ReasonNotStaking.c_str())));
    }
}
