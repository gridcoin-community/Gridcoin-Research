#ifndef RPCCONSOLE_H
#define RPCCONSOLE_H

#include "qt/guiutil.h"
#include "qt/peertablemodel.h"

#include "net.h"

#include <QDialog>
#include <QWidget>
#include <QCompleter>
#include <QMenu>

namespace Ui {
    class RPCConsole;
}
class ClientModel;

QT_BEGIN_NAMESPACE
class QMenu;
class QItemSelection;
QT_END_NAMESPACE

/** Local Bitcoin RPC console. */
class RPCConsole: public QDialog
{
    Q_OBJECT

public:
    explicit RPCConsole(QWidget *parent = 0);
    ~RPCConsole();

    void setClientModel(ClientModel *model);

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

private:
    Ui::RPCConsole *ui;
    ClientModel *clientModel;
    QStringList history;
    int historyPtr = 0;
    QList<NodeId> cachedNodeids;
    QMenu *peersTableContextMenu = nullptr;
    QMenu *banTableContextMenu = nullptr;
    QCompleter *autoCompleter;
    static QString FormatBytes(quint64 bytes);
    void setTrafficGraphRange(int mins);
    /** show detailed information on ui about selected node */
    void updateNodeDetail(const CNodeCombinedStats *stats);

    void startExecutor();

    enum ColumnWidths
    {
        NETNODEID_COLUMN_WIDTH = 50,
        ADDRESS_COLUMN_WIDTH = 130,
        PING_COLUMN_WIDTH = 50,
        SENT_COLUMN_WIDTH = 50,
        RECEIVED_COLUMN_WIDTH = 60,
        SUBVERSION_COLUMN_WIDTH = 130,
        BANSUBNET_COLUMN_WIDTH = 130,
        BANTIME_COLUMN_WIDTH = 130
    };

};

#endif // RPCCONSOLE_H
