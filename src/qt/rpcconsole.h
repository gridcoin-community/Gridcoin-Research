#ifndef BITCOIN_QT_RPCCONSOLE_H
#define BITCOIN_QT_RPCCONSOLE_H

#include "interfaces/node.h"
#include "qt/guiutil.h"
#include "qt/peertablemodel.h"
#include "qt/walletmodel.h"


#include <QDialog>
#include <QWidget>
#include <QCompleter>
#include <QMenu>
#include <QString>

#include <memory>
#include <string>
#include <vector>

namespace Ui {
    class RPCConsole;
}
class ClientModel;

QT_BEGIN_NAMESPACE
class QMenu;
class QItemSelection;
class QThread;
QT_END_NAMESPACE

namespace RPCConsoleDetail {
//! First token of a command line, seeing through one layer of quoting. Used
//! when the line does not parse, where there is no argv to consult.
std::string LeadingCommandToken(const QString& command);

//! The form of a console command line that is safe to echo, log or keep in
//! history: "<command> (arguments not shown)" when the command's arguments
//! are secret, otherwise the line unchanged.
QString RedactSensitiveArgs(const QString& command);

//! Run a parsed console command (args[0] is the method; an empty args runs
//! nothing). The passphrase methods (interfaces::RpcConsolePassphraseIndexes) go
//! to executeRpcConsoleSecretCommand with their passphrases copied into
//! SecureStrings, and every std::string in args that held a passphrase is
//! cleansed in place (its size kept) before return, a throw included. Their
//! result never has wallet_unlock_needed set: the command carries its own
//! passphrase, so the console does not keep it to run again under an unlock.
//! A passphrase longer than one arena of the secure pool holds
//! (LockedPool::ARENA_SIZE - 1 bytes) is refused before any SecureString is
//! built or the node is called: ok = false, output "Error: <method>: passphrase
//! too long", and the args are still cleansed. Other methods go to
//! executeRpcConsoleCommand.
interfaces::RpcConsoleResult DispatchConsoleCommand(interfaces::Node& node, std::vector<std::string>& args);
} // namespace RPCConsoleDetail

/** Local Bitcoin RPC console. */
class RPCConsole: public QDialog
{
    Q_OBJECT

public:
    explicit RPCConsole(QWidget* parent = nullptr);
    ~RPCConsole();

    void setClientModel(ClientModel *model);

    //! The wallet a command refused for its keys is authorised through (see
    //! executorFinished()). Null on teardown, which ends a capability still
    //! held for a command run again.
    void setWalletModel(WalletModel *model);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

protected:
    virtual bool eventFilter(QObject* obj, QEvent *event);

private slots:
    void on_lineEdit_returnPressed();
    void on_tabWidget_currentChanged(int index);
    /** open the debug.log from the current datadir */
    void on_openDebugLogfileButton_clicked();
    /** display messagebox with program parameters (same as bitcoin-qt --help) */
    void on_showCLOptionsButton_clicked();
    /** change the time range of the network traffic graph */
    void on_graphRangeSlider_valueChanged(int value);
    /** update traffic statistics */
    void updateTrafficStats(quint64 totalBytesIn, quint64 totalBytesOut);
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);
    /** clear traffic graph */
    void on_clearTrafficGraphButton_clicked();
    /** Show custom context menu on Peers tab */
    void showPeersTableContextMenu(const QPoint& point);
    /** Show custom context menu on Bans tab */
    void showBanTableContextMenu(const QPoint& point);
    /** Hides ban table if no bans are present */
    void showOrHideBanTableIfRequired();
    /** clear the selected node */
    void clearSelectedNode();
    //! The executor reports \p command refused for the wallet's keys, with
    //! \p error its reply. Acted on when the request has finished.
    void commandNeedsUnlock(const QString &command, const QString &error);
    //! The executor has finished a request or a run again.
    void executorFinished();

public slots:
    void clear();
    void message(int category, const QString &message, bool html = false);
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set number of blocks shown in the UI */
    void setNumBlocks(int count, int countOfPeers);
    /** Push scraper log entry to scraper log text box */
    void displayScraperLogMessage(const QString& string);
    /** Go forward or back in history */
    void browseHistory(int offset);
    /** Scroll console view to end */
    void scrollToEnd();
    /** Handle selection of peer in peers list */
    void peerSelected(const QItemSelection &selected, const QItemSelection &deselected);
    /** Handle selection caching before update */
    void peerLayoutAboutToChange();
    /** Handle updated peer information */
    void peerLayoutChanged();
    /** Disconnect a selected node on the Peers tab */
    void disconnectSelectedNode();
    /** Ban a selected node on the Peers tab */
    void banSelectedNode(int bantime);
    /** Unban a selected node on the Bans tab */
    void unbanSelectedNode();
    /** Show peer tab */
    void showPeersTab();


signals:
    // For RPC command executor
    void stopExecutor();
    void cmdRequest(const QString &command);
    //! Run a command again, once, under the capability just begun for it.
    void cmdRetry(const QString &command);

private:
    Ui::RPCConsole *ui;
    ClientModel *clientModel;
    QStringList history;
    int historyPtr = 0;
    QList<int64_t> cachedNodeids;
    QMenu *peersTableContextMenu = nullptr;
    QMenu *banTableContextMenu = nullptr;
    QCompleter *autoCompleter;
    static QString FormatBytes(quint64 bytes);
    void setTrafficGraphRange(int mins);
    /** show detailed information on ui about selected node */
    void updateNodeDetail(const interfaces::PeerInfo *stats);

    void startExecutor();
    /** Stop the executor thread synchronously (quit + wait). Called when the
        client model is cleared at shutdown, so a queued/in-flight command can
        no longer dereference the interfaces::Node after it is destroyed. */
    void stopExecutorThread();

    //! The executor's worker thread, non-null once startExecutor() has run.
    QThread *m_executorThread = nullptr;

    WalletModel *walletModel = nullptr;

    //! Requests sent to the executor that it has not finished.
    int m_commands_in_flight = 0;

    //! The last finished request was refused for the wallet's keys
    //! (commandNeedsUnlock()); its command and its error reply.
    bool m_unlock_pending = false;
    QString m_unlock_command;
    QString m_unlock_error;

    //! The capability for a command being run again, held until the executor
    //! has finished it. The input is disabled meanwhile, so nothing else runs
    //! under it.
    std::unique_ptr<WalletModel::UnlockContext> m_retry_unlock;

    enum ColumnWidths
    {
        NETNODEID_COLUMN_WIDTH = 60,
        ADDRESS_COLUMN_WIDTH = 200,
        PING_COLUMN_WIDTH = 80,
        SENT_COLUMN_WIDTH = 80,
        RECEIVED_COLUMN_WIDTH = 80,
        SUBVERSION_COLUMN_WIDTH = 130,
        BANSUBNET_COLUMN_WIDTH = 150,
        BANTIME_COLUMN_WIDTH = 150
    };

};

#endif // RPCCONSOLE_H
