#ifndef BITCOIN_QT_BITCOINGUI_H
#define BITCOIN_QT_BITCOINGUI_H

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
class VotingModel;
class TransactionView;
class OverviewPage;
class FavoritesPage;
class ReceiveCoinsPage;
class SendCoinsDialog;
class VotingPage;
class SignVerifyMessageDialog;
class Notificator;
class RPCConsole;
class DiagnosticsDialog;
class ClickLabel;

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
    explicit BitcoinGUI(QWidget* parent = nullptr);

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

    /** Set the voting model.
        The voting model facilitates presentation of and interaction with network polls and votes.
    */
    void setVotingModel(VotingModel *votingModel);

    /**
     * @brief Queries the state of privacy mode (mask values on overview screen).
     * @return boolean of the mask values state
     */
    bool isPrivacyModeActivated() const;

protected:
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private:
    ClientModel *clientModel;
    WalletModel *walletModel;
    ResearcherModel *researcherModel;
    VotingModel *votingModel;

    QStackedWidget *centralWidget;

    OverviewPage *overviewPage;
    FavoritesPage *addressBookPage;
    ReceiveCoinsPage *receiveCoinsPage;
    SendCoinsDialog *sendCoinsPage;
    TransactionView *transactionView;
    VotingPage *votingPage;
    SignVerifyMessageDialog *signVerifyMessageDialog;
    std::unique_ptr<QMessageBox> updateMessageDialog;

    QLabel *statusbarAlertsLabel;
    QLabel *labelEncryptionIcon;
    QLabel *labelStakingIcon;
    ClickLabel *labelConnectionsIcon;
    QLabel *labelBlocksIcon;
    QLabel *labelScraperIcon;
    ClickLabel *labelBeaconIcon;

    // Windows and Linux: collapse the main application's menu bar into a menu
    // button. On macOS, we'll continue to use the system's separate menu bar.
#ifdef Q_OS_MAC
    QMenuBar *appMenuBar;
#else
    QMenu *appMenuBar;
#endif
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
    QAction *resetblockchainAction;
    QAction *m_mask_values_action;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    Notificator *notificator;
    RPCConsole *rpcConsole;
    DiagnosticsDialog *diagnosticsDialog;

    QMovie *syncIconMovie;

    uint64_t nWeight;

#ifdef Q_OS_MAC
    CAppNapInhibitor* m_app_nap_inhibitor = nullptr;
#endif
    // name extension to change icons according to stylesheet
    QString sSheet;

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
    /** Set the difficulty of the block at the chain tip. */
    void setDifficulty(double difficulty);
    /** Set staking related information. */
    void setMinerStatus(bool staking, double net_weight, double coin_weight, double etts_days);
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
    /** Switch the active light/dark theme */
    void themeToggled();
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
    void resetblockchainClicked();
    void setPrivacy();
    bool tryQuit();

#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif
    /** Show incoming transaction notification for new transactions.

        The new items are those between start and end inclusive, under the given parent item.
    */
    void incomingTransaction(const QModelIndex & parent, int start, int end);
    /** Encrypt the wallet */
    void encryptWallet();
    /** Backup the wallet */
    void backupWallet();
    /** Change encrypted wallet passphrase */
    void changePassphrase();
    /** Ask for passphrase to unlock wallet temporarily */
    void unlockWallet();

    void lockWallet();

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);

    void updateWeight();
    void updateStakingIcon(bool staking, double net_weight, double coin_weight, double etts_days);
    void updateScraperIcon(int scraperEventtype, int status);
    void updateBeaconIcon();

    QString GetEstimatedStakingFrequency(unsigned int nEstimateTime);

    void handleNewPoll();
    void handleExpiredPoll();
};

//!
//! \brief Sets up and toggles hover states for the main toolbar icons.
//!
//! Qt stylesheets do not provide a way to manage QToolButton resting, active
//! and hover states for the button icons in a way that scales for a high-DPI
//! desktop. This class provides an event filter implementation that switches
//! the tool button icons in response to hover events.
//!
class ToolbarButtonIconFilter : public QObject
{
    Q_OBJECT

public:
    explicit ToolbarButtonIconFilter(QObject* parent, QIcon resting_icon, QIcon hover_icon);
    virtual ~ToolbarButtonIconFilter() { }

    static void apply(
        QObject* parent,
        QAction* tool_action,
        QWidget* tool_button,
        const int icon_size,
        const QString& base_icon_path);

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QIcon m_resting_icon;
    QIcon m_hover_icon;
}; // ToolbarButtonIconFilter

#endif // BITCOIN_QT_BITCOINGUI_H
