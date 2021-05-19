/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */


#include <QProcess>
#include <QInputDialog>

#include "qt/decoration.h"
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"

#include "diagnosticsdialog.h"
#include "receivecoinspage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "votingdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "researcher/researchermodel.h"
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
#include "main.h"
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
#include <QDesktopWidget>

#include <boost/lexical_cast.hpp>

#include "rpc/server.h"
#include "rpc/client.h"
#include "rpc/protocol.h"
#include "gridcoin/backup.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/superblock.h"

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include "util.h"

extern CWallet* pwalletMain;
extern std::string FromQString(QString qs);
extern CCriticalSection cs_ConvergedScraperStatsCache;

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
    QSettings settings;
    if (!restoreGeometry(settings.value("MainWindowGeometry").toByteArray())) {
        // Restore failed (perhaps missing setting), center the window
        setGeometry(QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,QDesktopWidget().availableGeometry(this).size()
                                        * 0.4,QDesktopWidget().availableGeometry(this)));
    }

    QFontDatabase::addApplicationFont(":/fonts/inter-bold");
    QFontDatabase::addApplicationFont(":/fonts/inter-medium");
    QFontDatabase::addApplicationFont(":/fonts/inter-regular");
    QFontDatabase::addApplicationFont(":/fonts/inconsolata-regular");

    // This slightly enlarges the application's base font size on Windows and
    // Linux. MacOS often uses a different reference DPI so the size used for
    // Windows and Linux causes the rendered text to appear smaller. For Mac,
    // we set an explicit application-wide font size to Qt's default value to
    // normalize text size on controls like toolbar buttons and tabs that can
    // render smaller with the Cocoa integration plugin:
    //
    QFont appFont = qApp->font();
#ifndef Q_OS_MAC
    appFont.setPointSize(10);
#else
    appFont.setPointSize(13);
#endif
    qApp->setFont(appFont);

    // Qt paints some decorations directly and provides no stylesheet hooks to
    // customize appearance (for example: the rulers in a QWizardPage). We set
    // certain application palette colors to fully-transparent to render these
    // aspects invisible so that the painted features do not clash with visual
    // designs of the application's themes. Important display elements provide
    // the ability to customize appearances using stylesheets which override a
    // palette set for the application, so we do not risk concealing the vital
    // UI components, but consider removing the override to debug an obstinate
    // display issue. This code must run before loading a stylesheet:
    //
    QPalette pal(qApp->palette());
    pal.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    pal.setColor(QPalette::Mid, pal.color(QPalette::Base));
    // Set links to a lighter shade of blue for readability in the dark theme:
    pal.setColor(QPalette::Link, QColor(73, 144, 226));
    pal.setColor(QPalette::LinkVisited, QColor(73, 144, 226));
    qApp->setPalette(pal);

    setWindowTitle(tr("Gridcoin") + " " + tr("Wallet"));

#ifndef Q_OS_MAC
    qApp->setWindowIcon(QPixmap(":/images/gridcoin"));
    setWindowIcon(QPixmap(":/images/gridcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

#ifdef Q_OS_MAC
    m_app_nap_inhibitor = new CAppNapInhibitor;
    m_app_nap_inhibitor->disableAppNap();
    app_nap_enabled = false;
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

    addressBookPage = new QWidget(this);
    addressBook = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);
    QVBoxLayout *addressBookPageLayout = new QVBoxLayout();
    addressBookPageLayout->addWidget(addressBook);
    addressBookPage->setLayout(addressBookPageLayout);

    receiveCoinsPage = new ReceiveCoinsPage(this);

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
    statusBar();

    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

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

    QTimer *overview_update_timer = new QTimer(this);

    // Update every MODEL_UPDATE_DELAY seconds.
    overview_update_timer->start(MODEL_UPDATE_DELAY);

    QObject::connect(overview_update_timer, SIGNAL(timeout()), this, SLOT(updateGlobalStatus()));

    connect(openConfigAction, SIGNAL(triggered()), this, SLOT(openConfigClicked()));

    gotoOverviewPage();
}

BitcoinGUI::~BitcoinGUI()
{
    QSettings settings;
    settings.setValue("MainWindowGeometry", saveGeometry());
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete m_app_nap_inhibitor;
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
    // Applying a stylesheet can be rather expensive on a wallet with many
    // transactions. Avoid reloading styles if the theme didn't change:
    if (qssFileName == sSheet) {
        return;
    }

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
    if (walletModel) {
        setEncryptionStatus(walletModel->getEncryptionStatus());
    }

    // Reapply the appropriate beacon icon color scheme:
    if (researcherModel) {
        researcherModel->setTheme(sSheet);
    }
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

    historyAction = new QAction(tr("&History"), tabGroup);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));

    addressBookAction = new QAction(tr("&Favorites"), tabGroup);
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
    // No more than one action should be given this role to avoid overwriting actions
    // on platforms which move the actions based on the menu role (ex. macOS)
    aboutAction->setMenuRole(QAction::AboutRole);

    diagnosticsAction = new QAction(tr("&Diagnostics"), this);
    diagnosticsAction->setStatusTip(tr("Diagnostics"));
    diagnosticsAction->setMenuRole(QAction::TextHeuristicRole);

    optionsAction = new QAction(tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for Gridcoin"));
    // No more than one action should be given this role to avoid overwriting actions
    // on platforms which move the actions based on the menu role (ex. macOS)
    optionsAction->setMenuRole(QAction::PreferencesRole);
    openConfigAction = new QAction(tr("Open config &file..."), this);
    optionsAction->setToolTip(tr("Open the config file in your standard editor"));
    researcherAction = new QAction(tr("&Researcher Wizard..."), this);
    researcherAction->setToolTip(tr("Open BOINC and beacon settings for Gridcoin"));
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
    resetblockchainAction = new QAction(tr("&Reset blockchain data"), this);
    resetblockchainAction->setToolTip(tr("Remove blockchain data and start chain from zero"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(researcherAction, SIGNAL(triggered()), this, SLOT(researcherClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    connect(diagnosticsAction, SIGNAL(triggered()), this, SLOT(diagnosticsClicked()));
    connect(snapshotAction, SIGNAL(triggered()), this, SLOT(snapshotClicked()));
    connect(resetblockchainAction, SIGNAL(triggered()), this, SLOT(resetblockchainClicked()));
}

void BitcoinGUI::setIcons()
{
    const QToolBar* toolbar = findChild<QToolBar*>();
    const int toolbar_icon_size = GRC::ScalePx(this, 16);

    ToolbarButtonIconFilter::apply(
        this,
        overviewAction,
        toolbar->widgetForAction(overviewAction),
        toolbar_icon_size,
        ":/icons/overview_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        sendCoinsAction,
        toolbar->widgetForAction(sendCoinsAction),
        toolbar_icon_size,
        ":/icons/send_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        receiveCoinsAction,
        toolbar->widgetForAction(receiveCoinsAction),
        toolbar_icon_size,
        ":/icons/receive_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        historyAction,
        toolbar->widgetForAction(historyAction),
        toolbar_icon_size,
        ":/icons/history_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        addressBookAction,
        toolbar->widgetForAction(addressBookAction),
        toolbar_icon_size,
        ":/icons/address-book_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        votingAction,
        toolbar->widgetForAction(votingAction),
        toolbar_icon_size,
        ":/icons/voting_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        unlockWalletAction,
        toolbar->widgetForAction(unlockWalletAction),
        toolbar_icon_size,
        ":/icons/lock_open_" + sSheet);
    ToolbarButtonIconFilter::apply(
        this,
        lockWalletAction,
        toolbar->widgetForAction(lockWalletAction),
        toolbar_icon_size,
        ":/icons/lock_closed_" + sSheet);

    encryptWalletAction->setIcon(QPixmap(":/icons/lock_closed_"+sSheet));
    bxAction->setIcon(QPixmap(":/icons/block"));
    exchangeAction->setIcon(QPixmap(":/icons/ex"));
    websiteAction->setIcon(QPixmap(":/icons/www"));
    chatAction->setIcon(QPixmap(":/icons/chat"));
    boincAction->setIcon(QPixmap(":/images/boinc"));
    quitAction->setIcon(QPixmap(":/icons/quit"));
    aboutAction->setIcon(QPixmap(":/images/gridcoin"));
    diagnosticsAction->setIcon(QPixmap(":/images/gridcoin"));
    optionsAction->setIcon(QPixmap(":/icons/options"));
    researcherAction->setIcon(QPixmap(":/images/gridcoin"));
    toggleHideAction->setIcon(QPixmap(":/images/gridcoin"));
    backupWalletAction->setIcon(QPixmap(":/icons/filesave"));
    changePassphraseAction->setIcon(QPixmap(":/icons/key"));
    signMessageAction->setIcon(QPixmap(":/icons/edit"));
    verifyMessageAction->setIcon(QPixmap(":/icons/transaction_0"));
    exportAction->setIcon(QPixmap(":/icons/export"));
    openRPCConsoleAction->setIcon(QPixmap(":/icons/debugwindow"));
    snapshotAction->setIcon(QPixmap(":/images/gridcoin"));
    openConfigAction->setIcon(QPixmap(":/icons/edit"));
    resetblockchainAction->setIcon(QPixmap(":/images/gridcoin"));
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
    QMenu *file = appMenuBar->addMenu(tr("&File"));
#else
    // Windows and Linux: collapse the main application's menu bar into a menu
    // button. On macOS, we'll continue to use the system's separate menu bar.
    appMenuBar = new QMenu();
    QMenu *file = appMenuBar;
#endif

    // Configure the menus
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();

    if (!GetBoolArg("-testnet", false))
    {
        file->addAction(snapshotAction);
    }

    file->addAction(resetblockchainAction);

    file->addSeparator();
#ifdef Q_OS_MAC
    file->addAction(quitAction);
#endif

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);

    settings->addAction(unlockWalletAction);
    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(researcherAction);
    settings->addSeparator();
    settings->addAction(optionsAction);
    settings->addAction(openConfigAction);

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

#ifndef Q_OS_MAC
    file->addSeparator();
    file->addAction(quitAction);
#endif
}

void BitcoinGUI::createToolBars()
{
    ClickLabel *logoLabel = new ClickLabel();
    logoLabel->setObjectName("toolbarLogoLabel");
    QSizePolicy logoLabelSizePolicy = logoLabel->sizePolicy();
    logoLabelSizePolicy.setHorizontalStretch(2);
    logoLabel->setSizePolicy(logoLabelSizePolicy);
    connect(logoLabel, SIGNAL(clicked()), this, SLOT(websiteClicked()));

    QHBoxLayout *logoWrapperLayout = new QHBoxLayout();
    logoWrapperLayout->setContentsMargins(2, 0, 2, 0);
    logoWrapperLayout->setSpacing(0);

    QWidget *logoWrapper = new QWidget();
    logoWrapper->setObjectName("toolbarLogoWrapper");
    logoWrapper->setLayout(logoWrapperLayout);

#ifndef Q_OS_MAC
    // Windows and Linux: collapse the main application's menu bar into a menu
    // button. On macOS, we'll continue to use the system's separate menu bar.
    QPushButton *menuButton = new QPushButton();
    menuButton->setObjectName("toolbarMenuButton");
    menuButton->setToolTip(tr("Open menu."));
    menuButton->setMenu(appMenuBar);
    QSizePolicy menuButtonSizePolicy = menuButton->sizePolicy();
    menuButtonSizePolicy.setHorizontalStretch(1);
    menuButton->setSizePolicy(menuButtonSizePolicy);
    logoWrapperLayout->addWidget(menuButton);
    logoWrapperLayout->setAlignment(menuButton, Qt::AlignHCenter | Qt::AlignVCenter);
#else
    logoWrapperLayout->addStretch(1);
#endif

    logoWrapperLayout->addWidget(logoLabel);

    QPushButton *themeToggleButton = new QPushButton();
    themeToggleButton->setObjectName("themeToggleButton");
    themeToggleButton->setToolTip(tr("Toggle light/dark mode."));
    QSizePolicy themeToggleButtonSizePolicy = themeToggleButton->sizePolicy();
    themeToggleButtonSizePolicy.setHorizontalStretch(1);
    themeToggleButton->setSizePolicy(themeToggleButtonSizePolicy);
    connect(themeToggleButton, SIGNAL(clicked()), this, SLOT(themeToggled()));
    logoWrapperLayout->addWidget(themeToggleButton);
    logoWrapperLayout->setAlignment(themeToggleButton, Qt::AlignHCenter | Qt::AlignVCenter);

    QWidget *boincLabelSpacer = new QWidget();
    boincLabelSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    ClickLabel *boincLabel = new ClickLabel();
    boincLabel->setObjectName("toolbarBoincLabel");
    connect(boincLabel, SIGNAL(clicked()), this, SLOT(boincClicked()));

    // "Tabs" toolbar (vertical, aligned on left side of overview screen).
    QToolBar *toolbar = addToolBar("Tabs toolbar");
    toolbar->setObjectName("toolbar");
    addToolBar(Qt::LeftToolBarArea, toolbar);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    // Setting a taller height than the rendered icon provides additional
    // padding between the icon and the button text:
    toolbar->setIconSize(GRC::ScaleSize(this, 16, 24));
    toolbar->addWidget(logoWrapper);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->addAction(votingAction);
    toolbar->addSeparator();
    toolbar->addAction(unlockWalletAction);
    toolbar->addAction(lockWalletAction);
    toolbar->addWidget(boincLabelSpacer);
    toolbar->addWidget(boincLabel);
    toolbar->layout()->setAlignment(boincLabel, Qt::AlignHCenter | Qt::AlignBottom);

    addToolBarBreak(Qt::LeftToolBarArea);

    // Status bar notification icons (Status toolbar)
    QFrame *frameBlocks = new QFrame();

    // Show a red label in the status bar for testnet:
    if (GetBoolArg("-testnet")) {
        QLabel *testnetLabel = new QLabel();
        testnetLabel->setObjectName("testnetStatusLabel");
        testnetLabel->setText("TESTNET");

        statusBar()->addWidget(testnetLabel);
    }

    statusbarAlertsLabel = new QLabel();
    statusBar()->addWidget(statusbarAlertsLabel);
    statusBar()->layout()->setAlignment(statusbarAlertsLabel, Qt::AlignLeft | Qt::AlignVCenter);

    frameBlocks->setContentsMargins(0,0,0,0);

    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    frameBlocksLayout->setContentsMargins(5, 0, 5, 0);
    frameBlocksLayout->setSpacing(3);
    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new ClickLabel();
    connect(labelConnectionsIcon, SIGNAL(clicked()), this, SLOT(peersClicked()));
    labelBlocksIcon = new QLabel();
    labelScraperIcon = new QLabel();
    labelBeaconIcon = new ClickLabel();
    connect(labelBeaconIcon, SIGNAL(clicked()), this, SLOT(researcherClicked()));

    frameBlocksLayout->addWidget(labelBeaconIcon);
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addWidget(labelScraperIcon);
    frameBlocksLayout->addWidget(labelEncryptionIcon);

    //12-21-2015 Prevent Lock from falling off the page
    frameBlocksLayout->addStretch();

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(MODEL_UPDATE_DELAY);
        // Instead of calling updateStakingIcon here, simply set the icon to staking off.
        // This is to prevent problems since this GUI code can initialize before the core.
        labelStakingIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_staking_no_" + sSheet));
        labelStakingIcon->setToolTip(tr("Not staking: Miner is not initialized."));
    }

    statusBar()->addPermanentWidget(frameBlocks);

    addToolBarBreak(Qt::TopToolBarArea);
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

        // set stylesheet
        setOptionsStyleSheet(this->clientModel->getOptionsModel()->getCurrentStyle());
        connect(this->clientModel->getOptionsModel(),SIGNAL(walletStylesheetChanged(QString)),this,SLOT(setOptionsStyleSheet(QString)));

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

        rpcConsole->setClientModel(clientModel);
        addressBook->setOptionsModel(clientModel->getOptionsModel());
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

        overviewPage->setWalletModel(walletModel);
        addressBook->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setAddressTableModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        votingPage->setModel(walletModel);
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

void BitcoinGUI::setResearcherModel(ResearcherModel *researcherModel)
{
    this->researcherModel = researcherModel;

    if (!researcherModel) {
        return;
    }

    overviewPage->setResearcherModel(researcherModel);
    diagnosticsDialog->SetResearcherModel(researcherModel);

    updateBeaconIcon();
    connect(researcherModel, SIGNAL(beaconChanged()), this, SLOT(updateBeaconIcon()));
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
    trayIconMenu->addAction(researcherAction);
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

void BitcoinGUI::themeToggled()
{
    clientModel->getOptionsModel()->setCurrentStyle(sSheet == "light" ? "dark" : "light");
}

void BitcoinGUI::openConfigClicked()
{
    boost::filesystem::path pathConfig = GetConfigFile();
    /* Open gridcoinresearch.conf with the associated application */
    bool res = QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(pathConfig.string())));

#ifdef Q_OS_WIN
    // Workaround for windows specific behaviour
    if(!res) {
        res = QProcess::startDetached("C:\\Windows\\system32\\notepad.exe", QStringList{QString::fromStdString(pathConfig.string())});
    }
#endif
#ifdef Q_OS_MAC
    // Workaround for macOS-specific behaviour; see https://github.com/bitcoin/bitcoin/issues/15409
    if (!res) {
        res = QProcess::startDetached("/usr/bin/open", QStringList{"-t", QString::fromStdString(pathConfig.string())});
    }
#endif

    if (!res) {
        error("File Association Error", "Unable to open the config file. Please check your operating system"
                                        " file associations.", true);
    }
}

void BitcoinGUI::researcherClicked()
{
    if (!researcherModel || !walletModel) {
        return;
    }

    researcherModel->showWizard(walletModel);
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
        case 0: icon = ":/icons/status_connection_0"; break;
        case 1: case 2: icon = ":/icons/status_connection_1"; break;
        case 3: case 4: case 5: icon = ":/icons/status_connection_2"; break;
        case 6: case 7: case 8: case 9: icon = ":/icons/status_connection_3"; break;
        default: icon = ":/icons/status_connection_4"; break;
    }

    icon.append("_").append(sSheet);
    labelConnectionsIcon->setPixmap(GRC::ScaleStatusIcon(this, icon));

    if (n == 0)
    {
        setNumBlocks(0, 0); // Counts don't matter--just make it red
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
        labelBlocksIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_sync_stalled_" + sSheet));
        labelBlocksIcon->setToolTip(tr("Sync: no connections."));
        return;
    }

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
        labelBlocksIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_sync_done_" + sSheet));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        labelBlocksIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_sync_syncing_" + sSheet));
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

    updateMessageDialog.reset(new QMessageBox);

    updateMessageDialog->setWindowTitle(title);
    updateMessageDialog->setText(version);
    updateMessageDialog->setDetailedText(message);
    updateMessageDialog->setIcon(QMessageBox::Information);
    updateMessageDialog->setStandardButtons(QMessageBox::Ok);
    updateMessageDialog->setModal(false);
    connect(updateMessageDialog.get(), &QMessageBox::finished, [this](int) { updateMessageDialog.reset(); });
    // Due to slight delay in gui load this could appear behind the gui ui
    // The only other option available would make the message box stay on top of all applications

    QTimer::singleShot(5000, updateMessageDialog.get(), SLOT(show()));
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

void BitcoinGUI::resetblockchainClicked()
{
    QMessageBox Msg;

    Msg.setIcon(QMessageBox::Question);
    Msg.setText(tr("Do you want to delete blockchain data and sync from zero?"));
    Msg.setInformativeText(tr("Warning: After the blockchain data is deleted, the wallet will shutdown and when restarted will begin syncing from zero. Your balance will temporarily show as 0 GRC while syncing."));
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
        fResetBlockchainRequest = true;

        qApp->quit();
    }
}

void BitcoinGUI::diagnosticsClicked()
{
    diagnosticsDialog->show();
    diagnosticsDialog->raise();
    diagnosticsDialog->activateWindow();
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

void BitcoinGUI::peersClicked()
{
    if (rpcConsole != nullptr)
        rpcConsole->showPeersTab();
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
    connect(exportAction, SIGNAL(triggered()), addressBook, SLOT(exportClicked()));
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
        labelEncryptionIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_encryption_none_" + sSheet));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>not encrypted</b>!"));
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        if (fWalletUnlockStakingOnly) {
            labelEncryptionIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_encryption_unlocked_" + sSheet));
        } else {
            labelEncryptionIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_encryption_none_" + sSheet));
        }
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently %1 ").arg(fWalletUnlockStakingOnly ? tr("<b>unlocked for staking only</b>") : tr("<b>fully unlocked</b>")));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_encryption_locked_" + sSheet));
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
        if(!GRC::BackupWallet(*pwalletMain, FromQString(walletfilename))) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
    QString configfilename = QFileDialog::getSaveFileName(this, tr("Backup Config"), saveDir, tr("Wallet Config (*.conf)"));
    if(!configfilename.isEmpty()) {
        if(!GRC::BackupConfigFile(FromQString(configfilename))) {
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

void BitcoinGUI::updateGlobalStatus()
{
    LogPrint(BCLog::MISC, "BitcoinGUI::updateGlobalStatus()");

    // This is needed to prevent segfaulting due to early GUI initialization compared to core.
    if (miner_first_pass_complete)
    {
        TRY_LOCK(cs_main, locked_main);

        if (!locked_main) {
            return;
        }

        try
        {
            overviewPage->updateGlobalStatus();
            setNumConnections(clientModel->getNumConnections());

            QString warnings = clientModel->getStatusBarWarnings();

            if (!warnings.isEmpty())
            {
                statusbarAlertsLabel->setText(warnings);
                statusbarAlertsLabel->setVisible(true);
            }
            else
            {
                statusbarAlertsLabel->setVisible(false);
            }
        }
        catch(std::runtime_error &e)
        {
                LogPrintf("GENERAL RUNTIME ERROR!");
        }
    }
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

    nWeight = GRC::GetStakeWeight(*pwalletMain);
}

QString BitcoinGUI::GetEstimatedStakingFrequency(unsigned int nEstimateTime)
{
    QString text;

    if (!nEstimateTime)
    {
        text = tr("not available");

        return text;
    }

    // Start with 1/yr
    double frequency = 3600.0 * 24.0 * 365.0 / nEstimateTime;
    QString unit = tr("year");

    if (frequency >= 12.0 /* times per year */)
    {
        frequency /= 12.0;
        unit = tr("month");

        if (frequency >= 30.0 /* times per month */)
        {
            frequency /= 30.0;
            unit = tr("day");

            if (frequency >= 24.0 /* times per day */)
            {
                frequency /= 24.0;
                unit = tr("hour");
            }
        }
    }

    text = tr("%1 times per %2").arg(QString(RoundToString(frequency, 2).c_str())).arg(unit);

    return text;
}

void BitcoinGUI::updateStakingIcon()
{
    LogPrint(BCLog::MISC, "BitcoinGUI::updateStakingIcon()");

    QString estimated_staking_freq;

    const GlobalStatus::globalStatusType& globalStatus = g_GlobalStatus.GetGlobalStatus();

    estimated_staking_freq = GetEstimatedStakingFrequency(globalStatus.etts);

    if (globalStatus.staking)
    {
        labelStakingIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_staking_yes_" + sSheet));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br><b>Estimated</b> staking frequency is %3.")
                                     .arg(QString::number(globalStatus.coinWeight, 'f', 0))
                                     .arg(QString::number(globalStatus.netWeight, 'f', 0))
                                     .arg(estimated_staking_freq));

#ifdef Q_OS_MAC
        // If staking and app_nap_enabled, then disable appnap to ensure staking efficiency is maximized.
        if (app_nap_enabled)
        {
            m_app_nap_inhibitor->disableAppNap();
            app_nap_enabled = false;
        }
#endif
    }
    else if (!globalStatus.staking && !globalStatus.able_to_stake)
    {
        labelStakingIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_staking_problem_" + sSheet));
        //Part of this string won't be translated :(
        labelStakingIcon->setToolTip(tr("Unable to stake: %1")
                                     .arg(QString(globalStatus.ReasonNotStaking.c_str())));

#ifdef Q_OS_MAC
        // If not staking, not out of sync, and app nap disabled, enable app nap.
        if (!OutOfSyncByAge() && !app_nap_enabled)
        {
            m_app_nap_inhibitor->enableAppNap();
            app_nap_enabled = true;
        }
#endif
    }
    else
    {
        labelStakingIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_staking_no_" + sSheet));
        //Part of this string won't be translated :(
        labelStakingIcon->setToolTip(tr("Not staking currently: %1, <b>Estimated</b> staking frequency is %2.")
                                     .arg(QString(globalStatus.ReasonNotStaking.c_str()))
                                     .arg(estimated_staking_freq));

#ifdef Q_OS_MAC
        // If not staking, not out of sync, and app nap disabled, enable app nap.
        if (!OutOfSyncByAge() && !app_nap_enabled)
        {
            m_app_nap_inhibitor->enableAppNap();
            app_nap_enabled = true;
        }
#endif
    }
}


void BitcoinGUI::updateScraperIcon(int scraperEventtype, int status)
{
    LogPrint(BCLog::MISC, "BitcoinGUI::updateScraperIcon()");

    LOCK(cs_ConvergedScraperStatsCache);

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
        labelScraperIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_scraper_waiting_" + sSheet));
        labelScraperIcon->setToolTip(tr("Scraper: waiting on wallet to sync."));
    }
    else if (scraperEventtype == (int)scrapereventtypes::Sleep && status == CT_NEW)
    {
        labelScraperIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_scraper_inactive_" + sSheet));
        labelScraperIcon->setToolTip(tr("Scraper: superblock not needed - inactive."));
    }
    else if (scraperEventtype == (int)scrapereventtypes::Stats && (status == CT_NEW || status == CT_UPDATED || status == CT_UPDATING))
    {
        labelScraperIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_scraper_waiting_" + sSheet));
        labelScraperIcon->setToolTip(tr("Scraper: downloading and processing stats."));
    }
    else if ((scraperEventtype == (int)scrapereventtypes::Convergence  || scraperEventtype == (int)scrapereventtypes::SBContract)
             && (status == CT_NEW || status == CT_UPDATED) && nConvergenceTime)
    {
        labelScraperIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_scraper_ok_" + sSheet));

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
        labelScraperIcon->setPixmap(GRC::ScaleStatusIcon(this, ":/icons/status_scraper_no_convergence_" + sSheet));
        labelScraperIcon->setToolTip(tr("Scraper: No convergence able to be achieved. Will retry in a few minutes."));
    }

}

void BitcoinGUI::updateBeaconIcon()
{
    LogPrint(BCLog::MISC, "BitcoinGUI::updateBeaconIcon()");

    if (researcherModel->configuredForInvestorMode()
        || researcherModel->detectedPoolMode())
    {
        labelBeaconIcon->hide();
        return;
    }

    labelBeaconIcon->show();
    labelBeaconIcon->setPixmap(GRC::ScaleStatusIcon(this, researcherModel->getBeaconStatusIcon()));

    labelBeaconIcon->setToolTip(tr(
        "CPID: %1\n"
        "Beacon age: %2\n"
        "Expires: %3\n"
        "%4")
        .arg(researcherModel->formatCpid())
        .arg(researcherModel->formatBeaconAge())
        .arg(researcherModel->formatTimeToBeaconExpiration())
        .arg(researcherModel->formatBeaconStatus()));
}

// -----------------------------------------------------------------------------
// Class: ToolbarButtonIconFilter
// -----------------------------------------------------------------------------

ToolbarButtonIconFilter::ToolbarButtonIconFilter(
    QObject* parent,
    QIcon resting_icon,
    QIcon hover_icon)
    : QObject(parent)
    , m_resting_icon(std::move(resting_icon))
    , m_hover_icon(std::move(hover_icon))
{
}

void ToolbarButtonIconFilter::apply(
    QObject* parent,
    QAction* tool_action,
    QWidget* tool_button,
    const int icon_size,
    const QString& base_icon_path)
{
    const QPixmap inactive_pixmap = QIcon(base_icon_path).pixmap(icon_size, icon_size);
    const QPixmap active_pixmap = QIcon(base_icon_path + "_active").pixmap(icon_size, icon_size);

    QIcon hover_icon(active_pixmap);
    QIcon resting_icon;
    resting_icon.addPixmap(inactive_pixmap, QIcon::Normal, QIcon::Off);
    resting_icon.addPixmap(active_pixmap, QIcon::Normal, QIcon::On);

    tool_action->setIcon(resting_icon);
    tool_button->installEventFilter(new ToolbarButtonIconFilter(
        parent,
        std::move(resting_icon),
        std::move(hover_icon)));
}

bool ToolbarButtonIconFilter::eventFilter(QObject* target, QEvent* event)
{
    QToolButton* button = qobject_cast<QToolButton*>(target);

    if (!button || button->isChecked()) {
        return false;
    }

    if (event->type() == QEvent::Enter) {
        button->setIcon(m_hover_icon);
        return true;
    }

    if (event->type() == QEvent::Leave) {
        button->setIcon(m_resting_icon);
        return true;
    }

    return false;
}
