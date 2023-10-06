// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/sidestaketablemodel.h>
#include <qt/optionsmodel.h>
#include <node/ui_interface.h>

#include <QList>
#include <QTimer>
#include <QDebug>

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

    switch (static_cast<SideStakeTableModel::ColumnIndex>(m_column)) {
    case SideStakeTableModel::Address:
        return pLeft->m_key < pRight->m_key;
    case SideStakeTableModel::Allocation:
        return pLeft->m_allocation < pRight->m_allocation;
    case SideStakeTableModel::Description:
        return pLeft->m_description.compare(pRight->m_description) < 0;
    case SideStakeTableModel::Status:
        return pLeft->m_status < pRight->m_status;
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

        std::vector<GRC::SideStake_ptr> core_sidestakes = GRC::GetSideStakeRegistry().ActiveSideStakeEntries();

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
            return QString::fromStdString(rec->m_key.ToString());
        case Allocation:
            return rec->m_allocation * 100.0;
        case Description:
            return QString::fromStdString(rec->m_description);
        case Status:
            return QString::fromStdString(rec->StatusToString());
        } // no default case, so the compiler can warn about missing cases
        assert(false);
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
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
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

    double sidestake_allocation = 0.0;

    std::string sidestake_description = description.toStdString();

    m_edit_status = OK;

    if (!sidestake_address.IsValid()) {
        m_edit_status = INVALID_ADDRESS;
        return QString();
    }

    // Check for duplicate local sidestakes. Here we use the actual core sidestake registry rather than the
    // UI model.
    std::vector<GRC::SideStake_ptr> core_local_sidestake = registry.Try(sidestake_address, true);

    if (!core_local_sidestake.empty()) {
        m_edit_status = DUPLICATE_ADDRESS;
        return QString();
    }

    if (!ParseDouble(allocation.toStdString(), &sidestake_allocation)
        && (sidestake_allocation < 0.0 || sidestake_allocation > 1.0)) {
        m_edit_status = INVALID_ALLOCATION;
        return QString();
    }

    sidestake_allocation /= 100.0;

    registry.NonContractAdd(GRC::SideStake(sidestake_address,
                                           sidestake_allocation,
                                           sidestake_description,
                                           int64_t {0},
                                           uint256 {},
                                           GRC::SideStakeStatus::ACTIVE));

    updateSideStakeTableModel();

    return QString::fromStdString(sidestake_address.ToString());
}

SideStakeTableModel::EditStatus SideStakeTableModel::getEditStatus() const
{
    return m_edit_status;
}

void SideStakeTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    m_priv->refreshSideStakes();
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

static void RwSettingsUpdated(SideStakeTableModel* sidestake_model)
{
    qDebug() << QString("%1").arg(__func__);
    QMetaObject::invokeMethod(sidestake_model, "updateSideStakeTableModel", Qt::QueuedConnection);
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
