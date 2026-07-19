#include "clientmodel.h"
#include "qt/guilog.h"
#include "qt/bantablemodel.h"
#include "qt/peertablemodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include "clientversion.h"
#include "interfaces/handler.h"
#include "interfaces/staking.h"
#include "logging.h"
#include "util/time.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

#include <boost/version.hpp>

#include <sstream>

static const int64_t nClientStartupTime = GetTime();

ClientModel::ClientModel(interfaces::Node& node,
                         interfaces::StakingStatus& staking_status,
                         OptionsModel *optionsModel,
                         QObject *parent)
    : QObject(parent)
    , m_node(node)
    , m_staking_status(staking_status)
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
    banTableModel = new BanTableModel(m_node, this);
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
    return m_node.getNodeCount();
}

int ClientModel::getNumBlocks() const
{
    if (m_cached_num_blocks == -1) {
        m_cached_num_blocks = m_node.getNumBlocks();
    }

    return m_cached_num_blocks;
}

int ClientModel::getNumBlocksOfPeers() const
{
    if (m_cached_num_blocks_of_peers == -1) {
        m_cached_num_blocks_of_peers = m_node.getNumBlocksOfPeers();
    }

    return m_cached_num_blocks_of_peers;
}

double ClientModel::getDifficulty() const
{
    if (m_cached_difficulty == -1) {
        m_cached_difficulty = m_node.getDifficulty();
    }

    return m_cached_difficulty;
}

double ClientModel::getNetWeight() const
{
    if (m_cached_net_weight == -1) {
        m_cached_net_weight = m_staking_status.getNetworkWeight() / 80.0;
    }

    return m_cached_net_weight;
}

quint64 ClientModel::getTotalBytesRecv() const
{
    return m_node.getTotalBytesRecv();
}

quint64 ClientModel::getTotalBytesSent() const
{
    return m_node.getTotalBytesSent();
}

QDateTime ClientModel::getLastBlockDate() const
{
    if (m_cached_best_block_time != -1) {
        return QDateTime::fromSecsSinceEpoch(m_cached_best_block_time);
    }

    if (const int64_t tip_time = m_node.getLastBlockTime()) {
        return QDateTime::fromSecsSinceEpoch(tip_time);
    }

    return QDateTime::fromSecsSinceEpoch(1413033777); // Genesis block's time
}

void ClientModel::updateTimer()
{
    if (!optionsModel || !optionsModel->getSuppressNetworkGraph())
    {
        emit bytesChanged(getTotalBytesRecv(), getTotalBytesSent());
    }
}

void ClientModel::updateNumBlocks(int height, int64_t best_time, uint32_t target_bits)
{
    m_cached_num_blocks = height;
    m_cached_best_block_time = best_time;
    m_cached_difficulty = m_node.getBlockDifficulty(target_bits);

    // Non-blocking: skip the refreshes rather than wait on a contended
    // cs_main from a GUI-thread slot (same semantics as the previous
    // TRY_LOCK).
    if (const auto peers_height = m_node.tryGetNumBlocksOfPeers()) {
        m_cached_num_blocks_of_peers = *peers_height;
    }

    if (const auto net_weight = m_staking_status.tryGetNetworkWeight()) {
        m_cached_net_weight = *net_weight / 80.0;
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
        const std::string message = m_node.getAlertStatusBarMessage(hash_256);

        if (!message.empty())
        {
            emit error(tr("Network Alert"), QString::fromStdString(message), false);
        }
    }

    // Emit a numBlocksChanged when the status message changes,
    // so that the view recomputes and updates the status bar.
    emit numBlocksChanged(getNumBlocks(), getNumBlocksOfPeers());
}

void ClientModel::updateMinerStatus(bool staking, double coin_weight)
{
    if (staking) {
        // Non-blocking: keep the previous estimate when cs_main is contended
        // (same semantics as the previous TRY_LOCK).
        if (const auto etts_days = m_staking_status.tryGetEstimatedTimeToStake()) {
            m_cached_etts_days = *etts_days;
        }
    }

    emit minerStatusChanged(staking, getNetWeight(), coin_weight, m_cached_etts_days);
}

void ClientModel::refreshMinerStatus()
{
    // Re-feed the current staking state and coin weight to the overview. Used
    // when a GUI-side change (e.g. privacy mode) must refresh the staking
    // fields that are normally pushed from the core via MinerStatusChanged.
    updateMinerStatus(m_staking_status.isStaking(), m_staking_status.getCoinWeight());
}

void ClientModel::updateScraper(int scraperEventtype, int status, const QString message)
{
    if (scraperEventtype == (int)scrapereventtypes::Log)
        emit updateScraperLog(message);
    else
        emit updateScraperStatus(scraperEventtype, status);
}

void ClientModel::updatePSGTPool(const QString &revision_hash, int status, int reason)
{
    emit psgtPoolChanged(revision_hash, (quint8)status, reason);
}

interfaces::ScraperConvergenceSnapshot ClientModel::getScraperConvergenceSnapshot() const
{
    return m_node.getScraperConvergenceSnapshot();
}

bool ClientModel::isTestNet() const
{
    return m_node.isTestNet();
}

bool ClientModel::inInitialBlockDownload() const
{
    return m_node.isInitialBlockDownload();
}

QString ClientModel::getStatusBarWarnings() const
{
    QString warnings = QString::fromStdString(m_node.getWarnings());

    if (getDifficulty() < 0.1) {
        if (!warnings.isEmpty()) warnings += "; ";
        warnings += tr("Low difficulty!; ");
    }

    if (!m_staking_status.isStaking()) {
        warnings += tr("Miner: ");
        warnings += getMinerWarnings();
    }

    return warnings;
}

QString ClientModel::getMinerWarnings() const
{
    return QString::fromStdString(m_staking_status.getErrors());
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
    return QString::fromStdString(m_node.getClientVersion());
}

QString ClientModel::clientName() const
{
    return QString::fromStdString(CLIENT_NAME);
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromSecsSinceEpoch(nClientStartupTime).toString();
}

QString ClientModel::formatBoostVersion() const
{
    std::ostringstream s;
    s << "Using Boost "
      << BOOST_VERSION / 100000     << "."  // major version
      << BOOST_VERSION / 100 % 1000 << "."  // minor version
      << BOOST_VERSION % 100;               // patch level
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
    GUILogPrint(GUILogCategory::NOISY, "NotifyNumConnectionsChanged %i", newNumConnections);
    QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
                              Q_ARG(int, newNumConnections));
}

static void NotifyAlertChanged(ClientModel *clientmodel, const uint256 &hash, ChangeType status)
{
    GUILogPrintf("NotifyAlertChanged %s status=%i", hash.GetHex(), status);
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

static void PSGTPoolChanged(ClientModel *clientmodel, const uint256& revision_hash, ChangeType status, int reason)
{
    QMetaObject::invokeMethod(clientmodel, "updatePSGTPool", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(revision_hash.GetHex())),
                              Q_ARG(int, status),
                              Q_ARG(int, reason));
}

void ClientModel::subscribeToCoreSignals()
{
    // Register notification handlers, retaining each interfaces::Handler so
    // it is disconnected in unsubscribeFromCoreSignals() (from ~ClientModel).
    // Every callback below captures `this`; a notification firing after this
    // object is gone would otherwise invoke a slot on freed memory during
    // shutdown. The callbacks run on core threads and only marshal to the GUI
    // thread -- they must not take core locks (src/interfaces/README.md).
    m_handlers.emplace_back(m_node.handleNotifyBlocksChanged(
        [this](bool syncing, int height, int64_t best_time, uint32_t target_bits) {
            NotifyBlocksChanged(this, syncing, height, best_time, target_bits);
        }));
    m_handlers.emplace_back(m_node.handleBannedListChanged(
        [this]() { BannedListChanged(this); }));
    m_handlers.emplace_back(m_node.handleNotifyNumConnectionsChanged(
        [this](int num_connections) { NotifyNumConnectionsChanged(this, num_connections); }));
    m_handlers.emplace_back(m_node.handleNotifyAlertChanged(
        [this](const uint256& hash, ChangeType status) { NotifyAlertChanged(this, hash, status); }));
    m_handlers.emplace_back(m_node.handleNotifyScraperEvent(
        [this](const scrapereventtypes& event_type, ChangeType status, const std::string& message) {
            NotifyScraperEvent(this, event_type, status, message);
        }));
    m_handlers.emplace_back(m_node.handleMinerStatusChanged(
        [this](bool staking, double coin_weight) { MinerStatusChanged(this, staking, coin_weight); }));
    m_handlers.emplace_back(m_node.handlePSGTPoolChanged(
        [this](const uint256& revision_hash, ChangeType status, int reason) {
            PSGTPoolChanged(this, revision_hash, status, reason);
        }));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client: clearing the retained handlers runs each
    // interfaces::Handler destructor, which disconnects it.
    m_handlers.clear();
}
