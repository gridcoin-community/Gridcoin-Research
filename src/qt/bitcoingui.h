#ifndef BITCOINGUI_H
#define BITCOINGUI_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <stdint.h>
#include <memory>
#include "guiconstants.h"

#ifdef Q_OS_MAC
#include <qt/macos_appnap.h>
#endif

class TransactionTableModel;
class ClientModel;
class WalletModel;
class ResearcherModel;
class TransactionView;
class OverviewPage;
class AddressBookPage;
class SendCoinsDialog;
class VotingDialog;
class SignVerifyMessageDialog;
class Notificator;
class RPCConsole;
class DiagnosticsDialog;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QTableView;
class QAbstractItemModel;
class QModelIndex;
class QStackedWidget;
class QUrl;
class QMessageBox;
QT_END_NAMESPACE

/**
  Bitcoin GUI main class. This class represents the main window of the Bitcoin UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class BitcoinGUI : public QMainWindow
{
    Q_OBJECT
public:
    explicit BitcoinGUI(QWidget *parent = 0);

    ~BitcoinGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel *clientModel);
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    void setWalletModel(WalletModel *walletModel);

    /** Set the researcher model.
        The researcher model provides the BOINC context for the research reward system.
    */
    void setResearcherModel(ResearcherModel *researcherModel);

protected:
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private:
    ClientModel *clientModel;
    WalletModel *walletModel;
    ResearcherModel *researcherModel;

    QStackedWidget *centralWidget;

    OverviewPage *overviewPage;
    QWidget *transactionsPage;
    AddressBookPage *addressBookPage;
    AddressBookPage *receiveCoinsPage;
    SendCoinsDialog *sendCoinsPage;
    VotingDialog *votingPage;
    SignVerifyMessageDialog *signVerifyMessageDialog;
    std::unique_ptr<QMessageBox> updateMessageDialog;

    QLabel *labelEncryptionIcon;
    QLabel *labelStakingIcon;
    QLabel *labelConnectionsIcon;
    QLabel *labelBlocksIcon;
    QLabel *labelScraperIcon;
    QLabel *labelBeaconIcon;

    QMenuBar *appMenuBar;
    QAction *overviewAction;
    QAction *historyAction;
    QAction *quitAction;
    QAction *sendCoinsAction;
    QAction *addressBookAction;
    QAction *signMessageAction;
    QAction *bxAction;
    QAction *websiteAction;
    QAction *boincAction;
    QAction *chatAction;
    QAction *exchangeAction;
    QAction *votingAction;
    QAction *diagnosticsAction;
    QAction *verifyMessageAction;
    QAction *aboutAction;
    QAction *receiveCoinsAction;
    QAction *researcherAction;
    QAction *optionsAction;
    QAction *openConfigAction;
    QAction *toggleHideAction;
    QAction *exportAction;
    QAction *encryptWalletAction;
    QAction *backupWalletAction;
    QAction *changePassphraseAction;
    QAction *unlockWalletAction;
    QAction *lockWalletAction;
    QAction *openRPCConsoleAction;
    QAction *snapshotAction;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    Notificator *notificator;
    TransactionView *transactionView;
    RPCConsole *rpcConsole;
    DiagnosticsDialog *diagnosticsDialog;

    QMovie *syncIconMovie;

    uint64_t nWeight;

#ifdef Q_OS_MAC
    CAppNapInhibitor* m_app_nap_inhibitor = nullptr;
    bool app_nap_enabled = true;
#endif
    // name extension to change icons according to stylesheet
    QString sSheet;

    int STATUSBAR_ICONSIZE = UNSCALED_STATUSBAR_ICONSIZE * logicalDpiX() / 96;

    /** Create the main UI actions. */
    void createActions();
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray (notification) icon */
    void createTrayIcon();
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();
    /** Set Icons */
    void setIcons();


public slots:
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set number of blocks shown in the UI */
    void setNumBlocks(int count, int nTotalBlocks);
    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    /** Notify the user if there is an update available */
    void update(const QString& title, const QString& version, const QString& message);

    /** Notify the user of an error in the network or transaction handling code. */
    void error(const QString &title, const QString &message, bool modal);
    /** Asks the user whether to pay the transaction fee or to cancel the transaction.
       It is currently not possible to pass a return value to another thread through
       BlockingQueuedConnection, so an indirected pointer is used.
       https://bugreports.qt-project.org/browse/QTBUG-10440

      @param[in] nFeeRequired       the required fee
      @param[out] payFee            true to pay the fee, false to not pay the fee
    */
    void askFee(qint64 nFeeRequired, bool *payFee);

    void askQuestion(std::string caption, std::string body, bool *result);

    void handleURI(QString strURI);
    void setOptionsStyleSheet(QString qssFileName);

private slots:
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to address book page */
    void gotoAddressBookPage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage();
    /** Switch to voting page */
    void gotoVotingPage();

    /** Show Sign/Verify Message dialog and switch to sign message tab */
    void gotoSignMessageTab(QString addr = "");
    /** Show Sign/Verify Message dialog and switch to verify message tab */
    void gotoVerifyMessageTab(QString addr = "");

    /** Show configuration dialog */
    void optionsClicked();
    /** Show researcher/beacon configuration dialog */
    void researcherClicked();
    /** Show about dialog */
    void aboutClicked();
    /** Open config file */
    void openConfigClicked();

    void bxClicked();
    void websiteClicked();
    void exchangeClicked();
    void boincClicked();
    void boincStatsClicked();
    void chatClicked();
    void diagnosticsClicked();
    void peersClicked();
    void snapshotClicked();

#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif
    /** Show incoming transaction notification for new transactions.

        The new items are those between start and end inclusive, under the given parent item.
    */
    void incomingTransaction(const QModelIndex & parent, int start, int end);
    /** Encrypt the wallet */
    void encryptWallet(bool status);
    /** Backup the wallet */
    void backupWallet();
    /** Change encrypted wallet passphrase */
    void changePassphrase();
    /** Ask for passphrase to unlock wallet temporarily */
    void unlockWallet();

    void lockWallet();

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);
    /** simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    void updateWeight();
    void updateStakingIcon();
    void updateScraperIcon(int scraperEventtype, int status);
    void updateBeaconIcon();

    QString GetEstimatedStakingFrequency(unsigned int nEstimateTime);

    void updateGlobalStatus();
};

#endif
