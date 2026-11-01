// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_WINDOWCACHE_H
#define GRIDCOIN_QT_WINDOWCACHE_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

//!
//! \file windowcache.h
//!
//! Qt-free consumer-side reconciliation core for the windowed detailed
//! transaction table (windowed-model PR5).
//!
//! PR4's DetailedTxModel holds the FULL filtered+sorted replica (the cursor cap
//! is unlimited, so servedCount == totalAccepted). PR5 keeps the producer's
//! cursor cap unlimited — so the store read path is unchanged and getRows(first)
//! is an absolute accepted index — and moves the sliding window entirely into
//! the consumer: this object caches only a contiguous slice [cacheFirst,
//! cacheFirst + cacheSize) of the records and reports the full row count to the
//! view so the scrollbar spans the whole history. Rows outside the slice render
//! as placeholders until a scroll-driven fetch fills them.
//!
//! Correctness rests on a strict TWO-CHANNEL split, which is the central hazard
//! this class encapsulates (and the reason it is a Qt-free, unit-tested core):
//!
//!   - STRUCTURAL channel: the ordered Reset / Insert / Remove / Change delta
//!     stream drained from the WalletEventQueue. It is the SOLE owner of the
//!     virtual row count (m_total) and of the model's begin/endInsertRows /
//!     begin/endRemoveRows brackets. Each delta carries a seqno; applying one
//!     advances m_structural_seqno. A delta whose seqno is <= the high-water is
//!     already reflected and is skipped (the PR4-fix B gate).
//!
//!   - CONTENT channel: the scroll-driven getRows(first, count) slice fetch. It
//!     fills row CONTENT for a newly-visible region. It NEVER changes m_total and
//!     NEVER advances m_structural_seqno. A fetched slice is adopted ONLY when its
//!     epoch matches the cache's (no intervening sort/filter — those bump the
//!     cursor epoch and arrive as a structural Reset) AND its high_water EXACTLY
//!     equals m_structural_seqno (the fetch reflects precisely the structural
//!     state the consumer has applied — no more, no less). A mismatch means the
//!     fetch raced an insert/remove in either direction; it is discarded and the
//!     consumer re-requests after the next drain settles the seqno. This is the
//!     PR4-fix B race generalized to the scroll path: the content channel may
//!     never silently shift the structural coordinate system.
//!
//! The class is templated on the record type so the reconciliation arithmetic is
//! exercised in the ENABLE_GUI=OFF unit suite with a synthetic record (the same
//! risk-control discipline as the Qt-free Cursor): TransactionRecord pulls in Qt
//! (QList/QString) and cannot compile GUI-OFF, but this logic must.
//!
//! Threading: Qt-thread-only, like the model it backs. No internal locking.
//!

namespace GRC {

//!
//! \brief Sink the WindowCache drives to translate structural changes into the
//! host model's row-mutation signals.
//!
//! The cache owns the begin/end bracketing: it calls beginInsert BEFORE it grows
//! m_total and endInsert AFTER, so the host (a QAbstractItemModel) can forward
//! beginInsert(first,count) -> beginInsertRows(parent, first, first+count-1) and
//! the framework observes a consistent old-count/new-count transition. The
//! GUI-OFF unit suite implements a recording sink that asserts the bracket order
//! and the pre-mutation row count.
//!
struct WindowCacheSink {
    virtual ~WindowCacheSink() = default;
    virtual void beginReset() = 0;
    virtual void endReset() = 0;
    virtual void beginInsert(int first, int count) = 0;
    virtual void endInsert() = 0;
    virtual void beginRemove(int first, int count) = 0;
    virtual void endRemove() = 0;
    //! `count` rows starting at absolute `first` should be re-queried (a Change
    //! delta refreshed them in place, or a content fetch made them available).
    virtual void dataChanged(int first, int count) = 0;
};

//!
//! \brief A WindowCacheSink decorator that turns all of a batch's dataChanged calls into one.
//!
//! On macOS, with an accessibility client attached, every model notification on a table
//! view (dataChanged, rows inserted/removed, reset) makes Qt's Cocoa bridge rebuild one
//! accessibility element per table ROW, freed only when the run loop's autorelease pool
//! pops (qcocoaaccessibilityelement.mm, updateTableModel). One dataChanged per Change event
//! in a drained batch therefore cost K x rows elements per batch: gigabytes on a large
//! wallet during a catch-up (#3059). This sink makes it one per batch.
//!
//! While a batch is open, dataChanged(first, count) widens ONE pending inclusive range
//! [lo, hi]. Every structural call translates that range into the coordinates the model
//! will have once the bracket closes, then forwards unchanged; endBatch emits the range
//! once, in final coordinates. Outside a batch the sink is a pure pass-through, so a content
//! fill from the scroll path still notifies immediately.
//!
//! Translation runs in begin* (before forwarding). WindowCache calls begin* before it
//! mutates and emits dataChanged only outside brackets, so this is equivalent to
//! translating in end*, and it stays correct if a slot connected to rows-inserted ever
//! triggered a fill from inside end*.
//!
//! Qt-thread-only, like the model it serves.
//!
class CoalescingSink : public WindowCacheSink
{
public:
    enum class EndResult {
        None,     //!< nothing was pending (or the batch is still open, nested)
        Emitted,  //!< one dataChanged over the pending range
        Fallback, //!< the range came out invalid; one dataChanged over the whole table
    };

    //! \param target    the sink that drives the host model.
    //! \param row_count the host cache's current virtual row count (WindowCache::total()),
    //!                  read only in endBatch, after every bracket of the batch has closed.
    CoalescingSink(WindowCacheSink& target, std::function<int()> row_count)
        : m_target(target), m_row_count(std::move(row_count))
    {
    }

    void beginBatch() { ++m_depth; }

    //! Close the batch normally. At the outermost close, emit the pending range once.
    //! The pending state is cleared BEFORE forwarding, so a slot that throws cannot leave
    //! a stale range to be emitted again. An out-of-range result means a translation bug;
    //! clamping would keep the range valid but could miss the changed rows (a silent stale
    //! row), so the whole table is refreshed instead -- the same single rebuild on macOS.
    EndResult endBatch()
    {
        if (m_depth > 0) --m_depth;
        if (m_depth > 0 || !m_pending) return EndResult::None;
        const int lo = m_lo, hi = m_hi;
        m_pending = false;
        m_last_lo = lo;
        m_last_hi = hi;
        const int total = m_row_count();
        if (lo >= 0 && lo <= hi && hi < total) {
            m_target.dataChanged(lo, hi - lo + 1);
            return EndResult::Emitted;
        }
        if (total > 0) m_target.dataChanged(0, total);
        return EndResult::Fallback;
    }

    //! Close the batch on the exception path: drop the pending range and emit nothing
    //! (no signal is emitted during stack unwinding).
    void abandonBatch()
    {
        if (m_depth > 0) --m_depth;
        if (m_depth == 0) m_pending = false;
    }

    bool inBatch() const { return m_depth > 0; }

    //! The inclusive range the last batch closed with (before any fallback), for logging.
    std::pair<int, int> lastRange() const { return {m_last_lo, m_last_hi}; }

    void dataChanged(int first, int count) override
    {
        if (count <= 0) return;
        if (m_depth == 0) {
            m_target.dataChanged(first, count);
            return;
        }
        const int last = first + count - 1;
        if (!m_pending) {
            m_lo = first;
            m_hi = last;
            m_pending = true;
        } else {
            m_lo = std::min(m_lo, first);
            m_hi = std::max(m_hi, last);
        }
    }

    void beginInsert(int first, int count) override
    {
        if (m_pending && count > 0) {
            if (first <= m_lo) {            // at or above the range: it shifts down
                m_lo += count;
                m_hi += count;
            } else if (first <= m_hi) {     // inside: it also covers the new rows
                m_hi += count;
            }                               // below: unchanged
        }
        m_target.beginInsert(first, count);
    }
    void endInsert() override { m_target.endInsert(); }

    void beginRemove(int first, int count) override
    {
        if (m_pending && count > 0) {
            const int r0 = first, r1 = first + count - 1;
            if (r1 < m_lo) {                // entirely above: shift up
                m_lo -= count;
                m_hi -= count;
            } else if (r0 <= m_hi) {        // overlaps: clip
                const int before = std::max(0, std::min(r1, m_lo - 1) - r0 + 1);
                const int inside = std::min(r1, m_hi) - std::max(r0, m_lo) + 1;
                m_lo -= before;
                m_hi -= before + inside;
                if (m_hi < m_lo) m_pending = false;   // the whole range was removed
            }                               // entirely below: unchanged
        }
        m_target.beginRemove(first, count);
    }
    void endRemove() override { m_target.endRemove(); }

    //! A reset makes the view re-query every row, so a range pending before it is moot.
    void beginReset() override
    {
        m_pending = false;
        m_target.beginReset();
    }
    void endReset() override { m_target.endReset(); }

private:
    WindowCacheSink& m_target;
    std::function<int()> m_row_count;
    int m_depth = 0;
    bool m_pending = false;
    int m_lo = 0;
    int m_hi = 0;
    int m_last_lo = 0;
    int m_last_hi = 0;
};

//!
//! \brief Outcome of applying one structural delta.
//!
//! The structural apply* methods used to return a bare bool, which conflated two
//! completely different things: a delta correctly skipped because the cache
//! already reflects it (the routine PR4-fix B seqno gate), and a delta REJECTED
//! because its position does not fit the cache's virtual table. The first is
//! normal and happens on every reset refetch; the second means the producer's
//! cursor and this replica have diverged, which is a bug — and callers, having
//! only a bool, discarded both silently. Distinguishing them lets the host log
//! the divergence instead of swallowing it (#3257 review).
//!
enum class ApplyResult {
    Applied,           //!< the delta was applied and the seqno advanced
    AlreadyReflected,  //!< seqno <= the structural high-water; correctly skipped
    Rejected,          //!< malformed or out-of-range: replica/producer divergence
};

template <class Record>
class WindowCache
{
public:
    WindowCache() = default;

    // ---- queries -----------------------------------------------------------

    //! The virtual row count (full accepted set), owned by the structural channel.
    int total() const { return m_total; }
    //! Absolute index of the first cached row.
    int cacheFirst() const { return m_cache_first; }
    //! Number of contiguous cached rows: the slice is [cacheFirst, cacheFirst+cacheSize).
    int cacheSize() const { return static_cast<int>(m_rows.size()); }
    //! Structural high-water: the seqno of the last applied structural delta.
    uint64_t structuralSeqno() const { return m_structural_seqno; }
    //! The cursor sort/filter generation this cache is coherent with.
    uint64_t epoch() const { return m_epoch; }

    //! True if absolute `row` is currently in the cached slice.
    bool has(int row) const
    {
        return row >= m_cache_first
            && row < m_cache_first + static_cast<int>(m_rows.size());
    }

    //! Pointer to the cached record at absolute `row`, or nullptr (placeholder)
    //! if `row` is outside the slice. Valid until the next mutating call.
    const Record* at(int row) const
    {
        if (!has(row)) return nullptr;
        return &m_rows[static_cast<std::size_t>(row - m_cache_first)];
    }

    // ---- structural channel ------------------------------------------------

    //! Constructor-time / re-attach seed: set the whole cache with NO sink
    //! signals (the host model is being (re)built; it issues its own reset). Sets
    //! the structural baseline to max(high_water, 0) == high_water.
    void seedInitial(std::vector<Record> rows, int cache_first, int total,
                     uint64_t epoch, uint64_t high_water)
    {
        m_rows = std::move(rows);
        m_cache_first = cache_first;
        m_total = total;
        m_epoch = epoch;
        m_structural_seqno = high_water;
    }

    //! Structural Reset (sort/filter change, or a coalesced refill): replace the
    //! whole cache from a fresh fetch. Skipped if already reflected. The new
    //! structural baseline is max(high_water, seqno) — the fetch's high-water if
    //! it is ahead of the Reset event's own seqno (PR4-fix B). Brackets the swap
    //! in beginReset/endReset.
    ApplyResult applyReset(WindowCacheSink& sink, uint64_t seqno, std::vector<Record> rows,
                           int cache_first, int total, uint64_t epoch, uint64_t high_water)
    {
        if (seqno <= m_structural_seqno) return ApplyResult::AlreadyReflected;
        sink.beginReset();
        m_rows = std::move(rows);
        m_cache_first = cache_first;
        m_total = total;
        m_epoch = epoch;
        sink.endReset();
        m_structural_seqno = high_water > seqno ? high_water : seqno;
        return ApplyResult::Applied;
    }

    //! Structural Insert of `records.size()` rows at absolute `pos`. Shifts the
    //! cache base or splices into the slice as the position dictates (see below),
    //! grows m_total inside the begin/endInsert bracket, and advances the seqno.
    ApplyResult applyInsert(WindowCacheSink& sink, uint64_t seqno, int pos,
                            const std::vector<Record>& records)
    {
        if (seqno <= m_structural_seqno) return ApplyResult::AlreadyReflected;
        const int count = static_cast<int>(records.size());
        if (count <= 0) return ApplyResult::Rejected;    // empty insert: nothing to do
        if (pos < 0 || pos > m_total) return ApplyResult::Rejected;

        const int base = m_cache_first;
        const int len = static_cast<int>(m_rows.size());

        sink.beginInsert(pos, count);
        m_total += count;
        if (pos < base) {
            // Entirely before the window: the cached rows keep their content but
            // their absolute positions all shift up by `count`.
            m_cache_first = base + count;
        } else if (pos <= base + len) {
            // Inside the slice (incl. either edge): splice the new rows in so they
            // render immediately (no placeholder flash for a top-of-window insert).
            m_rows.insert(m_rows.begin() + static_cast<std::size_t>(pos - base),
                          records.begin(), records.end());
        }
        // pos > base + len: entirely after the window; only m_total changed.
        sink.endInsert();
        m_structural_seqno = seqno;
        return ApplyResult::Applied;
    }

    //! Structural Remove of `count` rows at absolute `pos`. Erases the overlap
    //! with the slice and shifts the cache base down by however many removed rows
    //! were strictly before it; shrinks m_total inside the begin/endRemove bracket.
    ApplyResult applyRemove(WindowCacheSink& sink, uint64_t seqno, int pos, int count)
    {
        if (seqno <= m_structural_seqno) return ApplyResult::AlreadyReflected;
        // Subtraction form, not pos+count > m_total: avoids int wrap on pathological
        // input (count > 0 holds by short-circuit when the comparison is reached).
        if (count <= 0 || pos < 0 || pos > m_total - count) return ApplyResult::Rejected;

        const int base = m_cache_first;
        const int len = static_cast<int>(m_rows.size());
        const int cs = base, ce = base + len;            // cache abs range [cs, ce)
        const int rs = pos, re = pos + count;            // removal abs range [rs, re)
        const int olo = cs > rs ? cs : rs;               // overlap = [max(cs,rs),
        const int ohi = ce < re ? ce : re;               //           min(ce,re))

        sink.beginRemove(pos, count);
        m_total -= count;
        if (olo < ohi) {
            m_rows.erase(m_rows.begin() + static_cast<std::size_t>(olo - base),
                         m_rows.begin() + static_cast<std::size_t>(ohi - base));
        }
        // Rows removed strictly before the cache start pull the base down.
        const int min_re_cs = re < cs ? re : cs;
        const int removed_before_cache = (min_re_cs - rs) > 0 ? (min_re_cs - rs) : 0;
        m_cache_first = base - removed_before_cache;
        sink.endRemove();
        m_structural_seqno = seqno;
        return ApplyResult::Applied;
    }

    //! Structural Change: `count` rows at absolute `pos` were refreshed in place
    //! (no reorder). `fresh` holds the new records for [pos, pos+count) — the caller
    //! fetches exactly `count` rows via getRows, so fresh.size() == count in
    //! practice. The in-cache overlap is updated from `fresh` and dataChanged is
    //! emitted for the rows actually refreshed. No size change. The seqno is advanced
    //! UNCONDITIONALLY: a Change does not move rows, so the structural state
    //! (positions + count) is already correct and the event must be consumed exactly
    //! once. If `fresh` is shorter than `count` (a contract violation), the uncovered
    //! rows keep their cached values rather than reading out of bounds; the content
    //! channel refreshes them on the next fetch.
    ApplyResult applyChange(WindowCacheSink& sink, uint64_t seqno, int pos, int count,
                            const std::vector<Record>& fresh)
    {
        if (seqno <= m_structural_seqno) return ApplyResult::AlreadyReflected;
        // Subtraction form, not pos+count > m_total: avoids int wrap on pathological
        // input (count > 0 holds by short-circuit when the comparison is reached).
        if (count <= 0 || pos < 0 || pos > m_total - count) return ApplyResult::Rejected;

        const int base = m_cache_first;
        const int len = static_cast<int>(m_rows.size());
        const int cs = base, ce = base + len;
        const int rs = pos, re = pos + count;
        const int olo = cs > rs ? cs : rs;
        const int ohi = ce < re ? ce : re;
        // Update only the overlap we have records for.
        int updated_lo = -1, updated_hi = -1;
        for (int abs = olo; abs < ohi; ++abs) {
            const int fresh_idx = abs - pos;
            if (fresh_idx < 0 || fresh_idx >= static_cast<int>(fresh.size())) continue;
            m_rows[static_cast<std::size_t>(abs - base)] = fresh[static_cast<std::size_t>(fresh_idx)];
            if (updated_lo < 0) updated_lo = abs;
            updated_hi = abs;
        }
        m_structural_seqno = seqno;
        if (updated_lo >= 0) {
            sink.dataChanged(updated_lo, updated_hi - updated_lo + 1);
        }
        return ApplyResult::Applied;
    }

    // ---- content channel ---------------------------------------------------

    //! Adopt a scroll-driven slice fetch for [first, first+rows.size()). Adopted
    //! ONLY when the fetch reflects exactly the consumer's structural state:
    //! epoch == this cache's epoch (no intervening resort/refilter) AND
    //! high_water == m_structural_seqno (no intervening insert/remove in either
    //! direction). Otherwise the fetch is discarded (returns false) — the consumer
    //! re-requests after the next drain settles the seqno. NEVER touches m_total
    //! or m_structural_seqno. Emits dataChanged over the adopted slice so the view
    //! re-queries the rows that just became available (placeholder -> data).
    bool fillContent(WindowCacheSink& sink, int first, std::vector<Record> rows,
                     uint64_t epoch, uint64_t high_water)
    {
        if (epoch != m_epoch) return false;                 // raced a resort/refilter
        if (high_water != m_structural_seqno) return false; // raced an insert/remove
        const int count = static_cast<int>(rows.size());
        // Reject a slice that would fall outside the virtual table [0, m_total).
        // Defensive, and consistent with applyInsert/applyRemove: a legitimate
        // fetch is clamped by getRows to the served window, and the matched-seqno
        // gate above guarantees the fetch's total equals m_total, so first+count
        // <= m_total always holds for a well-formed fetch. A malformed one must not
        // set the slice past the table end, or has()/at() would report rows
        // >= m_total as present, corrupting the [cacheFirst, cacheFirst+cacheSize)
        // invariant (PR5-A review finding). Subtraction form (count >= 0) avoids the
        // int wrap a first+count comparison would have on pathological input.
        if (first < 0 || first > m_total - count) return false;
        m_cache_first = first;
        m_rows = std::move(rows);
        if (count > 0) sink.dataChanged(first, count);
        return true;
    }

private:
    int m_total = 0;                  //!< virtual row count (structural channel owns it)
    int m_cache_first = 0;            //!< absolute index of m_rows[0]
    std::vector<Record> m_rows;       //!< the cached contiguous slice
    uint64_t m_structural_seqno = 0;  //!< high-water of applied structural deltas
    uint64_t m_epoch = 0;             //!< cursor sort/filter generation the cache matches
};

//!
//! \brief Close a drained batch on \p sink, first refreshing \p cache's cached slice if the
//! batch carried a new chain tip.
//!
//! Confirmation and maturity progress are derived from the tip height at render time
//! (GRC::DisplayTxStatus), and the producer no longer re-sends a record when only those moved,
//! so a new tip changes what every cached row DISPLAYS without any row event. The slice is
//! added after every structural event of the batch, so it is already in final coordinates and
//! inside the table by WindowCache's own invariants, and it covers a slice a reset rebuilt
//! earlier in the batch; the sink unions it with the batch's other changes, so the batch still
//! emits at most one dataChanged. An empty slice adds nothing.
//!
template <class Record>
CoalescingSink::EndResult CloseBatch(CoalescingSink& sink, const WindowCache<Record>& cache, bool saw_tip)
{
    if (saw_tip && cache.cacheSize() > 0) {
        sink.dataChanged(cache.cacheFirst(), cache.cacheSize());
    }
    return sink.endBatch();
}

} // namespace GRC

#endif // GRIDCOIN_QT_WINDOWCACHE_H
