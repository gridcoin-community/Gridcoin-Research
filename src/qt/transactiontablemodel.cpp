#include "transactiontablemodel.h"
#include "guiutil.h"
#include "transactionrecord.h"
#include "guiconstants.h"
#include "transactiondesc.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "wallet/wallet.h"
#include "node/ui_interface.h"
#include "util.h"

#include <QLocale>
#include <QList>
#include <QColor>
#include <QIcon>
#include <QDateTime>
#include <QtAlgorithms>

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignRight|Qt::AlignVCenter
    };

// Comparison operator for sort/binary search of model tx list
struct TxLessThan
{
    bool operator()(const TransactionRecord &a, const TransactionRecord &b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const TransactionRecord &a, const uint256 &b) const
    {
        return a.hash < b;
    }
    bool operator()(const uint256 &a, const TransactionRecord &b) const
    {
        return a < b.hash;
    }
};

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

    /* Local cache of wallet.
     * As it is in the same order as the CWallet, by definition
     * this is sorted by sha256.
     */
    QList<TransactionRecord> cachedWallet;

    /* Query entire wallet anew from core.
     */
    void loadWallet()
    {
        cachedWallet.clear();
        {
            LOCK2(cs_main, wallet->cs_wallet);

            bool fLimitTxnDisplay = walletModel->getOptionsModel()->getLimitTxnDisplay();
            int64_t limitTxnDateTime = walletModel->getOptionsModel()->getLimitTxnDateTime();

            for(std::map<uint256, CWalletTx>::iterator it = wallet->mapWallet.begin(); it != wallet->mapWallet.end(); ++it)
            {
                if (TransactionRecord::showTransaction(it->second, fLimitTxnDisplay, limitTxnDateTime))
                {
                    cachedWallet.append(TransactionRecord::decomposeTransaction(wallet, it->second));
                }
            }
        }
    }


    /* Update QList using new query from core.
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
    //! - TxAdded payloads carry records that were already decomposed at the
    //!   producer side, so no wallet lookup is needed. The consumer applies
    //!   the user's datetime-limit filter (a record-level predicate on
    //!   record.time) and inserts the surviving records as a contiguous
    //!   range at the binary-search position determined by the tx hash.
    //!
    //! - TxRemoved payloads carry just the hash. The consumer locates the
    //!   matching range in cachedWallet by binary search and removes it.
    //!
    //! - TxUpdated payloads carry hash + status. No row mutation is needed:
    //!   per-row status (depth, Confirmed/Confirming/etc.) is refreshed
    //!   lazily on read inside index(), and updateConfirmations() drives
    //!   a per-block table-wide dataChanged that refreshes the visible
    //!   rows. The event is acknowledged via verbose logging only.
    //!
    void applyEventBatch(const std::vector<GRC::WalletEvent>& events)
    {
        const bool fLimitTxnDisplay = walletModel->getOptionsModel()->getLimitTxnDisplay();
        const int64_t limitTxnDateTime = walletModel->getOptionsModel()->getLimitTxnDateTime();

        for (const auto& ev : events) {
            std::visit([&](auto&& payload) {
                using P = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<P, GRC::TxAddedPayload>) {
                    applyTxAdded(payload, fLimitTxnDisplay, limitTxnDateTime);
                } else if constexpr (std::is_same_v<P, GRC::TxRemovedPayload>) {
                    applyTxRemoved(payload);
                } else if constexpr (std::is_same_v<P, GRC::TxUpdatedPayload>) {
                    LogPrint(BCLog::LogFlags::VERBOSE,
                             "applyEventBatch: TxUpdated %s status=%d (no row mutation)",
                             payload.hash.GetHex(), payload.status);
                } else if constexpr (std::is_same_v<P, GRC::ChainTipChangedPayload>) {
                    // ChainTipChanged is handled at the WalletModel drain
                    // level (triggers updateConfirmations + checkBalanceChanged
                    // once per batch). No per-event row mutation here.
                }
            }, ev.payload);
        }
    }

    void applyTxAdded(const GRC::TxAddedPayload& payload,
                      bool fLimitTxnDisplay,
                      int64_t limitTxnDateTime)
    {
        // Apply the consumer-side datetime filter. The producer has already
        // applied the wtx-level visibility checks (orphan coinstake/coinbase,
        // legacy OP_RETURN) under the locks it held at notification time.
        QList<TransactionRecord> toInsert;
        toInsert.reserve(payload.records.size());
        for (const TransactionRecord& rec : payload.records) {
            if (fLimitTxnDisplay && rec.time < limitTxnDateTime) {
                continue;
            }
            toInsert.append(rec);
        }
        if (toInsert.isEmpty()) {
            return;
        }

        // All records in a payload share the same hash, so they slot into
        // cachedWallet as a single contiguous range at the lower_bound
        // position for that hash.
        const uint256& hash = toInsert.front().hash;
        auto lower = std::lower_bound(cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        auto upper = std::upper_bound(cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        if (lower != upper) {
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "applyTxAdded: %s already in model — skipping duplicate insert", hash.GetHex());
            return;
        }

        const int lowerIndex = static_cast<int>(lower - cachedWallet.begin());

        parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex + toInsert.size() - 1);
        int insert_idx = lowerIndex;
        for (const TransactionRecord& rec : std::as_const(toInsert)) {
            cachedWallet.insert(insert_idx, rec);
            insert_idx += 1;
        }
        parent->endInsertRows();
    }

    void applyTxRemoved(const GRC::TxRemovedPayload& payload)
    {
        auto lower = std::lower_bound(cachedWallet.begin(), cachedWallet.end(), payload.hash, TxLessThan());
        auto upper = std::upper_bound(cachedWallet.begin(), cachedWallet.end(), payload.hash, TxLessThan());
        if (lower == upper) {
            // Not in cachedWallet — may have been filtered out at insert time,
            // or never inserted (e.g. an orphan caught by the producer-side
            // showTransaction check). No row work needed.
            return;
        }

        const int lowerIndex = static_cast<int>(lower - cachedWallet.begin());
        const int upperIndex = static_cast<int>(upper - cachedWallet.begin());
        parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
        cachedWallet.erase(lower, upper);
        parent->endRemoveRows();
    }

    int size()
    {
        return cachedWallet.size();
    }

    TransactionRecord *index(int idx)
    {
        if(idx >= 0 && idx < cachedWallet.size())
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
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());

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
    TransactionRecord *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void TransactionTableModel::updateDisplayUnit()
{
    // emit dataChanged to update Amount column with the current unit
    emit dataChanged(index(0, Amount), index(priv->size()-1, Amount));
}
