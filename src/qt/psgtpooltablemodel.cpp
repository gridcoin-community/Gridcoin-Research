// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "psgtpooltablemodel.h"

#include "bitcoinunits.h"
#include "interfaces/psgt.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QDateTime>

#include <algorithm>

namespace {
//! Newest-first, deduplicated-by-image cap on the recently-completed history.
constexpr int MAX_HISTORY_ROWS = 20;
} // namespace

PSGTPoolTableModel::PSGTPoolTableModel(interfaces::PSGTPoolContext& psgt_context,
                                       WalletModel* wallet_model, QObject* parent)
    : QAbstractTableModel(parent)
    , m_psgt_context(psgt_context)
    , m_wallet_model(wallet_model)
{
    refresh();
}

int PSGTPoolTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int PSGTPoolTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

const PSGTPoolTableModel::Row* PSGTPoolTableModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) return nullptr;
    return &m_rows.at(row);
}

PSGTPoolTableModel::Row PSGTPoolTableModel::MapRow(const interfaces::PSGTPoolRow& src) const
{
    // The wallet-relevance and payment-destination derivation now happens
    // node-side (interfaces::PSGTPoolContext::entries); this only maps the value
    // row to the display Row.
    Row row;
    row.image_hex = src.image_hex;
    row.image_address = src.image_address;
    row.revision_hex = src.revision_hex;
    row.sigs_valid = src.valid_sigs;
    row.sigs_required = src.sigs_required;
    row.sigs_total = src.sigs_total;
    row.time_received = src.time_received;
    row.in_pool = src.in_pool;
    row.amount = src.amount;
    row.destination = src.destination.empty()
                          ? QObject::tr("(nonstandard)")
                          : QString::fromStdString(src.destination);
    row.is_mine = src.relevance != interfaces::PSGTRelevance::NOT_MINE;
    row.status = (src.relevance == interfaces::PSGTRelevance::MINE_AWAITING_YOU)
                     ? RowStatus::AwaitingYourSignature
                     : RowStatus::AwaitingOthers;
    return row;
}

void PSGTPoolTableModel::refresh()
{
    beginResetModel();
    m_rows.clear();

    for (const interfaces::PSGTPoolRow& src : m_psgt_context.entries()) {
        m_rows.push_back(MapRow(src));
    }

    // Append the recently-completed history (rows no longer pooled).
    for (const Row& hist : m_history) {
        m_rows.push_back(hist);
    }

    endResetModel();
}

void PSGTPoolTableModel::handlePoolChanged(const QString& revision_hash, quint8 change_type, int reason)
{
    // On a removal, capture the wallet-relevant entry as history before it is
    // gone from the view. CT_DELETED is the removal case (see ui_interface
    // ChangeType). The signal carries the exact revision hash that left, so we
    // relocate that specific row rather than scanning by pool membership --
    // scanning could mis-order or re-promote rows when several of ours are
    // pending. Prior CT_UPDATED events have already refresh()ed the cached
    // rows, so a live row's revision matches the pool's current revision.
    if (change_type == CT_DELETED) {
        const std::string removed = revision_hash.toStdString();

        for (const Row& row : m_rows) {
            if (row.in_pool && row.is_mine && row.revision_hex == removed) {
                Row completed = row;
                // Map the pool's removal reason (int, see ui_interface) to a
                // specific terminal status so the history row is meaningful.
                switch (static_cast<interfaces::PSGTRemovalReason>(reason)) {
                case interfaces::PSGTRemovalReason::EXPIRED:          completed.status = RowStatus::Expired;    break;
                case interfaces::PSGTRemovalReason::COMPLETED:        completed.status = RowStatus::Completed;   break;
                case interfaces::PSGTRemovalReason::CONFLICT_MEMPOOL:
                case interfaces::PSGTRemovalReason::CONFLICT_BLOCK:   completed.status = RowStatus::Conflicted;  break;
                default:                                              completed.status = RowStatus::Removed;     break;
                }
                completed.in_pool = false;
                // Newest first; dedupe by image; bound the length.
                for (int i = m_history.size() - 1; i >= 0; --i) {
                    if (m_history.at(i).image_hex == completed.image_hex) m_history.removeAt(i);
                }
                m_history.prepend(completed);
                while (m_history.size() > MAX_HISTORY_ROWS) {
                    m_history.removeLast();
                }
                break;
            }
        }
    }

    refresh();
}

QVariant PSGTPoolTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();

    const Row& row = m_rows.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Status:
            switch (row.status) {
            case RowStatus::AwaitingYourSignature: return tr("Awaiting your signature");
            case RowStatus::AwaitingOthers:        return tr("Awaiting others");
            // History rows, labelled from the pool's removal reason.
            case RowStatus::Expired:               return tr("Expired");
            case RowStatus::Completed:             return tr("Completed");
            case RowStatus::Conflicted:            return tr("Conflict");
            case RowStatus::Removed:               return tr("Removed");
            }
            return QVariant();
        case Destination:
            return row.destination;
        case Amount: {
            const int unit = (m_wallet_model && m_wallet_model->getOptionsModel())
                                 ? m_wallet_model->getOptionsModel()->getDisplayUnit()
                                 : BitcoinUnits::BTC;
            return BitcoinUnits::formatWithUnit(unit, row.amount);
        }
        case Signatures:
            return QStringLiteral("%1 / %2 (of %3)")
                .arg(row.sigs_valid).arg(row.sigs_required).arg(row.sigs_total);
        case Age: {
            // Compute the age from time_received for history rows too (a
            // constant "just now" grows stale), and clamp to >= 0 so a local
            // clock behind time_received never renders "-3 second(s)".
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            const qint64 secs = std::max<qint64>(0, now - row.time_received);
            if (secs < 60) return tr("%n second(s)", nullptr, (int)secs);
            if (secs < 3600) return tr("%n minute(s)", nullptr, (int)(secs / 60));
            if (secs < 86400) return tr("%n hour(s)", nullptr, (int)(secs / 3600));
            return tr("%n day(s)", nullptr, (int)(secs / 86400));
        }
        case Image:
            // The arrangement's P2SH address (precomputed node-side), not the raw
            // image hash, which a user cannot correlate with the multisig address.
            // The raw image hash remains the command id and the RPC key.
            return QString::fromStdString(row.image_address);
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == Amount || index.column() == Signatures) {
            return (int)(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return QVariant();
}

QVariant PSGTPoolTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();

    switch (section) {
    case Status:      return tr("Status");
    case Destination: return tr("Destination");
    case Amount:      return tr("Amount");
    case Signatures:  return tr("Signatures");
    case Age:         return tr("Age");
    case Image:       return tr("Multisig address");
    }
    return QVariant();
}
