#ifndef CLIENTMODEL_H
#define CLIENTMODEL_H

#include <QObject>

#include <atomic>

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class BanTableModel;
class PeerTableModel;

struct ConvergedScraperStats;
class CWallet;

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

/** Model for Bitcoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT
public:
    explicit ClientModel(OptionsModel* optionsModel, QObject* parent = nullptr);
    ~ClientModel();

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

    QString formatFullVersion() const;
    QString formatBuildDate() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

    QString formatBoostVersion()  const;
    const ConvergedScraperStats& getConvergedScraperStatsCache() const;
private:
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
};

#endif // CLIENTMODEL_H
