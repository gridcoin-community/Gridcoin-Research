// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SIDESTAKETABLEMODEL_H
#define BITCOIN_QT_SIDESTAKETABLEMODEL_H

#include <QAbstractTableModel>
#include <memory>
#include "gridcoin/sidestake.h"

class OptionsModel;
class SideStakeTablePriv;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SideStakeLessThan
{
public:
    SideStakeLessThan(int column, Qt::SortOrder order);

    bool operator()(const GRC::SideStake& left, const GRC::SideStake& right) const;

private:
    int m_column;
    Qt::SortOrder m_order;
};

//!
//! \brief The SideStakeTableModel class represents the core sidestake registry as a model which can be consumed
//! and updated by the GUI.
//!
class SideStakeTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SideStakeTableModel(OptionsModel* parent = nullptr);
    ~SideStakeTableModel();

    enum ColumnIndex {
        Address,
        Allocation,
        Description,
        Status
    };

    static constexpr std::initializer_list<ColumnIndex> all_ColumnIndex = {Address,
                                                                           Allocation,
                                                                           Description,
                                                                           Status};

    /** Return status of edit/insert operation */
    enum EditStatus {
        OK,                     /**< Everything ok */
        NO_CHANGES,             /**< No changes were made during edit operation */
        INVALID_ADDRESS,        /**< Unparseable address */
        DUPLICATE_ADDRESS,      /**< Address already in sidestake registry */
        INVALID_ALLOCATION,     /**< Allocation is invalid (i.e. not parseable or not between 0.0 and 100.0) */
        INVALID_DESCRIPTION     /**< Description contains an invalid character */
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    void sort(int column, Qt::SortOrder order);
    /*@}*/

    /** Add a sidestake to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(const QString &address, const QString &allocation, const QString description);

    EditStatus getEditStatus() const;

public Q_SLOTS:
    void refresh();

private:
    QStringList m_columns;
    std::unique_ptr<SideStakeTablePriv> m_priv;
    EditStatus m_edit_status;
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

signals:

    void updateSideStakeTableModelSig();

public slots:
    void updateSideStakeTableModel();
};

#endif // BITCOIN_QT_SIDESTAKETABLEMODEL_H
