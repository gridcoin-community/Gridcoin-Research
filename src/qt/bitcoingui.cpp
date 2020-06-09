/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */


#include <QProcess>
#include <QInputDialog>

#include <fstream>

#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"

#include "global_objects_noui.hpp"
#include "diagnosticsdialog.h"
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
#include "wallet/wallet.h"
#include "init.h"
#include "block.h"
#include "miner.h"
#include "main.h"
#include "backup.h"
#include "clicklabel.h"
#include "univalue.h"
#include "upgradeqt.h"

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
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDesktopServices> // for opening URLs
#include <QUrl>
#include <QStyle>
#include <QNetworkInterface>
#include <QDesktopWidget>

#include <boost/lexical_cast.hpp>

#include "rpcserver.h"
#include "rpcclient.h"
#include "rpcprotocol.h"
#include "contract/polls.h"
#include "neuralnet/beacon.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/superblock.h"

#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include "boinc.h"
#include "util.h"

extern CWallet* pwalletMain;
extern std::string getMacAddress();

extern std::string FromQString(QString qs);
extern std::string qtExecuteDotNetStringFunction(std::string function, std::string data);

void GetGlobalStatus();

bool IsConfigFileEmpty();

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
    nWeight(0)
{

    setGeometry(QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,QDesktopWidget().availableGeometry(this).size() * 0.6,QDesktopWidget().availableGeometry(this)));

    setWindowTitle(tr("Gridcoin") + " " + tr("Wallet"));

#ifndef Q_OS_MAC
    qApp->setWindowIcon(QPixmap(":/images/gridcoin"));
    setWindowIcon(QPixmap(":/images/gridcoin"));
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

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Clicking on the current poll label on the overview page simply sends you to the voting page
    connect(overviewPage, SIGNAL(pollLabelClicked()), this, SLOT(gotoVotingPage()));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

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

std::string FromQString(QString qs)
{
    std::string sOut = qs.toUtf8().constData();
    return sOut;
}

void BitcoinGUI::setOptionsStyleSheet(QString qssFileName)
{
    // setting the style sheets for the app
    QFile qss(":/stylesheets/"+qssFileName);
    if (qss.open(QIODevice::ReadOnly)){
        QTextStream qssStream(&qss);
        QString sMainWindowHTML = qssStream.readAll();
        qss.close();

        qApp->setStyleSheet(sMainWindowHTML);
    }
    sSheet=qssFileName;
    setIcons();
    // reset encryption status to apply icon color changes
    if(walletModel)
        setEncryptionStatus(walletModel->getEncryptionStatus());
}


void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(tr("&Overview"), tabGroup);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));

    sendCoinsAction = new QAction(tr("&Send"), tabGroup);
    sendCoinsAction->setToolTip(tr("Send coins to a Gridcoin address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));

    receiveCoinsAction = new QAction(tr("&Receive"), tabGroup);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));

    historyAction = new QAction(tr("&Transactions"), tabGroup);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));

    addressBookAction = new QAction(tr("&Address Book"), tabGroup);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));

    votingAction = new QAction(tr("&Voting"), tabGroup);
    votingAction->setToolTip(tr("Voting"));
    votingAction->setCheckable(true);
    votingAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));

    bxAction = new QAction(tr("&Block Explorer"), this);
    bxAction->setStatusTip(tr("Block Explorer"));
    bxAction->setMenuRole(QAction::TextHeuristicRole);

    exchangeAction = new QAction(tr("&Exchange"), this);
    exchangeAction->setStatusTip(tr("Web Site"));
    exchangeAction->setMenuRole(QAction::TextHeuristicRole);

    websiteAction = new QAction(tr("&Web Site"), this);
    websiteAction->setStatusTip(tr("Web Site"));
    websiteAction->setMenuRole(QAction::TextHeuristicRole);

    chatAction = new QAction(tr("&GRC Chat Room"), this);
    chatAction->setStatusTip(tr("GRC Chatroom"));
    chatAction->setMenuRole(QAction::TextHeuristicRole);

    boincAction = new QAction(tr("&BOINC"), this);
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
    connect(votingAction, SIGNAL(triggered()), this, SLOT(gotoVotingPage()));

    connect(websiteAction, SIGNAL(triggered()), this, SLOT(websiteClicked()));
    connect(bxAction, SIGNAL(triggered()), this, SLOT(bxClicked()));
    connect(exchangeAction, SIGNAL(triggered()), this, SLOT(exchangeClicked()));
    connect(boincAction, SIGNAL(triggered()), this, SLOT(boincStatsClicked()));
    connect(chatAction, SIGNAL(triggered()), this, SLOT(chatClicked()));

    quitAction = new QAction(tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);

    aboutAction = new QAction(tr("&About Gridcoin"), this);
    aboutAction->setToolTip(tr("Show information about Gridcoin"));
    aboutAction->setMenuRole(QAction::AboutRole);

    diagnosticsAction = new QAction(tr("&Diagnostics"), this);
    diagnosticsAction->setStatusTip(tr("Diagnostics"));
    diagnosticsAction->setMenuRole(QAction::TextHeuristicRole);

    newUserWizardAction = new QAction(tr("&New User Wizard"), this);
    newUserWizardAction->setStatusTip(tr("New User Wizard"));
    newUserWizardAction->setMenuRole(QAction::TextHeuristicRole);

    optionsAction = new QAction(tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for Gridcoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(tr("&Backup Wallet/Config..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet/config to another location"));
    changePassphraseAction = new QAction(tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(tr("&Unlock Wallet..."), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(tr("&Lock Wallet"), this);
    lockWalletAction->setToolTip(tr("Lock wallet"));
    signMessageAction = new QAction(tr("Sign &message..."), this);
    verifyMessageAction = new QAction(tr("&Verify message..."), this);

    exportAction = new QAction(tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));
    snapshotAction = new QAction(tr("&Snapshot Download"), this);
    snapshotAction->setToolTip(tr("Download and apply latest snapshot"));

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
    connect(diagnosticsAction, SIGNAL(triggered()), this, SLOT(diagnosticsClicked()));
    connect(newUserWizardAction, SIGNAL(triggered()), this, SLOT(newUserWizardClicked()));
    connect(snapshotAction, SIGNAL(triggered()), this, SLOT(snapshotClicked()));
}

void BitcoinGUI::setIcons()
{
    overviewAction->setIcon(QPixmap(":/icons/overview_"+sSheet));
    sendCoinsAction->setIcon(QPixmap(":/icons/send_"+sSheet));
    receiveCoinsAction->setIcon(QPixmap(":/icons/receiving_addresses_"+sSheet));
    historyAction->setIcon(QPixmap(":/icons/history_"+sSheet));
    addressBookAction->setIcon(QPixmap(":/icons/address-book_"+sSheet));
    votingAction->setIcon(QPixmap(":/icons/voting_"+sSheet));
    unlockWalletAction->setIcon(QPixmap(":/icons/lock_open_"+sSheet));
    lockWalletAction->setIcon(QPixmap(":/icons/lock_closed_"+sSheet));

    encryptWalletAction->setIcon(QPixmap(":/icons/lock_closed_"+sSheet));

    bxAction->setIcon(QPixmap(":/icons/block"));
    exchangeAction->setIcon(QPixmap(":/icons/ex"));
    websiteAction->setIcon(QPixmap(":/icons/www"));
    chatAction->setIcon(QPixmap(":/icons/chat"));
    boincAction->setIcon(QPixmap(":/images/boinc"));
    quitAction->setIcon(QPixmap(":/icons/quit"));
    aboutAction->setIcon(QPixmap(":/images/gridcoin"));
    newUserWizardAction->setIcon(QPixmap(":/images/gridcoin"));
    diagnosticsAction->setIcon(QPixmap(":/images/gridcoin"));
    optionsAction->setIcon(QPixmap(":/icons/options"));
    toggleHideAction->setIcon(QPixmap(":/images/gridcoin"));
    backupWalletAction->setIcon(QPixmap(":/icons/filesave"));
    changePassphraseAction->setIcon(QPixmap(":/icons/key"));
    signMessageAction->setIcon(QPixmap(":/icons/edit"));
    verifyMessageAction->setIcon(QPixmap(":/icons/transaction_0"));
    exportAction->setIcon(QPixmap(":/icons/export"));
    openRPCConsoleAction->setIcon(QPixmap(":/icons/debugwindow"));
    snapshotAction->setIcon(QPixmap(":/images/gridcoin"));
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

    if (!GetBoolArg("-testnet", false))
    {
        file->addSeparator();
        file->addAction(snapshotAction);
    }

    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);

    settings->addAction(unlockWalletAction);
    settings->addAction(lockWalletAction);
    settings->addSeparator();
    // This new wizard menu item is disabled until we make the wizard more advanced, because the existing one it makes no sense
    // to run it after the conf file is created.
    //settings->addAction(newUserWizardAction);
    //settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *community = appMenuBar->addMenu(tr("&Community"));
    community->addAction(bxAction);
    community->addAction(exchangeAction);
    community->addAction(boincAction);
    community->addAction(chatAction);
    community->addSeparator();
    community->addAction(websiteAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(diagnosticsAction);
    help->addSeparator();
    help->addAction(aboutAction);
}

void BitcoinGUI::createToolBars()
{
    // "Tabs" toolbar (vertical, aligned on left side of overview screen).
    QToolBar *toolbar = addToolBar("Tabs toolbar");
    toolbar->setObjectName("toolbar");
    addToolBar(Qt::LeftToolBarArea, toolbar);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolbar->setIconSize(QSize(50, 25));
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->addAction(votingAction);

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar->addWidget(spacer);
    spacer->setObjectName("spacer");
    // Unlock Wallet
    toolbar->addAction(unlockWalletAction);
    toolbar->addAction(lockWalletAction);

    addToolBarBreak(Qt::LeftToolBarArea);


    // Status bar notification icons (Status toolbar)
    QToolBar *toolbar2 = addToolBar("Status toolbar");
    addToolBar(Qt::LeftToolBarArea, toolbar2);
    toolbar2->setOrientation(Qt::Vertical);
    //toolbar2->setGeometry(0, 0, STATUSBAR_ICONSIZE, 0);
    toolbar2->setMinimumWidth(STATUSBAR_ICONSIZE);
    toolbar2->setContentsMargins(0, 0, 0, 0);
    toolbar2->setMovable(false);
    toolbar2->setObjectName("toolbar2");

    QFrame *frameBlocks = new QFrame();

    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setMinimumWidth(STATUSBAR_ICONSIZE);

    QVBoxLayout *frameBlocksLayout = new QVBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(1,0,1,0);
    frameBlocksLayout->setSpacing(-1);
    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    labelScraperIcon = new QLabel();
    labelBeaconIcon = new QLabel();

    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addWidget(labelScraperIcon);
    frameBlocksLayout->addWidget(labelBeaconIcon);

    //12-21-2015 Prevent Lock from falling off the page
    frameBlocksLayout->addStretch();

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        // Instead of calling updateStakingIcon here, simply set the icon to staking off.
        // This is to prevent problems since this GUI code can initialize before the core.
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Not staking: Miner is not initialized."));
    }

    QTimer *timerBeaconIcon = new QTimer(labelBeaconIcon);
    connect(timerBeaconIcon, SIGNAL(timeout()), this, SLOT(updateBeaconIcon()));
    timerBeaconIcon->start(30 * 1000);
    updateBeaconIcon();

    toolbar2->addWidget(frameBlocks);

    addToolBarBreak(Qt::TopToolBarArea);


    // Top tool bar (clickable Gridcoin and BOINC logos)
    QToolBar *toolbar3 = addToolBar("Logo bar");
    addToolBar(Qt::TopToolBarArea, toolbar3);
    toolbar3->setOrientation(Qt::Horizontal);
    toolbar3->setMovable(false);
    toolbar3->setObjectName("toolbar3");
    ClickLabel *grcLogoLabel = new ClickLabel();
    grcLogoLabel->setObjectName("gridcoinLogoHorizontal");
    connect(grcLogoLabel, SIGNAL(clicked()), this, SLOT(websiteClicked()));
    toolbar3->addWidget(grcLogoLabel);
    QWidget* logoSpacer = new QWidget();
    logoSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar3->addWidget(logoSpacer);
    logoSpacer->setObjectName("logoSpacer");
    ClickLabel *boincLogoLabel = new ClickLabel();
    boincLogoLabel->setObjectName("boincLogo");
    connect(boincLogoLabel, SIGNAL(clicked()), this, SLOT(boincClicked()));
    toolbar3->addWidget(boincLogoLabel);

    // Use a red color for the toolbars background if on testnet.
    if (GetBoolArg("-testnet"))
    {
        toolbar2->setStyleSheet("background-color:darkRed");
        toolbar3->setStyleSheet("background-color:darkRed");
    }
    else
    {
        toolbar2->setStyleSheet("background-color:rgb(65,0,127)");
        toolbar3->setStyleSheet("background-color:rgb(65,0,127)");
    }


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
            qApp->setWindowIcon(QPixmap(":/images/gridcoin_testnet"));
            setWindowIcon(QPixmap(":/images/gridcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QPixmap(":/images/gridcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("Gridcoin client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QPixmap(":/images/gridcoin_testnet"));
                toggleHideAction->setIcon(QPixmap(":/images/gridcoin_testnet"));
            }

            aboutAction->setIcon(QPixmap(":/images/gridcoin_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Start with out-of-sync message for scraper/NN.
        updateScraperIcon((int)scrapereventtypes::OutOfSync, CT_UPDATING);
        connect(clientModel, SIGNAL(updateScraperStatus(int, int)), this, SLOT(updateScraperIcon(int, int)));

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
    trayIcon->setIcon(QPixmap(":/images/gridcoin"));
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

void BitcoinGUI::setNumConnections(int n)
{
    QString icon;
    switch (n)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));

    if (n == 0)
    {
        labelConnectionsIcon->setToolTip(tr("No active connections to the Gridcoin network. "
                                            "If this persists more than a few minutes, please check your configuration "
                                            "and your network connectivity."));
    }
    else
    {
        labelConnectionsIcon->setToolTip(tr("%n active connection(s) to the Gridcoin network", "", n));
    }
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
         text = tr("%n second(s) ago", "", secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago", "", secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago", "", secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago", "", secs/(60*60*24));
    }

    // Set icon state: not synced icon if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        labelBlocksIcon->setPixmap(QIcon(":/icons/notsynced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;

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

void BitcoinGUI::update(const QString &title, const QString& version, const QString &message)
{
    // Create our own message box; A dialog can go here in future for qt if we choose
    QMessageBox* updatemsg = new QMessageBox;

    updatemsg->setAttribute(Qt::WA_DeleteOnClose);
    updatemsg->setWindowTitle(title);
    updatemsg->setText(version);
    updatemsg->setDetailedText(message);
    updatemsg->setIcon(QMessageBox::Information);
    updatemsg->setStandardButtons(QMessageBox::Ok);
    updatemsg->setModal(false);
    // Due to slight delay in gui load this could appear behind the gui ui
    // The only other option available would make the message box stay on top of all applications

    QTimer::singleShot(5000, updatemsg, SLOT(show()));
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
    fsbridge::ofstream myConfig;
    myConfig.open(GetDataDir() / "gridcoinresearch.conf");

    myConfig << "email=" << boinc_email << "\n"
        << "addnode=node.gridcoin.us\n"
        << "addnode=www.grcpool.com\n"
        << "addnode=seeds.gridcoin.ifoggz-network.xyz\n"
        << "addnode=ec2-3-81-39-58.compute-1.amazonaws.com\n"
        << "addnode=addnode-us-central.cycy.me\n"
        << "addnode=gridcoin.ddns.net\n";

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
    if (IsConfigFileEmpty())
    {
        QString boincemail = "";
        //Typhoon- Check to see if boinc exists in default path - 11-19-2014

        fs::path sourcefile = GetBoincDataDir() / "client_state.xml";
        std::string sout = GetFileContents(sourcefile);
        //bool BoincInstalled = true;
        std::string sBoincNarr = "";
        if (sout == "-1")
        {
            LogPrintf("BOINC not installed in default location! ");
            //BoincInstalled=false;
            sBoincNarr = "BOINC is not installed in the default location " + sourcefile.string() + "!  Please set boincdatadir in the gridcoinresearch.conf file to the correct path where the BOINC client_state.xml file resides.";
        }

        bool ok;
        boincemail = QInputDialog::getText(this, tr("New User Wizard"),
                                          tr("Please enter your BOINC E-mail address, or click <Cancel> to skip for now:"),
                                          QLineEdit::Normal,
                                          "", &ok);

        if (ok && !boincemail.isEmpty())
        {
            std::string new_email = tostdstring(boincemail);
            boost::to_lower(new_email);
            LogPrintf("User entered %s ",new_email);
            //Create Config File
            CreateNewConfigFile(new_email);
            QString strMessage = tr("Created new Configuration File Successfully. ");
            QMessageBox::warning(this, tr("New Account Created - Welcome Aboard!"), strMessage);

            // Reload BOINC CPIDs now that we know the user's email address:
            NN::Researcher::Reload();
        }
        else
        {
            //Create Config File
            CreateNewConfigFile("investor");
            QString strMessage = tr("To get started with BOINC, run the BOINC client, choose projects, then populate the gridcoinresearch.conf file in %appdata%\\GridcoinResearch with your BOINC e-mail address.  To run this wizard again, please delete the gridcoinresearch.conf file. ");
            QMessageBox::warning(this, tr("New User Wizard - Skipped"), strMessage);
        }
        // Read in the mapargs, and set the seed nodes 10-13-2015
        ReadConfigFile(mapArgs, mapMultiArgs);

        //Force some addnodes in to get user started
        ForceInAddNode("node.gridcoin.us");
        ForceInAddNode("www.grcpool.com");
        ForceInAddNode("seeds.gridcoin.ifoggz-network.xyz");
        ForceInAddNode("ec2-3-81-39-58.compute-1.amazonaws.com");
        ForceInAddNode("addnode-us-central.cycy.me");
        ForceInAddNode("gridcoin.ddns.net");

        if (sBoincNarr != "")
        {
                QString qsMessage = tr(sBoincNarr.c_str());
                QMessageBox::warning(this, tr("Attention! - BOINC Path Error!"), qsMessage);
        }
    }
}



void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();

    // On new transaction, make an info balloon
    // Unless the initial block download is in progress OR transaction notification
    // is disabled, to prevent balloon-spam.
    if(!(clientModel->inInitialBlockDownload() || walletModel->getOptionsModel()->getDisableTrxNotifications()))
    {
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
                                 "Address: %4")
                              .arg(date)
                              .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
    }
}

void BitcoinGUI::snapshotClicked()
{
    QMessageBox Msg;

    Msg.setIcon(QMessageBox::Question);
    Msg.setText(tr("Do you wish to download and apply the latest snapshot? If yes the wallet will shutdown and perform the task."));
    Msg.setInformativeText(tr("Warning: Canceling after stage 2 will result in sync from 0 or corrupted blockchain files."));
    Msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    Msg.setDefaultButton(QMessageBox::No);

    int result = Msg.exec();
    bool fProceed;

    switch (result)
    {
        case QMessageBox::Yes    :    fProceed = true;     break;
        case QMessageBox::No     :    fProceed = false;    break;
        default                  :    fProceed = false;    break;
    }

    if (!fProceed)
    {
        Msg.close();

        return;
    }

    else
    {
        fSnapshotRequest = true;

        qApp->quit();
    }
}

void BitcoinGUI::diagnosticsClicked()
{
    diagnosticsDialog->show();
    diagnosticsDialog->raise();
    diagnosticsDialog->activateWindow();
}

// Note this is for the menu item. The menu item is disabled until we implement a more advanced wizard.
void BitcoinGUI::newUserWizardClicked()
{
    NewUserWizard();
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
    QDesktopServices::openUrl(QUrl("https://boinc.berkeley.edu"));
}

void BitcoinGUI::boincStatsClicked()
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

void BitcoinGUI::gotoVotingPage()
{
    votingAction->setChecked(true);
    votingPage->resetData();
    centralWidget->setCurrentWidget(votingPage);

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
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open_"+sSheet).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently %1 ").arg(fWalletUnlockStakingOnly ? tr("<b>unlocked for staking only</b>") : tr("<b>fully unlocked</b>")));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed_"+sSheet).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
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
    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
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

void BitcoinGUI::timerfire()
{
    try
    {
        static bool bNewUserWizardNotified = false;
         if (!bNewUserWizardNotified)
         {
             bNewUserWizardNotified=true;
             NewUserWizard();
         }

         // TODO: Check if these SetRPCResponse calls are really needed.
        /*if (bGlobalcomInitialized)
        {
            //R Halford - Allow .NET to talk to Core: 6-21-2015
            #ifdef WIN32
                std::string sData = qtExecuteDotNetStringFunction("GetDotNetMessages","");
                if (!sData.empty())
                {
                    std::string RPCCommand = ExtractXML(sData,"<COMMAND>","</COMMAND>");
                    std::string Argument1 = ExtractXML(sData,"<ARG1>","</ARG1>");
                    std::string Argument2 = ExtractXML(sData,"<ARG2>","</ARG2>");

                    if (RPCCommand=="rain")
                    {
                        try
                        {
                            std::string response = executeRain(Argument1+Argument2);
                            qtExecuteGenericFunction("SetRPCResponse"," "+response);
                        }
                        catch (const UniValue& objError)
                        {
                            qtExecuteGenericFunction("SetRPCResponse", find_value(objError, "message").get_str());
                        }
                    }
                }
            #endif
        }*/

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
            LogPrintf("GENERAL RUNTIME ERROR!");
    }


}

QString BitcoinGUI::GetEstimatedStakingFrequency(unsigned int nEstimateTime)
{
    QString text;

    if (!nEstimateTime)
    {
        text = tr("undefined");

        return text;
    }

    // Start with 1/yr
    double frequency = 3600.0 * 24.0 * 365.0 / nEstimateTime;
    QString unit = tr("year");
    double next_unit_threshold = 1.0;

    if (frequency >= next_unit_threshold * 12.0)
    {
        frequency /= 12.0;
        unit = tr("month");
    }

    if (frequency >= next_unit_threshold * 30.0)
    {
        frequency /= 30.0;
        unit = tr("day");
    }

    if (frequency >= next_unit_threshold * 24.0)
    {
        frequency /= 24.0;
        unit = tr("hour");
    }

    text = tr("%1 times per %2").arg(QString(RoundToString(frequency, 2).c_str())).arg(unit);

    return text;
}



void BitcoinGUI::updateStakingIcon()
{
    // If the miner has not initialized, get out to avoid a lock crash.
    if (!miner_first_pass_complete) return;

    uint64_t nWeight, nLastInterval, nNetworkWeight;
    std::string ReasonNotStaking;
    QString estimated_staking_freq;
    bool staking;
    bool able_to_stake;

    {
        LOCK(MinerStatus.lock);

        // nWeight is in GRC units rather than miner weight units because this is more familiar to users.
        nWeight = MinerStatus.WeightSum / 80.0;
        nLastInterval = MinerStatus.nLastCoinStakeSearchInterval;
        ReasonNotStaking = MinerStatus.ReasonNotStaking;

        able_to_stake = MinerStatus.able_to_stake;
    }

    staking = nLastInterval && nWeight;
    nNetworkWeight = GetEstimatedNetworkWeight() / 80.0;

    // It is ok to run this regardless of staking status, because it bails early in
    // the not able to stake situation.
    estimated_staking_freq = GetEstimatedStakingFrequency(GetEstimatedTimetoStake());

    if (staking)
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br><b>Estimated</b> staking frequency is %3.")
                                     .arg(nWeight).arg(nNetworkWeight)
                                     .arg(estimated_staking_freq));
    }
    else if (!staking && !able_to_stake)
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_unable").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        //Part of this string wont be translated :(
        labelStakingIcon->setToolTip(tr("Unable to stake: %1")
                                     .arg(QString(ReasonNotStaking.c_str())));
    }
    else //if (miner_first_pass_complete)
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        //Part of this string wont be translated :(
        labelStakingIcon->setToolTip(tr("Not staking currently: %1, <b>Estimated</b> staking frequency is %2.")
                                     .arg(QString(ReasonNotStaking.c_str()))
                                     .arg(estimated_staking_freq));
    }
}


void BitcoinGUI::updateScraperIcon(int scraperEventtype, int status)
{
    const ConvergedScraperStats& ConvergedScraperStatsCache = clientModel->getConvergedScraperStatsCache();

    int64_t nConvergenceTime = ConvergedScraperStatsCache.nTime;

    QString qsExcludedProjects;
    QString qsIncludedScrapers;
    QString qsExcludedScrapers;
    QString qsScrapersNotPublishing;

    bool bDisplayScrapers = false;

    // Note that the translation macro tr is applied in the setToolTip call below.
    // If the convergence cache has excluded projects...
    if (!ConvergedScraperStatsCache.Convergence.vExcludedProjects.empty())
    {
        qsExcludedProjects = QString(((std::string)boost::algorithm::join(ConvergedScraperStatsCache.Convergence.vExcludedProjects, ", ")).c_str());
    }
    else
    {
        qsExcludedProjects = tr("none");
    }

    // If scraper logging category is turned on then show scrapers in tooltip...
    if (LogInstance().WillLogCategory(BCLog::LogFlags::SCRAPER))
    {
        bDisplayScrapers = true;

        // No need to include "none" for included scrapers, because if no scrapers there will not be a convergence.
        qsIncludedScrapers = QString(((std::string)boost::algorithm::join(ConvergedScraperStatsCache.Convergence.vIncludedScrapers, ", ")).c_str());

        if (!ConvergedScraperStatsCache.Convergence.vExcludedScrapers.empty())
        {
            qsExcludedScrapers = QString(((std::string)boost::algorithm::join(ConvergedScraperStatsCache.Convergence.vExcludedScrapers, ", ")).c_str());
        }
        else
        {
            qsExcludedScrapers = tr("none");
        }

        if (!ConvergedScraperStatsCache.Convergence.vScrapersNotPublishing.empty())
        {
            qsScrapersNotPublishing = QString(((std::string)boost::algorithm::join(ConvergedScraperStatsCache.Convergence.vScrapersNotPublishing, ", ")).c_str());
        }
        else
        {
            qsScrapersNotPublishing = tr("none");
        }
    }

    if (scraperEventtype == (int)scrapereventtypes::OutOfSync && status == CT_UPDATING)
    {
        labelScraperIcon->setPixmap(QIcon(":/icons/notsynced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelScraperIcon->setToolTip(tr("Scraper: waiting on wallet to sync."));
    }
    else if (scraperEventtype == (int)scrapereventtypes::Sleep && status == CT_NEW)
    {
        labelScraperIcon->setPixmap(QIcon(":/icons/gray_scraper").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelScraperIcon->setToolTip(tr("Scraper: superblock not needed - inactive."));
    }
    else if (scraperEventtype == (int)scrapereventtypes::Stats && (status == CT_NEW || status == CT_UPDATED || status == CT_UPDATING))
    {
        labelScraperIcon->setPixmap(QIcon(":/icons/notsynced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelScraperIcon->setToolTip(tr("Scraper: downloading and processing stats."));
    }
    else if ((scraperEventtype == (int)scrapereventtypes::Convergence  || scraperEventtype == (int)scrapereventtypes::SBContract)
             && (status == CT_NEW || status == CT_UPDATED) && nConvergenceTime)
    {
        labelScraperIcon->setPixmap(QIcon(":/icons/green_scraper").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        if (bDisplayScrapers)
        {
            labelScraperIcon->setToolTip(tr("Scraper: Convergence achieved, date/time %1 UTC. \n"
                                            "Project(s) excluded: %2. \n"
                                            "Scrapers included: %3. \n"
                                            "Scraper(s) excluded: %4. \n"
                                            "Scraper(s) not publishing: %5.")
                                         .arg(QString(DateTimeStrFormat("%x %H:%M:%S", nConvergenceTime).c_str()))
                                         .arg(qsExcludedProjects)
                                         .arg(qsIncludedScrapers)
                                         .arg(qsExcludedScrapers)
                                         .arg(qsScrapersNotPublishing));
        }
        else
        {
            labelScraperIcon->setToolTip(tr("Scraper: Convergence achieved, date/time %1 UTC. \n"
                                            " Project(s) excluded: %2.")
                                         .arg(QString(DateTimeStrFormat("%x %H:%M:%S", nConvergenceTime).c_str()))
                                         .arg(qsExcludedProjects));
        }
    }
    else if ((scraperEventtype == (int)scrapereventtypes::Convergence  || scraperEventtype == (int)scrapereventtypes::SBContract)
             && status == CT_DELETED)
    {
        labelScraperIcon->setPixmap(QIcon(":/icons/white_and_red_x").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelScraperIcon->setToolTip(tr("Scraper: No convergence able to be achieved. Will retry in a few minutes."));
    }

}

void BitcoinGUI::updateBeaconIcon()
{
    // Intent is to be an investor. Suppress icon.
    if (NN::Researcher::ConfiguredForInvestorMode())
    {
        labelBeaconIcon->hide();
        return;
    }

    const NN::CpidOption cpid = NN::Researcher::Get()->Id().TryCpid();

    if (!cpid)
    {
        labelBeaconIcon->setPixmap(QIcon(":/icons/beacon_red").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelBeaconIcon->setToolTip(tr("Wallet status is INVESTOR, but the wallet is not configured for investor mode. "
                                       "If you intend to be an investor only, please remove the email entry in the config file, "
                                       "otherwise you need to check your email entry and your BOINC installation."));
        return;
    }

    const NN::BeaconOption beacon = NN::GetBeaconRegistry().Try(*cpid);

    if (!beacon)
    {
        labelBeaconIcon->setPixmap(QIcon(":/icons/beacon_red").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelBeaconIcon->setToolTip(tr("There is a CPID, %1, but there is no active beacon. "
                                       "Your beacon may be expired or never advertised. Please "
                                       "advertise a new beacon.")
                                    .arg(cpid->ToString().c_str()));
        return;
    }

    constexpr double seconds_in_day = 24.0 * 60.0 * 60.0;
    constexpr int32_t renewal_warning_days = 15;

    const int64_t now = GetAdjustedTime();
    const int32_t beacon_age_days = std::round(beacon->Age(now) / seconds_in_day);
    const int32_t days_to_expiration = (NN::Beacon::MAX_AGE / seconds_in_day) - beacon_age_days;

    QString icon_res;
    QString status;

    if (days_to_expiration <= renewal_warning_days)
    {
        icon_res = ":/icons/beacon_red";
        status = tr("BEACON SHOULD BE RENEWED IMMEDIATELY TO AVOID LAPSE.");
    }
    else if (beacon->Renewable(now))
    {
        icon_res = ":/icons/beacon_yellow";
        status = tr("Beacon should be renewed.");
    }
    else if (NN::Quorum::GetMagnitude(*cpid) == 0)
    {
        icon_res = ":/icons/beacon_red";
        status = tr(
            "Magnitude is zero, which may prevent staking with research rewards. "
            "Please check your magnitude after the next superblock.");
    }
    else
    {
        icon_res = ":/icons/beacon_green";
        status = tr("Beacon status is good.");
    }

    labelBeaconIcon->setPixmap(QIcon(icon_res).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    labelBeaconIcon->setToolTip(tr("CPID: %1\n"
                                   "Beacon age: %2 day(s)\n"
                                   "Expires: %3 day(s)\n"
                                   "%4.")
                                .arg(cpid->ToString().c_str())
                                .arg(beacon_age_days)
                                .arg(days_to_expiration)
                                .arg(status));
}
