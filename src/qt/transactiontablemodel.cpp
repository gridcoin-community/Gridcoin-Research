#include "transactiontablemodel.h"
#include "guiutil.h"
#include "transactionrecord.h"
#include "guiconstants.h"
#include "transactiondesc.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "main.h"
#include "wallet/wallet.h"
#include "node/ui_interface.h"
#include "util.h"

#include <QLocale>
#include <QList>
#include <QColor>
#include <QIcon>
#include <QDateTime>

#include <cassert>
#include <cstddef>
#include <vector>

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignRight|Qt::AlignVCenter
    };

// The cached-wallet ORDERING (time DESC, hash ASC, idx ASC) and the position /
// contiguous-range computation now live producer-side in GRC::WalletTxStore
// (src/qt/wallettxstore.{h,cpp}), built on the Qt-free GRC::TxOrderLess
// (src/qt/txorder.h). This model is a thin consumer: it keeps a replica that
// it mutates in lockstep from the producer's position-stamped RowsInserted /
// RowsRemoved events — it never computes positions itself.

// Private implementation
class TransactionTablePriv
{
public:
    TransactionTablePriv(CWallet *wallet, WalletModel *walletModel, TransactionTableModel *parent):
            wallet(wallet),
            walletModel(walletModel),
            parent(parent)
    {
    }
    CWallet *wallet;
    WalletModel *walletModel;
    TransactionTableModel *parent;

    //!
    //! \brief Consumer-side replica of the ordered wallet view, kept in
    //! lockstep with the producer-owned GRC::WalletTxStore by replaying its
    //! position-stamped RowsInserted / RowsRemoved events. Each entry is one
    //! row of the table. This is Qt-thread-exclusive (only data(),
    //! applyEventBatch, and loadWallet touch it, all on the Qt main thread),
    //! so it needs no lock. The producer's authoritative ordering store is a
    //! separate object guarded by cs_store; the consumer never reads it on the
    //! render path. (The replica shrinks to a viewport slice in the windowing
    //! step, PR5.)
    //!
    std::vector<TransactionRecord> cachedWallet;

    /* Query entire wallet anew from core, via the producer store. */
    void loadWallet()
    {
        // reloadAndSnapshot rebuilds the producer-side ordering store (under
        // cs_main + cs_wallet, blocking producers and discarding any stale
        // queued events) and returns a fresh, fully decomposed, sorted snapshot
        // for this replica. The datetime-display cutoff is applied store-side.
        const bool fLimitTxnDisplay = walletModel->getOptionsModel()->getLimitTxnDisplay();
        const int64_t limitTxnDateTime = walletModel->getOptionsModel()->getLimitTxnDateTime();
        cachedWallet = walletModel->getTxStore().reloadAndSnapshot(fLimitTxnDisplay, limitTxnDateTime);
    }


    /* Reset the cached transaction list with a fresh query from core.
     */
    void refreshWallet()
    {
        LogPrint(BCLog::MISC, "refreshWallet()");

        parent->beginResetModel();

        loadWallet();

        parent->endResetModel();
    }

    //!
    //! \brief Apply a batch of WalletEvents drained from the producer→GUI
    //! queue. This is the new incremental update path; it runs on the Qt
    //! main thread and does NOT take cs_main or cs_wallet.
    //!
    //! - RowsInserted payloads carry a producer-computed position and the
    //!   decomposed records (already datetime-filtered and de-duped store-side).
    //!   The consumer splices them into its replica at exactly that position
    //!   inside one beginInsertRows/endInsertRows bracket. It reads ONLY the
    //!   payload, never the producer store, so the replica tracks the begin/end
    //!   replay even when a drain batch contains several inserts that reorder
    //!   rows — which keeps the QSortFilterProxyModel on top consistent.
    //!
    //! - RowsRemoved payloads carry a producer-computed position + count for a
    //!   contiguous run; the consumer erases that range in one
    //!   beginRemoveRows/endRemoveRows bracket.
    //!
    //! - ChainTipChanged: no row mutation. Per-row status (confirmations) is
    //!   refreshed lazily on read in index(), driven once per batch by
    //!   updateConfirmations() at the WalletModel drain level.
    //!
    void applyEventBatch(const std::vector<GRC::WalletEvent>& events)
    {
        for (const auto& ev : events) {
            std::visit([&](auto&& payload) {
                using P = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<P, GRC::RowsInsertedPayload>) {
                    applyRowsInserted(payload);
                } else if constexpr (std::is_same_v<P, GRC::RowsRemovedPayload>) {
                    applyRowsRemoved(payload);
                } else if constexpr (std::is_same_v<P, GRC::ChainTipChangedPayload>) {
                    // Handled at the WalletModel drain level (updateConfirmations
                    // + checkBalanceChanged once per batch). No row mutation.
                }
            }, ev.payload);
        }
    }

    void applyRowsInserted(const GRC::RowsInsertedPayload& payload)
    {
        if (payload.records.empty()) {
            return;
        }

        // The store computes `position` against an ordering that mirrors this
        // replica (both start identical and replay the same position-stamped
        // events in seqno order), so position is provably in range for the
        // current producer. Validate it explicitly anyway: the DEPLOYED build
        // is -DNDEBUG, so an assert here would vanish and a bad position would
        // become a huge size_t -> out-of-bounds insert (heap corruption). If a
        // future producer regression ever emits a bad position, degrade safely
        // — log and skip — rather than corrupt the heap.
        if (payload.position < 0
                || static_cast<std::size_t>(payload.position) > cachedWallet.size()) {
            LogPrintf("ERROR: %s: RowsInserted position %d out of range [0, %u] — skipping",
                      __func__, payload.position,
                      static_cast<unsigned int>(cachedWallet.size()));
            return;
        }
        const std::size_t insertIdx = static_cast<std::size_t>(payload.position);
        const std::size_t insertCount = payload.records.size();

        parent->beginInsertRows(QModelIndex(),
                                static_cast<int>(insertIdx),
                                static_cast<int>(insertIdx + insertCount - 1));
        cachedWallet.insert(cachedWallet.begin() + insertIdx,
                            payload.records.begin(), payload.records.end());
        parent->endInsertRows();
    }

    void applyRowsRemoved(const GRC::RowsRemovedPayload& payload)
    {
        // A RowsRemoved is emitted only for a real, non-empty erase the store
        // located, so the range is provably in this replica. Validate anyway —
        // the deployed -DNDEBUG build drops asserts, and a bad position/count
        // would become a huge size_t -> out-of-bounds erase (heap corruption).
        // Degrade safely on a future producer regression.
        if (payload.position < 0 || payload.count <= 0
                || static_cast<std::size_t>(payload.position)
                       + static_cast<std::size_t>(payload.count) > cachedWallet.size()) {
            LogPrintf("ERROR: %s: RowsRemoved range [%d, +%d) out of bounds for %u rows — skipping",
                      __func__, payload.position, payload.count,
                      static_cast<unsigned int>(cachedWallet.size()));
            return;
        }
        const std::size_t pos = static_cast<std::size_t>(payload.position);
        const std::size_t count = static_cast<std::size_t>(payload.count);

        parent->beginRemoveRows(QModelIndex(),
                                static_cast<int>(pos),
                                static_cast<int>(pos + count - 1));
        cachedWallet.erase(cachedWallet.begin() + pos,
                           cachedWallet.begin() + pos + count);
        parent->endRemoveRows();
    }

    int size()
    {
        return static_cast<int>(cachedWallet.size());
    }

    TransactionRecord *index(int idx)
    {
        if(idx >= 0 && static_cast<std::size_t>(idx) < cachedWallet.size())
        {
            TransactionRecord *rec = &cachedWallet[idx];

            // Get required locks upfront. This avoids the GUI from getting
            // stuck if the core is holding the locks for a longer time - for
            // example, during a wallet rescan.
            //
            // If a status update is needed (blocks came in since last check),
            //  update the status of this transaction from the wallet. Otherwise,
            // simply reuse the cached status.
            TRY_LOCK(cs_main, lockMain);
            if(lockMain)
            {
                TRY_LOCK(wallet->cs_wallet, lockWallet);
                if(lockWallet && rec->statusUpdateNeeded())
                {
                    std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);

                    if(mi != wallet->mapWallet.end())
                    {
                        rec->updateStatus(mi->second);
                    }
                }
            }
            return rec;
        }
        else
        {
            return nullptr;
        }
    }

    QString describe(TransactionRecord *rec)
    {
        {
            LOCK2(cs_main, wallet->cs_wallet);
            std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);
            if(mi != wallet->mapWallet.end())
            {
                return TransactionDesc::toHTML(wallet, mi->second, rec->vout);
            }
        }
        return QString("");
    }

};

TransactionTableModel::TransactionTableModel(CWallet* wallet, WalletModel *parent):
        QAbstractTableModel(parent),
        wallet(wallet),
        walletModel(parent),
        priv(new TransactionTablePriv(wallet, walletModel, this))
{
    columns << QString() << tr("Date") << tr("Type") << tr("Address") << tr("Amount");

    priv->loadWallet();

    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &TransactionTableModel::updateDisplayUnit);
    connect(walletModel->getOptionsModel(), &OptionsModel::LimitTxnDisplayChanged, this, &TransactionTableModel::refreshWallet);
}

TransactionTableModel::~TransactionTableModel()
{
    delete priv;
}

void TransactionTableModel::applyEventBatch(const std::vector<GRC::WalletEvent>& events)
{
    LogPrint(BCLog::LogFlags::VERBOSE, "TransactionTableModel::applyEventBatch(%u events)",
             static_cast<unsigned int>(events.size()));

    priv->applyEventBatch(events);
}

void TransactionTableModel::refreshWallet()
{
    priv->refreshWallet();
}

void TransactionTableModel::updateConfirmations()
{
    LogPrint(BCLog::MISC, "TransactionTableModel::updateConfirmations()");

    // Blocks came in since last poll.
    // Invalidate status (number of confirmations) and (possibly) description
    //  for all rows. Qt is smart enough to only actually request the data for the
    //  visible rows.
    emit dataChanged(index(0, Status), index(priv->size()-1, Status));
    emit dataChanged(index(0, ToAddress), index(priv->size()-1, ToAddress));
}

int TransactionTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return priv->size();
}

int TransactionTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return columns.length();
}

QString TransactionTableModel::formatTxStatus(const TransactionRecord *wtx) const
{
    QString status;

    switch(wtx->status.status)
    {
    case TransactionStatus::OpenUntilBlock:
        status = tr("Open for %n more block(s)","",wtx->status.open_for);
        break;
    case TransactionStatus::OpenUntilDate:
        status = tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx->status.open_for));
        break;
    case TransactionStatus::Offline:
        status = tr("Offline");
        break;
    case TransactionStatus::Unconfirmed:
        status = tr("Unconfirmed");
        break;
    case TransactionStatus::Confirming:
        status = tr("Confirming (%1 of %2 recommended confirmations)<br>").arg(wtx->status.depth).arg(TransactionRecord::RecommendedNumConfirmations);
        break;
    case TransactionStatus::Confirmed:
        status = tr("Confirmed (%1 confirmations)").arg(wtx->status.depth);
        break;
    case TransactionStatus::Conflicted:
        status = tr("Conflicted");
        break;
    case TransactionStatus::Immature:
        status = tr("Immature (%1 confirmations, will be available after %2)<br>").arg(wtx->status.depth).arg(wtx->status.depth + wtx->status.matures_in);
        break;
    case TransactionStatus::MaturesWarning:
        status = tr("This block was not received by any other nodes<br> and will probably not be accepted!");
        break;
    case TransactionStatus::NotAccepted:
        status = tr("Generated but not accepted");
        break;
    }

    return status;
}

QString TransactionTableModel::formatTxDate(const TransactionRecord *wtx) const
{
    if(wtx->time)
    {
        return GUIUtil::dateTimeStr(wtx->time);
    }
    else
    {
        return QString();
    }
}

/* Look up address in address book, if found return label (address)
   otherwise just return (address)
 */
QString TransactionTableModel::lookupAddress(const std::string &address, bool tooltip) const
{
    QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));
    QString description;
    if(!label.isEmpty())
    {
        description += label + QString(" ");
    }

	if(label.isEmpty() || walletModel->getOptionsModel()->getDisplayAddresses() || tooltip)
    {
        description += QString::fromStdString(address);
    }

    return description;
}

QString TransactionTableModel::formatTxType(const TransactionRecord *wtx) const
{
    switch(wtx->type)
    {
    case TransactionRecord::RecvWithAddress:
        return tr("Received with");
    case TransactionRecord::RecvFromOther:
        return tr("Received from");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return tr("Sent to");
    case TransactionRecord::SendToSelf:
        return tr("Payment to yourself");
    case TransactionRecord::Generated:
        {
            switch (wtx->status.generated_type)
            {
            case GRC::MinedType::POS:
                return tr("Mined - PoS");
            case GRC::MinedType::POR:
                return tr("Mined - PoS+RR");
            case GRC::MinedType::ORPHANED:
                return tr("Mined - Orphaned");
            case GRC::MinedType::POS_SIDE_STAKE_RCV:
                return tr("PoS Side Stake Received");
            case GRC::MinedType::POR_SIDE_STAKE_RCV:
                return tr("PoS+RR Side Stake Received");
            case GRC::MinedType::POS_SIDE_STAKE_SEND:
                return tr("PoS Side Stake Sent");
            case GRC::MinedType::POR_SIDE_STAKE_SEND:
                return tr("PoS+RR Side Stake Sent");
            case GRC::MinedType::MRC_RCV:
                return tr("MRC Payment Received");
            case GRC::MinedType::MRC_SEND:
                return tr("MRC Payment Sent");
            case GRC::MinedType::SUPERBLOCK:
                return tr("Mined - Superblock");
            default:
                return tr("Mined - Unknown");
            }
    }
    case TransactionRecord::BeaconAdvertisement:
        return tr("Beacon Advertisement");
    case TransactionRecord::Poll:
        return tr("Poll");
    case TransactionRecord::Vote:
        return tr("Vote");
    case TransactionRecord::MRC:
        return tr("Manual Rewards Claim Request");
    case TransactionRecord::Message:
        return tr("Message");
    default:
        return QString();
    }
}


QVariant TransactionTableModel::txAddressDecoration(const TransactionRecord *wtx) const
{
    switch(wtx->type)
    {
    case TransactionRecord::Generated:
    {
        switch (wtx->status.generated_type)
        {
        case GRC::MinedType::POS:
            return QIcon(":/icons/tx_pos");
        case GRC::MinedType::POR:
            return QIcon(":/icons/tx_por");
        case GRC::MinedType::ORPHANED:
            return QIcon(":/icons/transaction_conflicted");
        case GRC::MinedType::POS_SIDE_STAKE_RCV:
            return QIcon(":/icons/tx_pos_ss");
        case GRC::MinedType::POR_SIDE_STAKE_RCV:
            return QIcon(":/icons/tx_por_ss");
        case GRC::MinedType::POS_SIDE_STAKE_SEND:
            return QIcon(":/icons/tx_pos_ss_sent");
        case GRC::MinedType::POR_SIDE_STAKE_SEND:
            return QIcon(":/icons/tx_por_ss_sent");
        case GRC::MinedType::MRC_RCV:
            return QIcon(":/icons/tx_por_ss");
        case GRC::MinedType::MRC_SEND:
            return QIcon(":/icons/tx_por_ss_sent");
        case GRC::MinedType::SUPERBLOCK:
            return QIcon(":/icons/superblock");
        default:
            return QIcon(":/icons/transaction_0");
        }
    }
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::RecvFromOther:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return QIcon(":/icons/tx_output");
    case TransactionRecord::BeaconAdvertisement:
        return QIcon(":/icons/tx_contract_beacon");
    case TransactionRecord::Poll:
    case TransactionRecord::Vote:
        return QIcon(":/icons/tx_contract_voting");
    case TransactionRecord::MRC:
        return QIcon(":/icons/tx_contract_mrc");
    case TransactionRecord::Message:
        return QIcon(":/icons/message");
    default:
        return QIcon(":/icons/tx_inout");
    }
    return QVariant();
}

QString TransactionTableModel::formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const
{
    switch(wtx->type)
    {
    case TransactionRecord::RecvFromOther:
    case TransactionRecord::SendToOther:
        return QString::fromStdString(wtx->address);
   case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
        return lookupAddress(wtx->address, tooltip);
    case TransactionRecord::SendToSelf:
    case TransactionRecord::Message:
        return lookupAddress(wtx->address, tooltip);
    default:
        return tr("(n/a)");
    }
}

QVariant TransactionTableModel::addressColor(const TransactionRecord *wtx) const
{
    // Show addresses without label in a less visible color
    switch(wtx->type)
    {
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
        {
        QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
        if(label.isEmpty())
            return COLOR_BAREADDRESS;
        } break;
    case TransactionRecord::SendToSelf:
        return COLOR_BAREADDRESS;
    default:
        break;
    }
    return QVariant();
}

QString TransactionTableModel::formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed) const
{
    QString str = BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), wtx->credit + wtx->debit);
    if(showUnconfirmed)
    {
        if(!wtx->status.countsForBalance)
        {
            str = QString("[") + str + QString("]");
        }
    }
    return QString(str);
}

QVariant TransactionTableModel::txStatusDecoration(const TransactionRecord *wtx) const
{
    switch(wtx->status.status)
    {
    case TransactionStatus::OpenUntilBlock:
    case TransactionStatus::OpenUntilDate:
        return QColor(64,64,255);
    case TransactionStatus::Offline:
        return QColor(192,192,192);
    case TransactionStatus::Unconfirmed:
        return QIcon(":/icons/transaction_0");
    case TransactionStatus::Confirming:
        switch(wtx->status.depth)
        {
        case 1: return QIcon(":/icons/transaction_1");
        case 2: return QIcon(":/icons/transaction_2");
        case 3: return QIcon(":/icons/transaction_3");
        case 4: return QIcon(":/icons/transaction_4");
        default: return QIcon(":/icons/transaction_5");
        };
    case TransactionStatus::Confirmed:
        return QIcon(":/icons/transaction_confirmed");
    case TransactionStatus::Conflicted:
        return QIcon(":/icons/transaction_conflicted");
    case TransactionStatus::Immature: {
        int total = wtx->status.depth + wtx->status.matures_in;
        int part = (wtx->status.depth * 4 / total) + 1;
        return QIcon(QString(":/icons/transaction_%1").arg(part));
        }
    case TransactionStatus::MaturesWarning:
    case TransactionStatus::NotAccepted:
        return QIcon(":/icons/transaction_0");
    }
    return QColor(0,0,0);
}

QString TransactionTableModel::formatTooltip(const TransactionRecord *rec) const
{
    QString tooltip = formatTxStatus(rec) + QString("\n") + formatTxType(rec);
    if(rec->type==TransactionRecord::RecvFromOther || rec->type==TransactionRecord::SendToOther ||
       rec->type==TransactionRecord::SendToAddress || rec->type==TransactionRecord::RecvWithAddress)
    {
        tooltip += QString(" ") + formatTxToAddress(rec, true);
    }

    QString explanation = formatTxTypeExplanation(rec);
    if (!explanation.isEmpty()) {
        tooltip += QString(": ") + explanation;
    }

    return tooltip;
}

QString TransactionTableModel::formatTxTypeExplanation(const TransactionRecord *rec) const
{
    if (rec->type == TransactionRecord::Generated) {
        switch (rec->status.generated_type) {
        case GRC::MinedType::POS:
            return tr("Earned by staking a block (Proof of Stake).");
        case GRC::MinedType::POR:
            return tr("Earned by staking a block with BOINC research rewards (Proof of Stake + Research Rewards).");
        case GRC::MinedType::SUPERBLOCK:
            return tr("Earned by staking a superblock.");
        case GRC::MinedType::ORPHANED:
            return tr("This staked block was orphaned and did not become part of the chain.");
        case GRC::MinedType::POS_SIDE_STAKE_RCV:
            return tr("Side stake received from another staker's Proof of Stake reward.");
        case GRC::MinedType::POR_SIDE_STAKE_RCV:
            return tr("Side stake received from another staker's research reward.");
        case GRC::MinedType::POS_SIDE_STAKE_SEND:
            return tr("Your staking reward automatically allocated this side stake; no manual send occurred.");
        case GRC::MinedType::POR_SIDE_STAKE_SEND:
            return tr("Your research staking reward automatically allocated this side stake; no manual send occurred.");
        case GRC::MinedType::MRC_RCV:
            return tr("MRC payment received from a staker who included your MRC claim.");
        case GRC::MinedType::MRC_SEND:
            return tr("Your staked block automatically paid this MRC claim; no manual send occurred.");
        default:
            return tr("Mined transaction of unknown type.");
        }
    }

    switch (rec->type) {
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return tr("Funds sent to another address.");
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::RecvFromOther:
        return tr("Funds received.");
    case TransactionRecord::SendToSelf:
        return tr("Payment to yourself (e.g. change consolidation).");
    case TransactionRecord::BeaconAdvertisement:
        return tr("Beacon advertisement linking your CPID to your wallet.");
    case TransactionRecord::Poll:
        return tr("On-chain poll creation transaction.");
    case TransactionRecord::Vote:
        return tr("On-chain vote transaction.");
    case TransactionRecord::Message:
        return tr("On-chain message transaction.");
    case TransactionRecord::MRC:
        return tr("Manual Rewards Claim request submitted to the network.");
    case TransactionRecord::Other:
        return tr("Transaction with mixed inputs and outputs.");
    default:
        return QString();
    }
}

QVariant TransactionTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    // Resolve the row each call rather than trusting index.internalPointer().
    // The pointer stored at createIndex() time can dangle: std::vector
    // reallocation on insert (and element shifts on insert/erase past the
    // held row) invalidate every TransactionRecord*. QSortFilterProxyModel
    // and selection state hold persistent QModelIndexes across mutations.
    TransactionRecord *rec = priv->index(index.row());
    if (!rec) return QVariant();

    const auto column = static_cast<ColumnIndex>(index.column());
    switch (role) {
    case Qt::DecorationRole:
        switch (column) {
        case Status:
            return txStatusDecoration(rec);
        case Date: return {};
        case Type: return {};
        case ToAddress:
            return txAddressDecoration(rec);
        case Amount: return {};
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    case Qt::DisplayRole:
        switch (column) {
        case Status: return {};
        case Date:
            return formatTxDate(rec);
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, false);
        case Amount:
            return formatTxAmount(rec);
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch (column) {
        case Status:
            return QString::fromStdString(rec->status.sortKey);
        case Date:
            return rec->time;
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, true);
        case Amount:
            return rec->credit + rec->debit;
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::AccessibleTextRole:
        // For screen readers (issue #2604). Column 0 (Status) returns a full row
        // description so QListView consumers (e.g. OverviewPage's recent-transactions
        // list, which displays only the Status column with a custom delegate that
        // composes multiple roles visually) get a meaningful announcement instead of
        // silence. Other columns return "<header>: <value>" so QTableView consumers
        // (transaction history) get contextual per-cell navigation.
        switch (column) {
        case Status:
            return tr("%1, %2 %3, amount %4, %5")
                .arg(formatTxDate(rec),
                     formatTxType(rec),
                     formatTxToAddress(rec, true),
                     formatTxAmount(rec),
                     rec->status.countsForBalance ? tr("confirmed") : tr("unconfirmed"));
        case Date:
            return tr("Date: %1").arg(formatTxDate(rec));
        case Type:
            return tr("Type: %1").arg(formatTxType(rec));
        case ToAddress:
            return tr("Address: %1").arg(formatTxToAddress(rec, true));
        case Amount:
            return tr("Amount: %1").arg(formatTxAmount(rec));
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
        // Non-confirmed (but not immature) as transactions are grey
        if(!rec->status.countsForBalance && rec->status.status != TransactionStatus::Immature)
        {
            return COLOR_UNCONFIRMED;
        }
        if(index.column() == Amount && (rec->credit+rec->debit) < 0)
        {
            return COLOR_NEGATIVE;
        }
        if(index.column() == ToAddress)
        {
            return addressColor(rec);
        }
        break;
    case TypeRole:
        return rec->type;
    case DateRole:
        return QDateTime::fromSecsSinceEpoch(rec->time);
    case LongDescriptionRole:
        return priv->describe(rec);
    case AddressRole:
        return QString::fromStdString(rec->address);
    case LabelRole:
        return walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));
    case AmountRole:
        return rec->credit + rec->debit;
    case TxIDRole:
        return QString::fromStdString(rec->getTxID());
    case ConfirmedRole:
        return rec->status.countsForBalance;
    case FormattedAmountRole:
        return formatTxAmount(rec, false);
    case StatusRole:
        return rec->status.status;
    }
    return QVariant();
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole)
        {
            switch(section)
            {
            case Status:
                return tr("Transaction status. Hover over this field to show number of confirmations.");
            case Date:
                return tr("Date and time that the transaction was received.");
            case Type:
                return tr("Type of transaction.");
            case ToAddress:
                return tr("Destination address of transaction.");
            case Amount:
                return tr("Amount removed from or added to balance.");
            }
        }
    }
    return QVariant();
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (row < 0 || row >= priv->size()) {
        return QModelIndex();
    }
    // Pass an explicit nullptr internalPointer — data() resolves the row
    // each call (see comment there). Storing a TransactionRecord* would
    // dangle across any vector insert/erase that shifts the row's
    // storage. The nullptr is explicit (rather than relying on the
    // default-arg overload of createIndex) to match the third-arg
    // convention used by the sibling table models and avoid any
    // Qt-version-specific overload-resolution drift.
    return createIndex(row, column, nullptr);
}

void TransactionTableModel::updateDisplayUnit()
{
    // emit dataChanged to update Amount column with the current unit
    emit dataChanged(index(0, Amount), index(priv->size()-1, Amount));
}
