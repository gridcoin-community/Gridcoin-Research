#include "clientmodel.h"
#include "qt/bantablemodel.h"
#include "qt/peertablemodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include "alert.h"
#include "main.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/superblock.h"
#include "ui_interface.h"
#include "util.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

static const int64_t nClientStartupTime = GetTime();
extern ConvergedScraperStats ConvergedScraperStatsCache;

ClientModel::ClientModel(OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), optionsModel(optionsModel), peerTableModel(nullptr),
    banTableModel(nullptr), cachedNumBlocks(0), cachedNumBlocksOfPeers(0), pollTimer(0)
{
    numBlocksAtStartup = -1;
    peerTableModel = new PeerTableModel(this);
    banTableModel = new BanTableModel(this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(MODEL_UPDATE_DELAY);
    pollTimer->start();
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));

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
    LOCK(cs_main);
    return nBestHeight;
}

int ClientModel::getNumBlocksAtStartup()
{
    if (numBlocksAtStartup == -1) numBlocksAtStartup = getNumBlocks();
    return numBlocksAtStartup;
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
    LOCK(cs_main);
    if (pindexBest)
        return QDateTime::fromTime_t(pindexBest->GetBlockTime());
    else
        return QDateTime::fromTime_t(1393221600); // Genesis block's time
}

void ClientModel::updateTimer()
{
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    // Some quantities (such as number of blocks) change so fast that we don't want to be notified for each change.
    // Periodically check and update with a timer.
    int newNumBlocks = getNumBlocks();
    int newNumBlocksOfPeers = getNumBlocksOfPeers();

    if(cachedNumBlocks != newNumBlocks || cachedNumBlocksOfPeers != newNumBlocksOfPeers)
    {
        cachedNumBlocks = newNumBlocks;
        cachedNumBlocksOfPeers = newNumBlocksOfPeers;

        emit numBlocksChanged(newNumBlocks, newNumBlocksOfPeers);
	}
	if (GetArg("-suppressnetworkgraph", "false") != "true")
	{
		emit bytesChanged(getTotalBytesRecv(), getTotalBytesSent());
	}

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

int ClientModel::getNumBlocksOfPeers() const
{
    return GetNumBlocksOfPeers();
}

QString ClientModel::getStatusBarWarnings() const
{
    return QString::fromStdString(GetWarnings("statusbar"));
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

QString ClientModel::formatBuildDate() const
{
    return QString::fromStdString(CLIENT_DATE);
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

QString ClientModel::getDifficulty() const
{
	//12-2-2014;R Halford; Display POR Diff on RPC Console
	double PORDiff = GRC::GetCurrentDifficulty();

	std::string diff = RoundToString(PORDiff,4);
	return QString::fromStdString(diff);

}

void ClientModel::updateBanlist()
{
    banTableModel->refresh();
}


// Handlers for core signals
static void NotifyBlocksChanged(ClientModel *clientmodel)
{
    // This notification is too frequent. Don't trigger a signal.
    // Don't remove it, though, as it might be useful later.

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


// This is ugly but is the easiest way to support the wide range of boost versions and deal with the
// boost placeholders global namespace pollution fix for later versions (>= 1.73) without breaking earlier ones.
#if BOOST_VERSION >= 107300
void ClientModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.NotifyBlocksChanged.connect(boost::bind(NotifyBlocksChanged, this));
    uiInterface.BannedListChanged.connect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyNumConnectionsChanged.connect(boost::bind(NotifyNumConnectionsChanged, this, boost::placeholders::_1));
    uiInterface.NotifyAlertChanged.connect(boost::bind(NotifyAlertChanged, this, boost::placeholders::_1, boost::placeholders::_2));
    uiInterface.NotifyScraperEvent.connect(boost::bind(NotifyScraperEvent, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.NotifyBlocksChanged.disconnect(boost::bind(NotifyBlocksChanged, this));
    uiInterface.BannedListChanged.disconnect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyNumConnectionsChanged.disconnect(boost::bind(NotifyNumConnectionsChanged, this, boost::placeholders::_1));
    uiInterface.NotifyAlertChanged.disconnect(boost::bind(NotifyAlertChanged, this, boost::placeholders::_1, boost::placeholders::_2));
    uiInterface.NotifyScraperEvent.disconnect(boost::bind(NotifyScraperEvent, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
}
#else
void ClientModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.NotifyBlocksChanged.connect(boost::bind(NotifyBlocksChanged, this));
    uiInterface.BannedListChanged.connect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyNumConnectionsChanged.connect(boost::bind(NotifyNumConnectionsChanged, this, _1));
    uiInterface.NotifyAlertChanged.connect(boost::bind(NotifyAlertChanged, this, _1, _2));
    uiInterface.NotifyScraperEvent.connect(boost::bind(NotifyScraperEvent, this, _1, _2, _3));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.NotifyBlocksChanged.disconnect(boost::bind(NotifyBlocksChanged, this));
    uiInterface.BannedListChanged.disconnect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyNumConnectionsChanged.disconnect(boost::bind(NotifyNumConnectionsChanged, this, _1));
    uiInterface.NotifyAlertChanged.disconnect(boost::bind(NotifyAlertChanged, this, _1, _2));
    uiInterface.NotifyScraperEvent.disconnect(boost::bind(NotifyScraperEvent, this, _1, _2, _3));
}
#endif
