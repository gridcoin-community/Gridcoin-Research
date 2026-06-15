// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DETAILEDTXMODEL_H
#define BITCOIN_QT_DETAILEDTXMODEL_H

#include "qt/transactionrecord.h"
#include "qt/txfilter.h"
#include "qt/wallet_event_queue.h"
#include "qt/windowcache.h"
#include "uint256.h"

#include <QAbstractTableModel>

#include <vector>

class WalletModel;
class TransactionTableModel;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

//!
//! \brief Windowed-model consumer for the detailed transaction-history view.
//!
//! Replaces the client-side TransactionFilterProxy: it registers a VIEW_DETAILED
//! cursor in the producer GRC::WalletTxStore (server-side filter + sort, maintained
//! off cs_main/cs_wallet on the store-worker) and renders through
//! TransactionTableModel::formatRole — so the detailed table reads identical role
//! values with no formatter duplication. It is the TransactionView's direct model
//! (no proxy): a header click drives sort() → setViewSort; the filter UI drives
//! setFilter() → setViewFilter.
//!
//! As of PR5-B this is a VIRTUAL windowed model. The producer cursor cap stays
//! UNLIMITED (so getRows(first) is an absolute accepted index), but only a
//! contiguous viewport slice of records is cached here, in a Qt-free
//! GRC::WindowCache<TransactionRecord>:
//!   - rowCount() == m_cache.total() — the full accepted count, so the scrollbar
//!     spans the whole history;
//!   - data(row) renders m_cache.at(row) via formatRole, or returns a placeholder
//!     QVariant() for an off-window row (data() is pure — no fetch on the paint path);
//!   - the STRUCTURAL channel (drained WalletEvents) routes through the cache + a
//!     nested CacheSink that forwards begin/end{Insert,Remove,Reset}Rows;
//!   - the CONTENT channel (viewport-driven getRows slice) fills the cache via
//!     fillContent (epoch + high-water gated; never advances the structural seqno).
//! TransactionView drives the viewport reporting, selection-ensure and
//! anchor-on-resort; CSV export and click-through use the producer's getAllRows /
//! rowForKey (the cache holds only a slice).
//!
class DetailedTxModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit DetailedTxModel(WalletModel* walletModel, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    //! Header-click sort: forwards to the producer cursor's setViewSort; the
    //! resulting RowsReset refills this model (QTableView manages the indicator).
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    //! Push a new filter spec to the producer cursor (the filter UI calls this).
    void setFilter(const GRC::FilterSpec& spec);

    //! Model index of the (first part of the) row whose tx hash is \p hash in the
    //! current filtered+sorted view, or an invalid index if absent/filtered — maps
    //! a click-through (e.g. from OverviewPage) to a row here. Resolved over the
    //! producer (store.rowForKey), so it works for ANY accepted row, in or out of
    //! the cached slice.
    QModelIndex indexForTxid(const uint256& hash) const;

    //! The whole filtered+sorted set in view order (producer getAllRows) — for CSV
    //! export, which must cover every matching row, not just the cached window.
    std::vector<TransactionRecord> getAllRows() const;

    //! Accepted-view row of (\p hash, \p idx) or -1 (producer rowForKey). idx < 0 ==
    //! first part. Backs anchor-on-resort and click-through.
    int rowForKey(const uint256& hash, int idx = -1) const;

    //! The exact (hash, idx) identity of the cached record at \p row, or false if
    //! \p row is outside the cached slice. Used to capture a SPECIFIC decomposed
    //! part for anchor-on-resort: a transaction's parts share one hash but have
    //! distinct idx/amount/address, so they scatter under an Amount/Address sort —
    //! anchoring by hash alone (rowForKey idx=-1 → the min-position part) lands on
    //! the wrong row. Capturing idx lets restoreAnchor return to the EXACT part.
    bool keyAt(int row, uint256& hash, int& idx) const;

    //! Bring \p row into the cached slice if it is not already (a viewport-sized,
    //! row-centred fetch) so the selected/operated row is never a placeholder for
    //! copy / Show details. No-op if already cached. Called by TransactionView at
    //! selection time and before a context action.
    void ensureRowCached(int row);

public slots:
    //! TransactionView reports the visible row range [firstVisible, lastVisible]
    //! (scroll / resize / show). Coalesced via a single-shot timer; the timeout
    //! fetches the range + a margin into the cache.
    void onViewportChanged(int firstVisible, int lastVisible);

signals:
    //! Emitted after a structural Reset has been applied to the cache (sort/filter/
    //! reload) — TransactionView restores the resort anchor on it.
    void viewReset();

private slots:
    //! Apply a drained wallet-event batch; ignores everything not VIEW_DETAILED.
    void applyEventBatch(const std::vector<GRC::WalletEvent>& events);
    //! Debounced content fetch for the last-reported viewport range.
    void onFetchTimeout();

private:
    //! Drain the event queue (so structuralSeqno is current), then fetch
    //! [first, first+count) into the cache via fillContent. On a (rare) gate
    //! rejection — a worker push raced the read — re-arm a bounded retry.
    void fetchWindow(int first, int count);

    //! Forwarders so the nested CacheSink drives our QAbstractItemModel row signals
    //! from within a DetailedTxModel member (avoids any protected-base access
    //! subtlety, and keeps the begin/end bracket ownership in the cache).
    void sinkBeginReset() { beginResetModel(); }
    void sinkEndReset() { endResetModel(); }
    void sinkBeginInsert(int first, int count) { beginInsertRows(QModelIndex(), first, first + count - 1); }
    void sinkEndInsert() { endInsertRows(); }
    void sinkBeginRemove(int first, int count) { beginRemoveRows(QModelIndex(), first, first + count - 1); }
    void sinkEndRemove() { endRemoveRows(); }
    void sinkDataChanged(int first, int count);

    //! Adapter handed to WindowCache: turns its structural callbacks into this
    //! model's row signals. A member (not a base) because WindowCacheSink's
    //! dataChanged(int,int) would otherwise overload QAbstractItemModel's
    //! dataChanged signal — a moc hazard.
    struct CacheSink : public GRC::WindowCacheSink {
        explicit CacheSink(DetailedTxModel* model) : m(model) {}
        DetailedTxModel* m;
        void beginReset() override { m->sinkBeginReset(); }
        void endReset() override { m->sinkEndReset(); }
        void beginInsert(int first, int count) override { m->sinkBeginInsert(first, count); }
        void endInsert() override { m->sinkEndInsert(); }
        void beginRemove(int first, int count) override { m->sinkBeginRemove(first, count); }
        void endRemove() override { m->sinkEndRemove(); }
        void dataChanged(int first, int count) override { m->sinkDataChanged(first, count); }
    };

    WalletModel* m_walletModel;
    TransactionTableModel* m_ttm;                 //!< formatter source (formatRole/headerData)
    GRC::WindowCache<TransactionRecord> m_cache;  //!< viewport slice + virtual rowCount
    CacheSink m_sink;                             //!< structural-callback adapter

    QTimer* m_fetchTimer = nullptr;               //!< single-shot scroll-fetch debounce
    int m_pending_first = 0;                      //!< last-reported visible range
    int m_pending_last = 0;
    int m_fetch_retries = 0;                      //!< bounded retry on fillContent rejection
    bool m_fetch_in_progress = false;             //!< reentrancy guard: fetchWindow drains
                                                  //!< the queue synchronously, so a nested
                                                  //!< fetch (via an applied event) is skipped
};

#endif // BITCOIN_QT_DETAILEDTXMODEL_H
