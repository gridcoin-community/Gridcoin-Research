// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BANTABLEMODEL_H
#define BITCOIN_QT_BANTABLEMODEL_H

#include "interfaces/node.h"

#include <memory>

#include <QAbstractTableModel>
#include <QStringList>

class ClientModel;
class BanTablePriv;

class BannedNodeLessThan
{
public:
    BannedNodeLessThan(int nColumn, Qt::SortOrder fOrder) :
        column(nColumn), order(fOrder) {}
    bool operator()(const interfaces::BannedNode& left, const interfaces::BannedNode& right) const;

private:
    int column;
    Qt::SortOrder order;
};

/**
   Qt model providing information about banned peers, similar to the
   "listbanned" RPC call. Used by the rpc console UI.
 */
class BanTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit BanTableModel(interfaces::Node& node, ClientModel *parent = nullptr);
    ~BanTableModel();
    void startAutoRefresh();
    void stopAutoRefresh();

    enum ColumnIndex {
        Address = 0,
        Bantime = 1
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    void sort(int column, Qt::SortOrder order);
    bool shouldShow();
    /*@}*/

public Q_SLOTS:
    void refresh();

private:
    interfaces::Node& m_node;
    ClientModel *clientModel;
    QStringList columns;
    std::unique_ptr<BanTablePriv> priv;
};

#endif // BITCOIN_QT_BANTABLEMODEL_H
