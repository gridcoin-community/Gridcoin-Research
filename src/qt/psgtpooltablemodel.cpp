// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "psgtpooltablemodel.h"

#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <key_io.h>
#include <psgt.h>
#include <script/standard.h>
#include <util.h>
#include <wallet/wallet.h>

#include <QDateTime>

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

    // Largest output is the payment; the rest is (usually) change.
    CAmount best = 0;
    const CTxOut* payment = nullptr;
    for (const CTxOut& txout : entry.psgt.tx.vout) {
        if (txout.nValue >= best) {
            best = txout.nValue;
            payment = &txout;
        }
    }
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

void PSGTPoolTableModel::handlePoolChanged(quint8 change_type)
{
    // On a removal, capture the wallet-relevant entry as history before it is
    // gone from the pool. CT_DELETED is the removal case (see ui_interface
    // ChangeType); the pool has already erased it, so we cannot re-read it and
    // instead rely on any matching current row we still hold.
    if (change_type == CT_DELETED) {
        for (const Row& row : m_rows) {
            if (row.in_pool && row.is_mine && !g_psgt_pool.Get(row.image)) {
                Row completed = row;
                completed.status = RowStatus::Complete;
                completed.in_pool = false;
                // Newest first; dedupe by image; bound the length.
                for (int i = m_history.size() - 1; i >= 0; --i) {
                    if (m_history.at(i).image == completed.image) m_history.removeAt(i);
                }
                m_history.prepend(completed);
                while (m_history.size() > MAX_HISTORY_ROWS) {
                    m_history.removeLast();
                }
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
            case RowStatus::Complete:              return tr("Completed");
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
            if (!row.in_pool) return tr("just now");
            const qint64 secs = QDateTime::currentSecsSinceEpoch() - row.time_received;
            if (secs < 60) return tr("%n second(s)", nullptr, (int)secs);
            if (secs < 3600) return tr("%n minute(s)", nullptr, (int)(secs / 60));
            if (secs < 86400) return tr("%n hour(s)", nullptr, (int)(secs / 3600));
            return tr("%n day(s)", nullptr, (int)(secs / 86400));
        }
        case Image:
            return QString::fromStdString(row.image.ToString());
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
    case Image:       return tr("Multisig image");
    }
    return QVariant();
}
