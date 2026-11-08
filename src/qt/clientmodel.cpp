#include "clientmodel.h"
#include "qt/guilog.h"
#include "qt/bantablemodel.h"
#include "qt/peertablemodel.h"
#include "guiconstants.h"
#include "qt/guistatecache.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include "clientversion.h"
#include "interfaces/handler.h"
#include "interfaces/staking.h"
#include "util/time.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

#include <boost/version.hpp>

#include <algorithm>
#include <sstream>

static const int64_t nClientStartupTime = GetTime();

ClientModel::ClientModel(interfaces::Node& node,
                         interfaces::GuiState& gui_state,
                         OptionsModel *optionsModel,
                         QObject *parent)
    : QObject(parent)
    , m_node(node)
    , optionsModel(optionsModel)
    , peerTableModel(nullptr)
    , banTableModel(nullptr)
    , m_gui_state_cache(new GuiStateCache(gui_state, this))
    , pollTimer(nullptr)
{
    peerTableModel = new PeerTableModel(this);
    banTableModel = new BanTableModel(m_node, this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(MODEL_UPDATE_DELAY);
    pollTimer->start();
    connect(pollTimer, &QTimer::timeout, this, &ClientModel::updateTimer);

    connect(m_gui_state_cache, &GuiStateCache::changed, this, &ClientModel::updateFromGuiState);

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
    return m_gui_state_cache->snapshot().height;
}

int ClientModel::getNumBlocksOfPeers() const
{
    return std::max(m_gui_state_cache->snapshot().peers_height, 0);
}

double ClientModel::getDifficulty() const
{
    return m_gui_state_cache->snapshot().difficulty;
}

double ClientModel::getNetWeight() const
{
    return m_gui_state_cache->snapshot().net_weight / interfaces::STAKE_WEIGHT_UNITS_PER_COIN;
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
    if (const int64_t tip_time = m_gui_state_cache->snapshot().tip_time) {
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

void ClientModel::updateFromGuiState()
{
    // A push comes whenever any part changed, about every eight seconds while the
    // miner runs. Each signal is emitted only when a field its slots read has
    // changed since the previous push; the first push emits all of them.
    const interfaces::GuiStateSnapshot& now = m_gui_state_cache->snapshot();
    const interfaces::GuiStateSnapshot& was = m_last_pushed;
    const bool first = !m_has_last_pushed;

    // setDifficulty reads the difficulty and the network weight.
    if (first || now.difficulty != was.difficulty || now.net_weight != was.net_weight) {
        emit difficultyChanged(getDifficulty());
    }

    // setNumBlocks reads the height, the peers' height, the tip time and the
    // sync state; the status bar warnings it shows follow the sync state.
    if (first || now.height != was.height || now.peers_height != was.peers_height
        || now.tip_time != was.tip_time || now.initial_download != was.initial_download
        || now.out_of_sync != was.out_of_sync) {
        emit numBlocksChanged(getNumBlocks(), getNumBlocksOfPeers());
    }

    // setMinerStatus reads the staking flag, both weights, the estimate and the
    // miner's errors (in the status bar warnings).
    if (first || now.staking != was.staking || now.coin_weight != was.coin_weight
        || now.net_weight != was.net_weight || now.has_etts != was.has_etts
        || now.etts_seconds != was.etts_seconds || now.miner_errors != was.miner_errors
        || now.out_of_sync != was.out_of_sync) {
        emitMinerStatus();
    }

    m_last_pushed = now;
    m_has_last_pushed = true;
}

void ClientModel::emitMinerStatus()
{
    const interfaces::GuiStateSnapshot& snapshot = m_gui_state_cache->snapshot();

    emit minerStatusChanged(snapshot.staking, getNetWeight(), snapshot.coin_weight,
                            snapshot.has_etts ? snapshot.etts_seconds : 0.0);
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

void ClientModel::refreshMinerStatus()
{
    // Re-emit the staking state and coin weight of the pushed copy. Used when a
    // GUI-side change (e.g. privacy mode) must refresh the staking fields, and
    // to hydrate them when the GUI attaches.
    emitMinerStatus();
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

bool ClientModel::isMainNet() const
{
    return m_node.isMainNet();
}

bool ClientModel::inInitialBlockDownload() const
{
    return m_gui_state_cache->snapshot().initial_download;
}

QString ClientModel::getStatusBarWarnings() const
{
    QString warnings = QString::fromStdString(m_node.getWarnings());

    if (getDifficulty() < 0.1) {
        if (!warnings.isEmpty()) warnings += "; ";
        warnings += tr("Low difficulty!; ");
    }

    if (!m_gui_state_cache->snapshot().staking) {
        warnings += tr("Miner: ");
        warnings += getMinerWarnings();
    }

    return warnings;
}

QString ClientModel::getMinerWarnings() const
{
    return QString::fromStdString(m_gui_state_cache->snapshot().miner_errors);
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
    // The callbacks run on core threads and only marshal to the GUI thread --
    // they must not take core locks or call back into the node (see the handler
    // contract in src/interfaces/handler.h).
    //
    // Every callback captures `this`, and disconnecting does NOT wait for one
    // already executing on a core thread, so each is wrapped in
    // m_notify_lifetime.guard(): retire() (called from unsubscribeFromCoreSignals
    // below) both stops future deliveries and blocks until any in-flight callback
    // has finished. Without it, ThreadStakeMiner emitting MinerStatusChanged while
    // this object is being destroyed is a use-after-free.
    m_handlers.emplace_back(m_node.handleBannedListChanged(m_notify_lifetime.guard(
        [this]() { BannedListChanged(this); })));
    m_handlers.emplace_back(m_node.handleNotifyNumConnectionsChanged(m_notify_lifetime.guard(
        [this](int num_connections) { NotifyNumConnectionsChanged(this, num_connections); })));
    m_handlers.emplace_back(m_node.handleNotifyAlertChanged(m_notify_lifetime.guard(
        [this](const uint256& hash, ChangeType status) { NotifyAlertChanged(this, hash, status); })));
    m_handlers.emplace_back(m_node.handleNotifyScraperEvent(m_notify_lifetime.guard(
        [this](const scrapereventtypes& event_type, ChangeType status, const std::string& message) {
            NotifyScraperEvent(this, event_type, status, message);
        })));
    m_handlers.emplace_back(m_node.handlePSGTPoolChanged(m_notify_lifetime.guard(
        [this](const uint256& revision_hash, ChangeType status, int reason) {
            PSGTPoolChanged(this, revision_hash, status, reason);
        })));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Retire FIRST, and before any member of this object is destroyed: this both
    // refuses further deliveries and waits out any callback already running on a
    // core thread, so nothing can be touching `this` once it returns. Clearing the
    // handlers afterwards disconnects them, but disconnecting alone never waited.
    m_notify_lifetime.retire();

    // Disconnect signals from client: clearing the retained handlers runs each
    // interfaces::Handler destructor, which disconnects it.
    m_handlers.clear();
}
