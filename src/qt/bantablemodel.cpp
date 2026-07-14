// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/bantablemodel.h>

#include <qt/clientmodel.h>

#include <QDebug>
#include <QList>
#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <cassert>
#include <utility>

bool BannedNodeLessThan::operator()(const interfaces::BannedNode& left, const interfaces::BannedNode& right) const
{
    const interfaces::BannedNode* pLeft = &left;
    const interfaces::BannedNode* pRight = &right;

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch (static_cast<BanTableModel::ColumnIndex>(column)) {
    case BanTableModel::Address:
        return pLeft->address.compare(pRight->address) < 0;
    case BanTableModel::Bantime:
        return pLeft->ban_until < pRight->ban_until;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return false; // Unreachable: keeps release builds (NDEBUG) well-defined.
}

// private implementation
class BanTablePriv
{
public:
    /** Local cache of banned peers */
    QList<interfaces::BannedNode> cachedBanlist;
    /** Column to sort nodes by (default to unsorted) */
    int sortColumn{-1};
    /** Order (ascending or descending) to sort nodes by */
    Qt::SortOrder sortOrder;

    /** Pull a full list of banned nodes through the node interface into our cache */
    void refreshBanlist(interfaces::Node& node)
    {
        const std::vector<interfaces::BannedNode> banned = node.getBanned();

        cachedBanlist.clear();
        cachedBanlist.reserve(banned.size());
        for (const auto& entry : banned)
        {
            cachedBanlist.append(entry);
        }

        if (sortColumn >= 0)
            // sort cachedBanlist (use stable sort to prevent rows jumping around unnecessarily)
            std::stable_sort(cachedBanlist.begin(), cachedBanlist.end(), BannedNodeLessThan(sortColumn, sortOrder));
    }

    int size() const
    {
        return cachedBanlist.size();
    }

    interfaces::BannedNode *index(int idx)
    {
        if (idx >= 0 && idx < cachedBanlist.size())
            return &cachedBanlist[idx];

        return nullptr;
    }
};

BanTableModel::BanTableModel(interfaces::Node& node, ClientModel *parent) :
    QAbstractTableModel(parent),
    m_node(node),
    clientModel(parent)
{
    columns << tr("IP/Netmask") << tr("Banned Until");
    priv.reset(new BanTablePriv());

    // load initial data
    refresh();
}

BanTableModel::~BanTableModel()
{
    // Intentionally left empty
}

int BanTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return priv->size();
}

int BanTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return columns.length();
}

QVariant BanTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    interfaces::BannedNode *rec = static_cast<interfaces::BannedNode*>(index.internalPointer());

    const auto column = static_cast<ColumnIndex>(index.column());
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Address:
            return QString::fromStdString(rec->address);
        case Bantime:
            QDateTime date = QDateTime::fromMSecsSinceEpoch(0);
            date = date.addSecs(rec->ban_until);
            return QLocale::system().toString(date, QLocale::LongFormat);
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    }

    return QVariant();
}

QVariant BanTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

Qt::ItemFlags BanTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex BanTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    interfaces::BannedNode *data = priv->index(row);

    if (data)
        return createIndex(row, column, data);
    return QModelIndex();
}

void BanTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    priv->refreshBanlist(m_node);
    Q_EMIT layoutChanged();
}

void BanTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}

bool BanTableModel::shouldShow()
{
    return priv->size() > 0;
}
