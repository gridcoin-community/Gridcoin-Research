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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <vector>

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignLeft|Qt::AlignVCenter,
        Qt::AlignRight|Qt::AlignVCenter
    };

// Composite-key ordering for the cached transaction list. Three-level key:
//
//   1. time DESCENDING — newest first; matches the default user-facing
//      ordering for the table.
//   2. hash ASCENDING  — clusters all decomposed records of one tx into a
//      contiguous range. Without this, two txs that share a wall-clock
//      second AND have overlapping decomposition indices would interleave
//      (A0, B0, A1, B1), forcing insert/remove to handle disjoint ranges.
//      With hash at second level, each tx is always a coherent block.
//   3. idx ASCENDING   — within a single tx (same time, same hash), preserves
//      the order produced by TransactionRecord::decomposeTransaction. idx
//      values are unique within a tx, so this is a true total order: no two
//      records ever compare equal.
struct TxRecordOrder
{
    bool operator()(const TransactionRecord &a, const TransactionRecord &b) const
    {
        if (a.time != b.time) return a.time > b.time;
        if (a.hash != b.hash) return a.hash < b.hash;
        return a.idx < b.idx;
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

    //!
    //! \brief Local cache of wallet transactions, sorted by TxRecordOrder
    //! (time DESC, hash ASC, idx ASC). Each entry is one row of the table.
    //!
    //! Companion `hashIndex` maps tx-hash → vector positions for O(1)
    //! lookup. Records with the same hash are always contiguous in the
    //! vector (the second-level hash tiebreaker in TxRecordOrder
    //! guarantees this).
    //!
    std::vector<TransactionRecord> cachedWallet;
    std::unordered_multimap<uint256, std::size_t, BlockHasher> hashIndex;

    //!
    //! \brief Rebuild hashIndex from cachedWallet from scratch. Used after
    //! loadWallet() and any bulk operation where it's cheaper to rebuild
    //! than to maintain incrementally.
    //!
    void rebuildHashIndex()
    {
        hashIndex.clear();
        // Reserve with headroom so the default max_load_factor=1.0 doesn't
        // trigger an immediate rehash on the first emplace.
        hashIndex.reserve(cachedWallet.size() * 2 + 1);
        for (std::size_t i = 0; i < cachedWallet.size(); ++i) {
            hashIndex.emplace(cachedWallet[i].hash, i);
        }
    }

    //!
    //! \brief Bump every hashIndex value whose position is >= `from` by
    //! `delta`. Used to maintain the index after a vector insert (positive
    //! delta) or erase (negative delta).
    //!
    //! O(M) where M = total hashIndex entries. Acceptable per design
    //! (the typical insert/remove is rare and pays this only once).
    //!
    void shiftHashIndex(std::size_t from, std::ptrdiff_t delta)
    {
        for (auto& kv : hashIndex) {
            if (kv.second >= from) {
                kv.second = static_cast<std::size_t>(
                    static_cast<std::ptrdiff_t>(kv.second) + delta);
            }
        }
    }

    /* Query entire wallet anew from core.
     */
    void loadWallet()
    {
        cachedWallet.clear();
        // hashIndex is reset by rebuildHashIndex() below.
        {
            LOCK2(cs_main, wallet->cs_wallet);

            const bool fLimitTxnDisplay = walletModel->getOptionsModel()->getLimitTxnDisplay();
            const int64_t limitTxnDateTime = walletModel->getOptionsModel()->getLimitTxnDateTime();

            // Reserve a reasonable upper bound up front to avoid the
            // ~18 reallocations a 300k-tx wallet would otherwise pay.
            // Each wtx typically decomposes to 1-3 records; double the
            // wtx count as a safe lower estimate.
            cachedWallet.reserve(wallet->mapWallet.size() * 2);

            for(std::map<uint256, CWalletTx>::iterator it = wallet->mapWallet.begin(); it != wallet->mapWallet.end(); ++it)
            {
                if (!TransactionRecord::showTransaction(it->second)) continue;

                const QList<TransactionRecord> decomposed =
                    TransactionRecord::decomposeTransaction(wallet, it->second);

                // Apply the datetime-limit filter at the RECORD level on
                // rec.time (= CWalletTx::GetTxTime), matching the Date
                // column shown in the GUI and the cutoff applied in
                // applyTxAdded. showTransaction's wtx-level cutoff
                // compares wtx.nTime (signing time), which can drift
                // from GetTxTime for confirmed txs and produces a
                // visibly inconsistent filter on initial load vs
                // incremental adds.
                for (const TransactionRecord& rec : decomposed) {
                    if (fLimitTxnDisplay && rec.time < limitTxnDateTime) continue;
                    cachedWallet.push_back(rec);
                }
            }
        }
        std::sort(cachedWallet.begin(), cachedWallet.end(), TxRecordOrder());
        rebuildHashIndex();
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
    //! - TxAdded payloads carry records that were already decomposed at the
    //!   producer side, so no wallet lookup is needed. The consumer applies
    //!   the user's datetime-limit filter (a record-level predicate on
    //!   record.time) and inserts the surviving records as a contiguous
    //!   range at the TxRecordOrder lower_bound position. Same-hash records
    //!   cluster naturally under that ordering, so a single beginInsertRows
    //!   covers the whole tx.
    //!
    //! - TxRemoved payloads carry just the hash. The consumer locates the
    //!   matching range via hashIndex (O(1) average) and removes it as a
    //!   single contiguous block.
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
        //
        // All records in a payload share `time` and `hash`, so the datetime
        // filter is all-or-nothing: either every record clears the cutoff
        // or none do.
        std::vector<TransactionRecord> toInsert;
        toInsert.reserve(payload.records.size());
        for (const TransactionRecord& rec : payload.records) {
            if (fLimitTxnDisplay && rec.time < limitTxnDateTime) {
                continue;
            }
            toInsert.push_back(rec);
        }
        if (toInsert.empty()) {
            return;
        }

        // Sort the new records by TxRecordOrder. decomposeTransaction
        // already produces them in idx order (which is the third-level
        // tiebreaker, with same time + same hash), so this is normally
        // a no-op — but enforce defensively against any future producer
        // change.
        std::sort(toInsert.begin(), toInsert.end(), TxRecordOrder());

        // Skip duplicate insert: hashIndex.find() is O(1) average and
        // tells us whether ANY record of this tx is already cached.
        // Because the producer sends all records of a tx as one payload,
        // a present hash means the full set is already there.
        const uint256& hash = toInsert.front().hash;
        if (hashIndex.find(hash) != hashIndex.end()) {
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "applyTxAdded: %s already in model — skipping duplicate insert", hash.GetHex());
            return;
        }

        // All records in toInsert share `time` and `hash` and differ only
        // in `idx`. Under TxRecordOrder they sort to a contiguous range
        // and slot into cachedWallet at the lower_bound position of the
        // first record.
        auto pos = std::lower_bound(cachedWallet.begin(), cachedWallet.end(),
                                    toInsert.front(), TxRecordOrder());
        const std::size_t insertIdx = static_cast<std::size_t>(pos - cachedWallet.begin());
        const std::size_t insertCount = toInsert.size();

        parent->beginInsertRows(QModelIndex(),
                                static_cast<int>(insertIdx),
                                static_cast<int>(insertIdx + insertCount - 1));

        // Order matters: shift the index BEFORE the vector insert (the
        // shift uses logical positions, not iterators, so the vector's
        // unshifted state doesn't matter), then do the insert, then add
        // the new index entries for the inserted range. Doing it in this
        // order keeps the index consistent at every observable point.
        shiftHashIndex(insertIdx, static_cast<std::ptrdiff_t>(insertCount));
        cachedWallet.insert(pos, toInsert.begin(), toInsert.end());
        for (std::size_t k = 0; k < insertCount; ++k) {
            hashIndex.emplace(hash, insertIdx + k);
        }

        parent->endInsertRows();
    }

    void applyTxRemoved(const GRC::TxRemovedPayload& payload)
    {
        auto range = hashIndex.equal_range(payload.hash);
        if (range.first == range.second) {
            // Not in cachedWallet — may have been filtered out at insert time,
            // or never inserted (e.g. an orphan caught by the producer-side
            // showTransaction check). No row work needed.
            return;
        }

        // Collect the positions for this hash. Same-hash records are
        // contiguous in the vector under TxRecordOrder (the second-level
        // hash tiebreaker guarantees clustering), so the min and max
        // bound a single range.
        std::size_t minPos = std::numeric_limits<std::size_t>::max();
        std::size_t maxPos = 0;
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second < minPos) minPos = it->second;
            if (it->second > maxPos) maxPos = it->second;
        }
        const std::size_t removeCount = maxPos - minPos + 1;

        // Defensive checks for the contiguity invariant the erase relies
        // on. If a future change ever lets same-hash records de-cluster
        // (e.g. switching the comparator to (time, idx, hash), or a
        // post-decomposition mutation of `time`), these asserts surface
        // it before the erase silently drops unrelated rows.
        assert(removeCount == static_cast<std::size_t>(std::distance(range.first, range.second)));
        assert(cachedWallet[minPos].hash == payload.hash);
        assert(cachedWallet[maxPos].hash == payload.hash);

        parent->beginRemoveRows(QModelIndex(),
                                static_cast<int>(minPos),
                                static_cast<int>(maxPos));

        cachedWallet.erase(cachedWallet.begin() + minPos,
                           cachedWallet.begin() + maxPos + 1);
        hashIndex.erase(payload.hash);
        shiftHashIndex(maxPos + 1, -static_cast<std::ptrdiff_t>(removeCount));

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
    // Pass no internalPointer / internalId — data() resolves the row each
    // call (see comment there). Storing a TransactionRecord* would dangle
    // across any vector insert/erase that shifts the row's storage.
    return createIndex(row, column);
}

void TransactionTableModel::updateDisplayUnit()
{
    // emit dataChanged to update Amount column with the current unit
    emit dataChanged(index(0, Amount), index(priv->size()-1, Amount));
}
