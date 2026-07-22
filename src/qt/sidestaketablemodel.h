// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SIDESTAKETABLEMODEL_H
#define BITCOIN_QT_SIDESTAKETABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include <cstdint>
#include <memory>

class OptionsModel;
class SideStakeTablePriv;

namespace interfaces {
class Handler;
class SideStakeManager;
} // namespace interfaces

//!
//! \brief The SideStakeTableModel class represents the sidestake registry as a
//! single unified table (mandatory + local entries) for the GUI, sourced through
//! interfaces::SideStakeManager. No core types cross into the GUI: rows are
//! value-type interfaces::SideStakeEntry and add/edit/delete are command calls.
//!
class SideStakeTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SideStakeTableModel(interfaces::SideStakeManager& sidestake_manager,
                                 OptionsModel* parent = nullptr);
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

    //! Whether the entry at \p row is a mandatory (contract) sidestake, which
    //! cannot be edited or deleted. Reads the interfaces::SideStakeEntry value
    //! row, so callers need no core sidestake type. Returns false for an
    //! out-of-range row.
    bool isMandatory(int row) const;

public Q_SLOTS:
    void refresh();

private:
    QStringList m_columns;
    std::unique_ptr<SideStakeTablePriv> m_priv;
    EditStatus m_edit_status;

    //! The node-side sidestake registry boundary (Phase 1d-ii). Owned by the
    //! process and outlives this model.
    interfaces::SideStakeManager& m_sidestake_manager;

    //! High-water mark of the local sidestake revision seen across mutation
    //! returns and accepted notifications (design §4.4). RwSettingsUpdated
    //! notifications with revision <= this are dropped (they coalesce and prevent
    //! a redundant refetch after our own write).
    uint64_t m_local_revision_high_water;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    //! Retained node subscriptions, released on teardown so a signal that fires
    //! after this model is destroyed cannot invoke a slot on freed memory (issue
    //! #3129). The Handlers disconnect on destruction.
    std::unique_ptr<interfaces::Handler> m_rw_settings_handler;
    std::unique_ptr<interfaces::Handler> m_mandatory_handler;

signals:

    void updateSideStakeTableModelSig();

public slots:
    void updateSideStakeTableModel();

    //! RwSettingsUpdated notification (local sidestake change). Reads the current
    //! local revision fresh (on this GUI thread, after the node's reload slot has
    //! run) and refetches only when it advances past the high-water mark.
    void localSideStakeUpdated();

    //! MandatorySideStakeChanged notification (contract sidestake change).
    //! Payload-free: always refetches the unified table.
    void mandatorySideStakeChanged();
};

#endif // BITCOIN_QT_SIDESTAKETABLEMODEL_H
