// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/sidestaketablemodel.h>
#include <qt/optionsmodel.h>

#include <interfaces/handler.h>
#include <interfaces/sidestake.h>

#include <QMetaObject>

#include <algorithm>
#include <cassert>
#include <utility>

namespace {

//! Sorts the unified table's value rows. Address sorts by the encoded address
//! string (what the column displays); Status by the precomputed sort key
//! (mandatory statuses first, local statuses shifted above them).
class SideStakeLessThan
{
public:
    SideStakeLessThan(int column, Qt::SortOrder order)
        : m_column(column)
        , m_order(order)
    {}

    bool operator()(const interfaces::SideStakeEntry& left,
                    const interfaces::SideStakeEntry& right) const
    {
        const interfaces::SideStakeEntry* pLeft = &left;
        const interfaces::SideStakeEntry* pRight = &right;

        if (m_order == Qt::DescendingOrder) {
            std::swap(pLeft, pRight);
        }

        switch (static_cast<SideStakeTableModel::ColumnIndex>(m_column)) {
        case SideStakeTableModel::Address:
            return pLeft->address < pRight->address;
        case SideStakeTableModel::Allocation:
            return pLeft->allocation_percent < pRight->allocation_percent;
        case SideStakeTableModel::Description:
            return pLeft->description < pRight->description;
        case SideStakeTableModel::Status:
            return pLeft->status_sort_key < pRight->status_sort_key;
        } // no default case, so the compiler can warn about missing cases

        assert(false);
        return false;
    }

private:
    int m_column;
    Qt::SortOrder m_order;
};

//! Map the interface edit status onto the model's public EditStatus enum
//! (OptionsDialog inspects the latter).
SideStakeTableModel::EditStatus MapEditStatus(interfaces::SideStakeEditStatus status)
{
    switch (status) {
    case interfaces::SideStakeEditStatus::OK:                  return SideStakeTableModel::OK;
    case interfaces::SideStakeEditStatus::NO_CHANGES:          return SideStakeTableModel::NO_CHANGES;
    case interfaces::SideStakeEditStatus::INVALID_ADDRESS:     return SideStakeTableModel::INVALID_ADDRESS;
    case interfaces::SideStakeEditStatus::DUPLICATE_ADDRESS:   return SideStakeTableModel::DUPLICATE_ADDRESS;
    case interfaces::SideStakeEditStatus::INVALID_ALLOCATION:  return SideStakeTableModel::INVALID_ALLOCATION;
    case interfaces::SideStakeEditStatus::INVALID_DESCRIPTION: return SideStakeTableModel::INVALID_DESCRIPTION;
    }

    assert(false);
    return SideStakeTableModel::OK;
}

} // anonymous namespace

//! Holds the cached value-row snapshot and the current sort, sourced entirely
//! from interfaces::SideStakeManager (no core types).
class SideStakeTablePriv
{
public:
    std::vector<interfaces::SideStakeEntry> m_cached_sidestakes;
    int m_sort_column{-1};
    Qt::SortOrder m_sort_order{Qt::AscendingOrder};

    void setEntries(std::vector<interfaces::SideStakeEntry> entries)
    {
        m_cached_sidestakes = std::move(entries);
        applySort();
    }

    void applySort()
    {
        if (m_sort_column >= 0) {
            std::stable_sort(m_cached_sidestakes.begin(), m_cached_sidestakes.end(),
                             SideStakeLessThan(m_sort_column, m_sort_order));
        }
    }

    int size() const
    {
        return static_cast<int>(m_cached_sidestakes.size());
    }

    const interfaces::SideStakeEntry* at(int idx) const
    {
        if (idx >= 0 && idx < size()) {
            return &m_cached_sidestakes[idx];
        }

        return nullptr;
    }
};

SideStakeTableModel::SideStakeTableModel(interfaces::SideStakeManager& sidestake_manager, OptionsModel* parent)
    : QAbstractTableModel(parent)
    , m_edit_status(OK)
    , m_sidestake_manager(sidestake_manager)
    , m_local_revision_high_water(0)
{
    m_columns << tr("Address") << tr("Allocation") << tr("Description") << tr("Status");
    m_priv.reset(new SideStakeTablePriv());

    subscribeToCoreSignals();

    // Deliberately do NOT refresh() here. This model is constructed early in
    // main() (via OptionsModel), before SelectParams(gArgs.GetChainName()) and
    // before the sidestake registry is loaded during node init. Because entries()
    // returns value rows with the address already encoded (EncodeDestination uses
    // the active chain params), an initial refresh under the provisional MAIN
    // params could cache mainnet-prefixed strings on testnet/regtest. The model
    // is empty until its first refresh, which happens when OptionsDialog binds it
    // (OptionsDialog::setModel -> refresh()) — well after final chain selection
    // and registry load. The registry is empty at construction anyway, so nothing
    // is lost by deferring.
}

SideStakeTableModel::~SideStakeTableModel()
{
    unsubscribeFromCoreSignals();
}

int SideStakeTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_priv->size();
}

int SideStakeTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_columns.length();
}

QVariant SideStakeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const interfaces::SideStakeEntry* rec = m_priv->at(index.row());

    if (!rec) {
        return QVariant();
    }

    const auto column = static_cast<ColumnIndex>(index.column());
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Address:
            return QString::fromStdString(rec->address);
        case Allocation:
            return QString().setNum(rec->allocation_percent, 'f', 2) + QString("\%");
        case Description:
            return QString::fromStdString(rec->description);
        case Status:
            return QString::fromStdString(rec->status);
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    } else if (role == Qt::EditRole) {
        switch (column) {
        case Address:
            return QString::fromStdString(rec->address);
        case Allocation:
            return QString().setNum(rec->allocation_percent, 'f', 2);
        case Description:
            return QString::fromStdString(rec->description);
        case Status:
            return QString::fromStdString(rec->status);
        } // no default case, so the compiler can warn about missing cases
    } else if (role == Qt::TextAlignmentRole) {
        switch (column) {
        case Address:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Allocation:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case Description:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Status:
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
        default:
            return QVariant();
        }
    }

    return QVariant();
}

bool SideStakeTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    if (role != Qt::EditRole) {
        return false;
    }

    const interfaces::SideStakeEntry* rec = m_priv->at(index.row());

    if (!rec) {
        return false;
    }

    // Copy the address before any mutation, which invalidates rec via refetch.
    const std::string address = rec->address;

    m_edit_status = OK;

    switch (index.column())
    {
    case Address:
    {
        // The address of a sidestake entry is not editable.
        return false;
    }
    case Allocation:
    {
        bool parse_ok = false;
        const double allocation_percent = value.toDouble(&parse_ok);

        if (!parse_ok) {
            m_edit_status = INVALID_ALLOCATION;
            return false;
        }

        const interfaces::SideStakeEditResult result =
            m_sidestake_manager.setAllocation(address, allocation_percent);

        m_edit_status = MapEditStatus(result.status);

        if (result.status != interfaces::SideStakeEditStatus::OK) {
            return false;
        }

        m_local_revision_high_water = std::max(m_local_revision_high_water, result.local_revision);

        break;
    }
    case Description:
    {
        const interfaces::SideStakeEditResult result =
            m_sidestake_manager.setDescription(address, value.toString().toStdString());

        m_edit_status = MapEditStatus(result.status);

        if (result.status != interfaces::SideStakeEditStatus::OK) {
            return false;
        }

        m_local_revision_high_water = std::max(m_local_revision_high_water, result.local_revision);

        break;
    }
    case Status:
        // Status is not editable
        return false;
    }

    updateSideStakeTableModel();

    return true;
}

QVariant SideStakeTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < m_columns.size())
        {
            return m_columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags SideStakeTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    const interfaces::SideStakeEntry* rec = m_priv->at(index.row());

    if (!rec) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (!rec->is_mandatory && (index.column() == Allocation || index.column() == Description)) {
        retval |= Qt::ItemIsEditable;
    }

    return retval;
}

QModelIndex SideStakeTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    if (m_priv->at(row)) {
        return createIndex(row, column);
    }

    return QModelIndex();
}

QString SideStakeTableModel::addRow(const QString &address, const QString &allocation, const QString description)
{
    m_edit_status = OK;

    // Parse the allocation (percent) GUI-side; the node validates address,
    // duplicate, allocation total and description.
    bool parse_ok = false;
    const double allocation_percent = allocation.toDouble(&parse_ok);

    if (!parse_ok) {
        m_edit_status = INVALID_ALLOCATION;
        return QString();
    }

    const interfaces::SideStakeEditResult result =
        m_sidestake_manager.addLocal(address.toStdString(), allocation_percent, description.toStdString());

    m_edit_status = MapEditStatus(result.status);

    if (result.status != interfaces::SideStakeEditStatus::OK) {
        return QString();
    }

    m_local_revision_high_water = std::max(m_local_revision_high_water, result.local_revision);

    updateSideStakeTableModel();

    return QString::fromStdString(result.address);
}

bool SideStakeTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);

    const interfaces::SideStakeEntry* rec = m_priv->at(row);

    if (count != 1 || !rec || rec->is_mandatory)
    {
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove mandatory sidestakes.
        return false;
    }

    const interfaces::SideStakeEditResult result = m_sidestake_manager.deleteLocal(rec->address);

    if (result.status != interfaces::SideStakeEditStatus::OK) {
        return false;
    }

    m_local_revision_high_water = std::max(m_local_revision_high_water, result.local_revision);

    updateSideStakeTableModel();

    return true;
}

SideStakeTableModel::EditStatus SideStakeTableModel::getEditStatus() const
{
    return m_edit_status;
}

bool SideStakeTableModel::isMandatory(int row) const
{
    const interfaces::SideStakeEntry* rec = m_priv->at(row);
    return rec && rec->is_mandatory;
}

void SideStakeTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();

    interfaces::SideStakeSnapshot snapshot = m_sidestake_manager.entries();

    m_local_revision_high_water = std::max(m_local_revision_high_water, snapshot.local_revision);

    m_priv->setEntries(std::move(snapshot.entries));

    m_edit_status = OK;

    Q_EMIT layoutChanged();
}

void SideStakeTableModel::sort(int column, Qt::SortOrder order)
{
    // Re-sort the cached rows without refetching from the node.
    Q_EMIT layoutAboutToBeChanged();

    m_priv->m_sort_column = column;
    m_priv->m_sort_order = order;
    m_priv->applySort();

    Q_EMIT layoutChanged();
}

void SideStakeTableModel::updateSideStakeTableModel()
{
    refresh();

    emit updateSideStakeTableModelSig();
}

void SideStakeTableModel::localSideStakeUpdated()
{
    // Read the revision fresh here (not at notification time): this slot runs on
    // the GUI thread after the whole synchronous RwSettingsUpdated emission,
    // including the node's own reload slot, has completed — so it reflects the
    // reload regardless of slot order. Drop stale/coalesced notifications,
    // including the one echoing our own just-applied write (design §4.4);
    // refresh() then advances the high-water mark to the refetched revision.
    if (m_sidestake_manager.localRevision() <= m_local_revision_high_water) {
        return;
    }

    updateSideStakeTableModel();
}

void SideStakeTableModel::mandatorySideStakeChanged()
{
    // Mandatory (contract) changes are rare and carry no revision; always refetch
    // the unified table.
    updateSideStakeTableModel();
}

void SideStakeTableModel::subscribeToCoreSignals()
{
    // Retain the handlers so they are severed in unsubscribeFromCoreSignals()
    // (from ~SideStakeTableModel); each callback captures `this` and a signal
    // firing after this object is gone would otherwise invoke a slot on freed
    // memory. The callbacks only marshal to the GUI thread (issue #3129).
    m_rw_settings_handler = m_sidestake_manager.handleRwSettingsUpdated(
        [this]() {
            QMetaObject::invokeMethod(this, "localSideStakeUpdated", Qt::QueuedConnection);
        });

    m_mandatory_handler = m_sidestake_manager.handleMandatorySideStakeChanged(
        [this]() {
            QMetaObject::invokeMethod(this, "mandatorySideStakeChanged", Qt::QueuedConnection);
        });
}

void SideStakeTableModel::unsubscribeFromCoreSignals()
{
    // Disconnect from the node: resetting each handler runs its destructor, which
    // disconnects the subscription (issue #3129).
    m_rw_settings_handler.reset();
    m_mandatory_handler.reset();
}
