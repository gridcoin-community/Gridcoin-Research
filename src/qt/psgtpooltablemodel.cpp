// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "psgtpooltablemodel.h"

#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <key_io.h>
#include <node/psgt_pool.h>
#include <psgt.h>
#include <script/standard.h>
#include <util.h>
#include <wallet/wallet.h>

#include <QDateTime>

#include <algorithm>
#include <variant>

extern CWallet* pwalletMain;

namespace {
//! Newest-first, deduplicated-by-image cap on the recently-completed history.
constexpr int MAX_HISTORY_ROWS = 20;
} // namespace

PSGTPoolTableModel::PSGTPoolTableModel(WalletModel* wallet_model, QObject* parent)
    : QAbstractTableModel(parent)
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

PSGTPoolTableModel::Row PSGTPoolTableModel::MakeRow(const PSGTPoolEntry& entry) const
{
    Row row;
    row.image = entry.image;
    row.revision = entry.revision_hash;
    row.sigs_valid = entry.valid_sigs;
    row.sigs_required = entry.sigs_required;
    row.sigs_total = entry.sigs_total;
    row.time_received = entry.time_received;

    // The payment is the largest output that is NOT change. On a multisig
    // spend the change returns to the arrangement's own P2SH address (the
    // image), and change can exceed the payment -- so exclude any output paying
    // back to the image before taking the largest. If every output pays to the
    // image (degenerate) fall back to the largest overall so the row still
    // shows something.
    CAmount best = 0;
    const CTxOut* payment = nullptr;
    CAmount best_any = 0;
    const CTxOut* payment_any = nullptr;
    for (const CTxOut& txout : entry.psgt.tx.vout) {
        if (txout.nValue >= best_any) {
            best_any = txout.nValue;
            payment_any = &txout;
        }
        CTxDestination dest;
        const bool is_change = ExtractDestination(txout.scriptPubKey, dest)
            && std::holds_alternative<CScriptID>(dest)
            && std::get<CScriptID>(dest) == entry.image;
        if (!is_change && txout.nValue >= best) {
            best = txout.nValue;
            payment = &txout;
        }
    }
    if (!payment) payment = payment_any; // all outputs were change (degenerate)
    if (payment) {
        row.amount = payment->nValue;
        CTxDestination dest;
        row.destination = ExtractDestination(payment->scriptPubKey, dest)
                              ? QString::fromStdString(EncodeDestination(dest))
                              : QObject::tr("(nonstandard)");
    }

    // Wallet relevance: does this wallet hold a key of the arrangement?
    if (pwalletMain && !entry.psgt.inputs.empty()) {
        txnouttype script_type;
        std::vector<std::vector<unsigned char>> vSolutions;
        if (Solver(entry.psgt.inputs[0].redeem_script, script_type, vSolutions)
            && script_type == TX_MULTISIG) {
            LOCK(pwalletMain->cs_wallet);
            for (unsigned int i = 1; i + 1 < vSolutions.size(); ++i) {
                const CPubKey pubkey(vSolutions[i]);
                if (pubkey.IsValid() && pwalletMain->HaveKey(pubkey.GetID())) {
                    row.is_mine = true;
                    break;
                }
            }
        }
    }

    const bool awaiting_me = row.is_mine && pwalletMain
        && !WITH_LOCK(pwalletMain->cs_wallet, return PSGTSignedBy(*pwalletMain, entry.psgt));
    row.status = awaiting_me ? RowStatus::AwaitingYourSignature : RowStatus::AwaitingOthers;

    return row;
}

void PSGTPoolTableModel::refresh()
{
    beginResetModel();
    m_rows.clear();

    for (const PSGTPoolEntry& entry : g_psgt_pool.GetAll()) {
        m_rows.push_back(MakeRow(entry));
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
        uint256 removed;
        removed.SetHex(revision_hash.toStdString());

        for (const Row& row : m_rows) {
            if (row.in_pool && row.is_mine && row.revision == removed) {
                Row completed = row;
                // Map the pool's removal reason (int, see ui_interface) to a
                // specific terminal status so the history row is meaningful.
                switch (static_cast<PSGTRemovalReason>(reason)) {
                case PSGTRemovalReason::EXPIRED:          completed.status = RowStatus::Expired;    break;
                case PSGTRemovalReason::COMPLETED:        completed.status = RowStatus::Completed;   break;
                case PSGTRemovalReason::CONFLICT_MEMPOOL:
                case PSGTRemovalReason::CONFLICT_BLOCK:   completed.status = RowStatus::Conflicted;  break;
                default:                                  completed.status = RowStatus::Removed;     break;
                }
                completed.in_pool = false;
                // Newest first; dedupe by image; bound the length.
                for (int i = m_history.size() - 1; i >= 0; --i) {
                    if (m_history.at(i).image == completed.image) m_history.removeAt(i);
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
            // Show the arrangement's P2SH address, not the raw CScriptID hex.
            // row.image.ToString() is the uint160 in reversed-byte form, which a
            // user cannot correlate with the multisig address; encode it as the
            // recognizable P2SH address instead. The raw image hash remains in
            // the Details dialog and the RPC.
            return QString::fromStdString(EncodeDestination(CTxDestination(row.image)));
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
