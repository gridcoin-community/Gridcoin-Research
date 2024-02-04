// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guiutil.h"
#include "qt/voting/additionalfieldstablemodel.h"
#include "qt/voting/votingmodel.h"

#include <QtConcurrentRun>
#include <QStringList>

using namespace GRC;

namespace {
class AdditionalFieldsTableDataModel : public QAbstractTableModel
{
public:
    AdditionalFieldsTableDataModel()
    {
        qRegisterMetaType<QList<QPersistentModelIndex>>();
        qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>();

        m_columns
                << tr("Name")
                << tr("Value")
                << tr("Required");
    }

    int rowCount(const QModelIndex &parent) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return m_rows.size();
    }

    int columnCount(const QModelIndex &parent) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return m_columns.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid()) {
            return QVariant();
        }

        const AdditionalFieldEntry* row = static_cast<const AdditionalFieldEntry*>(index.internalPointer());

        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case AdditionalFieldsTableModel::Name:
                return row->m_name;
            case AdditionalFieldsTableModel::Value:
                return row->m_value;
            case AdditionalFieldsTableModel::Required:
                return row->m_required;
            } // no default case, so the compiler can warn about missing cases
            assert(false);

        case AdditionalFieldsTableModel::SortRole:
            switch (index.column()) {
            case AdditionalFieldsTableModel::Name:
                return row->m_name;
            case AdditionalFieldsTableModel::Value:
                return row->m_value;
            case AdditionalFieldsTableModel::Required:
                return row->m_required;
            } // no default case, so the compiler can warn about missing cases
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal) {
            if (role == Qt::DisplayRole && section < m_columns.size()) {
                return m_columns[section];
            }
        }

        return QVariant();
    }

    QModelIndex index(int row, int column, const QModelIndex &parent) const override
    {
        Q_UNUSED(parent);

        if (row > static_cast<int>(m_rows.size())) {
            return QModelIndex();
        }

        void* data = static_cast<void*>(const_cast<AdditionalFieldEntry*>(&m_rows[row]));

        return createIndex(row, column, data);
    }

    void reload(std::vector<AdditionalFieldEntry> rows)
    {
        emit layoutAboutToBeChanged();
        m_rows = std::move(rows);
        emit layoutChanged();
    }

private:
    QStringList m_columns;
    std::vector<AdditionalFieldEntry> m_rows;
}; // AdditionalFieldsTableDataModel
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: AdditionalFieldsTableModel
// -----------------------------------------------------------------------------

AdditionalFieldsTableModel::AdditionalFieldsTableModel(QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_data_model(new AdditionalFieldsTableDataModel())
{
    setSourceModel(m_data_model.get());
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseSensitive);
    setFilterKeyColumn(ColumnIndex::Name);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortRole(SortRole);
}

AdditionalFieldsTableModel::~AdditionalFieldsTableModel()
{
}

void AdditionalFieldsTableModel::setPollItem(const PollItem* poll_item)
{
    m_poll_item = poll_item;
}

int AdditionalFieldsTableModel::size() const
{
    return m_data_model->rowCount(QModelIndex());
}

bool AdditionalFieldsTableModel::empty() const
{
    return size() == 0;
}

QString AdditionalFieldsTableModel::columnName(int offset) const
{
    return m_data_model->headerData(offset, Qt::Horizontal, Qt::DisplayRole).toString();
}

const AdditionalFieldEntry* AdditionalFieldsTableModel::rowItem(int row) const
{
    QModelIndex index = this->index(row, 0, QModelIndex());
    index = mapToSource(index);

    return static_cast<AdditionalFieldEntry*>(index.internalPointer());
}

void AdditionalFieldsTableModel::refresh()
{
    if (!m_poll_item) {
        return;
    }

        std::vector<AdditionalFieldEntry> additional_fields;

        for (const auto& iter : m_poll_item->m_additional_field_entries) {
            additional_fields.push_back(iter);
        }

        static_cast<AdditionalFieldsTableDataModel*>(m_data_model.get())
            ->reload(additional_fields);
}

Qt::SortOrder AdditionalFieldsTableModel::custom_sort(int column)
{
    if (sortColumn() == column) {
        QSortFilterProxyModel::sort(column, static_cast<Qt::SortOrder>(!sortOrder()));
    } else {
        QSortFilterProxyModel::sort(column, Qt::AscendingOrder);
    }

    return sortOrder();
}
