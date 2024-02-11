// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guiutil.h"
#include "qt/voting/polltablemodel.h"
#include "qt/voting/votingmodel.h"
#include "logging.h"
#include "util.h"
#include "util/threadnames.h"

#include <QtConcurrentRun>
#include <QSortFilterProxyModel>
#include <QStringList>

using namespace GRC;

PollTableDataModel::PollTableDataModel()
{
    qRegisterMetaType<QList<QPersistentModelIndex>>();
    qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>();

    m_columns
        << tr("Title")
        << tr("Poll Type")
        << tr("Duration")
        << tr("Expiration")
        << tr("Weight Type")
        << tr("Votes")
        << tr("Total Weight")
        << tr("% of Active Vote Weight")
        << tr("Validated")
        << tr("Top Answer")
        << tr("Stale Results");
}

int PollTableDataModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

int PollTableDataModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_columns.size();
}

QVariant PollTableDataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const PollItem* row = static_cast<const PollItem*>(index.internalPointer());

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case PollTableModel::Title:
                    return row->m_title;
                case PollTableModel::PollType:
                    if (row->m_version >= 3) {
                        return row->m_type_str;
                    } else {
                        return QString{};
                    }
                case PollTableModel::Duration:
                    return row->m_duration;
                case PollTableModel::Expiration:
                    return GUIUtil::dateTimeStr(row->m_expiration);
                case PollTableModel::WeightType:
                    return row->m_weight_type_str;
                case PollTableModel::TotalVotes:
                    return row->m_total_votes;
                case PollTableModel::TotalWeight:
                    return QString::number(row->m_total_weight);
                case PollTableModel::VotePercentAVW:
                    return QString::number(row->m_vote_percent_AVW, 'f', 4);
                case PollTableModel::Validated:
                    return row->m_validated;
                case PollTableModel::TopAnswer:
                    return row->m_top_answer;
                case PollTableModel::StaleResults:
                    return row->m_stale;
            } // no default case, so the compiler can warn about missing cases
            assert(false);

        case Qt::TextAlignmentRole:
            switch (index.column()) {
                case PollTableModel::Duration:
                    // Pass-through case
                case PollTableModel::TotalVotes:
                    // Pass-through case
                case PollTableModel::TotalWeight:
                    // Pass-through case
                case PollTableModel::VotePercentAVW:
                    // Pass-through case
                case PollTableModel::Validated:
                    return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            }
            break;

        case PollTableModel::SortRole:
            switch (index.column()) {
                case PollTableModel::Title:
                    return row->m_title;
                case PollTableModel::PollType:
                    return row->m_type_str;
                case PollTableModel::Duration:
                    return row->m_duration;
                case PollTableModel::Expiration:
                    return row->m_expiration;
                case PollTableModel::WeightType:
                    return row->m_weight_type_str;
                case PollTableModel::TotalVotes:
                    return row->m_total_votes;
                case PollTableModel::TotalWeight:
                    return QVariant::fromValue(row->m_total_weight);
                case PollTableModel::VotePercentAVW:
                    return QVariant::fromValue(row->m_vote_percent_AVW);
                case PollTableModel::Validated:
                    return row->m_validated;
                case PollTableModel::TopAnswer:
                    return row->m_top_answer;
                case PollTableModel::StaleResults:
                    return row->m_stale;
            } // no default case, so the compiler can warn about missing cases
            assert(false);
    }

    return QVariant();
}

QVariant PollTableDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole && section < m_columns.size()) {
            return m_columns[section];
        }
    }

    return QVariant();
}

QModelIndex PollTableDataModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    if (row > static_cast<int>(m_rows.size())) {
        return QModelIndex();
    }

    void* data = static_cast<void*>(const_cast<PollItem*>(&m_rows[row]));

    return createIndex(row, column, data);
}

Qt::ItemFlags PollTableDataModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void PollTableDataModel::reload(std::vector<PollItem> rows)
{
    emit layoutAboutToBeChanged();
    m_rows = std::move(rows);
    emit layoutChanged();
}

void PollTableDataModel::handlePollStaleFlag(QString poll_txid_string)
{
    emit layoutAboutToBeChanged();

    for (auto& iter : m_rows) {
        if (iter.m_id == poll_txid_string) {
            iter.m_stale = true;
        }
    }

    emit layoutChanged();
}

// -----------------------------------------------------------------------------
// Class: PollTableModel
// -----------------------------------------------------------------------------

PollTableModel::PollTableModel(QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_data_model(new PollTableDataModel())
    , m_filter_flags(GRC::PollFilterFlag::NO_FILTER)
{
    setSourceModel(m_data_model.get());
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(ColumnIndex::Title);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortRole(SortRole);
}

PollTableModel::~PollTableModel()
{
    // Nothing to do yet...
}

void PollTableModel::setModel(VotingModel* model)
{
    m_voting_model = model;

    // Connect poll stale handler to newVoteReceived signal from voting model, which propagates
    // from the core.
    connect(m_voting_model, &VotingModel::newVoteReceived, this, &PollTableModel::handlePollStaleFlag);
}

void PollTableModel::setPollFilterFlags(PollFilterFlag flags)
{
    m_filter_flags = flags;
}

bool PollTableModel::includesActivePolls() const
{
    return (m_filter_flags & PollFilterFlag::ACTIVE) != 0;
}

int PollTableModel::size() const
{
    return m_data_model->rowCount(QModelIndex());
}

bool PollTableModel::empty() const
{
    return size() == 0;
}

QString PollTableModel::columnName(int offset) const
{
    return m_data_model->headerData(offset, Qt::Horizontal, Qt::DisplayRole).toString();
}

const PollItem* PollTableModel::rowItem(int row) const
{
    QModelIndex index = this->index(row, 0, QModelIndex());
    index = mapToSource(index);

    return static_cast<PollItem*>(index.internalPointer());
}

void PollTableModel::refresh()
{
    if (!m_voting_model || !m_refresh_mutex.tryLock()) {
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: m_refresh_mutex is already taken, so tryLock failed",
                 __func__);

        return;
    } else {
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: m_refresh_mutex trylock succeeded.",
                 __func__);
    }

    QtConcurrent::run([this]() {
        RenameThread("PollTableModel_refresh");
        util::ThreadSetInternalName("PollTableModel_refresh");

        static_cast<PollTableDataModel*>(m_data_model.get())
            ->reload(m_voting_model->buildPollTable(m_filter_flags));

        m_refresh_mutex.unlock();
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: m_refresh_mutex lock released.",
                 __func__);
    });
}

void PollTableModel::handlePollStaleFlag(QString poll_txid_string)
{
    m_data_model->handlePollStaleFlag(poll_txid_string);

    emit newVoteReceivedAndPollMarkedDirty();
}

void PollTableModel::changeTitleFilter(const QString& pattern)
{
    emit layoutAboutToBeChanged();
    setFilterFixedString(pattern);
    emit layoutChanged();
}

Qt::SortOrder PollTableModel::custom_sort(int column)
{
    if (sortColumn() == column) {
        QSortFilterProxyModel::sort(column, static_cast<Qt::SortOrder>(!sortOrder()));
    } else {
        QSortFilterProxyModel::sort(column, Qt::AscendingOrder);
    }

    return sortOrder();
}
