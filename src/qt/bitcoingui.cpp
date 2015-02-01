/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */


#include <QApplication>
#include <QProcess>

#if defined(WIN32) && defined(QT_GUI)
#include <QAxObject>
#include <ActiveQt/qaxbase.h>
#include <ActiveQt/qaxobject.h>
#endif

#include <QInputDialog>
#include <fstream>

#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "upgradedialog.h"
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
#include "upgrader.h"

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
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>

#include <boost/lexical_cast.hpp>

#include "bitcoinrpc.h"

#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()

extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
double GetPoSKernelPS();
int ReindexWallet();
extern int RebootClient();
extern QString ToQstring(std::string s);
extern int qtTrackConfirm(std::string txid);
extern std::string qtGRCCodeExecutionSubsystem(std::string sCommand);
extern void qtUpdateConfirm(std::string txid);
extern void qtInsertConfirm(double dAmt, std::string sFrom, std::string sTo, std::string txid);
extern void qtSetSessionInfo(std::string defaultgrcaddress, std::string cpid, double magnitude);


void TallyInBackground();

double cdbl(std::string s, int place);
std::string getfilecontents(std::string filename);

double GetUntrustedMagnitude(std::string cpid, double& out_owed);


std::string BackupGridcoinWallet();
int nTick = 0;
int nTickRestart = 0;
int nBlockCount = 0;
int nTick2 = 0;
int nRegVersion;
int nNeedsUpgrade = 0;
double GetPoBDifficulty();

std::string GetBoincDataDir2();
extern int CreateRestorePoint();
extern int DownloadBlocks();

void StopGridcoin3();
bool OutOfSyncByAge();
void ThreadCPIDs();
int Races(int iMax1000);
std::string GetGlobalStatus();
std::string GetHttpPage(std::string cpid);
bool TallyNetworkAverages(bool ColdBoot);
void LoadCPIDsInBackground();
void InitializeCPIDs();
void RestartGridcoinMiner();
extern int UpgradeClient();
extern void CheckForUpgrade();
extern int CloseGuiMiner();
extern int AddressUser();

bool IsConfigFileEmpty();

extern void ExecuteCode();


extern void startWireFrameRenderer();
extern void stopWireFrameRenderer();



std::string RetrieveMd5(std::string s1);
void WriteAppCache(std::string key, std::string value);
void RestartGridcoin10();

void HarvestCPIDs(bool cleardata);
extern int RestartClient();

extern int ReindexWallet();
#ifdef WIN32
QAxObject *globalcom = NULL;
QAxObject *globalwire = NULL;
#endif
int ThreadSafeVersion();
void FlushGridcoinBlockFile(bool fFinalize);
extern int ReindexBlocks();
bool OutOfSync();




BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    unlockWalletAction(0),
    lockWalletAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0),
    upgrader(0),
    nWeight(0)
{
    setFixedSize(980, 550);
    setWindowTitle(tr("Gridcoin") + " " + tr("Wallet"));
    qApp->setStyleSheet("QMainWindow { background-image:url(:images/bkg);border:none;font-family:'Open Sans,sans-serif'; } #frame { } QToolBar QLabel { padding-top:15px;padding-bottom:10px;margin:0px; } #spacer { background:rgb(69,65,63);border:none; } #toolbar3 { border:none;width:1px; background-color: rgb(169,192,7); } #toolbar2 { border:none;width:10px; background-color:qlineargradient(x1: 0, y1: 0, x2: 0.5, y2: 0.5,stop: 0 rgb(210,220,7), stop: 1 rgb(98,116,3)); } #toolbar { border:none;height:100%;padding-top:20px; background: rgb(69,65,63); text-align: left; color: rgb(169,192,7); min-width:160px; max-width:160px;} QToolBar QToolButton:hover {background-color:qlineargradient(x1: 0, y1: 0, x2: 2, y2: 2,stop: 0 rgb(69,65,63), stop: 1 rgb(216,252,251),stop: 2 rgb(59,62,65));} QToolBar QToolButton { font-family:Century Gothic;padding-left:20px;padding-right:200px;padding-top:10px;padding-bottom:10px; width:100%; color: rgb(169,192,7); text-align: left; background-color: rgb(69,65,63) } #labelMiningIcon { padding-left:5px;font-family:Century Gothic;width:100%;font-size:10px;text-align:center;color: rgb(169,192,7); } QMenu { background: rgb(69,65,63); color: rgb(169,192,7); padding-bottom:10px; } QMenu::item { color: rgb(169,192,7); background-color: transparent; } QMenu::item:selected { background-color:qlineargradient(x1: 0, y1: 0, x2: 0.5, y2: 0.5,stop: 0 rgb(69,65,63), stop: 1 rgb(98,116,3)); } QMenuBar { background: rgb(69,65,63); color: rgb(169,192,7); } QMenuBar::item { font-size:12px;padding-bottom:8px;padding-top:8px;padding-left:15px;padding-right:15px;color: rgb(169,192,7); background-color: transparent; } QMenuBar::item:selected { background-color:qlineargradient(x1: 0, y1: 0, x2: 0.5, y2: 0.5,stop: 0 rgb(69,65,63), stop: 1 rgb(98,116,3)); }");

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

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    setCentralWidget(centralWidget);

    // Create status bar
    // statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setStyleSheet("frameBlocks { background: rgb(127,154,131); }");
    frameBlocks->setContentsMargins(0,0,0,0);

    frameBlocks->setMinimumWidth(30);
    frameBlocks->setMaximumWidth(30);
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
    frameBlocksLayout->addStretch();

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);
    progressBar->setOrientation(Qt::Vertical);
    progressBar->setObjectName("progress");
    progressBar->setStyleSheet("QProgressBar{"
                               "border: 1px solid transparent;"
							   "font-size:9px;"
                               "text-align: center;"
                               "color:rgba(0,0,0,100);"
                               "border-radius: 5px;"
                               "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(182, 182, 182, 100), stop:1 rgba(209, 209, 209, 100));"
                                   "}"
                               "QProgressBar::chunk{"
                               "background-color: rgba(0,255,0,100);"
                               "}");
    frameBlocks->setObjectName("frame");
	addToolBarBreak(Qt::LeftToolBarArea);
    QToolBar *toolbar2 = addToolBar(tr("Tabs toolbar"));
    addToolBar(Qt::LeftToolBarArea,toolbar2);
    toolbar2->setOrientation(Qt::Vertical);
    toolbar2->setMovable( false );
    toolbar2->setObjectName("toolbar2");
    toolbar2->setFixedWidth(25);
    toolbar2->addWidget(frameBlocks);
    toolbar2->addWidget(progressBarLabel);
    toolbar2->addWidget(progressBar);

	addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *toolbar3 = addToolBar(tr("Green bar"));
    addToolBar(Qt::TopToolBarArea,toolbar3);
    toolbar3->setOrientation(Qt::Horizontal);
    toolbar3->setMovable( false );
    toolbar3->setObjectName("toolbar3");
    toolbar3->setFixedHeight(2);

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

    connect(downloadAction, SIGNAL(triggered()), upgrader, SLOT(show()));
    connect(downloadAction, SIGNAL(triggered()), upgrader, SLOT(blocks()));

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
			printf("executing grcrestarter reindex");

			QString sFilename = "GRCRestarter.exe";
			QString sArgument = "";
			QString path = QCoreApplication::applicationDirPath() + "\\" + sFilename;
			QProcess p;
			if (!fTestNet)
			{
#ifdef WIN32
				if (!globalcom)
				{
					globalcom = new QAxObject("BoincStake.Utilization");
				}

				globalcom->dynamicCall("ReindexWallet()");
#endif
			}
			else
			{
#ifdef WIN32
				globalcom->dynamicCall("ReindexWalletTestNet()");
#endif
			}
			StartShutdown();
			return 1;
}



int CreateRestorePoint()
{
			printf("executing grcrestarter createrestorepoint");

			QString sFilename = "GRCRestarter.exe";
			QString sArgument = "";
			QString path = QCoreApplication::applicationDirPath() + "\\" + sFilename;
			QProcess p;


			if (!fTestNet)
			{
#ifdef WIN32
				globalcom->dynamicCall("CreateRestorePoint()");
#endif
			}
			else
			{
#ifdef WIN32
				globalcom->dynamicCall("CreateRestorePointTestNet()");
#endif
			}
			//RestartGridcoin

			return 1;
}



int DownloadBlocks()
{
			printf("executing grcrestarter downloadblocks");

			QString sFilename = "GRCRestarter.exe";
			QString sArgument = "";
			QString path = QCoreApplication::applicationDirPath() + "\\" + sFilename;
			QProcess p;

			#ifdef WIN32
				if (!globalcom)
				{
					globalcom = new QAxObject("BoincStake.Utilization");
				}

				globalcom->dynamicCall("DownloadBlocks()");
				StartShutdown();
			#endif

			return 1;
}




QString ToQstring(std::string s)
{
	QString str1 = QString::fromUtf8(s.c_str());
	return str1;
}



int RestartClient()
{

	printf("executing grcrestarter restart");

	QString sFilename = "GRCRestarter.exe";
			QString sArgument = "";
			QString path = QCoreApplication::applicationDirPath() + "\\" + sFilename;
			QProcess p;
	#ifdef WIN32
			globalcom->dynamicCall("RestartWallet()");
#endif
			StartShutdown();
			return 1;
}

void qtUpdateConfirm(std::string txid)
{
	int result = 0;
		
	#if defined(WIN32) && defined(QT_GUI)
		QString qsTxid = ToQstring(txid);
		QString sResult = globalcom->dynamicCall("UpdateConfirm(Qstring)",qsTxid).toString();
	#endif
		
}


void qtInsertConfirm(double dAmt, std::string sFrom, std::string sTo, std::string txid)
{

	#if defined(WIN32) && defined(QT_GUI)
	try
	{  
		int result = 0;
	 	std::string Confirm = RoundToString(dAmt,4) + "<COL>" + sFrom + "<COL>" + sTo + "<COL>" + txid;
		printf("Inserting confirm %s",Confirm.c_str());
		QString qsConfirm = ToQstring(Confirm);
		result = globalcom->dynamicCall("InsertConfirm(Qstring)",qsConfirm).toInt();
	}
	catch(...)
	{

	}
	#endif
}


void qtSetSessionInfo(std::string defaultgrcaddress, std::string cpid, double magnitude)
{

	#if defined(WIN32) && defined(QT_GUI)
	try
	{  
		int result = 0;
	 	std::string session = defaultgrcaddress + "<COL>" + cpid + "<COL>" + RoundToString(magnitude,1);
		printf("Setting Session Id %s",session.c_str());
		QString qsSession = ToQstring(session);
		result = globalcom->dynamicCall("SetSessionInfo(Qstring)",qsSession).toInt();
	}
	catch(...)
	{

	}
	#endif
}
    


std::string FromQString(QString qs)
{
	std::string sOut = qs.toUtf8().constData();
	return sOut;
}



int qtTrackConfirm(std::string txid)
{
	double result = 0;
	#if defined(WIN32) && defined(QT_GUI)
		QString qsConfirm = ToQstring(txid);
		printf("@t1");
		QString qsResult = globalcom->dynamicCall("TrackConfirm(Qstring)",qsConfirm).toString();
		std::string sResult = FromQString(qsResult);
		result = cdbl(sResult,0);
		printf("@t2 returned %f",(double)result);
	#endif
	return (int)result;
}


std::string qtGRCCodeExecutionSubsystem(std::string sCommand) 
{
	std::string sResult = "FAIL";
	#if defined(WIN32) && defined(QT_GUI)
		QString qsParms = ToQstring(sCommand);
		QString qsResult = globalcom->dynamicCall("GRCCodeExecutionSubsystem(Qstring)",qsParms).toString();
		sResult = FromQString(qsResult);
		printf("@qtGRCCodeExecutionSubsystem returned %s",sResult.c_str());
	#endif
	return sResult;

}
        


int RebootClient()
{
			printf("Executing reboot\r\n");

			QString sFilename = "GRCRestarter.exe";
			QString sArgument = "reboot";
			QString path = QCoreApplication::applicationDirPath() + "\\" + sFilename;
			QProcess p;
			if (!fTestNet)
			{
#ifdef WIN32
				globalcom->dynamicCall("RebootClient()");
#endif
			}
			else
			{
#ifdef WIN32
				globalcom->dynamicCall("RebootClient()");
#endif
			}

			StartShutdown();
			return 1;
}


void CheckForUpgrade()
{
	try
	{
				int nNeedsUpgrade = 0;
				bCheckedForUpgrade = true;
				#ifdef WIN32
				nNeedsUpgrade = globalcom->dynamicCall("ClientNeedsUpgrade()").toInt();
				#endif
				printf("Needs upgraded %f\r\n",(double)nNeedsUpgrade);
				if (nNeedsUpgrade) UpgradeClient();
	}
	catch(...)
	{

	}
}


int UpgradeClient()
{
			printf("Executing upgrade");

			QString sFilename = "GRCRestarter.exe";
			QString sArgument = "upgrade";
			QString path = QCoreApplication::applicationDirPath() + "\\" + sFilename;
			QProcess p;
			if (!fTestNet)
			{
#ifdef WIN32
				globalcom->dynamicCall("UpgradeWallet()");
#else
#endif
			}
			else
			{
#ifdef WIN32
				globalcom->dynamicCall("UpgradeWalletTestnet()");
#endif
			}

			StartShutdown();
			return 1;
}

QString IntToQstring(int o)
{
	std::string pre="";
	pre=strprintf("%d",o);
	QString str1 = QString::fromUtf8(pre.c_str());
	return str1;
}




int AddressUser()
{
		int result = 0;
		#if defined(WIN32) && defined(QT_GUI)
		double out_magnitude = 0;
		double out_owed = 0;
		try
		{
		out_magnitude = GetUntrustedMagnitude(GlobalCPUMiningCPID.cpid,out_owed);
	    if (fDebug) printf("Boinc Magnitude %f \r\n",out_magnitude);
		result = globalcom->dynamicCall("AddressUser(Qstring)",IntToQstring((int)out_magnitude)).toInt();
		}
		catch(...)
		{

		}
		#endif
		return result;
}

int CloseGuiMiner()
{
	try
	{
#ifdef WIN32
			globalcom->dynamicCall("CloseGUIMiner()");
#endif
	}
	catch(...) { return 0; }

	return 1;
}







void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a GridCoin address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);

	bxAction = new QAction(QIcon(":/icons/block"), tr("&Block Explorer"), this);
	bxAction->setStatusTip(tr("Block Explorer"));
	bxAction->setMenuRole(QAction::TextHeuristicRole);

	exchangeAction = new QAction(QIcon(":/icons/ex"), tr("&Exchange"), this);
	exchangeAction->setStatusTip(tr("Web Site"));
	exchangeAction->setMenuRole(QAction::TextHeuristicRole);

	boincAction = new QAction(QIcon(":/icons/boinc"), tr("&Boinc Stats"), this);
	boincAction->setStatusTip(tr("Boinc Stats"));
	boincAction->setMenuRole(QAction::TextHeuristicRole);

	websiteAction = new QAction(QIcon(":/icons/www"), tr("&Web Site"), this);
	websiteAction->setStatusTip(tr("Web Site"));
	websiteAction->setMenuRole(QAction::TextHeuristicRole);

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

	connect(websiteAction, SIGNAL(triggered()), this, SLOT(websiteClicked()));
	connect(bxAction, SIGNAL(triggered()), this, SLOT(bxClicked()));
	connect(exchangeAction, SIGNAL(triggered()), this, SLOT(exchangeClicked()));
	connect(boincAction, SIGNAL(triggered()), this, SLOT(boincClicked()));

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


	rebootAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Reboot Client"), this);
	rebootAction->setStatusTip(tr("Reboot Gridcoin"));
	rebootAction->setMenuRole(QAction::TextHeuristicRole);



	upgradeAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Upgrade Client"), this);
	upgradeAction->setStatusTip(tr("Upgrade Client"));
	upgradeAction->setMenuRole(QAction::TextHeuristicRole);


    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About GridCoin"), this);
    aboutAction->setToolTip(tr("Show information about GridCoin"));
    aboutAction->setMenuRole(QAction::AboutRole);



//	miningAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Mining Console"), this);
//	miningAction->setStatusTip(tr("Go to the mining console"));
//	miningAction->setMenuRole(QAction::TextHeuristicRole);


//	emailAction = new QAction(QIcon(":/icons/bitcoin"), tr("&E-Mail Center"), this);
//	emailAction->setStatusTip(tr("Go to the E-Mail center"));
//	emailAction->setMenuRole(QAction::TextHeuristicRole);

	sqlAction = new QAction(QIcon(":/icons/bitcoin"), tr("&SQL Query Analyzer"), this);
	sqlAction->setStatusTip(tr("SQL Query Analyzer"));
	sqlAction->setMenuRole(QAction::TextHeuristicRole);

	tickerAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Live Ticker"), this);
	tickerAction->setStatusTip(tr("Live Ticker"));
	tickerAction->setMenuRole(QAction::TextHeuristicRole);

	ticketListAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Tickets"), this);
	ticketListAction->setStatusTip(tr("Tickets"));
	ticketListAction->setMenuRole(QAction::TextHeuristicRole);


//	leaderboardAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Leaderboard"), this);
//	leaderboardAction->setStatusTip(tr("Leaderboard"));
//	leaderboardAction->setMenuRole(QAction::TextHeuristicRole);


	//aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
    //aboutQtAction->setToolTip(tr("Show information about Qt"));
    //aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for GridCoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
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
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
	//	connect(miningAction, SIGNAL(triggered()), this, SLOT(miningClicked()));
	//	connect(emailAction, SIGNAL(triggered()), this, SLOT(emailClicked()));
	connect(rebuildAction, SIGNAL(triggered()), this, SLOT(rebuildClicked()));
	connect(upgradeAction, SIGNAL(triggered()), this, SLOT(upgradeClicked()));
	connect(downloadAction, SIGNAL(triggered()), this, SLOT(downloadClicked()));
	connect(rebootAction, SIGNAL(triggered()), this, SLOT(rebootClicked()));
	connect(sqlAction, SIGNAL(triggered()), this, SLOT(sqlClicked()));
	connect(tickerAction, SIGNAL(triggered()), this, SLOT(tickerClicked()));

	connect(ticketListAction, SIGNAL(triggered()), this, SLOT(ticketListClicked()));

	//connect(leaderboardAction, SIGNAL(triggered()), this, SLOT(leaderboardClicked()));

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
//    settings->addAction(unlockWalletAction);
//    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);

//	QMenu *mining = appMenuBar->addMenu(tr("&Mining"));
//    mining->addSeparator();
//    mining->addAction(miningAction);

//	QMenu *email = appMenuBar->addMenu(tr("&E-Mail"));
//    email->addSeparator();
//    email->addAction(emailAction);

	QMenu *upgrade = appMenuBar->addMenu(tr("&Upgrade QT Client"));
	upgrade->addSeparator();
	upgrade->addAction(upgradeAction);


	QMenu *rebuild = appMenuBar->addMenu(tr("&Rebuild Block Chain"));
	rebuild->addSeparator();
	rebuild->addAction(rebuildAction);
	rebuild->addSeparator();
	rebuild->addAction(downloadAction);
	rebuild->addSeparator();
	rebuild->addAction(rebootAction);
	rebuild->addSeparator();

	QMenu *qmAdvanced = appMenuBar->addMenu(tr("&Advanced"));
	qmAdvanced->addSeparator();
	qmAdvanced->addAction(sqlAction);
	qmAdvanced->addAction(tickerAction);

	qmAdvanced->addAction(ticketListAction);


//	QMenu *leaderboard = appMenuBar->addMenu(tr("&Leaderboard"));
//	leaderboard->addSeparator();
//	leaderboard->addAction(leaderboardAction);



}

void BitcoinGUI::createToolBars()
{
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setObjectName("toolbar");
    addToolBar(Qt::LeftToolBarArea,toolbar);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setMovable( false );
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(50,25));
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
	toolbar->addAction(bxAction);
	toolbar->addAction(websiteAction);
	toolbar->addAction(exchangeAction);
	toolbar->addAction(boincAction);
//	toolbar->addAction(statisticsAction);
//	toolbar->addAction(blockAction);
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar->addWidget(spacer);
    spacer->setObjectName("spacer");
	toolbar->addAction(unlockWalletAction);
	toolbar->addAction(lockWalletAction);
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
                trayIcon->setToolTip(tr("GridCoin client") + QString(" ") + tr("[testnet]"));
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
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("GridCoin client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
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

    notificator = new Notificator(qApp->applicationName(), trayIcon);
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
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to GridCoin network", "", count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        if (strStatusBarWarnings.isEmpty())
            progressBarLabel->setVisible(false);

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

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
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
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
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
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
	ofstream myConfig;
	myConfig.open (path.string().c_str());
	std::string row = "cpumining=true\r\n";
	myConfig << row;
	row = "email=" + boinc_email + "\r\n";
	myConfig << row;
	row = "addnode=node.gridcoin.us \r\n";
	myConfig << row;
	row = "addnode=gridcoin.asia \r\n";
	myConfig << row;
	myConfig.close();
	return true;
}

void BitcoinGUI::NewUserWizard()
{
	if (!IsConfigFileEmpty()) return;
	    QString boincemail = "";
		//Typhoon- Check to see if boinc exists in default path - 11-19-2014

		std::string sourcefile = GetBoincDataDir2() + "client_state.xml";
		std::string sout = "";
		sout = getfilecontents(sourcefile);
		//bool BoincInstalled = true;
		std::string sBoincNarr = "";
		if (sout == "-1") 
		{
			printf("Boinc not installed in default location! \r\n");
			//BoincInstalled=false;
			std::string nicePath = GetBoincDataDir2();
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
		    QString strMessage = tr("To get started with Boinc, run the boinc client, choose projects, then populate the gridcoinresearch.conf file in %appdata%\\GridcoinResearch with your boinc e-mail address.  To run this wizard again, please delete the gridcoinresearch.conf file. ");
			QMessageBox::warning(this, tr("New User Wizard - Skipped"), strMessage);
		}
		
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






std::string Trim(int i)
{
	std::string s = "";
	s=boost::lexical_cast<std::string>(i);
	return s;
}


std::string TrimD(double i)
{
	std::string s = "";
	s=boost::lexical_cast<std::string>(i);
	return s;
}









bool IsInvalidChar(char c)
{
	int asc = (int)c;

	if (asc >= 0 && asc < 32) return true;
	if (asc > 128) return true;
	if (asc == 124) return true;
	return false;
}

std::string Clean(std::string s)
{
	char ch;
	std::string sOut = "";
	for (unsigned int i=0; i < s.length(); i++)
	{
		ch = s.at(i);
		if (IsInvalidChar(ch)==false) sOut = sOut + ch;

	}
	return sOut;
}

std::string RetrieveBlockAsString(int lSqlBlock)
{

	//Insert into Blocks (hash,confirmations,size,height,version,merkleroot,tx,time,nonce,bits,difficulty,boinchash,previousblockhash,nextblockhash) VALUES ();

	try {

		if (lSqlBlock==0) lSqlBlock=1;
		if (lSqlBlock > nBestHeight-2) return "";
		CBlock block;
		CBlockIndex* blockindex = MainFindBlockByHeight(lSqlBlock);
		block.ReadFromDisk(blockindex);

		std::string s = "";
		std::string d = "|";

		s = block.GetHash().GetHex() + d + "C" + d 	+ "1000" + d + Trim(blockindex->nHeight) + d;
		s = s + Trim(block.nVersion) + d + block.hashMerkleRoot.GetHex() + d + "TX" + d + Trim(block.GetBlockTime()) + d + Trim(block.nNonce) + d + Trim(block.nBits) + d;
		s = s + TrimD(GetDifficulty(blockindex)) + d	+		Clean(block.vtx[0].hashBoinc) + d 		+ blockindex->pprev->GetBlockHash().GetHex() + d;
		s = s + blockindex->pnext->GetBlockHash().GetHex();
		return s;
	}
	catch(...)
	{
		printf("Runtime error in RetrieveBlockAsString");
		return "";

	}

}



std::string RetrieveBlocksAsString(int lSqlBlock)
{
	std::string sout = "";
	if (lSqlBlock > nBestHeight-5) return "";

	for (int i = lSqlBlock; i < lSqlBlock+101; i++) {
		sout = sout + RetrieveBlockAsString(i) + "{ROW}";
	}
	return sout;
}



void BitcoinGUI::rebuildClicked()
{
	printf("Rebuilding...");
	ReindexBlocks();
}


void BitcoinGUI::upgradeClicked()
{
	printf("Upgrading Gridcoin...");
#ifdef WIN32
	// UpgradeClient();
#else

#endif
}

void BitcoinGUI::downloadClicked()
{
	DownloadBlocks();

}

void BitcoinGUI::rebootClicked()
{

	RebootClient();
}


void BitcoinGUI::sqlClicked()
{
#ifdef WIN32
	if (!globalcom)
	{
		globalcom = new QAxObject("BoincStake.Utilization");
	}
    globalcom->dynamicCall("ShowSql()");
#endif

}


void BitcoinGUI::ticketListClicked()
{
#ifdef WIN32
	if (!globalcom)
	{
		globalcom = new QAxObject("BoincStake.Utilization");
	}

			
	qtSetSessionInfo(DefaultWalletAddress(), GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.Magnitude);


    globalcom->dynamicCall("ShowTicketList()");
#endif

}


void BitcoinGUI::tickerClicked()
{
#ifdef WIN32
	if (!globalcom)
	{
		globalcom = new QAxObject("BoincStake.Utilization");
	}
    globalcom->dynamicCall("ShowTicker()");
#endif

}


/*
void BitcoinGUI::leaderboardClicked()
{
	#ifdef WIN32

	if (globalcom==NULL) {
		globalcom = new QAxObject("BoincStake.Utilization");
	}

    globalcom->dynamicCall("ShowLeaderboard()");
#endif
}
*/



int ReindexBlocks()
{

	ReindexWallet();
	return 1;

}


/*

void BitcoinGUI::miningClicked()
{

#ifdef WIN32

	if (globalcom==NULL) {
		globalcom = new QAxObject("BoincStake.Utilization");
	}

      globalcom->dynamicCall("ShowMiningConsole()");
#endif
}
*/


void BitcoinGUI::bxClicked()
{
	overviewPage->on_btnBX_pressed();
}

void BitcoinGUI::boincClicked()
{
	overviewPage->on_btnBoinc_pressed();
}
void BitcoinGUI::websiteClicked()
{
	overviewPage->on_btnWebsite_pressed();

}
void BitcoinGUI::exchangeClicked()
{
		overviewPage->on_btnExchange_pressed();

}



void startWireFrameRenderer()
{

#ifdef WIN32
	if (globalwire==NULL)
	{
		globalwire = new QAxObject("BoincStake.Utilization");
	}

      globalwire->dynamicCall("StartWireFrameRenderer()");
#endif
}

void stopWireFrameRenderer()
{

#ifdef WIN32
	if (globalwire==NULL)
	{
		globalwire = new QAxObject("BoincStake.Utilization");
	}

    globalwire->dynamicCall("StopWireFrameRenderer()");
#endif
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
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid GridCoin address or malformed URI parameters."));
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
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid GridCoin address or malformed URI parameters."));
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
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
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



void ReinstantiateGlobalcom()
{
#ifdef WIN32

			//Note, on Windows, if the performance counters are corrupted, rebuild them by going to an elevated command prompt and
	   		//issue the command: lodctr /r (to rebuild the performance counters in the registry)
			std::string os = GetArg("-os", "windows");
			if (os == "linux" || os == "mac")
			{
				printf("Instantiating globalcom for Linux");
				globalcom = new QAxObject("Boinc.LinuxUtilization");
			}
			else
			{
					printf("Instantiating globalcom for Windows");
					try
					{
						globalcom = new QAxObject("BoincStake.Utilization");
					}
					catch(...)
					{
						printf("Failed to instantiate globalcom.");
					}
					printf("Instantiated globalcom for Windows");

			}

			bGlobalcomInitialized = true;
		    if (bCheckedForUpgrade == false && !fTestNet && bProjectsInitialized)
			{
						int nNeedsUpgrade = 0;
						bCheckedForUpgrade = true;
						printf("Checking to see if Gridcoin needs upgraded\r\n");
						nNeedsUpgrade = globalcom->dynamicCall("ClientNeedsUpgrade()").toInt();
						if (nNeedsUpgrade) UpgradeClient();

						if (!bAddressUser)
						{
									bAddressUser = true;
									#if defined(WIN32) && defined(QT_GUI)
									int result = 0;
									result = AddressUser();
									#endif
						}
					

			}
#endif
}



void ExecuteCode()
{
	printf("Globalcom executing .net code\r\n");

	std::string q = "\"";
	std::string sCode = "For x = 1 to 5:sOut=sOut + " + q + "COUNTING: " + q + "+ trim(x):Next x:MsgBox(" + q + "Hello: "
		+ q + " + sOut,MsgBoxStyle.Critical," + q + "Message Title" + q + ")";

    QString qsCode = QString::fromUtf8(sCode.c_str());
	#ifdef WIN32
		globalcom->dynamicCall("ExecuteCode(Qstring)", qsCode);
	#endif

}



void BitcoinGUI::timerfire()
{


	try {


		
		if (nRegVersion==0 || Timer("start",10))
		{
			if (fDebug) printf("Starting globalcom...\r\n");
			nRegVersion=9999;
			if (!bNewUserWizardNotified)
			{
				bNewUserWizardNotified=true;
				NewUserWizard();
			}
			#ifdef WIN32
			if (globalcom==NULL) ReinstantiateGlobalcom();
			nRegVersion = globalcom->dynamicCall("Version()").toInt();
			sRegVer = boost::lexical_cast<std::string>(nRegVersion);
			#endif
		}


		
		if (Timer("status_update",1))
		{
			std::string status = GetGlobalStatus();
			bForceUpdate=true;
		}

		if (bForceUpdate)
		{
				bForceUpdate=false;
				overviewPage->updateglobalstatus();
				setNumConnections(clientModel->getNumConnections());
       
		}
 


		// RETIRING ALL OF THIS:

		if (false)
		{

				//		std::string time1 =  DateTimeStrFormat("%m-%d-%Y %H:%M:%S", tTime());
				//Backup the wallet once per day:
				if (Timer("backupwallet", 6*60*10))
				{
					std::string backup_results = BackupGridcoinWallet();
					printf("Daily backup results: %s\r\n",backup_results.c_str());
					//Create a restore point once per day
					//int r = CreateRestorePoint();
					//printf("Created restore point : %i",r);
				}

		


							if (false)
							{
							if (mapArgs["-restartnetlayer"] != "false")
							{
								if (Timer("restart_network",6*25))
								{
									//printf("\r\nRestarting gridcoin's network layer @ %s\r\n",time1.c_str());
									//RestartGridcoin10();
								}
							}
							}


								if (mapArgs["-resync"] != "")
								{
									double resync = cdbl(mapArgs["-resync"],0);
									if (Timer("resync", resync*6))
									{   //TODO: Test if this is even necessary...

										//RestartGridcoin10();
										//LoadBlockIndex(true);
									}
								}




					

									if (false)
									{
											if (Timer("sql",2))
											{

												#ifdef WIN32
												//Upload the current block to the GVM
												//printf("Ready to sync SQL...\r\n");
     											//QString lbh = QString::fromUtf8(hashBestChain.ToString().c_str());
	  											//Retrieve SQL high block number:
												int iSqlBlock = 0;
												iSqlBlock = globalcom->dynamicCall("RetrieveSqlHighBlock()").toInt();
     											//Send Gridcoin block to SQL:
												QString qsblock = QString::fromUtf8(RetrieveBlocksAsString(iSqlBlock).c_str());
												globalcom->dynamicCall("SetSqlBlock(Qstring)",qsblock);
	    										//Set Public Wallet Address
     											//globalcom->dynamicCall("SetPublicWalletAddress(QString)",pwa);
	    										//Set Best Block
	    										globalcom->dynamicCall("SetBestBlock(int)", nBestHeight);
												#endif
											}
									}
				}
		}
		catch(std::runtime_error &e)
		{
			printf("GENERAL RUNTIME ERROR!");
		}


}



QString BitcoinGUI::toqstring(int o)
{
	std::string pre="";
	pre=strprintf("%d",o);
	QString str1 = QString::fromUtf8(pre.c_str());
	return str1;
}

void BitcoinGUI::detectShutdown()
{
	//Note: This routine is used in Litecoin but not Peercoin
	/*
	 // Tell the main threads to shutdown.
     if (ShutdownRequested())
        QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
		*/
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
    updateWeight();

    if (nLastCoinStakeSearchInterval && nWeight)
    {
        uint64_t nNetworkWeight = GetPoSKernelPS();
        unsigned nEstimateTime = GetTargetSpacing(nBestHeight) * (nNetworkWeight / ((nWeight/COIN)+.001)) * 1;
		if (fDebug) printf("StakeIcon Vitals BH %f, NetWeight %f, Weight %f \r\n", (double)GetTargetSpacing(nBestHeight),(double)nNetworkWeight,(double)nWeight);
        QString text = GetEstimatedTime(nEstimateTime);
        //Halford - 1-9-2015 - Calculate time for POR Block:
		unsigned int nPOREstimate = (unsigned int)GetPOREstimatedTime(GlobalCPUMiningCPID.RSAWeight);
		QString PORText = "Estimated time to earn POR Reward: " + GetEstimatedTime(nPOREstimate);
		if (nPOREstimate == 0) PORText="";
        if (IsProtocolV2(nBestHeight+1))
        {
            nWeight /= COIN;
        }
	    labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br><b>Estimated</b> time to earn reward is %3. %4").arg(nWeight).arg(nNetworkWeight).arg(text).arg(PORText));
		msMiningErrors6 = "Interest: " + FromQString(text);
		if (nPOREstimate > 0) msMiningErrors6 += "; POR: " + FromQString(GetEstimatedTime(nPOREstimate));
    }
    else
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        if (pwalletMain && pwalletMain->IsLocked())
		{
            labelStakingIcon->setToolTip(tr("Not staking because wallet is locked"));
			msMiningErrors6="Wallet Locked";
		}
        else if (vNodes.empty())
		{
            labelStakingIcon->setToolTip(tr("Not staking because wallet is offline"));
			msMiningErrors6 = "Offline";
		}
        else if (IsInitialBlockDownload())
		{
            labelStakingIcon->setToolTip(tr("Not staking because wallet is syncing"));
			msMiningErrors6 = "Syncing";
		}
		else if (!nLastCoinStakeSearchInterval && !nWeight)
		{
			labelStakingIcon->setToolTip(tr("Not staking because you don't have mature coins and stake weight is too low."));
			msMiningErrors6 = "No Mature Coins; Stakeweight too low";
		}
        else if (!nWeight)
		{
            labelStakingIcon->setToolTip(tr("Not staking because you don't have mature coins"));
			msMiningErrors6 = "No mature coins";
		}
		else if (!nLastCoinStakeSearchInterval)
		{
			labelStakingIcon->setToolTip(tr("Searching for mature coins... Please wait"));
			msMiningErrors6 = "Searching for coins";
		}
        else
		{
            labelStakingIcon->setToolTip(tr("Not staking"));
			msMiningErrors6 = "Not staking yet";
		}
    }
}
