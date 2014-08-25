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



#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
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

extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
double GetPoSKernelPS();


std::string BackupGridcoinWallet();
int nTick = 0;
int nTickRestart = 0;
int nBlockCount = 0;
int nTick2 = 0;
int nRegVersion;
int nNeedsUpgrade = 0;
double GetPoBDifficulty();
extern int CreateRestorePoint();
extern int DownloadBlocks();
void StopGridcoin3();
bool OutOfSyncByAge();
bool bCheckedForUpgrade = false;
void ThreadCPIDs();
int Races(int iMax1000);
std::string GetGlobalStatus();
std::string GetHttpPage(std::string cpid);
bool TallyNetworkAverages(bool ColdBoot);
void LoadCPIDsInBackground();
void InitializeCPIDs();
void RestartGridcoinMiner();
extern int UpgradeClient();
extern int CloseGuiMiner();

extern void ExecuteCode();



std::string RetrieveMd5(std::string s1);
void WriteAppCache(std::string key, std::string value);
void RestartGridcoin10();

void HarvestCPIDs(bool cleardata);
extern int RestartClient();
extern int ReindexWallet();
#ifdef WIN32
QAxObject *globalcom = NULL;
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
    nWeight(0)
{
    resize(850, 550);
    setWindowTitle(tr("GridCoin") + " - " + tr("Wallet"));
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
    statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
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

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = qApp->style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

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



	upgradeAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Upgrade QT Client"), this);
	upgradeAction->setStatusTip(tr("Upgrade QT Client"));
	upgradeAction->setMenuRole(QAction::TextHeuristicRole);


    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About GridCoin"), this);
    aboutAction->setToolTip(tr("Show information about GridCoin"));
    aboutAction->setMenuRole(QAction::AboutRole);


	
	miningAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Mining Console"), this);
	miningAction->setStatusTip(tr("Go to the mining console"));
	miningAction->setMenuRole(QAction::TextHeuristicRole);


	emailAction = new QAction(QIcon(":/icons/bitcoin"), tr("&E-Mail Center"), this);
	emailAction->setStatusTip(tr("Go to the E-Mail center"));
	emailAction->setMenuRole(QAction::TextHeuristicRole);

	sqlAction = new QAction(QIcon(":/icons/bitcoin"), tr("&SQL Query Analyzer"), this);
	sqlAction->setStatusTip(tr("SQL Query Analyzer"));
	sqlAction->setMenuRole(QAction::TextHeuristicRole);

	leaderboardAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Leaderboard"), this);
	leaderboardAction->setStatusTip(tr("Leaderboard"));
	leaderboardAction->setMenuRole(QAction::TextHeuristicRole);

	
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

	connect(miningAction, SIGNAL(triggered()), this, SLOT(miningClicked()));
	connect(emailAction, SIGNAL(triggered()), this, SLOT(emailClicked()));
	connect(rebuildAction, SIGNAL(triggered()), this, SLOT(rebuildClicked()));
	connect(upgradeAction, SIGNAL(triggered()), this, SLOT(upgradeClicked()));
	connect(downloadAction, SIGNAL(triggered()), this, SLOT(downloadClicked()));

	connect(sqlAction, SIGNAL(triggered()), this, SLOT(sqlClicked()));
	connect(leaderboardAction, SIGNAL(triggered()), this, SLOT(leaderboardClicked()));

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

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);

	QMenu *mining = appMenuBar->addMenu(tr("&Mining"));
    mining->addSeparator();
    mining->addAction(miningAction);
		
	QMenu *email = appMenuBar->addMenu(tr("&E-Mail"));
    email->addSeparator();
    email->addAction(emailAction);
	
	QMenu *upgrade = appMenuBar->addMenu(tr("&Upgrade QT Client"));
	upgrade->addSeparator();
	upgrade->addAction(upgradeAction);


	QMenu *rebuild = appMenuBar->addMenu(tr("&Rebuild Block Chain"));
	rebuild->addSeparator();
	rebuild->addAction(rebuildAction);
	rebuild->addSeparator();
	rebuild->addAction(downloadAction);
	rebuild->addSeparator();
	
	QMenu *sql = appMenuBar->addMenu(tr("&SQL Query Analyzer"));
	sql->addSeparator();
	sql->addAction(sqlAction);

	QMenu *leaderboard = appMenuBar->addMenu(tr("&Leaderboard"));
	leaderboard->addSeparator();
	leaderboard->addAction(leaderboardAction);



}

void BitcoinGUI::createToolBars()
{
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);

    QToolBar *toolbar2 = addToolBar(tr("Actions toolbar"));
    toolbar2->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar2->addAction(exportAction);
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
		CBlockIndex* blockindex = FindBlockByHeight(lSqlBlock);
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
	UpgradeClient();
	
}

void BitcoinGUI::downloadClicked()
{
	DownloadBlocks();

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

void BitcoinGUI::leaderboardClicked()
{
	#ifdef WIN32

	if (globalcom==NULL) {
		globalcom = new QAxObject("BoincStake.Utilization");
	}
    
    globalcom->dynamicCall("ShowLeaderboard()");
#endif
}


int ReindexBlocks()
{

	int result = ReindexWallet();
	return 1;
		
}


void BitcoinGUI::miningClicked()
{
		
#ifdef WIN32

	if (globalcom==NULL) {
		globalcom = new QAxObject("BoincStake.Utilization");
	}
    
      globalcom->dynamicCall("ShowMiningConsole()");
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
		
					globalcom = new QAxObject("BoincStake.Utilization");
								printf("Instantiated globalcom for Windows");
		
			}
		
		    //	globalcom->dynamicCall("ShowMiningConsole()");
			//printf("Showing Mining Console");
			if (false && bCheckedForUpgrade == false && !fTestNet)
			{
						int nNeedsUpgrade = 0;
						bool bCheckedForUpgrade = true;
						printf("Checking to see if Gridcoin needs upgraded\r\n");
						nNeedsUpgrade = globalcom->dynamicCall("ClientNeedsUpgrade()").toInt();
						if (nNeedsUpgrade) UpgradeClient();
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


		std::string time1 =  DateTimeStrFormat("%m-%d-%Y %H:%M:%S", GetTime());
		if (Timer("timestamp",6*5))
		{
			printf("Timestamp: %s\r\n",time1.c_str());
		}

		if (bExecuteCode)
		{
			bExecuteCode = false;
			ExecuteCode();
		}
			
		//Backup the wallet once per day:
		if (Timer("backupwallet", 6*60*20))
		{

			std::string backup_results = BackupGridcoinWallet();
			printf("Daily backup results: %s\r\n",backup_results.c_str());
			//Create a restore point once per day
			//int r = CreateRestorePoint();
			//printf("Created restore point : %i",r);
		}

		if (nRegVersion==0 || Timer("start",10))
		{
			printf("Starting globalcom...\r\n");
			#ifdef WIN32
			if (globalcom==NULL) ReinstantiateGlobalcom();
			nRegVersion = globalcom->dynamicCall("Version()").toInt();
			sRegVer = boost::lexical_cast<std::string>(nRegVersion);
			#endif
		}

		
		
		if (Timer("status_update",2))
		{
			printf("Status Update");
			std::string status = GetGlobalStatus();
    		bForceUpdate=true;
		}

		if (Timer("update_boinc_magnitude", 6*10))
		{
				//double POB = GetPoBDifficulty();
				TallyNetworkAverages(false);
		}
   

		

		if (false)
		{
		if (mapArgs["-restartnetlayer"] != "false") 
		{
			if (Timer("restart_network",6*25))
			{
				printf("\r\nRestarting gridcoin's network layer @ %s\r\n",time1.c_str());
				RestartGridcoin10();
			}
		}
		}


        if (true)
		{
			if (Timer("gather_cpids",6*30))
			{
				printf("\r\nReharvesting cpids in background thread...\r\n");
				LoadCPIDsInBackground();
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
     				//QString pwa = QString::fromUtf8(DefaultWalletAddress().c_str()); 
					//globalcom->dynamicCall("SetPublicWalletAddress(QString)",pwa);
	    			//Set Best Block
	    			globalcom->dynamicCall("SetBestBlock(int)", nBestHeight);
					#endif
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

std::string tostdstring(QString q) 
{
	std::string ss1 = q.toLocal8Bit().constData();
	return ss1;
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




void BitcoinGUI::updateStakingIcon()
{
    updateWeight();

    if (nLastCoinStakeSearchInterval && nWeight)
    {
        uint64_t nNetworkWeight = GetPoSKernelPS();
        unsigned nEstimateTime = GetTargetSpacing(nBestHeight) * nNetworkWeight / (nWeight+.001);

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

        if (IsProtocolV2(nBestHeight+1))
        {
            nWeight /= COIN;
            nNetworkWeight /= COIN;
        }

        labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br>Expected time to earn reward is %3").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        if (pwalletMain && pwalletMain->IsLocked())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is locked"));
        else if (vNodes.empty())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is offline"));
        else if (IsInitialBlockDownload())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is syncing"));
        else if (!nWeight)
            labelStakingIcon->setToolTip(tr("Not staking because you don't have mature coins"));
        else
            labelStakingIcon->setToolTip(tr("Not staking"));
    }
}
