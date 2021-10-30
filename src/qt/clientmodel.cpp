#include "clientmodel.h"
#include "qt/bantablemodel.h"
#include "qt/peertablemodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include "alert.h"
#include "clientversion.h"
#include "main.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/superblock.h"
#include "node/ui_interface.h"
#include "util.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

static const int64_t nClientStartupTime = GetTime();
extern ConvergedScraperStats ConvergedScraperStatsCache;

ClientModel::ClientModel(OptionsModel *optionsModel, QObject *parent)
    : QObject(parent)
    , optionsModel(optionsModel)
    , peerTableModel(nullptr)
    , banTableModel(nullptr)
    , m_cached_num_blocks(-1)
    , m_cached_num_blocks_of_peers(-1)
    , m_cached_best_block_time(-1)
    , m_cached_difficulty(-1)
    , m_cached_net_weight(-1)
    , m_cached_etts_days(0)
    , pollTimer(nullptr)
{
    peerTableModel = new PeerTableModel(this);
    banTableModel = new BanTableModel(this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(MODEL_UPDATE_DELAY);
    pollTimer->start();
    connect(pollTimer, &QTimer::timeout, this, &ClientModel::updateTimer);

    subscribeToCoreSignals();
}

ClientModel::~ClientModel()
{
    unsubscribeFromCoreSignals();
}

int ClientModel::getNumConnections() const
{
    LOCK(cs_vNodes);
    return vNodes.size();
}

int ClientModel::getNumBlocks() const
{
    if (m_cached_num_blocks == -1) {
        LOCK(cs_main);
        m_cached_num_blocks = nBestHeight;
    }

    return m_cached_num_blocks;
}

int ClientModel::getNumBlocksOfPeers() const
{
    if (m_cached_num_blocks_of_peers == -1) {
        m_cached_num_blocks_of_peers = GetNumBlocksOfPeers();
    }

    return m_cached_num_blocks_of_peers;
}

double ClientModel::getDifficulty() const
{
    if (m_cached_difficulty == -1) {
        LOCK(cs_main);
        m_cached_difficulty = GRC::GetCurrentDifficulty();
    }

    return m_cached_difficulty;
}

double ClientModel::getNetWeight() const
{
    if (m_cached_net_weight == -1) {
        LOCK(cs_main);
        m_cached_net_weight = GRC::GetEstimatedNetworkWeight() / 80.0;
    }

    return m_cached_net_weight;
}

quint64 ClientModel::getTotalBytesRecv() const
{
    return CNode::GetTotalBytesRecv();
}

quint64 ClientModel::getTotalBytesSent() const
{
    return CNode::GetTotalBytesSent();
}

QDateTime ClientModel::getLastBlockDate() const
{
    if (m_cached_best_block_time != -1) {
        return QDateTime::fromTime_t(m_cached_best_block_time);
    }

    LOCK(cs_main);

    if (pindexBest) {
        return QDateTime::fromTime_t(pindexBest->GetBlockTime());
    }

    return QDateTime::fromTime_t(1413033777); // Genesis block's time
}

void ClientModel::updateTimer()
{
	if (gArgs.GetArg("-suppressnetworkgraph", "false") != "true")
	{
		emit bytesChanged(getTotalBytesRecv(), getTotalBytesSent());
	}
}

void ClientModel::updateNumBlocks(int height, int64_t best_time, uint32_t target_bits)
{
    m_cached_num_blocks = height;
    m_cached_best_block_time = best_time;
    m_cached_difficulty = GRC::GetBlockDifficulty(target_bits);

    {
        TRY_LOCK(cs_main, lockMain);

        if (lockMain) {
            m_cached_num_blocks_of_peers = GetNumBlocksOfPeers();
            m_cached_net_weight = GRC::GetEstimatedNetworkWeight() / 80.0;
        }
    }

    emit difficultyChanged(getDifficulty());
    emit numBlocksChanged(getNumBlocks(), getNumBlocksOfPeers());
}

void ClientModel::updateNumConnections(int numConnections)
{
    emit numConnectionsChanged(numConnections);
}

void ClientModel::updateAlert(const QString &hash, int status)
{
    // Show error message notification for new alert
    if(status == CT_NEW)
    {
        uint256 hash_256;
        hash_256.SetHex(hash.toStdString());
        CAlert alert = CAlert::getAlertByHash(hash_256);
        if(!alert.IsNull())
        {
            emit error(tr("Network Alert"), QString::fromStdString(alert.strStatusBar), false);
        }
    }

    // Emit a numBlocksChanged when the status message changes,
    // so that the view recomputes and updates the status bar.
    emit numBlocksChanged(getNumBlocks(), getNumBlocksOfPeers());
}

void ClientModel::updateMinerStatus(bool staking, double coin_weight)
{
    if (staking) {
        TRY_LOCK(cs_main, lockMain);

        if (lockMain) {
            m_cached_etts_days = GRC::GetEstimatedTimetoStake();
        }
    }

    emit minerStatusChanged(staking, getNetWeight(), coin_weight, m_cached_etts_days);
}

void ClientModel::updateScraper(int scraperEventtype, int status, const QString message)
{
    if (scraperEventtype == (int)scrapereventtypes::Log)
        emit updateScraperLog(message);
    else
        emit updateScraperStatus(scraperEventtype, status);
}

// Requires a lock on cs_ConvergedScraperStatsCache
const ConvergedScraperStats& ClientModel::getConvergedScraperStatsCache() const
{
    return ConvergedScraperStatsCache;
}

bool ClientModel::isTestNet() const
{
    return fTestNet;
}

bool ClientModel::inInitialBlockDownload() const
{
    return IsInitialBlockDownload();
}

QString ClientModel::getStatusBarWarnings() const
{
    QString warnings = QString::fromStdString(GetWarnings("statusbar"));

    if (getDifficulty() < 0.1) {
        if (!warnings.isEmpty()) warnings += "; ";
        warnings += tr("Low difficulty!; ");
    }

    if (!g_miner_status.StakingActive()) {
        warnings += tr("Miner: ");
        warnings += getMinerWarnings();
    }

    return warnings;
}

QString ClientModel::getMinerWarnings() const
{
    return QString::fromStdString(g_miner_status.FormatErrors());
}

OptionsModel *ClientModel::getOptionsModel()
{
    return optionsModel;
}

PeerTableModel *ClientModel::getPeerTableModel()
{
    return peerTableModel;
}

BanTableModel *ClientModel::getBanTableModel()
{
    return banTableModel;
}

QString ClientModel::formatFullVersion() const
{
    return QString::fromStdString(FormatFullVersion());
}

QString ClientModel::clientName() const
{
    return QString::fromStdString(CLIENT_NAME);
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromTime_t(nClientStartupTime).toString();
}

QString ClientModel::formatBoostVersion()  const
{
	//6-10-2014: R Halford: Updating Boost version to 1.5.5 to prevent sync issues; print the boost version to verify:
		std::ostringstream s;
		s << "Using Boost "
          << BOOST_VERSION / 100000     << "."  // major version
          << BOOST_VERSION / 100 % 1000 << "."  // minior version
          << BOOST_VERSION % 100;                // patch level
		return QString::fromStdString(s.str());
}

void ClientModel::updateBanlist()
{
    banTableModel->refresh();
}


// Handlers for core signals
static void NotifyBlocksChanged(
    ClientModel *clientmodel,
    bool syncing,
    int height,
    int64_t best_time,
    uint32_t target_bits)
{
    // Avoid spamming update events during the initial block download. A node
    // can trigger this signal hundreds of times per second.
    //
    if (syncing) {
        static int64_t last_update_ms = 0; // Thread synchronization not needed.
        const int64_t now = GetTimeMillis();

        if (last_update_ms + MODEL_UPDATE_DELAY > now) {
            return;
        }

        last_update_ms = now;
    }

    QMetaObject::invokeMethod(clientmodel, "updateNumBlocks", Qt::QueuedConnection,
                              Q_ARG(int, height),
                              Q_ARG(int64_t, best_time),
                              Q_ARG(uint32_t, target_bits));
}

static void NotifyNumConnectionsChanged(ClientModel *clientmodel, int newNumConnections)
{
    LogPrint(BCLog::NOISY, "NotifyNumConnectionsChanged %i", newNumConnections);
    QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
                              Q_ARG(int, newNumConnections));
}

static void NotifyAlertChanged(ClientModel *clientmodel, const uint256 &hash, ChangeType status)
{
    LogPrintf("NotifyAlertChanged %s status=%i", hash.GetHex(), status);
    QMetaObject::invokeMethod(clientmodel, "updateAlert", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(hash.GetHex())),
                              Q_ARG(int, status));
}

static void BannedListChanged(ClientModel *clientmodel)
{
    qDebug() << QString("%1: Requesting update for peer banlist").arg(__func__);
    QMetaObject::invokeMethod(clientmodel, "updateBanlist", Qt::QueuedConnection);
}


static void NotifyScraperEvent(ClientModel *clientmodel, const scrapereventtypes& ScraperEventtype, ChangeType status, const std::string& message)
{
    QMetaObject::invokeMethod(clientmodel, "updateScraper", Qt::QueuedConnection,
                              Q_ARG(int, (int)ScraperEventtype),
                              Q_ARG(int, status),
                              Q_ARG(QString, QString::fromStdString(message)));
}

static void MinerStatusChanged(ClientModel *clientmodel, bool staking, double coin_weight)
{
    qDebug() << QString("%1").arg(__func__);
    QMetaObject::invokeMethod(clientmodel, "updateMinerStatus", Qt::QueuedConnection,
                              Q_ARG(bool, staking),
                              Q_ARG(double, coin_weight));
}

void ClientModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.NotifyBlocksChanged_connect(boost::bind(NotifyBlocksChanged, this,
                                                      boost::placeholders::_1, boost::placeholders::_2,
                                                      boost::placeholders::_3, boost::placeholders::_4));
    uiInterface.BannedListChanged_connect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyNumConnectionsChanged_connect(boost::bind(NotifyNumConnectionsChanged, this,
                                                              boost::placeholders::_1));
    uiInterface.NotifyAlertChanged_connect(boost::bind(NotifyAlertChanged, this,
                                                     boost::placeholders::_1, boost::placeholders::_2));
    uiInterface.NotifyScraperEvent_connect(boost::bind(NotifyScraperEvent, this,
                                                     boost::placeholders::_1, boost::placeholders::_2,
                                                     boost::placeholders::_3));
    uiInterface.MinerStatusChanged_connect(boost::bind(MinerStatusChanged, this,
                                                     boost::placeholders::_1, boost::placeholders::_2));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
}
