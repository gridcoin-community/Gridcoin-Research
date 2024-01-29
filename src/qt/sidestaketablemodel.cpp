// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/sidestaketablemodel.h>
#include <qt/optionsmodel.h>
#include <node/ui_interface.h>
#include <gridcoin/support/enumbytes.h>

#include <QList>
#include <QTimer>
#include <QDebug>

namespace {

static void RwSettingsUpdated(SideStakeTableModel* sidestake_model)
{
    qDebug() << QString("%1").arg(__func__);
    QMetaObject::invokeMethod(sidestake_model, "updateSideStakeTableModel", Qt::QueuedConnection);
}

} // anonymous namespace

SideStakeLessThan::SideStakeLessThan(int column, Qt::SortOrder order)
    : m_column(column)
      , m_order(order)
{}

bool SideStakeLessThan::operator()(const GRC::SideStake& left, const GRC::SideStake& right) const
{
    const GRC::SideStake* pLeft = &left;
    const GRC::SideStake* pRight = &right;

    if (m_order == Qt::DescendingOrder) {
        std::swap(pLeft, pRight);
    }

    // For the purposes of sorting mandatory and local sidestakes in the GUI table, we will shift the local status enum to int
    // values that are above the mandatory enum values by OUT_OF_BOUND on the mandatory status enum.
    int left_status, right_status;

    if (pLeft->IsMandatory()) {
        left_status = static_cast<int>(std::get<GRC::MandatorySideStake::Status>(pLeft->GetStatus()).Value());
    } else {
        // For purposes of comparison, the enum value for local sidestake is shifted by the max entry of the mandatory
        // status enum.
        left_status = static_cast<int>(std::get<GRC::LocalSideStake::Status>(pLeft->GetStatus()).Value())
                      + static_cast<int>(GRC::MandatorySideStake::MandatorySideStakeStatus::OUT_OF_BOUND);
    }

    if (pRight->IsMandatory()) {
        right_status = static_cast<int>(std::get<GRC::MandatorySideStake::Status>(pRight->GetStatus()).Value());
    } else {
        // For purposes of comparison, the enum value for local sidestake is shifted by the max entry of the mandatory
        // status enum.
        right_status = static_cast<int>(std::get<GRC::LocalSideStake::Status>(pRight->GetStatus()).Value())
                      + static_cast<int>(GRC::MandatorySideStake::MandatorySideStakeStatus::OUT_OF_BOUND);
    }

    switch (static_cast<SideStakeTableModel::ColumnIndex>(m_column)) {
    case SideStakeTableModel::Address:
        return pLeft->GetDestination() < pRight->GetDestination();
    case SideStakeTableModel::Allocation:
        return pLeft->GetAllocation() < pRight->GetAllocation();
    case SideStakeTableModel::Description:
        return pLeft->GetDescription().compare(pRight->GetDescription()) < 0;
    case SideStakeTableModel::Status:
        return left_status < right_status;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

class SideStakeTablePriv
{
public:
    QList<GRC::SideStake> m_cached_sidestakes;
    int m_sort_column{-1};
    Qt::SortOrder m_sort_order;

    void refreshSideStakes()
    {
        m_cached_sidestakes.clear();

        std::vector<GRC::SideStake_ptr> core_sidestakes
            = GRC::GetSideStakeRegistry().ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, true);

        m_cached_sidestakes.reserve(core_sidestakes.size());

        for (const auto& entry : core_sidestakes) {
            m_cached_sidestakes.append(*entry);
        }

        if (m_sort_column >= 0) {
            std::stable_sort(m_cached_sidestakes.begin(), m_cached_sidestakes.end(), SideStakeLessThan(m_sort_column, m_sort_order));
        }
    }

    int size()
    {
        return m_cached_sidestakes.size();
    }

    GRC::SideStake* index(int idx)
    {
        if (idx >= 0 && idx < m_cached_sidestakes.size()) {
            return &m_cached_sidestakes[idx];
        }

        return nullptr;
    }
};

SideStakeTableModel::SideStakeTableModel(OptionsModel* parent)
    : QAbstractTableModel(parent)
{
    m_columns << tr("Address") << tr("Allocation") << tr("Description") << tr("Status");
    m_priv.reset(new SideStakeTablePriv());

    subscribeToCoreSignals();

    // load initial data
    refresh();
}

SideStakeTableModel::~SideStakeTableModel()
{
  // Intentionally left empty
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
    if(!index.isValid())
        return QVariant();

    GRC::SideStake* rec = static_cast<GRC::SideStake*>(index.internalPointer());

    const auto column = static_cast<ColumnIndex>(index.column());
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Address:
            return QString::fromStdString(CBitcoinAddress(rec->GetDestination()).ToString());
        case Allocation:
            return QString().setNum(rec->GetAllocation().ToPercent(), 'f', 2) + QString("\%");
        case Description:
            return QString::fromStdString(rec->GetDescription());
        case Status:
            return QString::fromStdString(rec->StatusToString());
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    } else if (role == Qt::EditRole) {
        switch (column) {
        case Address:
            return QString::fromStdString(CBitcoinAddress(rec->GetDestination()).ToString());
        case Allocation:
            return QString().setNum(rec->GetAllocation().ToPercent(), 'f', 2);
        case Description:
            return QString::fromStdString(rec->GetDescription());
        case Status:
            return QString::fromStdString(rec->StatusToString());
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

    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

    GRC::SideStake* rec = static_cast<GRC::SideStake*>(index.internalPointer());

    if (role != Qt::EditRole) {
        return false;
    }

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
        GRC::Allocation prior_total_allocation;

        // Save the original local sidestake (also in the core).
        GRC::SideStake orig_sidestake = *rec;

        if (orig_sidestake.GetAllocation().ToPercent() == value.toDouble()) {
            m_edit_status = NO_CHANGES;
            return false;
        }

        for (const auto& entry : registry.ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, true)) {
            CTxDestination destination = entry->GetDestination();
            GRC::Allocation allocation = entry->GetAllocation();

            if (destination == orig_sidestake.GetDestination()) {
                continue;
            }

            prior_total_allocation += allocation;
        }

        GRC::Allocation modified_allocation(value.toDouble() / 100.0);

        if (modified_allocation < 0 || prior_total_allocation + modified_allocation > 1) {
            m_edit_status = INVALID_ALLOCATION;

            LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: m_edit_status = %i",
                     __func__,
                     (int) m_edit_status);

            return false;
        }

        // Overwrite the existing sidestake entry with the modified allocation
        registry.NonContractAdd(GRC::LocalSideStake(orig_sidestake.GetDestination(),
                                                    modified_allocation,
                                                    orig_sidestake.GetDescription(),
                                                    std::get<GRC::LocalSideStake::Status>(orig_sidestake.GetStatus()).Value()),
                                true);

        break;
    }
    case Description:
    {
        std::string orig_value = value.toString().toStdString();
        std::string san_value = SanitizeString(orig_value, SAFE_CHARS_CSV);

        if (rec->GetDescription() == orig_value) {
            m_edit_status = NO_CHANGES;
            return false;
        }

        if (san_value != orig_value) {
            m_edit_status = INVALID_DESCRIPTION;
            return false;
        }

        // Save the original local sidestake (also in the core).
        GRC::SideStake orig_sidestake = *rec;

        // Overwrite the existing sidestake entry with the modified description
        registry.NonContractAdd(GRC::LocalSideStake(orig_sidestake.GetDestination(),
                                                    orig_sidestake.GetAllocation(),
                                                    san_value,
                                                    std::get<GRC::LocalSideStake::Status>(orig_sidestake.GetStatus()).Value()),
                                true);

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

    GRC::SideStake* rec = static_cast<GRC::SideStake*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (!rec->IsMandatory() && (index.column() == Allocation || index.column() == Description)) {
        retval |= Qt::ItemIsEditable;
    }

    return retval;
}

QModelIndex SideStakeTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    GRC::SideStake* data = m_priv->index(row);

    if (data)
        return createIndex(row, column, data);
    return QModelIndex();
}

QString SideStakeTableModel::addRow(const QString &address, const QString &allocation, const QString description)
{
    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();

    CBitcoinAddress sidestake_address;
    sidestake_address.SetString(address.toStdString());

    m_edit_status = OK;

    if (!sidestake_address.IsValid()) {
        m_edit_status = INVALID_ADDRESS;
        return QString();
    }

    // Check for duplicate local sidestakes. Here we use the actual core sidestake registry rather than the
    // UI model.
    std::vector<GRC::SideStake_ptr> core_local_sidestake = registry.Try(sidestake_address.Get(), GRC::SideStake::FilterFlag::LOCAL);

    if (!core_local_sidestake.empty()) {
        m_edit_status = DUPLICATE_ADDRESS;
        return QString();
    }

    GRC::Allocation prior_total_allocation;
    GRC::Allocation sidestake_allocation;

    // Get total allocation of all active/mandatory sidestake entries
    for (const auto& entry : registry.ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, true)) {
        prior_total_allocation += entry->GetAllocation();
    }

    // The new allocation must be parseable as a double, must be greater than or equal to 0, and
    // must result in a total allocation of less than 100%.
    double read_allocation = 0.0;

    if (!ParseDouble(allocation.toStdString(), &read_allocation)) {
        if (read_allocation < 0.0) {
            m_edit_status = INVALID_ALLOCATION;
            return QString();
        }

        sidestake_allocation += GRC::Allocation(read_allocation / 100.0);

        if (prior_total_allocation + sidestake_allocation > 1) {
            m_edit_status = INVALID_ALLOCATION;
            return QString();
        }
    }

    std::string sidestake_description = description.toStdString();
    std::string sanitized_description = SanitizeString(sidestake_description, SAFE_CHARS_CSV);

    if (sanitized_description != sidestake_description) {
        m_edit_status = INVALID_DESCRIPTION;
        return QString();
    }

    registry.NonContractAdd(GRC::LocalSideStake(sidestake_address.Get(),
                                                sidestake_allocation,
                                                sanitized_description,
                                                GRC::LocalSideStake::LocalSideStakeStatus::ACTIVE));

    updateSideStakeTableModel();

    return QString::fromStdString(sidestake_address.ToString());
}

bool SideStakeTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    GRC::SideStake* rec = m_priv->index(row);

    if (count != 1 || !rec || rec->IsMandatory())
    {
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove mandatory sidestakes.
        return false;
    }

    GRC::GetSideStakeRegistry().NonContractDelete(rec->GetDestination());

    updateSideStakeTableModel();

    return true;
}

SideStakeTableModel::EditStatus SideStakeTableModel::getEditStatus() const
{
    return m_edit_status;
}

void SideStakeTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    m_priv->refreshSideStakes();

    m_edit_status = OK;

    Q_EMIT layoutChanged();
}

void SideStakeTableModel::sort(int column, Qt::SortOrder order)
{
    m_priv->m_sort_column = column;
    m_priv->m_sort_order = order;
    refresh();
}

void SideStakeTableModel::updateSideStakeTableModel()
{
    refresh();

    emit updateSideStakeTableModelSig();
}

void SideStakeTableModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.RwSettingsUpdated_connect(boost::bind(RwSettingsUpdated, this));
}

void SideStakeTableModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client (currently no-op).
}
