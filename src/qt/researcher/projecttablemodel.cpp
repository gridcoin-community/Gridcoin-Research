// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/researcher.h"

#include "qt/researcher/projecttablemodel.h"
#include "qt/researcher/researchermodel.h"

#include <QIcon>

namespace {
//!
//! \brief Compares rows in a project table for sorting order.
//!
class CompareRowLessThan
{
public:
    CompareRowLessThan(int column, Qt::SortOrder order)
        : m_column(column)
        , m_order(order)
    {
    }

    bool operator()(const ProjectRow& left, const ProjectRow& right) const
    {
        const ProjectRow* pLeft = &left;
        const ProjectRow* pRight = &right;

        if (m_order == Qt::DescendingOrder) {
            std::swap(pLeft, pRight);
        }

        switch (m_column) {
            case ProjectTableModel::Name:
                return pLeft->m_name < pRight->m_name;
            case ProjectTableModel::Eligible:
                return pLeft->m_error < pRight->m_error;
            case ProjectTableModel::Whitelisted:
                return pLeft->m_whitelisted < pRight->m_whitelisted;
            case ProjectTableModel::Magnitude:
                return pLeft->m_magnitude < pRight->m_magnitude;
            case ProjectTableModel::RecentAverageCredit:
                return pLeft->m_rac < pRight->m_rac;
            case ProjectTableModel::Cpid:
                return pLeft->m_cpid < pRight->m_cpid;
        }

        return false;
    }

private:
    int m_column;
    Qt::SortOrder m_order;
}; // CompareRowLessThan
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: ProjectTableData
// -----------------------------------------------------------------------------

class ProjectTableData
{
public:
    ProjectTableData()
        : m_sort_column(static_cast<int>(ProjectTableModel::ColumnIndex::Name))
    {
    }

    size_t size() const
    {
        return m_rows.size();
    }

    ProjectRow* index(const int row)
    {
        if (row > static_cast<int>(m_rows.size())) {
            return nullptr;
        }

        return &m_rows[row];
    }

    void sort(const int column, const Qt::SortOrder order)
    {
        m_sort_column = column;
        m_sort_order = order;

        DoSort();
    }

    void reload(std::vector<ProjectRow> rows)
    {
        m_rows = std::move(rows);

        DoSort();
    }

private:
    int m_sort_column;
    Qt::SortOrder m_sort_order;
    std::vector<ProjectRow> m_rows;

    void DoSort()
    {
        std::stable_sort(
            m_rows.begin(),
            m_rows.end(),
            CompareRowLessThan(m_sort_column, m_sort_order));
    }
}; // ProjectTableStats

// -----------------------------------------------------------------------------
// Class: ProjectTableModel
// -----------------------------------------------------------------------------

ProjectTableModel::ProjectTableModel(ResearcherModel *model, const bool extended)
    : m_model(model)
    , m_data(new ProjectTableData())
    , m_extended(extended)
{
    m_columns
        << tr("Name")
        << tr("Eligible")
        << tr("Whitelist")
        << tr("Magnitude")
        << tr("Avg. Credit")
        << tr("CPID");
}

ProjectTableModel::~ProjectTableModel()
{
    // Nothing to do yet...
}

int ProjectTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_data->size();
}

int ProjectTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_columns.size();
}

QVariant ProjectTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const ProjectRow* row = static_cast<const ProjectRow*>(index.internalPointer());

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case Name:
                    return row->m_name;
                case Eligible:
                    if (!row->m_error.isEmpty()) {
                        return row->m_error;
                    }
                    break;
                case Cpid:
                    return row->m_cpid;
                case Magnitude:
                    return row->m_magnitude;
                case RecentAverageCredit:
                    return row->m_rac;
            }
            break;

        case Qt::DecorationRole:
            switch (index.column()) {
                case Eligible:
                    if (row->m_error.isEmpty()) {
                        return QIcon(":/icons/synced");
                    }
                    break;
                case Whitelisted:
                    if (row->m_whitelisted) {
                        return QIcon(":/icons/synced");
                    }
                    break;
            }
            break;

        case Qt::TextAlignmentRole:
            switch (index.column()) {
                case Magnitude:
                    // Pass-through case
                case RecentAverageCredit:
                    return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            }
            break;
    }

    return QVariant();
}

QVariant ProjectTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole && section < m_columns.size()) {
            return m_columns[section];
        }
    }

    return QVariant();
}

QModelIndex ProjectTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    ProjectRow* data = m_data->index(row);

    if (data) {
        return createIndex(row, column, data);
    }

    return QModelIndex();
}

Qt::ItemFlags ProjectTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void ProjectTableModel::sort(int column, Qt::SortOrder order)
{
    emit layoutAboutToBeChanged();
    m_data->sort(column, order);
    emit layoutChanged();
}

void ProjectTableModel::refresh()
{
    emit layoutAboutToBeChanged();
    m_data->reload(m_model->buildProjectTable(m_extended));
    emit layoutChanged();
}
