// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/detailedtxmodel.h"

#include "qt/transactiontablemodel.h"
#include "qt/walletmodel.h"
#include "qt/wallettxstore.h"
#include "util/system.h"

#include <QTimer>

#include <algorithm>
#include <type_traits>
#include <variant>

// Guard the standalone (Qt-free) mirrors in txfilter.h against the authoritative
// Qt-side enums so they can never silently drift. Relocated here from the retired
// transactionfilterproxy.cpp — this is now the primary detailed-view consumer.
static_assert(GRC::TXSTATUS_CONFLICTED  == TransactionStatus::Conflicted,
              "TXSTATUS_CONFLICTED mirror out of sync with TransactionStatus::Conflicted");
static_assert(GRC::TXSTATUS_NOTACCEPTED == TransactionStatus::NotAccepted,
              "TXSTATUS_NOTACCEPTED mirror out of sync with TransactionStatus::NotAccepted");
static_assert(static_cast<int>(GRC::TXCOL_STATUS)  == static_cast<int>(TransactionTableModel::Status),    "TXCOL_STATUS mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_DATE)    == static_cast<int>(TransactionTableModel::Date),      "TXCOL_DATE mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_TYPE)    == static_cast<int>(TransactionTableModel::Type),      "TXCOL_TYPE mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_ADDRESS) == static_cast<int>(TransactionTableModel::ToAddress), "TXCOL_ADDRESS mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_AMOUNT)  == static_cast<int>(TransactionTableModel::Amount),    "TXCOL_AMOUNT mirror drift");

namespace {
//! Bounded seed/Reset window: large enough to cover any plausible pre-layout
//! viewport; the first real viewport report fetches the visible range. Must NOT be
//! -1 ("all served"), which under the unlimited VIEW_DETAILED cap would return the
//! full replica and negate the windowing (windowed-model PR5-B).
constexpr int kInitialWindow = 200;
//! Scroll-fetch debounce (ms): coalesce a scroll burst into a single fetch.
constexpr int kDebounceMs = 100;
//! Hard backstop on a single fetch span so a mis-reported viewport can never
//! materialize the whole table (PR5-B review GAP #1).
constexpr int kMaxFetch = 4096;
//! Bounded fillContent-rejection retries before deferring to the next report.
constexpr int kMaxFetchRetries = 4;
} // namespace

DetailedTxModel::DetailedTxModel(WalletModel* walletModel, QObject* parent)
    : QAbstractTableModel(parent)
    , m_walletModel(walletModel)
    , m_ttm(walletModel->getTransactionTableModel())
    , m_sink(this)
{
    // Register the server-side view: the full history sorted by Date DESC, the
    // TransactionView's default ordering. The default FilterSpec is the
    // "show everything" initial state of the unfiltered detailed view. The one
    // runtime input is -showorphans, read once here (a launch-time arg with no
    // runtime mutation path, behavior-identical to reading it repeatedly).
    GRC::FilterSpec spec;
    spec.show_orphans = gArgs.GetBoolArg("-showorphans", false);
    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    store.registerView(GRC::VIEW_DETAILED, spec, GRC::TXCOL_DATE, GRC::TXSORT_DESC);

    // Seed a BOUNDED initial window into the cache (NOT count=-1: under the unlimited
    // VIEW_DETAILED cap that returns the full replica and negates the windowing).
    // kInitialWindow covers any plausible pre-layout viewport; the first
    // showEvent/resizeEvent reports the real visible range and fetches it. The
    // registration Reset that arrives on the first drain carries this same
    // high-water and is skipped by the cache's structural seqno gate.
    GRC::RowsResult seed = store.getRows(GRC::VIEW_DETAILED, 0, kInitialWindow);
    m_cache.seedInitial(std::move(seed.records), 0, seed.total_accepted,
                        seed.epoch, seed.high_water);

    m_fetchTimer = new QTimer(this);
    m_fetchTimer->setSingleShot(true);
    connect(m_fetchTimer, &QTimer::timeout, this, &DetailedTxModel::onFetchTimeout);

    connect(m_walletModel, &WalletModel::walletEventsDrained,
            this, &DetailedTxModel::applyEventBatch);
}

int DetailedTxModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_cache.total();
}

int DetailedTxModel::columnCount(const QModelIndex& parent) const
{
    // Forward to the formatter model so the column set stays single-sourced.
    return parent.isValid() ? 0 : m_ttm->columnCount(QModelIndex());
}

QVariant DetailedTxModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_cache.total()) {
        return QVariant();
    }
    // Pure on the paint path: render the cached record, or a placeholder for an
    // off-window row. NO fetch here — the viewport report drives content fills.
    const TransactionRecord* rec = m_cache.at(index.row());
    if (!rec) {
        return QVariant();
    }
    // Reuse the TransactionTableModel formatters. const_cast: formatRole takes a
    // non-const record (legacy getTxID()/describe() are non-const) but does not
    // mutate it; the pointer is valid only within this call (m_cache.at contract).
    return m_ttm->formatRole(const_cast<TransactionRecord*>(rec), index.column(), role);
}

QVariant DetailedTxModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return m_ttm->headerData(section, orientation, role);
}

void DetailedTxModel::sort(int column, Qt::SortOrder order)
{
    // Forward to the producer cursor; the resulting RowsReset refills this model
    // on the next drain. QTableView owns the header sort-indicator display. Kick
    // an immediate drain so the reorder lands now, not up to 500ms later (PR4-D).
    m_walletModel->getTxStore().setViewSort(
        GRC::VIEW_DETAILED, column,
        order == Qt::AscendingOrder ? GRC::TXSORT_ASC : GRC::TXSORT_DESC);
    m_walletModel->requestEventDrainSoon();
}

void DetailedTxModel::setFilter(const GRC::FilterSpec& spec)
{
    m_walletModel->getTxStore().setViewFilter(GRC::VIEW_DETAILED, spec);
    m_walletModel->requestEventDrainSoon();   // reflect the filter immediately (PR4-D)
}

QModelIndex DetailedTxModel::indexForTxid(const uint256& hash) const
{
    // Resolve over the producer, not the cached slice: a click-through can target a
    // row outside the window. rowForKey returns the accepted row in the live
    // filtered+sorted view, or -1 if absent/filtered.
    const int row = m_walletModel->getTxStore().rowForKey(GRC::VIEW_DETAILED, hash, -1);
    return row >= 0 ? index(row, 0) : QModelIndex();
}

std::vector<TransactionRecord> DetailedTxModel::getAllRows() const
{
    // CSV export must cover every matching row, not just the cached window — read
    // the full filtered+sorted set from the producer (cap-independent).
    return m_walletModel->getTxStore().getAllRows(GRC::VIEW_DETAILED).records;
}

int DetailedTxModel::rowForKey(const uint256& hash, int idx) const
{
    return m_walletModel->getTxStore().rowForKey(GRC::VIEW_DETAILED, hash, idx);
}

bool DetailedTxModel::keyAt(int row, uint256& hash, int& idx) const
{
    const TransactionRecord* rec = m_cache.at(row);
    if (!rec) {
        return false;
    }
    hash = rec->hash;
    idx = rec->idx;
    return true;
}

void DetailedTxModel::ensureRowCached(int row)
{
    const int total = m_cache.total();
    if (row < 0 || row >= total || m_cache.has(row)) {
        return;
    }
    // Cover BOTH the current viewport AND `row` when they are within reach, so an
    // in/near-viewport ensure never evicts the visible slice (fillContent REPLACES
    // it — PR5-B review GAP #4/cluster C). Only a FAR jump — where the view is
    // navigating to `row` anyway (focusTransaction / restoreAnchor scrollTo) — falls
    // back to a bounded window centred on the row. Clamp the (possibly stale) pending
    // range into the table first so the span can't blow up.
    const int pf = std::min(std::max(0, m_pending_first), total - 1);
    const int pl = std::min(std::max(0, m_pending_last), total - 1);
    const int margin = std::max(kInitialWindow, pl - pf + 1);
    const int lo = std::min(row, pf);
    const int hi = std::max(row, pl);
    int first = std::max(0, lo - margin);
    int last = std::min(total - 1, hi + margin);
    if (last - first + 1 > kMaxFetch) {
        // Row is far from the viewport: centre a bounded window on the row.
        first = std::max(0, row - kMaxFetch / 2);
        last = std::min(total - 1, first + kMaxFetch - 1);
    }
    fetchWindow(first, last - first + 1);
}

void DetailedTxModel::onViewportChanged(int firstVisible, int lastVisible)
{
    // Bail on a not-yet-laid-out / empty viewport (TransactionView clamps too); a
    // garbage range here must never reach onFetchTimeout (PR5-B review GAP #1).
    if (firstVisible < 0 || lastVisible < firstVisible) {
        return;
    }
    const int total = m_cache.total();
    if (total <= 0) {
        return;
    }
    // Clamp into the current table (rowAt can momentarily report stale indices during
    // a resize/Reset before relayout) so onFetchTimeout never sees an out-of-bounds
    // range (PR5-B review).
    m_pending_first = std::min(firstVisible, total - 1);
    m_pending_last = std::min(lastVisible, total - 1);
    if (m_pending_last < m_pending_first) {
        m_pending_last = m_pending_first;
    }
    m_fetch_retries = 0;                        // a fresh report restarts the retry budget
    if (m_fetchTimer) {
        m_fetchTimer->start(kDebounceMs);       // restart-to-coalesce a scroll burst
    }
}

void DetailedTxModel::onFetchTimeout()
{
    const int total = m_cache.total();
    if (total <= 0) {
        return;
    }
    // Clamp the last-reported range into the CURRENT table before using it: a
    // sort/filter Reset can shrink total below a pre-Reset m_pending_* (which is only
    // refreshed by the next viewport report), and an unclamped range yields a negative
    // count / out-of-bounds window that would evict the freshly-rebuilt slice (PR5-B
    // review cluster A). After clamping, pf <= pl and count >= 1 are guaranteed.
    const int pf = std::min(std::max(0, m_pending_first), total - 1);
    const int pl = std::min(std::max(0, m_pending_last), total - 1);
    // Margin ~ one viewport on each side, so small scrolls stay inside the cache and
    // the window is ~3x the visible range.
    const int span = std::max(1, pl - pf + 1);
    int first = std::max(0, pf - span);
    int last = std::min(total - 1, pl + span);
    int count = last - first + 1;
    // Hard backstop (GAP #1): never request more than kMaxFetch rows.
    if (count > kMaxFetch) {
        count = kMaxFetch;
    }
    // Hysteresis (GAP #6): skip when the wanted range is already fully cached.
    if (count > 0 && m_cache.has(first) && m_cache.has(first + count - 1)) {
        return;
    }
    fetchWindow(first, count);
}

void DetailedTxModel::fetchWindow(int first, int count)
{
    if (count <= 0 || first < 0 || m_fetch_in_progress) {
        return;
    }
    m_fetch_in_progress = true;
    // RAII reset so a throw from drainEventQueue / getRows / fillContent cannot wedge
    // the flag and freeze the viewport permanently (PR5-B review: exception safety).
    struct Guard { bool& f; ~Guard() { f = false; } } guard{m_fetch_in_progress};

    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    bool adopted = false;
    // Synchronous bounded retry: drain the queue FIRST so the cache's structural
    // seqno matches the store's high-water (GAP #2), then read + fill. The only
    // reason fillContent rejects here is a worker push landing between the drain and
    // getRows; retrying the drain+read resolves it in a couple of iterations, so the
    // visible/selected row is cached SYNCHRONOUSLY and a follow-up copy / Show-details
    // reads real data rather than a placeholder (PR5-B review GAP #3/#8). Each
    // drainEventQueue is sequential, not nested (the reentrancy guard in WalletModel
    // no-ops only a re-entry from WITHIN an apply).
    for (int attempt = 0; attempt < kMaxFetchRetries && !adopted; ++attempt) {
        m_walletModel->drainEventQueue();
        GRC::RowsResult r = store.getRows(GRC::VIEW_DETAILED, first, count);
        adopted = m_cache.fillContent(m_sink, first, std::move(r.records),
                                      r.epoch, r.high_water);
    }
    if (adopted) {
        m_fetch_retries = 0;
        return;
    }
    // Sustained churn (reorg/IBD storm pushing faster than we drain): defer to a
    // debounced retry; the next viewport report resets the budget. Bounded so it
    // cannot spin forever (the guard above resets m_fetch_in_progress on this exit).
    if (m_fetch_retries < kMaxFetchRetries) {
        ++m_fetch_retries;
        if (m_fetchTimer) {
            m_fetchTimer->start(kDebounceMs);
        }
    }
}

void DetailedTxModel::sinkDataChanged(int first, int count)
{
    const int lastCol = columnCount(QModelIndex()) - 1;
    emit dataChanged(index(first, 0), index(first + count - 1, lastCol));
}

void DetailedTxModel::applyEventBatch(const std::vector<GRC::WalletEvent>& events)
{
    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    for (const GRC::WalletEvent& ev : events) {
        const uint64_t seqno = ev.seqno;
        std::visit([&](auto&& payload) {
            using P = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<P, GRC::RowsResetPayload>) {
                if (payload.viewId != GRC::VIEW_DETAILED) return;
                if (seqno <= m_cache.structuralSeqno()) return;   // already reflected
                // Bounded refetch (NOT count=-1): the cache rebuilds its window; the
                // viewport fetch below fills the actual visible range. applyReset
                // brackets begin/endReset and sets the structural baseline to
                // max(high_water, seqno) — capturing any off-window insert/remove that
                // raced this Reset (PR4-fix B, via RowsResult atomicity).
                GRC::RowsResult r = store.getRows(GRC::VIEW_DETAILED, 0, kInitialWindow);
                if (m_cache.applyReset(m_sink, seqno, std::move(r.records), 0,
                                       r.total_accepted, r.epoch, r.high_water)) {
                    // The pre-Reset viewport range is meaningless against the rebuilt
                    // (possibly resized) table; reset it to the top — where a model
                    // reset scrolls the view — so the re-armed fetch targets a valid
                    // window. restoreAnchor's scrollTo (or the view's own post-reset
                    // scroll) then reports the real range and refines it (PR5-B review
                    // cluster A).
                    m_pending_first = 0;
                    m_pending_last = std::max(0, std::min(m_cache.total() - 1,
                                                          kInitialWindow - 1));
                    emit viewReset();                    // drives anchor-on-resort
                    if (m_fetchTimer) m_fetchTimer->start(kDebounceMs);   // refetch visible
                }
            } else if constexpr (std::is_same_v<P, GRC::RowsInsertedPayload>) {
                if (payload.viewId != GRC::VIEW_DETAILED) return;
                // payload carries the inserted records; the cache gates on seqno,
                // forwards begin/endInsertRows (before/after growing total) and either
                // splices into the slice or shifts the cache base.
                m_cache.applyInsert(m_sink, seqno, payload.position, payload.records);
            } else if constexpr (std::is_same_v<P, GRC::RowsRemovedPayload>) {
                if (payload.viewId != GRC::VIEW_DETAILED) return;
                m_cache.applyRemove(m_sink, seqno, payload.position, payload.count);
            } else if constexpr (std::is_same_v<P, GRC::RowsChangedPayload>) {
                if (payload.viewId != GRC::VIEW_DETAILED) return;
                if (seqno <= m_cache.structuralSeqno()) return;   // skip the wasted getRows
                // A Change carries no records (no reorder); refetch the changed slice.
                const std::vector<TransactionRecord> fresh =
                    store.getRows(GRC::VIEW_DETAILED, payload.first, payload.count).records;
                m_cache.applyChange(m_sink, seqno, payload.first, payload.count, fresh);
            }
            // RowCountChanged / ChainTipChanged / VIEW_FULL / VIEW_OVERVIEW are not
            // consumed here (the detailed view's cap stays unlimited).
        }, ev.payload);
    }
}
