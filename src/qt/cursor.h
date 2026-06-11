// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_CURSOR_H
#define GRIDCOIN_QT_CURSOR_H

#include "qt/txfilter.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

//!
//! \file cursor.h
//!
//! Qt-free per-view cursor for the windowed transaction-table model (PR3).
//!
//! A cursor maintains one filtered + sorted view over the producer's record
//! table: an ordered `view_index` of the ABSOLUTE record indices that pass the
//! view's GRC::FilterSpec, sorted by GRC::Less for the view's column/order. The
//! store-worker (PR2.5) drives it from the off-cs_main worker thread; this core
//! holds no records and no Qt types, so the same logic is unit-tested in the
//! ENABLE_GUI=OFF configuration CI actually exercises (the PR1 risk-control
//! move). See doc/transaction_table_windowed_model.md → "Addendum: the per-view
//! cursor view_index-maintenance algorithm" for the spec this implements,
//! including the four arithmetic hazards (identity-locate, erase-then-lower_bound,
//! off-window maintenance, reindex/splice discipline) and the served-window
//! translation (eviction/promotion at the boundary).
//!
//! The cursor is parameterized on two projector callables that read the backing
//! table (the store's m_records in production, a synthetic table in the tests):
//!   - FieldsFn(absidx) -> TxFilterFields  (filter inputs)
//!   - KeysFn(absidx)   -> SortKey         (sort inputs)
//! Callers MUST mutate the backing table BEFORE calling the corresponding
//! apply* method, so the projectors observe the post-mutation table.
//!

namespace GRC {

//!
//! \brief One change to the served window, in served-window-local coordinates.
//!
//! The consumer applies these to its replica: Reset (bulk refill `count` rows
//! via getRows), Insert/Remove `count` rows at `first`, or Change (re-read)
//! `count` rows at `first`.
//!
struct CursorDelta {
    enum Type { Reset, Insert, Remove, Change } type;
    int first;
    int count;
};

class Cursor
{
public:
    // Return by const reference: the store caches the projected fields/keys per
    // record (computed once at insert/update/refresh), so a sort comparison reads
    // the cache without re-projecting or re-allocating per comparison (PR4-fix F).
    // The reference is valid for the duration of the call — the caller holds
    // cs_store throughout any sort/lower_bound, so the backing cache cannot move.
    using FieldsFn = std::function<const TxFilterFields&(std::size_t)>;
    using KeysFn   = std::function<const SortKey&(std::size_t)>;

    Cursor(int view_id, FilterSpec filter, int sort_column, int sort_order,
           FieldsFn fields, KeysFn keys);

    //! Rebuild from scratch over the `n` records [0, n). Returns one Reset.
    std::vector<CursorDelta> rebuild(std::size_t n);

    //! The table inserted `count` records at absolute [P, P+count) (later
    //! absolute indices shifted +count). Shift-then-insert (spec Item 4).
    std::vector<CursorDelta> applyStoreInsert(std::size_t P, std::size_t count);

    //! The table removed absolute [P, P+count) (later absolute indices shifted
    //! -count). Remove-then-shift (spec Item 4). No projector reads needed.
    std::vector<CursorDelta> applyStoreRemove(std::size_t P, std::size_t count);

    //! Record `P`'s status/fields changed: re-evaluate membership + sort slot,
    //! maintaining view_index for ALL rows incl. off-window (spec Item 3),
    //! locating by identity (Item 1) and repositioning erase-then-lower_bound
    //! (Item 2).
    std::vector<CursorDelta> applyStatusUpdate(std::size_t P);

    //! Change the served-window cap (the stairstep resize). view_index is
    //! untouched; only the boundary moves (promotion/eviction at the edge).
    std::vector<CursorDelta> setLimit(int new_limit);

    //! Change sort column/order. Wholesale re-sort → single Reset.
    std::vector<CursorDelta> setSort(int sort_column, int sort_order);

    //! Change the filter. Wholesale re-evaluate over [0, n) → single Reset.
    std::vector<CursorDelta> setFilter(const FilterSpec& filter, std::size_t n);

    int viewId() const { return m_view_id; }
    uint64_t epoch() const { return m_epoch; }

    //! Number of rows the consumer sees: min(accepted, cap).
    std::size_t servedCount() const;
    //! Total accepted rows (the full sorted index size).
    std::size_t totalAccepted() const { return m_view_index.size(); }
    //! Absolute record index at served position `served_pos` (for getRows).
    std::size_t rowAt(std::size_t served_pos) const { return m_view_index[served_pos]; }

    //! Test/inspection: the full sorted absolute-index vector.
    const std::vector<std::size_t>& viewIndex() const { return m_view_index; }

private:
    //! Total order over absolute indices: CompareKeys for the active column/order,
    //! then the native record index as a deterministic tie-breaker. m_records is
    //! kept in RecordOrder (time, hash, idx), so a lower absolute index IS the
    //! earlier native record — this both makes ties reproducible across re-sorts
    //! and keeps the decomposed parts of one transaction contiguous. The proxy
    //! got this for free from QSortFilterProxyModel's stable_sort over the native
    //! source order; the cursor's std::sort needs the explicit total order.
    bool lessIndexed(std::size_t a, std::size_t b) const;

    //! Sorted insert slot for `absidx` via lower_bound over lessIndexed. Used ONLY
    //! for a row not currently present (never to locate an existing row — Item 1).
    std::size_t lowerBoundSlot(std::size_t absidx) const;
    //! Identity locate: position of absolute index `absidx` in view_index, or
    //! npos. Linear scan by value (Item 1).
    std::size_t findSlot(std::size_t absidx) const;
    //! Cap as a size_t (SIZE_MAX when unlimited).
    std::size_t cap() const;

    int m_view_id;
    FilterSpec m_filter;
    int m_sort_column;
    int m_sort_order;
    uint64_t m_epoch{0};

    FieldsFn m_fields;
    KeysFn m_keys;

    //! Accepted absolute indices, sorted by Less(Keys(a), Keys(b), col, order).
    std::vector<std::size_t> m_view_index;
};

} // namespace GRC

#endif // GRIDCOIN_QT_CURSOR_H
