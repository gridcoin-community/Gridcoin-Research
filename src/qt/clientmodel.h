#ifndef BITCOIN_QT_CLIENTMODEL_H
#define BITCOIN_QT_CLIENTMODEL_H

#include "interfaces/node.h"

#include <QObject>

#include <atomic>
#include <memory>
#include <vector>

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class BanTableModel;
class PeerTableModel;

namespace interfaces {
class Handler;
class StakingStatus;
} // namespace interfaces

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

/** Model for Bitcoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT
public:
    explicit ClientModel(interfaces::Node& node,
                         interfaces::StakingStatus& staking_status,
                         OptionsModel* optionsModel,
                         QObject* parent = nullptr);
    ~ClientModel();

    interfaces::Node& node() const { return m_node; }
    OptionsModel *getOptionsModel();
    PeerTableModel *getPeerTableModel();
    BanTableModel *getBanTableModel();

    int getNumConnections() const;
    int getNumBlocks() const;
    quint64 getTotalBytesRecv() const;
    quint64 getTotalBytesSent() const;

    QDateTime getLastBlockDate() const;

    //! Return true if client connected to testnet
    bool isTestNet() const;
    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;
    //! Return conservative estimate of total number of blocks, or 0 if unknown
    int getNumBlocksOfPeers() const;
    //! Return the difficulty of the block at the chain tip.
    double getDifficulty() const;
    //! Return estimated network staking weight from the average of recent blocks.
    double getNetWeight() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;
    //! Get miner and staking status warnings
    QString getMinerWarnings() const;

    //! Re-feed the current staking state and coin weight to the overview
    //! (used when a GUI-side change, e.g. privacy mode, must refresh the
    //! staking fields normally pushed from the core).
    void refreshMinerStatus();

    QString formatFullVersion() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

    QString formatBoostVersion() const;

    //! Value snapshot of the scraper convergence status (no locks cross this
    //! call; the node side takes the scraper cache lock internally).
    interfaces::ScraperConvergenceSnapshot getScraperConvergenceSnapshot() const;
private:
    interfaces::Node& m_node;
    interfaces::StakingStatus& m_staking_status;
    OptionsModel *optionsModel;
    PeerTableModel *peerTableModel;
    BanTableModel *banTableModel;

    mutable std::atomic<int> m_cached_num_blocks;
    mutable std::atomic<int> m_cached_num_blocks_of_peers;
    mutable std::atomic<int64_t> m_cached_best_block_time;
    mutable std::atomic<double> m_cached_difficulty;
    mutable std::atomic<double> m_cached_net_weight;
    mutable std::atomic<double> m_cached_etts_days;

    QTimer *pollTimer;

    //! Notification registrations held so they are disconnected on
    //! destruction. Each callback captures `this`; leaving them connected
    //! risked a core signal invoking a slot on a destroyed ClientModel during
    //! shutdown. interfaces::Handler disconnects on destruction.
    std::vector<std::unique_ptr<interfaces::Handler>> m_handlers;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
signals:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, int countOfPeers);
    void difficultyChanged(double difficulty);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);
    void minerStatusChanged(bool staking, double netWeight, double coinWeight, double etts_days);
    void updateScraperLog(QString message);
    void updateScraperStatus(int ScraperEventtype, int status);

    //! The PSGT pool (#2910) changed. change_type is a ui_interface ChangeType
    //! (CT_NEW / CT_UPDATED / CT_DELETED) narrowed to quint8 for the queued
    //! connection; the affected revision hash is carried for reference. reason is
    //! a PSGTRemovalReason code (as int) on a removal (CT_DELETED) and -1 for
    //! CT_NEW / CT_UPDATED -- consumers must read it only on CT_DELETED.
    void psgtPoolChanged(QString revision_hash, quint8 change_type, int reason);

    //! Asynchronous error notification
    void error(const QString &title, const QString &message, bool modal);

public slots:
    void updateNumBlocks(int height, int64_t best_time, uint32_t target_bits);
    void updateTimer();
    void updateBanlist();
    void updateNumConnections(int numConnections);
    void updateAlert(const QString &hash, int status);
    void updateMinerStatus(bool staking, double coin_weight);
    void updateScraper(int scraperEventtype, int status, const QString message);
    void updatePSGTPool(const QString &revision_hash, int status, int reason);
};

#endif // BITCOIN_QT_CLIENTMODEL_H
