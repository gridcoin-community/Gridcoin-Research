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
#include <QPointer>
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

        case Qt::AccessibleTextRole:
            // For screen readers (issue #2604). Title column returns a one-shot
            // row summary so a screen reader announces the most-important fields
            // (title, type, expiration, top answer) at row entry; other columns
            // prepend the header name so per-cell navigation is self-contained.
            switch (index.column()) {
                case PollTableModel::Title:
                    return tr("Poll \"%1\", type %2, expires %3, top answer %4")
                        .arg(row->m_title,
                             row->m_version >= 3 ? row->m_type_str : tr("(legacy)"),
                             GUIUtil::dateTimeStr(row->m_expiration),
                             row->m_top_answer.isEmpty() ? tr("none") : row->m_top_answer);
                case PollTableModel::PollType:
                    return tr("Poll type: %1")
                        .arg(row->m_version >= 3 ? row->m_type_str : tr("(legacy)"));
                case PollTableModel::Duration:
                    return tr("Duration: %1").arg(QString::number(row->m_duration));
                case PollTableModel::Expiration:
                    return tr("Expires: %1").arg(GUIUtil::dateTimeStr(row->m_expiration));
                case PollTableModel::WeightType:
                    return tr("Weight type: %1").arg(row->m_weight_type_str);
                case PollTableModel::TotalVotes:
                    return tr("Total votes: %1").arg(QString::number(row->m_total_votes));
                case PollTableModel::TotalWeight:
                    return tr("Total weight: %1").arg(QString::number(row->m_total_weight));
                case PollTableModel::VotePercentAVW:
                    return tr("Percent of active vote weight: %1")
                        .arg(QString::number(row->m_vote_percent_AVW, 'f', 4));
                case PollTableModel::Validated:
                    return tr("Validated: %1").arg(row->m_validated.toString());
                case PollTableModel::TopAnswer:
                    return tr("Top answer: %1")
                        .arg(row->m_top_answer.isEmpty() ? tr("none") : row->m_top_answer);
                case PollTableModel::StaleResults:
                    return tr("Stale results: %1").arg(row->m_stale ? tr("yes") : tr("no"));
            } // no default case, so the compiler can warn about missing cases
            assert(false);

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
    // Block until any in-flight refresh worker is done dereferencing `this`.
    // refresh() captures `this` directly to read m_voting_model /
    // m_filter_flags / m_data_model and to write m_refresh_in_flight; without
    // this wait the worker can touch those members after PollTableModel is
    // destroyed (use-after-free during GUI shutdown). A default-constructed
    // QFuture is already in the finished state, so this is a no-op when no
    // refresh has ever been launched. (The queued reload-on-the-GUI-thread
    // lambda guards its own captures with QPointer, so it remains safe even
    // if the event queue drains after the model is gone.)
    m_refresh_future.waitForFinished();
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
    bool expected = false;
    if (!m_voting_model ||
        !m_refresh_in_flight.compare_exchange_strong(expected, true)) {
        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: refresh already in flight, skipping",
                 __func__);

        return;
    }

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: refresh dispatched", __func__);

    m_refresh_future = QtConcurrent::run([this]() {
        RenameThread("PollTableModel_refresh");
        util::ThreadSetInternalName("PollTableModel_refresh");

        // Build the new poll table off the GUI thread (this is the slow
        // bit -- AVW recompute, candidate iteration).
        std::vector<PollItem> new_rows = m_voting_model->buildPollTable(m_filter_flags);

        // Hand the result back to the GUI thread for the actual model
        // mutation. m_rows is the std::vector backing a QAbstractItemModel
        // the views read concurrently via rowCount() / index() / data();
        // mutating it (and emitting layoutAboutToBeChanged / layoutChanged)
        // from this worker violates Qt model thread-affinity -- TSan G16
        // reports a real race against PollTableDataModel::rowCount on the
        // main thread. A QueuedConnection invoke onto m_data_model (whose
        // thread affinity is the GUI thread) runs reload() in the GUI
        // event loop, where every other access already happens.
        //
        // Both captures are wrapped in QPointer so the queued lambda is safe
        // even if PollTableModel / PollTableDataModel are destroyed before
        // the GUI event loop processes the post (e.g. shutdown race after
        // the worker thread has already exited). m_refresh_in_flight is
        // cleared in the GUI continuation -- after reload() actually runs --
        // so the debounce covers build + queued mutation, not just the
        // build; a stream of refresh() calls collapses to one full cycle
        // at a time.
        QMetaObject::invokeMethod(
            m_data_model.get(),
            [data_model = QPointer<PollTableDataModel>(
                 static_cast<PollTableDataModel*>(m_data_model.get())),
             self = QPointer<PollTableModel>(this),
             rows = std::move(new_rows)]() mutable {
                if (data_model) {
                    data_model->reload(std::move(rows));
                }
                if (self) {
                    self->m_refresh_in_flight.store(false);
                }
            },
            Qt::QueuedConnection);

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: refresh worker complete", __func__);
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
