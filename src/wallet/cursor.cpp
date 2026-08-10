// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/cursor.h"

#include <algorithm>
#include <cstdint>

namespace {
constexpr std::size_t NPOS = static_cast<std::size_t>(-1);
} // anonymous namespace

namespace GRC {

Cursor::Cursor(int view_id, FilterSpec filter, int sort_column, int sort_order,
               FieldsFn fields, KeysFn keys)
    : m_view_id(view_id)
    , m_filter(std::move(filter))
    , m_sort_column(sort_column)
    , m_sort_order(sort_order)
    , m_fields(std::move(fields))
    , m_keys(std::move(keys))
{
}

std::size_t Cursor::cap() const
{
    return m_filter.limit_rows < 0 ? NPOS : static_cast<std::size_t>(m_filter.limit_rows);
}

std::size_t Cursor::servedCount() const
{
    return std::min(m_view_index.size(), cap());
}

bool Cursor::lessIndexed(std::size_t a, std::size_t b) const
{
    const int c = CompareKeys(m_keys(a), m_keys(b), m_sort_column, m_sort_order);
    return c != 0 ? c < 0 : a < b;   // tie -> native record order (lower absidx first)
}

std::size_t Cursor::lowerBoundSlot(std::size_t absidx) const
{
    // Sorted insertion slot. Only valid for a row NOT currently present.
    auto it = std::lower_bound(
        m_view_index.begin(), m_view_index.end(), absidx,
        [&](std::size_t a, std::size_t b) { return lessIndexed(a, b); });
    return static_cast<std::size_t>(it - m_view_index.begin());
}

std::size_t Cursor::findSlot(std::size_t absidx) const
{
    // Identity locate (spec Item 1): never a sort-key binary search.
    auto it = std::find(m_view_index.begin(), m_view_index.end(), absidx);
    return it == m_view_index.end() ? NPOS : static_cast<std::size_t>(it - m_view_index.begin());
}

std::vector<CursorDelta> Cursor::rebuild(std::size_t n)
{
    m_view_index.clear();
    for (std::size_t i = 0; i < n; ++i) {
        if (Accepts(m_fields(i), m_filter)) m_view_index.push_back(i);
    }
    std::sort(m_view_index.begin(), m_view_index.end(),
              [&](std::size_t a, std::size_t b) { return lessIndexed(a, b); });
    ++m_epoch;
    return {{CursorDelta::Reset, 0, static_cast<int>(servedCount())}};
}

std::vector<std::size_t> Cursor::rowsAt(std::size_t first, std::size_t count) const
{
    std::vector<std::size_t> out;
    if (first >= m_view_index.size()) {
        return out;
    }
    const std::size_t end = std::min(m_view_index.size(), first + count);
    out.reserve(end - first);
    for (std::size_t i = first; i < end; ++i) {
        out.push_back(m_view_index[i]);
    }
    return out;
}

// Served-window translation for a single-row INSERT that already happened at
// `slot` (m_view_index has grown by 1). Eviction when the window was full.
void Cursor::emitInsertAt(std::size_t slot, std::size_t absidx, std::size_t cap_v,
                          std::vector<CursorDelta>& out) const
{
    const std::size_t size_after    = m_view_index.size();
    const std::size_t served_before = std::min(size_after - 1, cap_v);
    const std::size_t served_after  = std::min(size_after, cap_v);
    if (slot < served_after) {
        // Stamp the record now: `slot` is only meaningful against the view_index
        // as it stands at this instant (CursorDelta::rows).
        out.push_back({CursorDelta::Insert, static_cast<int>(slot), 1, {absidx}});
        if (served_before == cap_v) {  // window was full → last visible row evicted
            out.push_back({CursorDelta::Remove, static_cast<int>(cap_v), 1, {}});
        }
    }
}

// Served-window translation for a single-row REMOVE that already happened at
// `pos` (size_before = size before the erase). Promotion when an off-window row exists.
void Cursor::emitRemoveAt(std::size_t pos, std::size_t size_before, std::size_t cap_v,
                          std::vector<CursorDelta>& out) const
{
    const std::size_t served_before = std::min(size_before, cap_v);
    if (pos < served_before) {
        out.push_back({CursorDelta::Remove, static_cast<int>(pos), 1, {}});
        if (size_before > cap_v) {  // an off-window row promotes into the last visible slot
            // Capture the promoted record HERE. The erase has happened, so
            // view_index holds size_before-1 >= cap_v entries and cap_v-1 is in
            // range — but a later erase in the same batch can shrink it below
            // cap_v, which is exactly what made resolving this coordinate after
            // the batch an out-of-bounds read (#3257 review).
            out.push_back({CursorDelta::Insert, static_cast<int>(cap_v - 1), 1,
                           rowsAt(cap_v - 1, 1)});
        }
    }
}

std::vector<CursorDelta> Cursor::applyStoreInsert(std::size_t P, std::size_t count)
{
    std::vector<CursorDelta> out;
    const std::size_t cap_v = cap();
    // (a) the table grew at P → every later absolute index shifted +count.
    for (auto& e : m_view_index) {
        if (e >= P) e += count;
    }
    // (b) insert each newly-accepted record at its sorted slot.
    for (std::size_t i = P; i < P + count; ++i) {
        if (Accepts(m_fields(i), m_filter)) {
            const std::size_t slot = lowerBoundSlot(i);
            m_view_index.insert(m_view_index.begin() + slot, i);
            emitInsertAt(slot, i, cap_v, out);
        }
    }
    return out;
}

std::vector<CursorDelta> Cursor::applyStoreRemove(std::size_t P, std::size_t count)
{
    std::vector<CursorDelta> out;
    const std::size_t cap_v = cap();
    const std::size_t size_before = m_view_index.size();

    // Erase EVERY target entry before emitting anything, then translate against the
    // final index. Emitting per-erase (interleaved, as this used to) promotes a row
    // into the window as each part goes; when a transaction's parts straddle the
    // cap — one served, its sibling the first off-window row — the promotion picks
    // up that sibling, which the very next erase then removes. The delta would
    // carry a record the store has ALREADY erased from m_records (removeLocked
    // erases before driving the cursors), so the payload named a foreign row, or
    // ran off the end when the removed run sat at the table end (#3257 review).
    // Deferring the translation makes every emitted record come from the final,
    // fully-renumbered index, so a doomed row can never be named.

    // (a) locate the entries whose absolute value is in [P, P+count), by identity.
    std::vector<std::size_t> positions;
    for (std::size_t v = P; v < P + count; ++v) {
        const std::size_t pos = findSlot(v);
        if (pos != NPOS) positions.push_back(pos);
    }
    if (positions.empty()) {
        return out;   // none of the removed records was in this view
    }
    std::sort(positions.begin(), positions.end());
    for (std::size_t i = positions.size(); i-- > 0;) {
        m_view_index.erase(m_view_index.begin() + positions[i]);   // high to low
    }

    // (b) the table shrank by `count` at P → later survivors shift -count.
    for (auto& e : m_view_index) {
        if (e >= P + count) e -= count;
    }

    // (c) served-window translation. One Remove per erased row that WAS visible,
    // in ascending order — each already-emitted Remove has shifted the consumer's
    // later rows down by one, hence the running adjustment.
    const std::size_t served_before = std::min(size_before, cap_v);
    const std::size_t served_after  = std::min(m_view_index.size(), cap_v);
    std::size_t removed_in_window = 0;
    for (const std::size_t pos : positions) {
        if (pos >= served_before) continue;   // off-window: the consumer never had it
        out.push_back({CursorDelta::Remove,
                       static_cast<int>(pos - removed_in_window), 1, {}});
        ++removed_in_window;
    }

    // Off-window rows promoting into the freed slots, appended in one batch. Their
    // records come from the final index, so they are correct by construction.
    const std::size_t visible_now = served_before - removed_in_window;
    if (served_after > visible_now) {
        const std::size_t promoted = served_after - visible_now;
        out.push_back({CursorDelta::Insert, static_cast<int>(visible_now),
                       static_cast<int>(promoted), rowsAt(visible_now, promoted)});
    }
    return out;
}

std::vector<CursorDelta> Cursor::applyStatusUpdate(std::size_t P)
{
    std::vector<CursorDelta> out;
    const std::size_t cap_v = cap();
    const std::size_t old_pos = findSlot(P);          // identity (Item 1)
    const bool old_in = old_pos != NPOS;
    const bool new_in = Accepts(m_fields(P), m_filter);

    if (!old_in && !new_in) {
        return out;                                   // not shown before or after
    }
    if (!old_in && new_in) {                          // membership flip-in
        const std::size_t slot = lowerBoundSlot(P);
        m_view_index.insert(m_view_index.begin() + slot, P);
        emitInsertAt(slot, P, cap_v, out);
        return out;
    }
    if (old_in && !new_in) {                          // membership flip-out
        const std::size_t size_before = m_view_index.size();
        m_view_index.erase(m_view_index.begin() + old_pos);
        emitRemoveAt(old_pos, size_before, cap_v, out);
        return out;
    }

    // old_in && new_in: stays a member; may reposition. Erase-then-lower_bound
    // (Item 2). Total size is unchanged, so no membership promote/evict — only a
    // possible boundary CROSSING of the moved row itself.
    const std::size_t size_full = m_view_index.size();
    m_view_index.erase(m_view_index.begin() + old_pos);
    const std::size_t new_slot = lowerBoundSlot(P);
    m_view_index.insert(m_view_index.begin() + new_slot, P);

    const std::size_t served_full = std::min(size_full, cap_v);
    if (new_slot == old_pos) {                        // unchanged slot → just re-read
        if (old_pos < served_full) out.push_back({CursorDelta::Change, static_cast<int>(old_pos), 1, {}});
        return out;
    }
    const bool old_vis = old_pos < served_full;
    const bool new_vis = new_slot < served_full;
    if (old_vis && new_vis) {                          // reorder within the window
        out.push_back({CursorDelta::Remove, static_cast<int>(old_pos), 1, {}});
        out.push_back({CursorDelta::Insert, static_cast<int>(new_slot), 1, {P}});
    } else if (old_vis && !new_vis) {                  // moved out of the window
        out.push_back({CursorDelta::Remove, static_cast<int>(old_pos), 1, {}});
        // The row that promotes into the last visible slot, captured now.
        if (size_full > cap_v) out.push_back({CursorDelta::Insert, static_cast<int>(served_full - 1), 1,
                                              rowsAt(served_full - 1, 1)});
    } else if (!old_vis && new_vis) {                  // moved into the window
        out.push_back({CursorDelta::Insert, static_cast<int>(new_slot), 1, {P}});
        if (size_full > cap_v) out.push_back({CursorDelta::Remove, static_cast<int>(served_full), 1, {}});
    }
    // !old_vis && !new_vis: off-window move, no emission (view_index still maintained).
    return out;
}

std::vector<CursorDelta> Cursor::setLimit(int new_limit)
{
    std::vector<CursorDelta> out;
    const std::size_t served_old = servedCount();
    m_filter.limit_rows = new_limit;
    const std::size_t served_new = servedCount();
    if (served_new > served_old) {
        out.push_back({CursorDelta::Insert, static_cast<int>(served_old),
                       static_cast<int>(served_new - served_old),
                       rowsAt(served_old, served_new - served_old)});
    } else if (served_new < served_old) {
        out.push_back({CursorDelta::Remove, static_cast<int>(served_new),
                       static_cast<int>(served_old - served_new), {}});
    }
    return out;
}

std::vector<CursorDelta> Cursor::setSort(int sort_column, int sort_order)
{
    m_sort_column = sort_column;
    m_sort_order = sort_order;
    std::sort(m_view_index.begin(), m_view_index.end(),
              [&](std::size_t a, std::size_t b) { return lessIndexed(a, b); });
    ++m_epoch;
    return {{CursorDelta::Reset, 0, static_cast<int>(servedCount())}};
}

std::vector<CursorDelta> Cursor::setFilter(const FilterSpec& filter, std::size_t n)
{
    // Preserve the served-window cap. limit_rows rides along in FilterSpec, but it
    // is owned by setLimit (the view's own resize), not by the caller's filter —
    // and a caller building a FilterSpec from the UI's filter controls leaves it at
    // the -1 default, which would silently uncap the view. Latent today, since no
    // view both filters and caps, but the two are independent knobs and the wire
    // type should not couple them.
    const int32_t cap_kept = m_filter.limit_rows;
    m_filter = filter;
    m_filter.limit_rows = cap_kept;
    return rebuild(n);
}

} // namespace GRC
