// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/peertablemodel.h>

#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <interfaces/node.h>

#include <QDebug>
#include <QList>
#include <QTimer>

bool NodeLessThan::operator()(const interfaces::PeerInfo &left, const interfaces::PeerInfo &right) const
{
    const interfaces::PeerInfo *pLeft = &left;
    const interfaces::PeerInfo *pRight = &right;

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch (static_cast<PeerTableModel::ColumnIndex>(column)) {
    case PeerTableModel::NetNodeId:
        return pLeft->id < pRight->id;
    case PeerTableModel::Address:
        return pLeft->addr_name.compare(pRight->addr_name) < 0;
    case PeerTableModel::Subversion:
        return pLeft->subversion.compare(pRight->subversion) < 0;
    case PeerTableModel::Ping:
        return pLeft->ping_time < pRight->ping_time;
    case PeerTableModel::Sent:
        return pLeft->send_bytes < pRight->send_bytes;
    case PeerTableModel::Received:
        return pLeft->recv_bytes < pRight->recv_bytes;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

// private implementation
class PeerTablePriv
{
public:
    /** Local cache of peer information */
    QList<interfaces::PeerInfo> cachedNodeStats;
    /** Column to sort nodes by (default to unsorted) */
    int sortColumn{-1};
    /** Order (ascending or descending) to sort nodes by */
    Qt::SortOrder sortOrder;
    /** Index of rows by node ID */
    std::map<int64_t, int> mapNodeRows;

    /** Pull a full list of peers from the node interface into our cache. Peer
        stats now cross the boundary as interfaces::PeerInfo value snapshots (the
        node side reads CNodeStats under the connection manager); the list is
        empty when the connection manager is not up. */
    void refreshPeers(interfaces::Node& node)
    {
        cachedNodeStats.clear();

        std::vector<interfaces::PeerInfo> peers = node.getPeers();
        cachedNodeStats.reserve(peers.size());
        for (interfaces::PeerInfo& peer : peers)
            cachedNodeStats.append(std::move(peer));

        if (sortColumn >= 0)
            // sort cacheNodeStats (use stable sort to prevent rows jumping around unnecessarily)
            std::stable_sort(cachedNodeStats.begin(), cachedNodeStats.end(), NodeLessThan(sortColumn, sortOrder));

        // build index map
        mapNodeRows.clear();
        int row = 0;
        for (const interfaces::PeerInfo& stats : std::as_const(cachedNodeStats))
            mapNodeRows.insert(std::pair<int64_t, int>(stats.id, row++));
    }

    int size() const
    {
        return cachedNodeStats.size();
    }

    interfaces::PeerInfo *index(int idx)
    {
        if (idx >= 0 && idx < cachedNodeStats.size())
            return &cachedNodeStats[idx];

        return nullptr;
    }
};

PeerTableModel::PeerTableModel(ClientModel *parent) :
    QAbstractTableModel(parent),
    clientModel(parent),
    timer(nullptr)
{
    columns << tr("Node ID") << tr("Node/Service") << tr("Ping") << tr("Sent") << tr("Received") << tr("User Agent");
    priv.reset(new PeerTablePriv());

    // set up timer for auto refresh
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PeerTableModel::refresh);
    timer->setInterval(MODEL_UPDATE_DELAY);

    // load initial data
    refresh();
}

PeerTableModel::~PeerTableModel()
{
    // Intentionally left empty
}

void PeerTableModel::startAutoRefresh()
{
    timer->start();
}

void PeerTableModel::stopAutoRefresh()
{
    timer->stop();
}

int PeerTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return priv->size();
}

int PeerTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return columns.length();
}

QVariant PeerTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    interfaces::PeerInfo *rec = static_cast<interfaces::PeerInfo*>(index.internalPointer());

    const auto column = static_cast<ColumnIndex>(index.column());
    if (role == Qt::DisplayRole) {
        switch (column) {
        case NetNodeId:
            return (qint64)rec->id;
        case Address:
            // prepend to peer address down-arrow symbol for inbound connection and up-arrow for outbound connection
            return QString(rec->inbound ? "↓ " : "↑ ") + QString::fromStdString(rec->addr_name);
        case Subversion:
            if (!rec->subversion.empty()) {
                // remove leading and trailing slash
                return QString::fromStdString(rec->subversion.substr(1, rec->subversion.length() - 2));
            } else {
                return QString();
            }
        case Ping:
            return GUIUtil::formatPingTime(rec->ping_time);
        case Sent:
            return GUIUtil::formatBytes(rec->send_bytes);
        case Received:
            return GUIUtil::formatBytes(rec->recv_bytes);
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    } else if (role == Qt::TextAlignmentRole) {
        switch (column) {
            case Ping:
            case Sent:
            case Received:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QVariant();
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    }

    return QVariant();
}

QVariant PeerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags PeerTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex PeerTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    interfaces::PeerInfo *data = priv->index(row);

    if (data)
        return createIndex(row, column, data);
    return QModelIndex();
}

const interfaces::PeerInfo *PeerTableModel::getNodeStats(int idx)
{
    return priv->index(idx);
}

void PeerTableModel::refresh()
{
    if (!clientModel) return;

    Q_EMIT layoutAboutToBeChanged();
    priv->refreshPeers(clientModel->node());
    Q_EMIT layoutChanged();
}

int PeerTableModel::getRowByNodeId(int64_t nodeid)
{
    std::map<int64_t, int>::iterator it = priv->mapNodeRows.find(nodeid);
    if (it == priv->mapNodeRows.end())
        return -1;

    return it->second;
}

void PeerTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}
